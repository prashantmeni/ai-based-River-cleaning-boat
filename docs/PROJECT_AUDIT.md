# PROJECT AUDIT — AI-Based River Cleaning Boat

**Project:** AI-Based River Cleaning Boat Using ESP32-CAM and ESP32  
**Repository:** `D:\Projects\River_Cleaning_Boat`  
**Audit date:** 2026-10-26  
**Audited by:** Autonomous implementation agent

---

## 1. SUMMARY OF FINDINGS

The repository located at `D:\Projects\River_Cleaning_Boat` **was completely empty** at the time of this audit. No source files, firmware, AI model assets, dashboard code, configuration, build files, or documentation existed.

Consequently:

| Audit item | Finding |
|---|---|
| Directory structure | None (empty root) |
| Firmware source files | None |
| ESP32-CAM camera implementation | None |
| AI model files | None |
| Model architecture / input dimensions | None |
| Model labels / supported classes | None |
| Model inference library | None |
| Camera initialization code | None |
| Image preprocessing pipeline | None |
| Serial communication code | None |
| Motor-control code | None |
| Ultrasonic sensor implementation | None |
| GPS integration | None |
| Dashboard | None |
| Configuration files | None |
| Build configuration (PlatformIO / Arduino / ESP-IDF) | None |
| Documentation | None |
| Git history | None (not a git repository) |

**Conclusion: there is no existing implementation to preserve, repair, or integrate. The project must be built from scratch.** This audit document records that fact so that no false claim is made about "reusing" an existing AI model or firmware that does not exist.

---

## 2. WHAT ALREADY WORKS

Nothing. No code exists.

---

## 3. WHAT DOES NOT WORK

Everything. No firmware, model, dashboard, or build system exists.

---

## 4. COMPONENTS THAT CAN BE REUSED

None from this repository. The only reusable inputs are the **hardware components** listed in the project brief:

- AI-Thinker ESP32-CAM with OV2640 camera
- ESP32-CAM programmer module
- Standard ESP32 development board
- Dual-channel motor driver (model to be confirmed)
- 2 × DC gear motors (propulsion)
- 2 × low-speed DC geared motors (conveyor)
- Conveyor belts
- 2 × ultrasonic sensors
- GPS module
- Buzzer
- Li-ion cells, charging module, 5 V DC-DC buck converter
- Connecting wires

None of these have verified ratings or wiring in this repository.

---

## 5. COMPONENTS REQUIRING CREATION (FROM SCRATCH)

| Area | Deliverable |
|---|---|
| Firmware | ESP32-CAM AI vision unit firmware |
| Firmware | Standard ESP32 main controller firmware |
| AI model | Lightweight waste-detection model + inference runtime interface |
| Communication | Framed serial protocol (UART) between controllers |
| Navigation | Deterministic state machine on main controller |
| Obstacle avoidance | Ultrasonic sensor driver + filtering |
| Motor control | Dual-channel differential drive abstraction |
| Conveyor control | Conveyor state machine with timeout |
| GPS | NMEA parsing abstraction |
| Dashboard | Web-based telemetry + manual test interface |
| Build system | PlatformIO `platformio.ini` for both controllers |
| Documentation | All docs listed in Section 9 |
| Tests | Host-side Python tests for protocol, parsing, state machine |

---

## 6. CAN THE (NON-EXISTING) AI MODEL RUN ON ESP32-CAM?

There is no existing AI model file in the repository, so compatibility cannot be assessed against an actual asset.

**Engineering position:** A full-size YOLO model cannot run on an AI-Thinker ESP32-CAM. The module provides limited SRAM and typically no PSRAM; flash is shared with the application firmware. The implementation will therefore ship with:

1. A **tiny, quantized, grayscale-input neural network** (small enough to fit in ESP32-CAM flash/RAM) as the reference AI model, stored under `assets/model/`.
2. A **clear inference interface** so a trained model can be swapped in later.
3. An honest `AI_MODEL.md` documenting memory budget, input dimensions, and the fact that the model must be trained/converted by the user for their specific waste classes.

Until a trained model file is placed in `assets/model/` and validated on the actual ESP32-CAM, the AI path reports `INFERENCE_STATUS = MODEL_NOT_LOADED` and the main controller treats the vision unit as unhealthy. No detection is ever fabricated.

---

## 7. DOES THE MODEL REQUIRE CONVERSION / OPTIMIZATION?

No model exists, so no conversion is possible yet. When a model is added, the recommended path is:

- TensorFlow Lite Micro (or ESP-NN/TinyML-friendly format) with INT8 quantized weights.
- Input resolution kept low (e.g., 96×96 grayscale or 32×32 RGB) to fit RAM.
- Static tensor arena allocation.

---

## 8. HOW CODE WILL BE DIVIDED BETWEEN THE TWO CONTROLLERS

**Controller A — ESP32-CAM (AI Vision Unit):**
- OV2640 camera capture + frame buffering
- Image preprocessing (resize/grayscale/normalize) for the model
- Local TinyML inference (no network required)
- Confidence thresholding + temporal filtering
- Structured serial output of detection / no-detection / health messages
- Serial diagnostics

**Controller B — Standard ESP32 (Main Control Unit):**
- UART reception and validation of AI messages
- Dual motor driver control (differential steering, PWM)
- Ultrasonic obstacle sensing (with 5 V echo level shifting)
- GPS NMEA parsing
- Conveyor motor control with timeout
- Autonomous navigation state machine
- Safety manager, watchdog, emergency stop
- Buzzer control
- Telemetry to dashboard (optional local Wi-Fi or serial)

**Shared:**
- Message format, checksum, sequence numbering, field definitions (`shared/protocol/`)

---

## 9. KNOWN vs UNKNOWN HARDWARE CONNECTIONS

| Connection | Status |
|---|---|
| ESP32-CAM ↔ OV2640 camera | Fixed onboard (AI-Thinker pinout), no user wiring |
| ESP32-CAM ↔ Main ESP32 UART | **To be defined** (PIN_MAPPING.md) |
| Main ESP32 ↔ motor driver | **To be defined** (driver model unknown) |
| Main ESP32 ↔ ultrasonic sensors | **To be defined** (5 V echo level shifting required) |
| Main ESP32 ↔ GPS module | **To be defined** (UART) |
| Main ESP32 ↔ conveyor motor | **To be defined** (separate driver may be required) |
| Main ESP32 ↔ buzzer | **To be defined** |
| Power distribution / buck converter | **To be defined** |

All pin assignments are documented in `docs/PIN_MAPPING.md` and must be verified against the actual purchased hardware before powering up.

---

## 10. MISSING SPECIFICATIONS THAT BLOCK SAFE IMPLEMENTATION

The following are unknown and must be verified before hardware is energized:

1. Dual-channel motor driver model and current rating.
2. DC gear motor nominal voltage, stall current, and no-load current.
3. Ultrasonic sensor model (echo voltage — likely 5 V, requires level shifting to 3.3 V ESP32).
4. GPS module model and baud rate.
5. Battery cell count / nominal voltage and whether a protection BMS is present.
6. Whether the 5 V buck converter can supply motors + ESP32s simultaneously.

None of these are assumed. `docs/COMPONENT_LIST.md` and `docs/PIN_MAPPING.md` mark every unverified value as `TODO (verify)`.

---

## 11. IMPLEMENTATION PLAN (PHASES)

Per the project mandate, implementation proceeds immediately:

1. **Phase 1 — Audit** (this document): complete.
2. **Phase 2 — Dual-controller architecture**: shared protocol, pin mapping, firmware directories.
3. **Phase 3 — ESP32-CAM AI firmware**: camera, inference, serial detection output.
4. **Phase 4 — Main controller firmware**: UART receiver, motors, ultrasonic, GPS, conveyor, navigation state machine, safety.
5. **Phase 5 — Integration**: validated communication, freshness checks, fault handling.
6. **Phase 6 — Dashboard**: telemetry + bounded manual controls.
7. **Phase 7 — Verification**: compile checks, Python host tests, document outstanding hardware tests.
8. **Phase 8 — Final documentation**: build/flash/testing guides, troubleshooting, user guide, final report.

---

## 12. HONESTY STATEMENT

No claim is made that any AI model has been trained, converted, or run on an ESP32-CAM. No claim is made that any firmware has been flashed or that any hardware has been tested. All motor control defaults to **OFF/disabled** until explicit, verified initialization. The dashboard reports `UNKNOWN`/`NOT AVAILABLE` for any telemetry not actually produced by firmware.
