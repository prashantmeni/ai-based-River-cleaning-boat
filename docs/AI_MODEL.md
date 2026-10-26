# AI MODEL

**Project:** AI-Based River Cleaning Boat  
**Controller:** ESP32-CAM (AI vision unit)  
**Status:** PLANNED — no trained model file exists in the repository.

---

## 1. MODEL STATUS

No AI model file was found in the repository. The project brief states a model
"was previously built and tested using only the ESP32-CAM," but no model
asset, training code, or inference code exists in this repository. Therefore:

- No model architecture can be confirmed.
- No input dimensions, class labels, or confidence range can be asserted.
- No compatibility with ESP32-CAM RAM/flash/CPU can be verified.

Do **not** claim the model works until a trained model file is placed in
`assets/model/` and inference is demonstrated on the actual hardware.

---

## 2. REFERENCE MODEL (PLACEHOLDER)

Until a real trained model is provided, the firmware ships with a **reference
tiny neural network** small enough to fit the ESP32-CAM:

- Framework: TensorFlow Lite Micro (or ESP-NN) — INT8 quantized
- Input: 96×96 grayscale, normalized to [0,1] (int8 0..255)
- Classes: configurable via `AI_MODEL.md` / `shared/configuration/`
- Tensor arena: statically allocated, sized to fit available RAM (~200–300 KB)
- Inference: runs on CPU, no GPU/NPU on ESP32-CAM

Reference class mapping (placeholder, replace with trained labels):

| Class ID | Label | Notes |
|---|---|---|
| 0 | background / no waste | default |
| 1 | plastic bottle | placeholder |
| 2 | plastic bag | placeholder |
| 3 | general debris | placeholder |

These labels are **placeholders only**. Replace with the actual trained classes
before field testing.

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

## 4. PREPROCESSING PIPELINE

1. Capture frame from OV2640 (camera.cpp).
2. Convert to grayscale (if model trained on grayscale).
3. Resize to model input width×height.
4. Normalize pixel values to model input range.
5. Copy into statically allocated tensor input buffer.
6. Run inference.
7. Post-process: argmax class + confidence + bounding box (if model outputs one).

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

These belong in `shared/configuration/ai_config.h` (create before build).

---

## 7. NEXT STEPS (USER ACTION)

1. Train or obtain a small waste-detection model for your target classes.
2. Convert to INT8 TFLite Micro format.
3. Place the `.tflite` (or header array) in `assets/model/`.
4. Update `docs/AI_MODEL.md` with real input dimensions, class labels, and
   measured RAM/flash usage.
5. Wire the model into `firmware/esp32_cam_ai/src/inference.cpp`.
6. Validate inference on the ESP32-CAM before enabling autonomous motion.
