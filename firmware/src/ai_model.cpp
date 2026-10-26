#include "ai_model.h"
#include "config.h"
#include "camera.h"
#include <Arduino.h>
#include <string.h>
#include <esp_heap_caps.h>

#include <tflm_esp32.h>
#include <tensorflow/lite/micro/micro_profiler.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <tensorflow/lite/micro/micro_context.h>
#include <tensorflow/lite/micro/kernels/kernel_util.h>
#include <tensorflow/lite/micro/micro_context.h>
#include <tensorflow/lite/kernels/kernel_util.h>
#include <tensorflow/lite/c/builtin_op_data.h>
#include <tensorflow/lite/kernels/internal/quantization_util.h>
#include <tensorflow/lite/kernels/internal/common.h>

#include "../models/waste_model_data.h"

// TFLM needs ~101,376 B arena for this int8 model; 102KB fits the largest
// internal-DRAM block (110KB) once region_buf is off .bss.
#define TENSOR_ARENA_SIZE (107 * 1024)
// 1 = print per-invoke timing + TFLM per-op tick CSV (needs TF_LITE_USE_CTIME)
#define AI_PROFILE 0

#define TF_NUM_INPUTS  (INFERENCE_SIZE * INFERENCE_SIZE * 3)
#define TF_NUM_OUTPUTS NUM_CLASSES

// ---------------------------------------------------------------------------
// Fast int8 CONV_2D kernel.
// TFLM's reference kernel measured 3309/3371 ms per invoke (135 cycles/MAC,
// dominated by per-MAC RuntimeShape::Offset + bounds branching), while a
// plain pointer-walk MAC loop runs at ~21 cycles/MAC on this chip. This
// kernel implements the same math (SAME/VALID, stride, dilation, per-channel
// requantization, bias, NONE/RELU/RELU6 activations) with flattened pointer
// arithmetic.
// ---------------------------------------------------------------------------
#define FAST_CONV_MAX_CH 64

struct FastConvData {
    int32_t mult[FAST_CONV_MAX_CH];
    int32_t shift[FAST_CONV_MAX_CH];
    int32_t input_offset;
    int32_t output_offset;
    int32_t act_min;
    int32_t act_max;
    int stride_h, stride_w;
    int dil_h, dil_w;
    int pad_top, pad_left;
    int fh, fw;
};

static TfLiteStatus FastConvPrepare(TfLiteContext* ctx, TfLiteNode* node) {
    using namespace tflite;
    // TFLM prepare contract: tensors fetched via AllocateTemp*Tensor must be
    // freed before returning (else FinishPrepareNodeAllocations fails).
    auto* micro_context = tflite::GetMicroContext(ctx);
    TfLiteTensor* input = micro_context->AllocateTempInputTensor(node, 0);
    TfLiteTensor* filter = micro_context->AllocateTempInputTensor(node, 1);
    TfLiteTensor* bias = (tflite::NumInputs(node) == 3)
                             ? micro_context->AllocateTempInputTensor(node, 2)
                             : nullptr;
    TfLiteTensor* output = micro_context->AllocateTempOutputTensor(node, 0);
    auto free_all = [&]() {
        if (input) micro_context->DeallocateTempTfLiteTensor(input);
        if (filter) micro_context->DeallocateTempTfLiteTensor(filter);
        if (bias) micro_context->DeallocateTempTfLiteTensor(bias);
        if (output) micro_context->DeallocateTempTfLiteTensor(output);
    };
    if (!input || !filter || !output) {
        free_all();
        return kTfLiteError;
    }

    if (input->type != kTfLiteInt8 || filter->type != kTfLiteInt8 ||
        output->type != kTfLiteInt8) {
        MicroPrintf("FastConv: int8 model required (in=%d filt=%d out=%d)",
                    (int)input->type, (int)filter->type, (int)output->type);
        free_all();
        return kTfLiteError;
    }
    if (bias && bias->type != kTfLiteInt32) {
        MicroPrintf("FastConv: bias must be int32");
        free_all();
        return kTfLiteError;
    }

    const int in_h = input->dims->data[1], in_w = input->dims->data[2];
    const int in_c = input->dims->data[3];
    const int out_c = output->dims->data[3];
    const int fh = filter->dims->data[1], fw = filter->dims->data[2];
    const int f_ic = filter->dims->data[3];
    if (f_ic != in_c) {
        MicroPrintf("FastConv: grouped conv unsupported (%d vs %d)", f_ic, in_c);
        free_all();
        return kTfLiteError;
    }
    if (out_c > FAST_CONV_MAX_CH) {
        MicroPrintf("FastConv: channels %d > %d", out_c, FAST_CONV_MAX_CH);
        free_all();
        return kTfLiteError;
    }

    const auto* params = reinterpret_cast<const TfLiteConvParams*>(node->builtin_data);
    if (!params) {
        free_all();
        return kTfLiteError;
    }

    auto* d = (FastConvData*)ctx->AllocatePersistentBuffer(ctx, sizeof(FastConvData));
    if (!d) {
        free_all();
        return kTfLiteError;
    }
    memset(d, 0, sizeof(FastConvData));

    d->fh = fh;
    d->fw = fw;
    d->stride_h = params->stride_height;
    d->stride_w = params->stride_width;
    d->dil_h = params->dilation_height_factor ? params->dilation_height_factor : 1;
    d->dil_w = params->dilation_width_factor ? params->dilation_width_factor : 1;
    d->input_offset = -input->params.zero_point;
    d->output_offset = output->params.zero_point;

    const int eff_fh = (fh - 1) * d->dil_h + 1;
    const int eff_fw = (fw - 1) * d->dil_w + 1;
    if (params->padding == kTfLitePaddingSame) {
        const int ph = (output->dims->data[1] - 1) * d->stride_h + eff_fh - in_h;
        const int pw = (output->dims->data[2] - 1) * d->stride_w + eff_fw - in_w;
        d->pad_top = ph > 0 ? ph / 2 : 0;
        d->pad_left = pw > 0 ? pw / 2 : 0;
    } else {
        d->pad_top = 0;
        d->pad_left = 0;
    }

    // activation range in quantized output domain
    d->act_min = -128;
    d->act_max = 127;
    const float out_s = output->params.scale;
    const int32_t out_zp = output->params.zero_point;
    if (params->activation == kTfLiteActRelu) {
        d->act_min = out_zp > -128 ? out_zp : -128;
    } else if (params->activation == kTfLiteActRelu6) {
        d->act_min = out_zp > -128 ? out_zp : -128;
        int32_t q6 = (int32_t)__builtin_lroundf(6.0f / out_s) + out_zp;
        if (q6 < d->act_max) d->act_max = q6;
    }

    // per-channel output scale = input_scale * filter_scale[oc] / output_scale
    const float in_s = input->params.scale;
    if (filter->quantization.type != kTfLiteAffineQuantization ||
        !filter->quantization.params) {
        MicroPrintf("FastConv: filter needs affine per-channel quantization");
        free_all();
        return kTfLiteError;
    }
    const auto* aq = (const TfLiteAffineQuantization*)filter->quantization.params;
    if (!aq->scale || aq->scale->size < out_c) {
        MicroPrintf("FastConv: missing filter quantization scales");
        free_all();
        return kTfLiteError;
    }
    const float* fscales = aq->scale->data;
    for (int oc = 0; oc < out_c; oc++) {
        double scale = (double)in_s * (double)fscales[oc] / (double)out_s;
        tflite::QuantizeMultiplier(scale, &d->mult[oc], &d->shift[oc]);
    }

    free_all();
    node->user_data = d;
    return kTfLiteOk;
}

static TfLiteStatus FastConvEval(TfLiteContext* ctx, TfLiteNode* node) {
    const TfLiteEvalTensor* in_t = tflite::micro::GetEvalInput(ctx, node, 0);
    const TfLiteEvalTensor* w_t = tflite::micro::GetEvalInput(ctx, node, 1);
    const TfLiteEvalTensor* b_t = (tflite::NumInputs(node) == 3)
                                      ? tflite::micro::GetEvalInput(ctx, node, 2)
                                      : nullptr;
    TfLiteEvalTensor* out_t = tflite::micro::GetEvalOutput(ctx, node, 0);
    const auto* d = (const FastConvData*)node->user_data;
    if (!d || !in_t || !w_t || !out_t) return kTfLiteError;

    const int8_t* input = in_t->data.int8;
    const int8_t* weight = w_t->data.int8;
    const int32_t* bias = b_t ? b_t->data.i32 : nullptr;
    int8_t* output = out_t->data.int8;

    const int batches = in_t->dims->data[0];
    const int in_h = in_t->dims->data[1];
    const int in_w = in_t->dims->data[2];
    const int in_c = in_t->dims->data[3];
    const int out_c = out_t->dims->data[3];
    const int out_h = out_t->dims->data[1];
    const int out_w = out_t->dims->data[2];
    const int fh = d->fh, fw = d->fw;
    const int fw_ic = fw * in_c;      // filter row stride
    const int fh_fw_ic = fh * fw_ic;  // filter plane stride (per channel)
    const int in_row = in_w * in_c;   // input row stride
    const int out_row = out_w * out_c;

    const int32_t in_off = d->input_offset;
    const int32_t out_off = d->output_offset;
    const int32_t act_min = d->act_min, act_max = d->act_max;
    const int sh = d->stride_h, sw = d->stride_w;
    const int dh = d->dil_h, dw = d->dil_w;
    const int pt = d->pad_top, pl = d->pad_left;

    for (int b = 0; b < batches; b++) {
        const int8_t* in_b = input + (size_t)b * in_h * in_row;
        int8_t* out_b = output + (size_t)b * out_h * out_row;
        for (int oy = 0; oy < out_h; oy++) {
            int iy0 = oy * sh - pt;
            int8_t* out_r = out_b + (size_t)oy * out_row;
            for (int ox = 0; ox < out_w; ox++) {
                int ix0 = ox * sw - pl;
                int8_t* out_p = out_r + (size_t)ox * out_c;
                for (int oc = 0; oc < out_c; oc++) {
                    const int8_t* w = weight + (size_t)oc * fh_fw_ic;
                    int32_t acc = bias ? bias[oc] : 0;
                    int iy = iy0;
                    for (int fy = 0; fy < fh; fy++, iy += dh) {
                        if ((unsigned)iy >= (unsigned)in_h) { w += fw_ic; continue; }
                        const int8_t* in_r = in_b + (size_t)iy * in_row;
                        int ix = ix0;
                        for (int fx = 0; fx < fw; fx++, ix += dw) {
                            if ((unsigned)ix >= (unsigned)in_w) { w += in_c; continue; }
                            const int8_t* xr = in_r + (size_t)ix * in_c;
                            const int8_t* wp = w;
                            for (int ic = 0; ic < in_c; ic++) {
                                acc += (int32_t)wp[ic] * ((int32_t)xr[ic] + in_off);
                            }
                            w += in_c;
                        }
                    }
                    acc = tflite::MultiplyByQuantizedMultiplier(acc, d->mult[oc],
                                                                d->shift[oc]);
                    acc += out_off;
                    if (acc < act_min) acc = act_min;
                    if (acc > act_max) acc = act_max;
                    out_p[oc] = (int8_t)acc;
                }
            }
        }
    }
    return kTfLiteOk;
}

static TFLMRegistration FastConvReg() {
    TFLMRegistration r = tflite::micro::RegisterOp(nullptr, FastConvPrepare, FastConvEval);
    // MicroMutableOpResolver::AddBuiltin normally fills this; a hand-made
    // registration leaves it 0 (== BuiltinOperator_ADD) and AllocateTensors
    // then looks up the parser for the wrong op.
    r.builtin_code = tflite::BuiltinOperator_CONV_2D;
    return r;
}

// Wrapper resolver: serves the fast CONV_2D registration, everything else
// (including the CONV_2D builtin_data parser) comes from the inner resolver.
class FastConvResolver : public tflite::MicroOpResolver {
public:
    tflite::MicroMutableOpResolver<8> inner;
    FastConvResolver() {
        inner.AddConv2D();  // registration unused; parser still registered
        inner.AddMaxPool2D();
        inner.AddMean();
        inner.AddFullyConnected();
        inner.AddSoftmax();
        inner.AddDequantize();
        inner.AddQuantize();
        inner.AddReshape();
    }
    const TFLMRegistration* FindOp(tflite::BuiltinOperator op) const override {
        if (op == tflite::BuiltinOperator_CONV_2D) {
            static const TFLMRegistration reg = FastConvReg();
            return &reg;
        }
        return inner.FindOp(op);
    }
    const TFLMRegistration* FindOp(const char* op) const override {
        return inner.FindOp(op);
    }
    tflite::TfLiteBridgeBuiltinParseFunction GetOpDataParser(
        tflite::BuiltinOperator op) const override {
        return inner.GetOpDataParser(op);
    }
};

// ---------------------------------------------------------------------------
// Model / interpreter state
// ---------------------------------------------------------------------------
static FastConvResolver* resolver = nullptr;
static tflite::MicroInterpreter* interp = nullptr;
static tflite::MicroProfiler* profiler = nullptr;
static TfLiteTensor* tin = nullptr;
static TfLiteTensor* tout = nullptr;
static bool g_ok = false;

bool ai_init(void) {
    Serial.printf("[ai] sizeof-free: largest_dram=%u free_dram=%u\n",
                  (unsigned)heap_caps_get_largest_free_block(
                      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                  (unsigned)heap_caps_get_free_size(
                      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    // model weights must live in DRAM (flash/PSRAM thrash cache)
    static uint8_t* model_ram = nullptr;
    if (!model_ram) {
        model_ram = (uint8_t*)heap_caps_malloc(waste_model_data_len,
                        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!model_ram) model_ram = (uint8_t*)ps_malloc(waste_model_data_len);
        if (model_ram) {
            memcpy(model_ram, waste_model_data, waste_model_data_len);
            uint32_t a = (uint32_t)model_ram;
            bool is_psram = (a >= 0x3F800000 && a < 0x3FF00000);
            Serial.printf("[ai] model copy %s (%u B) ram=%p\n",
                          is_psram ? "PSRAM" : "DRAM",
                          (unsigned)waste_model_data_len, (void*)model_ram);
        } else {
            Serial.println("[ai] WARN model RAM copy failed, using flash");
            model_ram = (uint8_t*)waste_model_data;
        }
    }

    // arena in DRAM
    static uint8_t* arena = nullptr;
    if (!arena) {
        arena = (uint8_t*)heap_caps_malloc(TENSOR_ARENA_SIZE,
                        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!arena) arena = (uint8_t*)ps_malloc(TENSOR_ARENA_SIZE);
        if (!arena) {
            Serial.println("[ai] arena alloc failed");
            g_ok = false;
            return false;
        }
        Serial.printf("[ai] arena %s (%u B) at %p\n",
                      ((uint32_t)arena >= 0x3F800000 &&
                       (uint32_t)arena < 0x3FF00000) ? "PSRAM" : "DRAM",
                      (unsigned)TENSOR_ARENA_SIZE, (void*)arena);
    }

    if (!resolver) resolver = new FastConvResolver();
    if (!profiler) profiler = new tflite::MicroProfiler();

    const tflite::Model* model = tflite::GetModel(model_ram);
    interp = new tflite::MicroInterpreter(model, *resolver, arena,
                                          TENSOR_ARENA_SIZE, nullptr,
                                          profiler);
    if (interp->AllocateTensors() != kTfLiteOk) {
        Serial.println("[ai] AllocateTensors FAILED");
        g_ok = false;
        return false;
    }
    tin = interp->input(0);
    tout = interp->output(0);
    Serial.printf("[ai] model ready, in_type=%d out_type=%d (fast conv)\n",
                  (int)tin->type, (int)tout->type);
    g_ok = true;
    return true;
}

bool ai_infer(const uint8_t* rgb, int size, ai_result_t* out) {
    if (!g_ok || size != INFERENCE_SIZE || !out) return false;
    uint32_t t0 = millis();

    const int n = TF_NUM_INPUTS;
    if (tin->type == kTfLiteInt8) {
        const float scale = tin->params.scale;
        const int32_t zp = tin->params.zero_point;
        int8_t* q = tin->data.int8;
        for (int i = 0; i < n; i++) {
            int32_t v = (int32_t)__builtin_lroundf((rgb[i] * (1.0f / 255.0f)) / scale) + zp;
            if (v < -128) v = -128;
            if (v > 127) v = 127;
            q[i] = (int8_t)v;
        }
    } else {
        float* in = tin->data.f;
        for (int i = 0; i < n; i++) in[i] = rgb[i] * (1.0f / 255.0f);
    }
    uint32_t t1 = millis();

    profiler->ClearEvents();
    if (interp->Invoke() != kTfLiteOk) {
        out->ok = false;
        Serial.println("[ai] Invoke FAILED");
        return false;
    }
    uint32_t t2 = millis();
#if AI_PROFILE
    Serial.printf("[ai] quant=%ums invoke=%ums\n",
                  (unsigned)(t1 - t0), (unsigned)(t2 - t1));
    profiler->LogTicksPerTagCsv();
#else
    (void)t1;
    (void)t2;
#endif

    out->ok = true;
    out->best_class = 0;
    out->best_conf = 0;
    if (tout->type == kTfLiteInt8) {
        const float scale = tout->params.scale;
        const int32_t zp = tout->params.zero_point;
        const int8_t* o = tout->data.int8;
        for (int i = 0; i < NUM_CLASSES; i++) {
            float v = (o[i] - zp) * scale;
            out->region_score[i] = v;
            if (v > out->best_conf) { out->best_conf = v; out->best_class = i; }
        }
    } else {
        const float* o = tout->data.f;
        for (int i = 0; i < NUM_CLASSES; i++) {
            float v = o[i];
            out->region_score[i] = v;
            if (v > out->best_conf) { out->best_conf = v; out->best_class = i; }
        }
    }
    return true;
}

bool ai_infer_full(ai_result_t* out) {
    static uint8_t* buf = nullptr;
    if (!buf) {
        buf = (uint8_t*)heap_caps_malloc(INFERENCE_SIZE * INFERENCE_SIZE * 3,
                                         MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!buf) buf = (uint8_t*)ps_malloc(INFERENCE_SIZE * INFERENCE_SIZE * 3);
        if (!buf) { if (out) out->ok = false; return false; }
    }
    if (!camera_crop_resize(0, 0, 1.0f, 1.0f, buf, INFERENCE_SIZE)) {
        if (out) out->ok = false;
        return false;
    }
    return ai_infer(buf, INFERENCE_SIZE, out);
}
