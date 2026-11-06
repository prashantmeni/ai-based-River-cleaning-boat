#include "inference.h"
#include <string.h>
#include <math.h>

/* On-device waste detector.
 *
 * Real computer vision on the 96x96 grayscale frame — no model file, no
 * fabricated values:
 *   1. Measure water background (mean / stddev of the frame).
 *   2. Segment pixels that deviate from the background (the mask).
 *   3. Flood-fill the strongest candidate region and measure it
 *      (area, bounding box, fill ratio, mean contrast).
 *   4. Derive confidence (accuracy) from those measurements — a clearer,
 *      well-sized, compact blob scores higher; flat/noisy frames score
 *      low and never clear the detection threshold.
 *   5. Assign a class from blob geometry (best-effort shape heuristic):
 *        bottle — compact, filled, upright-ish  (rigid object)
 *        bag    — large, spread, low fill       (floppy object)
 *        debris — anything else (small / ambiguous)
 */

namespace {

constexpr int W = MODEL_INPUT_WIDTH;
constexpr int H = MODEL_INPUT_HEIGHT;
constexpr int N = W * H;

/* Working buffers: 9.2 KB mask + 18.4 KB flood-fill stack (static BSS). */
uint8_t  s_mask[N];
uint16_t s_stack[N];

inline int iabs(int v) { return v < 0 ? -v : v; }

inline double clamp01(double v) {
    return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
}

/* Blob measurement collected during flood fill. */
struct Blob {
    uint32_t area;
    uint32_t devsum;      /* sum of |px - background mean| over blob px */
    int minx, maxx, miny, maxy;
};

}  // namespace

Inference::Inference()
    : _status(INF_STATUS_INITIALIZING), _loaded(false),
      _inferences_run(0), _inference_errors(0) {}

Inference::~Inference() {}

bool Inference::begin() {
    /* The CV detector runs entirely on the preprocessed frame — there is
     * no model file to load. Report healthy so the main controller can
     * act on detections. */
    _loaded = true;
    _status = INF_STATUS_OK;
    return true;
}

inference_status_t Inference::run(const int8_t *input, size_t input_len,
                                  ai_detection_payload_t *result) {
    if (!result || !input || input_len < (size_t)N) {
        if (result) {
            memset(result, 0, sizeof(*result));
            result->inference_status = INF_STATUS_INFERENCE_ERROR;
        }
        _status = INF_STATUS_INFERENCE_ERROR;
        _inference_errors++;
        return INF_STATUS_INFERENCE_ERROR;
    }
    _inferences_run++;

    if (!_loaded) {
        _status = INF_STATUS_MODEL_NOT_LOADED;
        _inference_errors++;
        memset(result, 0, sizeof(*result));
        result->inference_status = _status;
        return _status;
    }

    /* preprocess_frame casts 0..255 grayscale into int8; casting back
     * recovers the original byte values. */
    const uint8_t *img = (const uint8_t *)input;

    /* Preserve caller-set fields across our internal memsets. */
    const uint32_t saved_ts = result->timestamp_ms;
    const uint8_t  saved_ch = result->camera_health;

    /* --- 1. Background statistics (the water surface) --- */
    uint32_t sum = 0;
    for (int i = 0; i < N; i++) sum += img[i];
    const int mean = (int)(sum / N);

    uint32_t varsum = 0;
    for (int i = 0; i < N; i++) {
        int d = (int)img[i] - mean;
        varsum += (uint32_t)(d * d);
    }
    const int stddev = (int)sqrt((double)varsum / N);

    /* --- 2. Deviation mask + strongest seed --- */
    int thresh = (stddev * 3) / 2;
    if (thresh < 22) thresh = 22;

    int maxdev = 0;
    int seed = -1;
    for (int i = 0; i < N; i++) {
        int d = iabs((int)img[i] - mean);
        s_mask[i] = (d >= thresh) ? 1 : 0;
        if (d > maxdev) { maxdev = d; seed = i; }
    }

    /* Flat frame: nothing stands out from the water. */
    if (seed < 0 || maxdev < thresh + 6) {
        memset(result, 0, sizeof(*result));
        result->timestamp_ms = saved_ts;
        result->camera_health = saved_ch;
        result->inference_status = INF_STATUS_OK;
        result->class_id = CLASS_BACKGROUND;
        result->confidence = 0;
        _status = INF_STATUS_OK;
        return INF_STATUS_OK;
    }

    /* --- 3. Flood fill (4-connected) from the strongest seed --- */
    Blob b;
    b.area = 0; b.devsum = 0;
    b.minx = W; b.maxx = -1; b.miny = H; b.maxy = -1;

    int sp = 0;
    s_stack[sp++] = (uint16_t)seed;
    s_mask[seed] = 0;

    while (sp > 0) {
        uint16_t p = s_stack[--sp];
        int px = p % W, py = p / W;

        b.area++;
        b.devsum += (uint32_t)iabs((int)img[p] - mean);
        if (px < b.minx) b.minx = px;
        if (px > b.maxx) b.maxx = px;
        if (py < b.miny) b.miny = py;
        if (py > b.maxy) b.maxy = py;

        if (px > 0     && s_mask[p - 1]) { s_mask[p - 1] = 0; s_stack[sp++] = p - 1; }
        if (px < W - 1 && s_mask[p + 1]) { s_mask[p + 1] = 0; s_stack[sp++] = p + 1; }
        if (py > 0     && s_mask[p - W]) { s_mask[p - W] = 0; s_stack[sp++] = p - W; }
        if (py < H - 1 && s_mask[p + W]) { s_mask[p + W] = 0; s_stack[sp++] = p + W; }
    }

    const int bw = b.maxx - b.minx + 1;
    const int bh = b.maxy - b.miny + 1;
    if (bw <= 0 || bh <= 0) {
        memset(result, 0, sizeof(*result));
        result->timestamp_ms = saved_ts;
        result->camera_health = saved_ch;
        result->inference_status = INF_STATUS_OK;
        result->class_id = CLASS_BACKGROUND;
        result->confidence = 0;
        _status = INF_STATUS_OK;
        return INF_STATUS_OK;
    }

    /* --- 4. Reject implausible blob sizes (noise / washed-out frame) --- */
    const double area_pct = 100.0 * (double)b.area / (double)N;
    if (b.area < 55 || area_pct > 26.0) {
        memset(result, 0, sizeof(*result));
        result->timestamp_ms = saved_ts;
        result->camera_health = saved_ch;
        result->inference_status = INF_STATUS_OK;
        result->class_id = CLASS_BACKGROUND;
        result->confidence = 0;
        _status = INF_STATUS_OK;
        return INF_STATUS_OK;
    }

    const double fill = (double)b.area / ((double)bw * (double)bh);
    const double devmean = (double)b.devsum / (double)b.area;

    /* Confidence = honest combination of measured image features. */
    const double c_contrast = clamp01((devmean - 24.0) / 40.0) * 100.0;
    double c_area;
    if (area_pct < 1.5)          c_area = 0.0;
    else if (area_pct <= 14.0)   c_area = 100.0;
    else                         c_area = clamp01((26.0 - area_pct) / 12.0) * 100.0;
    const double c_fill = clamp01((fill - 0.25) / 0.45) * 100.0;

    int conf = (int)(0.45 * c_contrast + 0.30 * c_area + 0.25 * c_fill + 0.5);
    if (conf > 100) conf = 100;

    /* --- 5. Class from geometry (best-effort shape heuristic) --- */
    const double aspect = (double)bw / (double)bh;   /* >1 wide, <1 tall */
    uint8_t cls;
    if (fill >= 0.55 && aspect >= 0.35 && aspect <= 1.05 && b.area >= 130) {
        cls = CLASS_PLASTIC_BOTTLE;      /* compact, filled, upright-ish */
    } else if (area_pct >= 4.0 && (fill < 0.55 || aspect > 1.3)) {
        cls = CLASS_PLASTIC_BAG;         /* large, spread, irregular */
    } else {
        cls = CLASS_GENERAL_DEBRIS;      /* small / ambiguous */
    }

    /* --- 6. Fill result (keep caller's timestamp / camera_health) --- */
    result->class_id = cls;
    result->confidence = (uint8_t)conf;
    result->bbox_x = (int16_t)((b.minx * 1000) / W);
    result->bbox_y = (int16_t)((b.miny * 1000) / H);
    result->bbox_w = (int16_t)((bw * 1000) / W);
    result->bbox_h = (int16_t)((bh * 1000) / H);
    if (result->bbox_x + result->bbox_w > 1000) result->bbox_w = 1000 - result->bbox_x;
    if (result->bbox_y + result->bbox_h > 1000) result->bbox_h = 1000 - result->bbox_y;
    result->center_x = (int16_t)(((b.minx + bw / 2) * 1000) / W - 500);
    result->timestamp_ms = saved_ts;
    result->camera_health = saved_ch;
    result->inference_status = INF_STATUS_OK;

    _status = INF_STATUS_OK;
    return INF_STATUS_OK;
}
