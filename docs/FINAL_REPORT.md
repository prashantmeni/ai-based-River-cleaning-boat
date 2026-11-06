# FINAL IMPLEMENTATION REPORT

**Project:** AI-Based River Cleaning Boat Using ESP32-CAM and ESP32  
**Repository:** `D:\Projects\River_Cleaning_Boat`  
**Report date:** 2026-10-26

---

## 1. WHAT WAS FOUND IN THE ORIGINAL PROJECT

The repository at `D:\Projects\River_Cleaning_Boat` was **completely empty**:
no source files, no AI model, no firmware, no dashboard, no build config, no
git history. There was nothing to preserve or repair; the project was built
from scratch.

---

## 2. WHAT WAS PRESERVED

Nothing from the repository (it was empty). The project brief's hardware list
and requirements were preserved as the design basis.

---

## 3. WHAT WAS CHANGED / CREATED

Everything in the repository is new. Full file list under docs/, firmware/,
shared/, dashboard/, dashboard-ui/, tests/, plus README.md and .gitignore.

---

## 4. NEW FILES CREATED

### Documentation (docs/)
- PROJECT_AUDIT.md — honest audit (repo was empty)
- SYSTEM_ARCHITECTURE.md — two-controller split
- COMPONENT_LIST.md — BOM with budget & unverified flags
- PIN_MAPPING.md — planned pin assignments, all unverified
- WIRING_DIAGRAM.md — textual wiring + 5V->3.3V level shift
- COMMUNICATION_PROTOCOL.md — framed serial protocol spec
- AI_MODEL.md — placeholder model, no trained model present
- STATE_MACHINE.md — navigation states & transitions
- BUILD_AND_FLASH.md — PlatformIO build/flash instructions
- TESTING_GUIDE.md — 5 test groups, simulated vs hardware
- TROUBLESHOOTING.md — failure symptoms & fixes
- USER_GUIDE.md — operating instructions & safety
- FINAL_REPORT.md — this document

### Shared
- shared/protocol/protocol.h — header-only framed protocol (checksum, sequence,
  message types, payload structs, encode/decode; PROTOCOL_FRAME_OVERHEAD = 9)
- shared/protocol/espnow_link.h — ESP-NOW link helpers (init, send, broadcast
  FF:FF:FF:FF:FF:FF, channel 1, sleep off) + optional WPA2 SoftAP on the main
  controller for the dashboard gateway

### Firmware: ESP32-CAM AI vision unit (Controller A)
- platformio.ini
- include/ai_config.h, camera.h, detection.h, inference.h, preprocess.h,
  serial_tx.h
- src/main.cpp, camera.cpp, detection.cpp, inference.cpp, preprocess.cpp,
  serial_tx.cpp

### Firmware: Standard ESP32 main controller (Controller B)
- platformio.ini (lib_deps: links2004/WebSockets for the dashboard gateway)
- include/boat_config.h, conveyor.h, gps.h, motor.h, navigation.h,
  obstacle.h, safety.h, serial_rx.h, telemetry.h, ws_gateway.h
- src/main.cpp, conveyor.cpp, gps.cpp, motor.cpp, navigation.cpp,
  obstacle.cpp, safety.cpp, serial_rx.cpp, telemetry.cpp, ws_gateway.cpp

### Dashboard
- dashboard-ui/ — React + TypeScript + Vite + Tailwind SPA: 9 pages, live
  WebSocket client, demo mode, CSV export, 51 vitest tests (see
  dashboard-ui/README.md)
- dashboard/index.html — legacy telemetry UI (sections A–F), bounded manual controls
- dashboard/bridge.py — legacy localhost HTTP + serial telemetry bridge (pyserial)

### Tests
- tests/test_protocol.py — frame validation (host-side simulated), 7/7 PASS
- tests/test_navigation.py — state machine transitions, 5/5 PASS
- dashboard-ui/src/**/*.test.{ts,tsx} — 51/51 PASS (vitest + jsdom)

---

## 5. FINAL ARCHITECTURE

Two separate microcontrollers, talking wirelessly.

**Controller A (ESP32-CAM):** OV2640 capture → grayscale preprocess → local
TinyML inference (stub until model provided) → confidence + temporal filter →
framed payload TX over **ESP-NOW** (AI_DETECTION / AI_NO_DETECTION /
AI_HEALTH), one protocol frame per datagram, broadcast, Wi-Fi channel 1, sleep
disabled. UART0 is debug console only. No motors.

**Controller B (standard ESP32):** ESP-NOW RX (ring buffer, payload
validation) → navigation state machine → differential motor drive (PWM+direction,
safe OFF defaults) → conveyor state machine with timeout → ultrasonic obstacle
sensing (5V echo level-shifted) → GPS NMEA parse → safety manager (watchdog,
emergency stop, comms-loss fault latch) → telemetry JSON over USB/UART0 **and**
over WebSocket. Hosts a WPA2 SoftAP ("RiverClean-Boat", channel 1,
192.168.4.1) with a WebSockets server on port 81 for the browser dashboard.

**Link layer:** START|VERSION|TYPE|SEQ2|LEN2|PAYLOAD|CHECKSUM|END, exactly one
frame per ESP-NOW datagram (max 73 bytes), 8-bit XOR checksum, sequence
numbers, stale-detection rejection, comms-loss safe state. No signal wires
between boards; common ground via USB/power.

**Dashboard (laptop):** dashboard-ui React app (`npm run dev`, localhost:5173)
streams telemetry over one of two selectable transports — cloud MQTT (WSS
`wss://broker.hivemq.com:8884/mqtt`, topics `riverclean/boat/telemetry` +
`riverclean/boat/command` + `riverclean/boat/ack`) or **direct Wi-Fi** to the
boat's own WebSocket gateway (`ws://192.168.4.1:81`, no internet required;
laptop joins the boat AP or the same router). The Settings endpoint field
auto-selects the transport: any URL containing `/mqtt` → broker, any other
`ws://`/`wss://` endpoint → direct gateway (`transportForUrl()` in
`connectionManager.ts`). Same JSON schema on both paths. The legacy USB path
(bridge.py) remains as fallback.

**Camera vision (detection-only, no video stream):** the ESP32-CAM serves NO
MJPEG stream — it was removed by requirement. `network_bringup()` still
cascades (home router → boat AP `RiverClean-Boat` → fallback AP
`RiverClean-CAM`) purely for network presence; detections travel only over
ESP-NOW. Each 150 ms the CAM captures 160×120 grayscale, runs the on-device
CV detector (`inference.cpp`: background mean/stddev → deviation mask →
flood-filled blob → confidence from measured contrast/area/fill → shape
heuristic class), and the main controller counts accepted detections per
class into telemetry (`bottle_count` / `bag_count` / `debris_count`).

**Status indicators (main controller):** GPIO17 = red (not ready: fault, no
GPS fix, or CAM link lost), GPIO18 = green (ready: all nominal), GPIO2 =
blue (blinking while connecting to the internet, solid once Wi-Fi + MQTT are
up), GPIO5 = buzzer (one beep on successful internet connection).

---

## 6. AI MODEL VERIFIED COMPATIBILITY STATUS

**No trained model file ships** (no dataset/training run exists), so model
accuracy claims would be dishonest. As an explicitly approved interim path,
the firmware now runs an **on-device CV detector** instead of a stub:
`inference.cpp` measures the water background, segments a contrasting blob,
derives confidence from the *actual image* (contrast, area, fill — a flat
frame scores low and never clears the 70% threshold), and assigns the class
by blob geometry (compact/upright → bottle, large/spread → bag, else
debris). Class labels are therefore **best-effort shape heuristics, not
learned recognition** — this is stated to the operator in the docs, and the
detection pipeline (temporal filter, confidence threshold) is unchanged.
A trained INT8 model remains the intended long-term replacement
(`assets/model/`, `Inference::begin()` swap).

A full-size YOLO cannot run on the AI-Thinker ESP32-CAM (limited SRAM, typically
no PSRAM). The reference path targets a small INT8 quantized network
(≤96×96 grayscale, static tensor arena). The user must train/convert a model,
place it in `assets/model/`, wire it into inference.cpp, and validate on
hardware before enabling autonomous motion.

---

## 7. COMMUNICATION PROTOCOL

Documented in docs/COMMUNICATION_PROTOCOL.md; implemented in
shared/protocol/protocol.h + shared/protocol/espnow_link.h. Framed, checksummed,
sequenced frames carried one-per-datagram over ESP-NOW (broadcast, channel 1,
2.4 GHz, no infrastructure network). Message types: AI_DETECTION(0x01),
AI_NO_DETECTION(0x02), AI_HEALTH(0x03), MAIN_STATUS(0x10), DIAG_COMMAND(0x20),
FAULT_REPORT(0x30). Stale detections rejected; comms loss (no frame ≥ 2 s)
latches FAULT_COMMS_LOST and stops motion.

Dashboard path (separate from the AI link): WebSocket JSON on
ws://192.168.4.1:81 — telemetry objects (single schema with the USB JSON),
hello on connect, and ack messages for commands sent as
`[#<id> ]CMD:<cmd>,<value>,<duration_ms>`.

---

## 8. FINAL PIN MAPPING

Documented in docs/PIN_MAPPING.md. **All values are planned and marked
TODO (verify)** — they must be confirmed against actual hardware before power-up.
No GPIO is assigned to two incompatible functions; input-only GPIOs (34,35,36,39)
are not used for outputs; 5V Echo uses a 1k+2k divider to 3.3V.

---

## 9. IMPLEMENTED AUTONOMOUS BEHAVIOURS

Navigation state machine: BOOT → SELF_TEST → IDLE → SEARCH →
TARGET_DETECTED → ALIGN → APPROACH → COLLECT → VERIFY_COLLECT →
TARGET_LOST / OBSTACLE_AVOID → FAULT / EMERGENCY_STOP.

- SEARCH: waits for fresh valid detections; no motion if safety fails.
- TARGET_DETECTED: confidence + freshness + consecutive-filter check.
- ALIGN: steer from center_x with deadband/gain/max-PWM (configurable).
- APPROACH: slow forward; obstacles and AI freshness monitored.
- COLLECT: conveyor ON (timed), conservative propulsion; recorded as
  attempted/timed collection (no full-basket sensor).
- TARGET_LOST: bounded re-search, then IDLE.
- OBSTACLE_AVOID: ultrasonic L/R with filtering/validity/hysteresis; stop if
  both blocked; no blind motion on invalid sensors.
- EMERGENCY_STOP / FAULT: all outputs OFF immediately.
- Motors OFF after boot; never automatic motion on startup.

---

## 10. DASHBOARD IMPROVEMENTS

**dashboard-ui/ (primary, React + TypeScript + Vite + Tailwind):**
- 5 pages (merged from 9): Overview (incl. collection stats + *Waste
  separated* per-class counts), AI Camera (detection-only lock, no video
  stream), Drive (manual drive, conveyor, autonomy, sensors,
  E-STOP), Diagnostics, Settings (+ demo scenarios, help) — responsive shell
  with sidebar, connection/state/fault pills, E-STOP banner; old routes
  redirect
- Live WebSocket client (auto-reconnect backoff, stale detection at 3 s,
  strict telemetry parser — wrong protocol version / missing fields rejected,
  never defaulted)
- Commands with client-generated ids + awaited firmware acks (≤3 s; timeout
  and rejection surfaced honestly, no optimistic success)
- Demo mode: 3 labelled scenarios (normal mission / fault / comms loss) with
  synthetic telemetry through the SAME store, banner-marked SIMULATION, live
  link disconnected while active
- CSV telemetry logger (persisted, capped, export) + raw JSON viewer, fault
  decoder, frame-error legend (2–8, 0xA1–0xA4), hello readout
- Satellite map (Leaflet) with honest offline-tiles notice (boat AP has no
  internet); settings in localStorage; no cloud, no paid API, no analytics
- Tests: 51/51 vitest (parser, commands, store, faults, CSV, demo engine,
  page-level); `npm run lint` and `npm run build` clean

**dashboard/ (legacy fallback):**
- Live overview (state, comms, detection, class, confidence, position,
  obstacles, GPS, motors, conveyor, collections, faults)
- Camera & AI status (no fake video; honest "unavailable")
- Map section (GPS only when fix valid; no fabricated location)
- Manual testing with bounded, auto-expiring commands
- Connectivity via localhost serial→HTTP bridge (pyserial)

---

## 11. BUILD RESULTS

**Both firmwares compile successfully** with PlatformIO + Arduino-ESP32
framework (espressif32@8.0.0, arduino-esp32 3.3.12):

| Target | RAM | Flash | Status |
|---|---|---|---|
| ESP32 Main + WebSockets gateway | 15.2% (49,776/327,680 B) | 79.5% (1,041,610/1,310,728 B) | OK |
| ESP32-CAM (AI vision) | 20.5% (67,280/327,680 B) | 33.0% (1,037,773/3,145,728 B) | OK |
| dashboard-ui production build | — | tsc + vite OK | OK |

Firmware binaries are generated at:
- `firmware/esp32_main_controller/.pio/build/esp32dev/firmware.bin`
- `firmware/esp32_cam_ai/.pio/build/esp32cam/firmware.bin`

**Flashing status (real hardware):**
- ESP32-CAM on COM4 — flashed repeatedly via
  `esptool.exe --port COM4 --no-stub ...` (stub flashing fails on this board).
- Standard ESP32 on COM8 — flashed repeatedly via
  `python -m platformio run -t upload --upload-port COM8` (kill dashboard
  bridge.py first; it holds the port).
- **Unresolved hardware issue:** the COM8 board has gone completely silent
  (no boot log, no ROM sync) three times; each time physical intervention on
  the bench revived it (exact action not confirmed). Suspects: loose
  EN/USB wiring, marginal power. Until resolved, the current Main build
  (WebSockets gateway) is compiled but **not yet flashed/verified**.

---

## 12. AUTOMATED TEST RESULTS

- tests/test_protocol.py: **7/7 PASS** (host-side simulated)
- tests/test_navigation.py: **5/5 PASS** (host-side simulated)
- dashboard-ui vitest: **51/51 PASS** (8 files: telemetry parser, command
  framing, store/stale logic, fault decoder, CSV export, demo engine,
  Navigation + Overview pages)
- `npm run lint` (ESLint, --max-warnings 0): 0 problems
- `npm run build` (tsc --noEmit + vite build): success

These validate logic only. The Python/vitest suites do NOT validate on actual
ESP32 hardware.

---

## 13. HARDWARE TESTS — PERFORMED vs OUTSTANDING

**Performed on real hardware (bench, out of water):**
- ESP-NOW link CAM→MAIN verified: `comms_ok: true`, `rx_bytes` climbing,
  `rx_errors/perr/drops = 0`, raw datagrams ending 0x7E.
- ESP-NOW channel resync verified live: router moved channel 1→6 mid-run,
  `[ESPNOW] channel 1 -> 6, link rebuilt` fired once, reception continued
  (`comms_ok` stayed true, `rx_bytes` kept climbing).
- Telemetry JSON over USB read by bridge.py (HTTP :8080) — stable at 5 Hz.
- Cloud MQTT path verified: telemetry on `broker.hivemq.com` (WSS 8884),
  command received (`CMD:0,0,0`), dashboard LINK ONLINE with live RX counter.
- Telemetry-silence root cause fixed: status JSON exceeded the 512-byte
  scratch buffer once `dbg` filled (577–598 B) and every telemetry line was
  silently dropped; buffer is now 1024 B (main.cpp) — verified live after
  reflash (`comms_ok:true`, rx_bytes climbing, 29 MQTT msgs/20 s).
- CAM router cascade verified: boot log `[CAM] joined 'SSB-JIO'
  ip=192.168.29.224`, MJPEG `GET /stream` answered HTTP 200
  `multipart/x-mixed-replace` from the laptop. *(Historical — the stream has
  since been removed by requirement; the CAM is detection-only now.)*
- Manual FORWARD command → left/right PWM 120/120 → auto-expired to 0.
- AUTONOMOUS_ON → state SEARCH; AUTONOMOUS_OFF → state IDLE (safety guard).
- Root cause fix verified by flash: PROTOCOL_FRAME_OVERHEAD 7→9 (every frame
  had failed BAD_END before).
- Stream removal verified: CAM boot log no longer prints any `stream=`/mDNS
  HTTP line, no server task starts; detections only.
- CV detector verified on hardware: boot log `[CAM] CV detector ready (blob +
  shape classifier)`, CAM sends AI_NO_DETECTION/AI_HEALTH frames at 150 ms /
  2 s cadence, main telemetry `rx_bytes` climbing with 0 errors.
- New telemetry fields verified live: `bottle_count`/`bag_count`/
  `debris_count` present in every JSON line (0/0/0 on the bench).
- CV detector + counters verified end-to-end on hardware: telemetry
  transitioned to `confidence:75, class_id:3 (general debris)` with
  `debris_count:1` after reflash — real image data, counted once.
- Cloud MQTT command/ack round trip verified live (4/4): `#cWEB1 CMD:1,0,0`
  → `{"status":"ok","id":"cWEB1"}` (the previous silent-drop bug — firmware
  required `CMD:` at byte 0 and published no acks — is fixed), garbage →
  `rejected/invalid command`, out-of-range cmd → `rejected`, plain
  `CMD:1,0,0` (Flutter form) → `ok`. Acks on `riverclean/boat/ack`;
  dashboard subscribes to it.
- COM8 CP210x wedge recovered without physical replug via elevated
  `pnputil /restart-device` (UAC); large app image written in 4 chunked
  esptool passes (115200, each hash-verified) after single-pass writes kept
  dying mid-transfer — procedure documented in TROUBLESHOOTING.md.
- Analog TDS meter (DFRobot Gravity SEN0244 → GPIO34/ADC1) integrated and
  verified live: `tds_ppm`/`tds_mv` present in every telemetry line,
  1 s cadence, oversampled + min/max-rejected, unstable inputs →
  `tds_ppm:-1` (invalid), ppm clamped 0–3000, `TDS_CALIBRATION` for a
  reference-meter check. Bench without probe reads the ~140 mV floating
  floor — documented as expected, raw mV always shown beside ppm.
  Dashboard (Sensors panel + CSV) and Flutter Monitor show both fields;
  51/51 tests, lint, tsc, flutter analyze, release APK all green.
- Dashboard: `tsc --noEmit`, 51/51 vitest, eslint clean after stream-UI
  removal + *Waste separated* panel + count parsing/CSV columns.
- Status LED/buzzer firmware flashed (blue blink→solid on MQTT connect +
  beep, red = not ready, green = ready) — **logic flashed, physical LED
  behaviour still to be eyeballed on the bench**.

**Outstanding (require hardware/bench/tank):**
- Real waste objects in front of the CAM → confirm detection, class label,
  accuracy %, and per-class counts rise on the dashboard (CV heuristic has
  only been bench-verified against empty frames so far).
- Dashboard ↔ boat direct-WiFi WebSocket end-to-end: dashboard side done
  (transport auto-selection in `connectionManager.ts`, direct `WebSocketClient`
  used for any non-`/mqtt` endpoint, Settings copy, 57/57 tests + lint + build)
  and firmware gateway ready (`ws_gateway.cpp`: hello on connect, telemetry
  broadcast, `#id CMD:` + ack); **live E2E still pending a powered main
  board** — the ESP32 stopped answering on COM8 (no boot log, esptool
  "No serial data received") and its AP is not broadcasting; check power /
  press EN / re-seat USB first, then run `ws_e2e.py` or set the dashboard
  endpoint to ws://192.168.4.1:81 from the boat AP.
- E-STOP latch/clear over the live link; comms-loss fault latch behaviour
  with the CAM actually powered off.
- Physical confirmation of RGB status LEDs + buzzer beep sequence.
- Motor direction + PWM mapping; stop; max PWM respected (hardware loaded).
- Conveyor timeout; conveyor stops after CONVEYOR_RUN_MS.
- Ultrasonic distance readings; 5V echo level-shift confirmed; stop distance.
- GPS NMEA parse + fix validity outdoors.
- Full navigation sequence SEARCH→...→COLLECT in a tank (boat restrained for
  first motor tests).
- COM8 board instability root cause (see §11).

None of the outstanding items are claimed as passed.

---

## 14. ADDITIONAL HARDWARE THAT MAY BE NECESSARY

- A **separate conveyor motor driver**: resolved — a second HW-095 (L298N)
  board is owned; one channel drives the conveyor (EN=GPIO23,
  IN1=GPIO22, IN2=GPIO21).
- Li-ion **protection BMS** if the charging module is not a full BMS.
- Bulk capacitance / fuse / main switch for motor startup current.
- Verified ratings for: motor driver, DC gear motors, ultrasonic modules
  (echo voltage), GPS module (baud), buck converter (current), battery cell
  count.

None of these are assumed; all are flagged `TODO (verify)` in docs.

---

## 15. EXACT BUILD AND FLASH COMMANDS

```
# Install tools
pip install platformio pyserial

# Controller A (ESP32-CAM) — COM4 on this bench
cd firmware/esp32_cam_ai
python -m platformio run                     # compile
# Flash (stub upload fails on this board — --no-stub is MANDATORY):
C:\Users\<you>\.platformio\packages\tool-esptool\esptool.exe ^
  --port COM4 --no-stub --baud 57600 --before default-reset --after hard-reset ^
  write-flash 0x1000 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin ^
  0x10000 firmware.bin
# (bins in .pio/build/esp32cam/; boot_app0.bin from framework tools/partitions)

# Controller B (standard ESP32) — COM8 on this bench
cd ../esp32_main_controller
python -m platformio run
# kill dashboard/bridge.py first — it holds COM8
python -m platformio run -t upload --upload-port COM8

# Dashboard (laptop) — primary: React app
cd ../dashboard-ui
npm install
npm run dev          # http://localhost:5173 (join RiverClean-Boat Wi-Fi first)
npm run build        # production bundle in dist/

# Dashboard — legacy fallback: USB serial bridge
python dashboard/bridge.py --port COM8 --baud 115200
# open http://localhost:8080

# Tests
python tests/test_protocol.py     # 7/7
python tests/test_navigation.py   # 5/5
cd dashboard-ui && npm run test   # 51/51, npm run lint, npm run build
```

Correct the serial port (`COM3`, `/dev/ttyUSB0`, etc.) for your machine.

---

## 16. NEXT PHYSICAL TESTING STEPS

1. ~~**Revive/stabilise the COM8 board**~~ — done (root cause: telemetry JSON
   overflowing a 512-byte buffer once `dbg` filled; buffer now 1024 B).
2. **Point real waste at the CAM** and confirm the dashboard shows the class
   name, accuracy %, and rising `bottle_count`/`bag_count`/`debris_count`
   (stream removed — detection is the only camera output now).
3. Confirm every pin/rating in docs/PIN_MAPPING.md & docs/COMPONENT_LIST.md
   with a multimeter and datasheets (mark all `TODO (verify)` as confirmed).
4. Wire common ground, fuse, main switch, 5V buck, level shifters.
5. Add motor power (fused, boat restrained, props off ground) — verify
   direction mapping, safe-off defaults, then E-STOP latch/clear over the
   dashboard; verify the RGB status LED sequence (blue blink → blue solid +
   beep → green when ready / red on fault).
6. Optional: train the INT8 model on real waste images, drop it into
   `assets/model/`, swap the CV heuristic in `Inference::begin()/run()`,
   rebuild, and compare detection quality.
7. Run full navigation sequence in a controlled tank.
8. Document results; iterate on thresholds/gains.

---

## HONESTY STATEMENT

Firmware WAS compiled for both controllers and flashed repeatedly to real
hardware on COM4/COM8; a subset of hardware tests has passed (ESP-NOW link,
USB telemetry, manual PWM, autonomy toggle) — listed precisely in §13. The
current Main build with the WebSocket gateway compiles but is NOT yet flashed
or verified (COM8 board in a dead state awaiting bench diagnosis). No claim
is made that any AI model was trained, converted, or ran on the ESP32-CAM
(the stub reports INF_STATUS_MODEL_NOT_LOADED). No claim is made that the
boat is operational or that any tank test passed. The browser is never the
safety controller: E-STOP, fault latches and command guards run in firmware.
All motor outputs default to OFF; all unverified hardware values are flagged.
Missing telemetry is shown as Unavailable/UNKNOWN, never fabricated; demo
data is banner-labelled SIMULATION.
