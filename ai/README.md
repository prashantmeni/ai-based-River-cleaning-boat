# AI pipeline — dataset → train → convert → verify

All scripts run with plain `python` (Windows paths resolved from `__file__`,
run from anywhere). Dependencies: `tensorflow`, `numpy`, `pillow`,
`remotezip` (dataset tools), optional `kaggle` CLI for source #1.

## 1. Dataset

```powershell
python ai/dataset_tools/acquire_dataset.py   # Kaggle / HuggingFace / fallback URLs -> ai/data/raw/<class>/
python ai/dataset_tools/build_dataset.py     # 5-class set from source ZIPS (CC-BY-4.0 + others)
python ai/dataset_tools/build_background.py  # real trash-free water frames -> background class
```

Sources and mapping (see script docstrings for URLs/licenses):

| Source | Classes fed |
|---|---|
| gonzz2026 floating-rubbish YOLO (CC-BY-4.0) | plastic_bottle, plastic_wrapper, other_visible_waste |
| bryandts organic/recyclable ZIP | leaf_or_organic_debris |
| cordmaur earth_surface_water | background_or_no_waste |
| SARA trash-in-river images | other_visible_waste |

Honest limitation: only part of the set is real river imagery; the rest is
real-object cut-outs on procedural/synthetic water. `model_report.json`
records provenance and per-class counts (≈310/class, 5,150 images total).

## 2. Train

```powershell
python ai/training/train.py
```

Produces `ai/models/waste_classifier.keras` and refreshes
`ai/evaluation/model_report.json` (held-out accuracy, per-class
precision/recall, confusion matrix).

## 3. Convert (int8, ESP32-CAM)

```powershell
python ai/models/convert_int8.py
```

Produces `ai/models/waste_classifier.tflite` (10,960 bytes) and regenerates
`firmware/models/waste_model_data.h`. See script docstring for the TFLM
constraints (int8 I/O, no per-channel quant on Dense) — breaking them yields
AllocateTensors failures on device.

## 4. Verify

```powershell
python -m unittest discover -s tests/unit -v          # artifact sync tests (no hardware)
python tests/integration/test_device.py               # boot + inference over COM3
python tests/integration/test_device.py --http        # + dashboard HTTP (Wi-Fi)
```

## 5. On-device accuracy checkpoint

Current honest numbers (see `docs/model-card.md`):

- Keras float model, held-out test split: **55.9%** (`model_report.json`)
- Int8 TFLite, 300-image sample: **61.3%** (184/300, printed by
  `convert_int8.py`)
- On-camera confidences: 0.30–0.43 on a desk scene — below the 0.55
  confirmation gate; retune after real water footage.
