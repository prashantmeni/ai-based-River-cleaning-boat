# AI-Based River Cleaning Boat — working prototype

**Dual-controller architecture (P2 refactor):** an ESP32-CAM does vision only
(on-device int8 TFLite, live MJPEG dashboard over its own AP), a standard
ESP32 runs the boat (state machine, motors, conveyor, sonar, GPS), and the
two talk over a validated UART protocol. The AI model and pipeline from the
working P1 prototype are preserved unchanged.

## Status (current)

| Area | State |
|---|---|
| On-device inference (CAM) | **Working — ~680 ms/cycle** (custom fast int8 conv kernel; stock TFLM was 3.3 s) |
| Camera → 96×96 pipeline | Working (QVGA RGB565, PSRAM path) |
| Web dashboard + MJPEG | **Working** — `RiverBot-AP` / `192.168.4.1` |
| UART protocol (shared lib) | **Implemented + 40/40 protocol tests PASS**; link-loss/dup/stale rules enforced in code |
| Main-controller firmware | **Builds (RAM 8.5%, Flash 22.6%) — compile-verified only, no ESP32 board attached yet** |
| Vision firmware after refactor | **Builds (RAM 18.8%, Flash 33.3%)** — same model/pipeline as P1 |
| Auto state machine | Moved to main controller (SELF_TEST → SEARCH → … → RETURN/STOP), compiled, untested on HW |
| Motors / conveyor / GPS / sonars | Rewritten for parts-list hardware (L298N ×2, HC-SR04 ×3, TinyGPSPlus) — **not yet wired** |
| Model accuracy | **61.3%** on synthetic held-out set — honest baseline, see `docs/model-card.md` |
| Real-water validation | Not yet done — `docs/water-test.md` |

Build success ≠ hardware validation. Per-test status: `docs/testing_plan.md`.

## Architecture

```text
[Camera+TFLM+Wi-Fi]  ESP32-CAM (env: esp32cam)      [dashboard, /cmd/*]
        │  UART1 115200 NDJSON: hb ←, det ←, fault ←, cmd →, state →
        ▼
[State machine]      ESP32 dev board (env: esp32main)
  sonar ×3 · GPS · L298N drive ×4 · conveyor · estop/battery/buzzer stubs
```

Data ownership: camera/AI/detections live on the CAM; motion/sensor/fault
state lives on the main controller; each side transmits its own heartbeats
and a common `state` message. Protocol spec: `docs/communication_protocol.md`.

## Layout

```text
firmware/              CAM project (src/ ai_model, camera, dashboard, link)
firmware/esp32_main/   main-controller project (state machine, motors, conveyor,
                       ultrasonic, gps, basket, link, buzzer)
firmware/models/       waste_model_data.h  int8 model bytes (10,960 B)
shared_protocol/       rb_protocol library (protocol.h/.cpp) — used by both
ai/                    dataset build + training + conversion (see ai/README.md)
docs/                  architecture · audit-report · communication_protocol ·
                       wiring · build_and_flash · testing_plan · model-card ·
                       water-test · hardware-inventory · troubleshooting ·
                       test-report · licenses
tests/protocol/        40 host tests (framing, validation, seq, config sync)
tests/unit/            7 artifact-integrity tests (host, stdlib)
tests/integration/     device boot/inference/dashboard over COM3
research/              reference repos (TFLcam, FloW, esp-tflite-micro, …)
tools/ dashboard/      (scaffolding)
```

## Build & flash (both targets)

```powershell
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";"
          + [System.Environment]::GetEnvironmentVariable("Path","User")

cd firmware              && pio run -e esp32cam                 # vision
cd firmware\esp32_main   && pio run -e esp32main                # boat

# Flash (build first; stop keeper2 — it holds COM3)
pio run -e esp32cam  -t upload --upload-port COM3     # in firmware/
pio run -e esp32main -t upload --upload-port <COMx>   # in firmware\esp32_main
```

Full commands, detached-build recipe, and honest build results:
`docs/build_and_flash.md`.

## Tests

```powershell
python -m unittest discover -s tests/protocol -v   # 40 protocol tests, no HW
python -m unittest discover -s tests/unit -v       # 7 artifact tests, no HW
python tests/integration/test_device.py            # boot + inference over COM3
python tests/integration/test_device.py --http     # + dashboard via RiverBot-AP
```

Latest: protocol `Ran 40 tests … OK`; unit `Ran 7 tests … OK`; P1
integration `SERIAL: PASS`, `HTTP: PASS` (re-run after P2 flash).
Per-level evidence: `docs/test-report.md`; HIL plan: `docs/testing_plan.md`.

## Docs

| File | Covers |
|---|---|
| `docs/audit-report.md` | P1 code audit, keep/modify/move/retire decisions |
| `docs/architecture.md` | Dual-controller design, data ownership, budgets |
| `docs/communication_protocol.md` | UART NDJSON protocol v1 specification |
| `docs/wiring.md` | **Dual-board** pin map, L298N wiring, power, safety |
| `docs/build_and_flash.md` | Build/upload both targets, logs, quirks |
| `docs/testing_plan.md` | Automated results + 8 HIL scenarios + status keys |
| `docs/model-card.md` | Model spec, honest accuracy, on-device perf |
| `docs/hardware-inventory.md` | Present vs planned parts |
| `docs/water-test.md` | Bench → still-water → river procedures |
| `docs/test-report.md` | Completion levels with evidence |
| `docs/troubleshooting.md` | Build/flash/TFLM/Wi-Fi failure playbook |
| `docs/licenses.md` | Third-party license inventory |
| `ai/README.md` | Dataset → train → convert → verify pipeline |

## Dashboard

1. Join AP **RiverBot-AP** (password `cleanriver`), IP **192.168.4.1**.
2. Open `/` (status + controls) or `/stream` (MJPEG).

Endpoints: `/` · `/status.json` · `/cmd/{start,stop,fwd,rev,left,right,stopm,conveyor,reset}` · `/stream`

Commands are forwarded to the main controller over UART as protocol `cmd`
messages (whitelist: `auto_on,auto_off,fwd,rev,left,right,stopm,conveyor,reset,clear`).

> PC Wi-Fi quirk: the Ralink RT3290 client roams off `RiverBot-AP` within
> ~15–60 s. Reconnect with `netsh wlan connect name="RiverBot-AP"` (or use a
> phone — the phone is the verified video client).

## Key numbers

- Inference: int8 input, 5-class softmax, arena 107 KiB DRAM; 680 ms/cycle.
- Protocol: 115200 8N1, max 191-char NDJSON lines; link timeout 3000 ms
  (main←CAM) / 10000 ms (CAM←main); det staleness 4000 ms; seq dup-guard.
- Main controller: control loop 100 ms, sensor 200 ms, GPS 1 s, tx 2 Hz.
- Confirmation gate: conf ≥ 0.55 for 3 frames (identical thresholds in
  `config.h` and `config_main.h` — enforced by a test).

## Known limitations

1. **ESP32 dev board not connected** → main firmware compile-verified only;
   scenarios 2–7 of the HIL plan pending hardware.
2. Model accuracy 61.3% (synthetic data) — retrain on real footage.
3. Battery cell count unknown → battery ADC disabled; buzzer + e-stop button
   absent from parts list → stubs/disabled.
4. Wi-Fi client roaming on the PC (above).
5. Pin map provisional — verify `docs/wiring.md` against both config headers
   before powering motors.
