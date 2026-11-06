# AI MODEL

**Project:** AI-Based River Cleaning Boat  
**Controller:** ESP32-CAM (AI vision unit)  
**Status:** DETECTOR SHIPPED — on-device CV heuristic (no trained model file).

---

## 1. MODEL STATUS

No trained model file exists in the repository (no dataset, no training run,
no `assets/model/` asset), so no trained-model accuracy can be claimed.

What **does** run on the hardware today, by explicit user approval as the
interim detection method, is an **on-device computer-vision detector**
(`firmware/esp32_cam_ai/src/inference.cpp`):

- Real blob segmentation on the captured frame — not a stub, not a fake.
- Confidence is derived from **measured image properties only** (contrast
  vs. water background, blob area, fill ratio) — a flat frame scores low and
  never clears the threshold.
- Class assignment is a **shape heuristic** (compact/upright → bottle,
  large/spread → bag, otherwise debris) — best effort, not learned
  recognition. Class labels are reported to the operator as such.

Do **not** claim trained-model accuracy. Replacing the heuristic with a
trained INT8 model remains a user action (§7).

---

## 2. WHAT ACTUALLY RUNS (SHIPPED)

`Inference::run()` (interval `INFERENCE_INTERVAL_MS = 150`, 160×120
grayscale frame):

1. Estimate water background: frame mean + std-deviation.
2. Build a deviation mask (pixel differs from mean by `max(22, 1.5σ)`).
3. Flood-fill the strongest qualifying seed; drop blobs < ~55 px or > ~26%
   of the frame (rejects noise and whole-frame lighting changes).
4. Confidence = weighted blend of measured contrast, area, fill ratio.
5. Class from blob geometry (shape heuristic, labels above).
6. Fill `bbox` (0..1000) and `center_x` (-500..+500); preserve caller's
   `timestamp_ms`/`camera_health`.

Above `CONFIDENCE_THRESHOLD` (70, twice in a row via the main controller's
temporal filter) the CAM sends AI_DETECTION, else AI_NO_DETECTION. Static
buffers: `s_mask[9216]` + `s_stack[9216]` bytes, no heap allocation.

Reference class mapping (`firmware/esp32_cam_ai/include/ai_config.h`):

| Class ID | Label | Notes |
|---|---|---|
| 0 | background | no detection |
| 1 | plastic bottle | compact/upright blob (shape heuristic) |
| 2 | plastic bag | large, sparse, wide blob (shape heuristic) |
| 3 | general debris | everything else above threshold |

The dashboard and per-class waste counters (`bottle_count`/`bag_count`/
`debris_count`) consume these class IDs unchanged, whether they came from
the heuristic or a future trained model.

---

## 3. MEMORY BUDGET (ESP32-CAM, AI-Thinker variant)

| Resource | Approx. available | Usage guidance |
|---|---|---|
| SRAM | ~320 KB (usable less) | tensor arena + stack |
| PSRAM | typically NONE on AI-Thinker ESP32-CAM | do not rely on it |
| Flash | 4 MB (shared with firmware + filesystem) | model + program |

A full-size YOLO cannot run here. Use:
- Input resolution ≤ 96×96 (grayscale preferred)
- INT8 quantized weights
- Static tensor arena allocation
- Reuse of frame buffers
- Controlled inference frequency (e.g., one inference per 100–200 ms)

---

## 4. PREPROCESSING PIPELINE (SHIPPED)

1. Capture frame from OV2640 at QQVGA (160×120), grayscale (`camera.cpp`).
2. Mean/std-dev background estimation (no resize/normalize — heuristic works
   directly on the captured pixels).
3. Deviation mask + flood-fill blob extraction.
4. Confidence + shape class + bbox/center computation.

(If a trained model replaces this: resize to model input, normalize, run
inference, argmax — see §7.)

---

## 5. DETECTION OUTPUT

The AI subsystem produces `ai_detection_payload_t` (see
shared/protocol/protocol.h) with:
- class_id, confidence (0..100)
- bbox_x/y/w/h (0..1000 normalized)
- center_x (-500..+500 relative to image centre)
- timestamp_ms, camera_health, inference_status

When no target is above the confidence threshold, the ESP32-CAM sends
AI_NO_DETECTION — never silence, never a fabricated detection.

---

## 6. CONFIGURABLE THRESHOLDS

| Parameter | Default | Purpose |
|---|---|---|
| CONFIDENCE_THRESHOLD | 70 (0..100) | min confidence for a valid detection |
| CONSECUTIVE_DETECTIONS | 2 | frames in a row required to commit |
| INFERENCE_INTERVAL_MS | 150 | ms between inference runs |

Defined in `firmware/esp32_cam_ai/include/ai_config.h` (detected and
enforced); main-controller side in `shared/configuration/`.

---

## 7. NEXT STEPS (USER ACTION)

1. Train or obtain a small waste-detection model for your target classes.
2. Convert to INT8 TFLite Micro format.
3. Place the `.tflite` (or header array) in `assets/model/`.
4. Update `docs/AI_MODEL.md` with real input dimensions, class labels, and
   measured RAM/flash usage.
5. Wire the model into `firmware/esp32_cam_ai/src/inference.cpp`.
6. Validate inference on the ESP32-CAM before enabling autonomous motion.
