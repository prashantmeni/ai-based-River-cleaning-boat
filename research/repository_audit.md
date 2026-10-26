# Repository Audit

Date: 2026-10-21
Auditor: AI Engineering Team

## Repositories Cloned and Inspected

### 1. espressif/esp-tflite-micro
- Status: CLONED, INSPECTED
- License: Apache 2.0
- Runtime: ESP-IDF (v5.x-v6.x), not Arduino-first
- Examples: hello_world, micro_speech, person_detection
- ESP32 (classic) person_detection: 380ms inference with ESP-NN at 240MHz
- Models included: int8 quantized person detection model
- Compatibility: ESP-IDF component; usable with Arduino via manual integration
- Verdict: Official, well-maintained, best runtime. Selected as primary inference runtime target.

### 2. maarten-pennings/TFLcam
- Status: CLONED, INSPECTED
- License: see LICENSE file
- Runtime: Arduino framework, ESP32-CAM specific
- Approach: Loads TFLite model from SD card via EloquentTinyML
- Proven performance: ~5 FPS on ESP32-CAM (rock-paper-scissors, 28x46 grayscale)
- RAM limitation documented: trouble allocating large consecutive blocks
- Verdict: Best proof-of-concept for ESP32-CAM + TFLite. Patterns reused: camera crop -> preprocess -> invoke.

### 3. Ankush703-web/River_Floating_Trash_Detection_System
- Status: CLONED, INSPECTED
- Runtime: Desktop only (PyTorch + Detectron2)
- Model: Faster R-CNN R50-FPN, .pth checkpoint
- Weights: RFT_model.pth claimed but not verified present in shallow clone
- mAP@0.5: 70.6%, 8 classes (bottle, grass, branch, milk-box, plastic-bag, plastic-garbage, ball, leaf)
- Verdict: NOT deployable on ESP32-CAM. Use only as class-vocabulary reference and dataset pointer.

### 4. ORCAUBOAT/FloW-Dataset
- Status: CLONED (metadata only)
- Content: README + Pictures, actual dataset requires application at orca-tech.cn
- Types: FloW-Img (vision), FloW-RI (image + mmWave radar)
- Verdict: Dataset not directly downloadable. Document as acquisition path; not usable offline.

## Repositories Not Cloned (network/time)
- smit-sms/Plastic-Detection-in-River
- AniLeo-01/Plastic-In-River-Detection
- ajv95/floatingplasticdebrisdetection
- survivingwithandroid/ESP32-CAM-tensorflow-lite
- glazzerini/fml

Assessment: These are additional desktop/research implementations. None supersede the ESP32-CAM-native approaches above. If needed later, clone into research/repos.

## Model Decision

Requirements: must run on AI-Thinker ESP32-CAM (ESP32, 520KB SRAM + 4MB PSRAM), int8 or float32 TFLite, input <= 128x128.

Selected approach: Compact quantized CNN classifier trained on waste classes, region-based classification (left/centre/right) per TFLcam pattern.

Rationale:
- Faster R-CNN and YOLO .pt models cannot fit ESP32-CAM
- TFLcam proves TFLite inference works on ESP32-CAM at 5+ FPS
- esp-tflite-micro provides optimized kernels (ESP-NN)
- Region-based classification is the documented fallback when object detection is too large
