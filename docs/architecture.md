# Architecture — Dual-Controller River Cleaning Boat (p2.txt Phase 2)

Single-controller ESP32-CAM design → two controllers, UART link.
Baseline before refactor: commit `24848ca` (see `docs/audit-report.md`).

## Overview

```
+----------------------------+        UART1 115200 8N1        +-----------------------------+
| Controller A: ESP32-CAM    |  TX=GPIO14 ────────────→ RX=26  | Controller B: ESP32-WROOM   |
| AI Vision Controller       |  RX=GPIO13 ←──────────── TX=27  | Main Boat Controller       |
|                            |        (common GND)            |                             |
|  camera (OV2640)           |                                |  state machine (11 states) |
|  int8 TFLite 680 ms        |                                |  L298N #1: drive L/R       |
|  region classification     |                                |  L298N #2: conveyor        |
|  MJPEG dashboard :80/:81   |                                |  US1/US2 obstacle, US3 fill|
|  thermal guard (AI duty)   |                                |  GPS (TinyGPSPlus)         |
|  detection counters        |                                |  stats, faults, buzzer stub |
|  NO motors/GPS/sensors     |                                |  NO Wi-Fi, NO AI, NO camera|
+----------------------------+                                +-----------------------------+
```

## Data ownership

| Data | Owner | Consumers |
|---|---|---|
| Frames, inference, class+conf+region | CAM | ESP32 (via `det`), dashboard |
| Detection counters (cls hits) | CAM | dashboard (local) |
| Boat state, collection attempts/confirmed | ESP32 | CAM dashboard (via `state`) |
| Obstacle distances, basket fill, GPS, conveyor status | ESP32 | CAM dashboard (via `state`) |
| Motor/conveyor actuation | ESP32 only | — |
| Thermal/AI duty (58/70/50 °C) | CAM only | ESP32 logs via `hb` |
| Dashboard UI + live video | CAM | browser |
| Auto/manual commands | browser → CAM `/cmd/*` → forwarded `cmd` msg → ESP32 |

## Startup sequence

1. Both boards boot independently (either order).
2. CAM: AI arena → camera → dashboard → link UART (sends `hb` 1 Hz).
3. ESP32: self-test (sonar/GPS init) → link UART (sends `hb`+`state` 1-2 Hz) →
   state `SELF_TEST → IDLE` only when link healthy.
4. Link established when each side receives any valid message within timeout.

## Detection → collection workflow

1. CAM runs classifier every `AI_PERIOD_MS` (1500 ms, thermal-aware).
2. If conf ≥ 0.55 → CAM sends `det` (class, conf, region, seq).
3. ESP32 applies **staleness rule**: det older than 4000 ms or from dead link
   is discarded. Persistence lock: same class+region for 3 dets → `DETECT`
   (= p2 "WASTE_DETECTED") → `ALIGN → APPROACH → COLLECT` (existing SM).
4. Obstacles override steering at any time; both blocked → stop/collect-ready.
5. Basket full → stop + `RETURN_OR_STOP` (standby; **no return-to-home claim** —
   no compass/flow sensor exists).

## Fault handling / safe states

| Fault | Detection | Response (ESP32) |
|---|---|---|
| Link lost | no valid CAM msg > 3000 ms | conveyor stop, motors stop, state `RETURN_OR_STOP`, `fault=LINK_LOST`, auto-recovers to IDLE when link returns |
| Stale detection | det age > 4000 ms | treated as no-detection (never steer on old data) |
| CAM fault msg | `fault` message / `hb cam=0\|ai=0` | as link lost (`fault=CAM_AI`) |
| Sonar invalid (both) > 3 s in AUTO | readings −1 | motors stop, `fault=SONAR` (manual mode unaffected) |
| Basket full | fill state | stop + standby (existing) |
| GPS invalid | no fix | logged only; navigation does not depend on GPS |
| E-stop (software `/cmd` or future input) | `sm_emergency_stop` | FAULT_STOP latched until clear |
| Battery low | `BATTERY_SENSE_ENABLED 0` (no hardware spec) | not implemented — flagged |

CAM-side: camera/AI self-test failure → CAM reports `fault`, dashboard shows
FAULT, AI keeps retry semantics of existing code; CAM never drives actuators.

## Memory / timing budgets

- CAM unchanged where it matters: arena 109,568 B DRAM, model 10,960 B,
  AI 680 ms @ 1500 ms period, stream pauses AI (existing behavior).
- ESP32 main: loop ≤ 100 ms tick; ultrasonic pulseIn ≤ 25 ms ×3 samples;
  JSON build ≤ 1 ms; no `delay()` > 20 ms anywhere in control paths.

## Repository layout (adapted, minimal moves)

```
firmware/                  existing PlatformIO project — CAM target [env:esp32cam]
firmware/src/              camera, ai_model, dashboard, link (NEW), main (slimmed)
firmware/models/           waste_model_data.h (untouched)
firmware/esp32_main/       NEW nested PlatformIO project [env:esp32main] (ESP32-WROOM)
shared_protocol/rb_protocol/  NEW PlatformIO library (protocol.h/.cpp) used by both
docs/                      +architecture +communication_protocol +build_and_flash
                           +testing_plan +audit-report; wiring.md/test-report updated
tests/protocol/            NEW host tests (protocol rules + config consistency)
```

Boat-control sources (`state_machine, motors, conveyor, ultrasonic, gps,
basket`) **move** into `firmware/esp32_main/src/` (motors/conveyor adapted for
L298N — justified: PCA9685 never hardware-validated and absent from parts
list). Nothing is deleted; baseline commit preserves originals.
