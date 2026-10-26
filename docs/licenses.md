# Third-party license inventory

## Firmware libraries (`firmware/.pio/libdeps/esp32cam/`)

| Library | Version | License | Notes |
|---|---|---|---|
| `tflm_esp32` (eloquentarduino) | 2.0.0 | **MIT** | Vendored TFLite-Micro sources + ESP port; LICENSE in package |
| `esp32-camera` (espressif) | — | **Apache-2.0** | OV2640 driver |
| `ArduinoJson` | 7.4.3 | **MIT** | Dashboard JSON |
| `TinyGPSPlus` | 1.1.0 | **LGPL-2.1** | Header license block: LGPL v2.1 or later — dynamically/statically linked into the ESP32 app; LGPL obligations (source availability of the library, relinkability) apply to distribution of the firmware image |
| `EloquentTinyML` | 3.0.1 | *not used* | Was referenced historically; **removed from `lib_deps`** — all inference goes through `tflm_esp32` directly. Folder may linger in `.pio/libdeps` but is not linked |

## Frameworks, cores, tools

| Component | License | Notes |
|---|---|---|
| Arduino-ESP32 core (framework) | LGPL-2.1 | Bundled into the app image |
| ESP-IDF components (as pulled by platform) | Apache-2.0 / MIT (mixed) | See `framework-arduinoespressif32` package |
| TensorFlow / TensorFlow Lite | Apache-2.0 | Model format + training code |
| PlatformIO | Apache-2.0 | Build tool |
| espressif32 platform package (toolchain etc.) | mixed (GPL toolchain binaries with linking exceptions) | gcc/esp tools |

## AI training / conversion scripts (first-party)

Original code in `ai/` (MIT-licensed with this project unless noted).
Runtime deps: TensorFlow (Apache-2.0), NumPy (BSD-3), Pillow (MIT-CMU),
`remotezip` (MIT) — installed into the host Python, not redistributed here.

## Datasets and model lineage

| Source | Use | License / terms |
|---|---|---|
| gonzz2026 "floating-rubbish" dataset (HuggingFace) | bottle/wrapper/waste crops + real water frames | **CC-BY-4.0** (stated by dataset card — verify before redistribution) |
| bryandts organic/recyclable waste ZIP | `leaf_or_organic_debris` | Dataset-host terms apply |
| cordmaur `earth_surface_water` | background water imagery | Dataset-host terms apply |
| SARA trash-in-river images | `other_visible_waste` | Dataset-host terms apply |
| Procedural water textures (`ai/dataset_tools/build_background.py`) | synthetic backgrounds | First-party generated |

Dataset images are **not** committed to the project for redistribution —
`ai/dataset_tools/acquire_dataset.py` re-fetches them. Keep provenance
notes with any derived dataset.

## Research/reference repos (`research/`)

Cloned for study only (TFLcam, FloW, Plastic-Detection-in-River,
River_Floating_Trash_Detection_System, esp-tflite-micro, …). Their licenses
govern their own trees; no code was copied into `firmware/` or `ai/` without
independent implementation. See `research/repository_audit.md`.
