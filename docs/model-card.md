# Model card — river-waste classifier

## What it is

Quantized image classifier that runs fully on-device on an ESP32-CAM via
TensorFlow Lite for Microcontrollers. Detects floating waste classes in a
96×96 RGB crop and steers the boat's collection logic.

| Property | Value |
|---|---|
| Input | `[1, 96, 96, 3]`, **int8** (scale 0.0039216, zero-point −128 = raw uint8) |
| Output | `[1, 5]`, float32 softmax (TFLM SOFTMAX on int8, then dequantized) |
| Size on flash | 10,960 bytes (`firmware/models/waste_model_data.h`) |
| Quantization | full int8 PTQ (weights + activations), per-channel weights, **per-tensor for Dense layers** (TFLM FC kernel has no per-channel support) |
| Architecture | Conv(8)→Pool→Conv(16)→Pool→Conv(16)→Pool→GlobalMean→FC(16)→FC(5)→Softmax |
| Ops used | CONV_2D, MAX_POOL_2D, MEAN, FULLY_CONNECTED, SOFTMAX (all TFLM built-in) |
| Training data | 5,150 images, synthetic river/lake backgrounds + junk (see `ai/dataset_tools/build_dataset.py`) |

## Classes (index order)

0. `plastic_bottle`
1. `plastic_wrapper`
2. `leaf_or_organic_debris`
3. `other_visible_waste`
4. `background_or_no_waste`

## Accuracy — read this honestly

- **Held-out sample set: 184/300 = 61.3%** (verified against the saved
  int8 `.tflite` with correct input quantization).
- Backgrounds are synthetic; performance on real murky water, glare, and
  partial submersion is **untested**. Treat deployed confidences as weak
  evidence, not ground truth.
- On-camera confidences observed on a desk scene: 0.30–0.43 (argmax often
  class 1/3). The firmware confirmation gate is `DETECT_CONF_THRESHOLD 0.55`
  with `DETECT_PERSIST_FRAMES 3` — so nothing currently "locks" without
  better imagery or a tuned threshold. **Re-evaluate the threshold against
  real water footage** (`docs/water-test.md`).

## Runtime performance (measured, ESP32-CAM @ 240 MHz)

| Metric | Value |
|---|---|
| Full AI cycle (crop+resize → quantize → invoke) | **~680 ms** (target ≤1.5 s ✓) |
| `Invoke()` total | 657 ms (per-op CSV via `AI_PROFILE 1`) |
| CONV_2D | 594 ms — custom int8 kernel (~25 cycles/MAC; the stock TFLM reference conv was 3309 ms) |
| MAX_POOL_2D | 59 ms |
| MEAN / FC / SOFTMAX / DEQUANTIZE | ≤3 ms |
| Input quantization step | 16–18 ms |
| Tensor arena | 107 KiB, internal DRAM (model copy 10,960 B also DRAM) |
| Heartbeat cadence | 1 Hz with `ai=1` steady |

The custom CONV_2D kernel lives in `firmware/src/ai_model.cpp`
(`FastConvEval`): flattened pointer-walk MACs, SAME/VALID padding, stride,
dilation, per-channel requantization, bias, NONE/RELU/RELU6. It must free
all `AllocateTemp*Tensor` handles in prepare (TFLM contract).

## Training / rebuild

```text
python ai/training/train.py        # float Keras model + model_report.json
python ai/models/convert_int8.py   # int8 PTQ → waste_classifier.tflite +
                                   # regenerates firmware/models/waste_model_data.h
python -m unittest discover -s tests/unit   # header/artifact sync check
```

Full pipeline incl. dataset acquisition: `ai/README.md`.

Conversion constraints that matter:
- `converter._experimental_disable_per_channel_quantization_for_dense_layers = True`
  — otherwise TFLM dies with *FullyConnected per-channel quantization not yet supported*.
- Keep float output unless you also patch TFLM's SOFTMAX path for int8.
