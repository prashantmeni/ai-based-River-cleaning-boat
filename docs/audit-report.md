# Audit Report — Pre-Refactor (Dual-ESP32 Migration, p2.txt)

Date: 2026-10-24 · Scope: read-only inspection before any edit (p2.txt §3).
Verification key: **[V]** physically tested on hardware · **[C]** compiles/builds
only · **[U]** untested code · **[D]** documented assumption.

## 1. Existing architecture (current, pre-refactor)

Single controller: AI-Thinker ESP32-CAM (COM3, CH340) runs EVERYTHING:

```
camera → AI (int8 TFLite, 680 ms) → region classification → state machine
      → motors/conveyor/gps/ultrasonic drivers (no hardware fitted)
      → Wi-Fi AP + MJPEG dashboard (port 80/81)
      → thermal guard, heartbeats, serial diagnostics
```

- Build: PlatformIO `firmware/` single env `esp32cam` (espressif32@6.7.0,
  Arduino, huge_app.csv, PSRAM, -O2). [C+V]
- Loop: cooperative `loop()` at 100 ms control / 200 ms sensors / 1 s GPS /
  1 s thermal / 500 ms dashboard JSON; separate FreeRTOS `stream_task` core 1
  for MJPEG. [V]
- Repository is **NOT a git repo** (no history to preserve; baseline commit
  recommended before refactor — see §7).

## 2. Files responsible for each feature

| Feature | Files | State |
|---|---|---|
| Boot / orchestration | `firmware/src/main.cpp` (336 ln) | [V] |
| Camera (RGB565 QVGA, software JPEG stream) | `camera.cpp/h` | [V] |
| AI inference (custom int8 conv) | `ai_model.cpp/h`, `models/waste_model_data.h` (691 ln) | [V] |
| Region/steering classification | `main.cpp: ai_with_regions()` | [V] |
| State machine (11 states) | `state_machine.cpp/h` | [C] via /cmd bench [V]; motors paths [U] |
| Motors (PCA9685 I2C) | `motors.cpp/h` | **[U] — device never found; PCA9685 not in parts list** |
| Conveyor (PCA9685 ch2) | `conveyor.cpp/h` | **[U]** |
| Ultrasonic US1/US2 | `ultrasonic.cpp/h` | **[U] — sensor not fitted** |
| GPS (TinyGPSPlus, UART1 on 2/4) | `gps.cpp/h` | **[U] — module not fitted** |
| Basket fill logic | `basket.cpp/h` | [U] — feeds provisional US1; reports SENSOR_FAULT |
| Dashboard + stream | `dashboard.cpp/h` (port 80, stream_task 81) | [V] phone; [V] laptop rare |
| Thermal guard | `main.cpp:248-263` + config | [V] (58/70/50, th=0/1/2 seen) |
| Wi-Fi policy helpers | `tools/wifi_test.ps1` | [V] |
| Tests | `tests/unit/` (7), `tests/integration/` (3) | [V] last run PASS/exit 0 |
| Docs | `docs/*` 7 files, `README.md` | current pre-refactor status |

## 3. AI model (preserve — do NOT retrain)

- Artifact: `firmware/models/waste_model_data.h` — 10,960 B int8 TFLite array.
- Type: **image classification** (softmax over 5 classes — NOT object
  detection; no bounding boxes exist. Protocol must preserve that).
- Input: `[1,96,96,3]` int8 (scale 1/255, zp −128) · Output: `[1,5]` float32.
- Classes (`config.h`): plastic_bottle, plastic_wrapper, leaf_or_organic_debris,
  other_visible_waste, background_or_no_waste.
- Runtime: TFLM via `eloquentarduino/tflm_esp32@^2.0.0`; arena 109,568 B DRAM;
  invoke 657 ms; cycle 680 ms with custom fast conv (stock TFLM 3.3 s).
- Accuracy: int8 sample 61.3 % (synthetic held-out) — known weak point [V-measured].
- Gate: `DETECT_CONF_THRESHOLD 0.55`, persist 3 frames, 3 region crops.
- Unit tests pin these facts (`test_model_artifact.py` parses
  `config.h` + model header paths) — **paths/values must stay stable**. [V]

## 4. Implemented vs physically tested (honest split)

**[V] physically verified:** boot, inference, heartbeats, self-test, dashboard
endpoints, MJPEG (phone), thermal throttle levels, Wi-Fi join/restore flow,
L298N-free graceful degradation messages, endpoint matrix, stream bound-fix.

**[U] never exercised with hardware:** motor drivers (both PCA9685 AND L298N),
conveyor, both ultrasonics, GPS, basket fill, buzzer (no code), battery
sensing (no code), emergency-stop button (no code/hardware).

**Assumptions previously made that this audit corrects:**
- `config.h` plans PCA9685, but the parts list has **L298N ×2 + DC geared
  motors** and NO PCA9685 → `motors.cpp`/`conveyor.cpp` must target L298N on
  the main controller (justified rewrite: never hardware-validated, and the
  ESP32-CAM lacks free pins anyway). [D→decision Phase 5]
- Basket sensor was "provisional US1" because CAM pin-poor; dual architecture
  frees pins — basket gets a dedicated transducer (parts list has HC-SR04 ×3). [D]
- Buzzer: only `PCA_CH_BUZZER 3` define exists — **no implementation, no
  hardware in parts list** → ship a disabled stub (`PIN_BUZZER -1`), do not
  invent hardware. [D]
- Battery: charging module listed but **no battery / cell count** → battery
  ADC code stays `BATTERY_SENSE_ENABLED 0` with documented divider TODO. [D]

## 5. Pin assignments & conflicts (current CAM config.h)

| Function | GPIO | Status |
|---|---|---|
| I2C SDA/SCL (PCA9685) | 13/14 | [U]; PCA9685 absent → **freed** in refactor |
| US1 TRIG/ECHO | 12/15 | [U]; GPIO12 = strapping (must be low at boot — HC-SR04 TRIG high-Z OK); ECHO needs 5V→3.3V divider; both SD-slot pins (SD card unusable while assigned) |
| US2 | disabled | pin-budget sacrifice |
| GPS ESP RX/TX | 2/4 | [U]; GPIO2 strap (safe, GPS idles high); GPIO4 = flash LED (conflict documented) |
| Console/flash UART0 | 1/3 | untouched, needed for logging+tests |
| Camera | 0,5,18,19,21–23,25–27,32,34–39 | fixed AI-Thinker wiring; GPIO0 XCLK/strap; PSRAM on GPIO16 → **GPIO16 never usable** |
| Free after refactor on CAM | 13,14 (+2,4,12,15 freed) | chosen for UART link: **TX=14, RX=13** (non-strap, avoids 12/15 strap risk) [D] |

Conflict resolutions for dual architecture: GPS/US/PCA/I2C move off the CAM;
SD-slot pins remain shared with microSD (SD not used — accepted); flash-LED/GPS
LED note retired with GPS move.

## 6. Dependencies & build commands

- `platformio.ini` (single env): espressif32@6.7.0 · esp32cam · ArduinoJson@^7 ·
  tflm_esp32@^2 · TinyGPSPlus@^1.0.3 · esp32-camera · flags:
  `BOARD_HAS_PSRAM CORE_DEBUG_LEVEL=1 F_CPU=240M -I models -O2 TF_LITE_USE_CTIME`.
- Build/flash: `cd firmware; pio run -t upload --upload-port COM3` (921600).
- Tests: `python -m unittest discover -s tests/unit -v` (7) ·
  `python tests/integration/test_device.py [--http]`.
- Serial markers the integration test REQUIRES (must survive refactor):
  `[ai] model ready`, `[selftest] camera+ai ready`, `>=2 [ai] cycle … conf=`,
  `[hb] … ai=1 cam=1`.

## 7. Missing features, bugs, risks (refactor-relevant)

1. **No second board connected** (COM ports currently none): standard ESP32
   target can be compile-verified only until hardware arrives — will be
   flagged as blocker, never claimed as hardware-validated. [V-absence]
2. No git → initialize + baseline commit BEFORE edits (safety, not history rewrite).
3. Model accuracy 61.3 %; conf gate rarely crossed in the wild.
4. Stream: single client; laptop Ralink link flaky (environmental, documented).
5. Heat: die 58-66 °C under load — thermal guard verified; keep AI duty 45 %.
6. State machine assumes `camera_ok` — must become `link_ok && cam_ok` inputs.
7. `count_confirmed` hard-wired 0 — needs an honest counting rule (Phase 5).
8. No RTH capability (no compass/flow) — RETURN_OR_STOP stays "stop+standby",
   per p2.txt "do not claim autonomous return unless supported".

## 8. Disposition (create / modify / move / retire)

**Preserve untouched:** `ai_model.*`, `models/waste_model_data.h`, `camera.*`,
`dashboard.*` (extend snapshot merge only), `tools/*`, `tests/unit/*`,
`docs/model-card.md`, ai/ pipeline.

**Modify:** `main.cpp` (remove boat drivers, add link+merge), `config.h`
(CAM-only subset + link pins), `dashboard` snapshot fields fed from link,
`README.md`, `docs/wiring.md`, `docs/test-report.md` (addendum).

**Move to new `firmware/esp32_main/` project (content preserved, then
adapted):** `state_machine.*`, `basket.*`, `gps.*`, `ultrasonic.*` (near-verbatim),
`motors.*`, `conveyor.*` (rewritten for L298N — justified, never validated).

**Add:** `shared_protocol/protocol.h` (+ specs), `firmware/esp32_main/` project,
`tests/protocol/test_protocol.py`, docs: architecture / communication_protocol /
build_and_flash / testing_plan, this audit report.

**Retire (from CAM build only):** PCA9685 driver usage, CAM-side state machine,
CAM-side sensors/GPS — no code deleted, only relocated per above.

**Blockers to declare honestly:** standard ESP32 not yet on hand/COM (if so);
battery spec unknown; buzzer not in parts list.
