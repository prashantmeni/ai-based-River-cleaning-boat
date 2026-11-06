# TROUBLESHOOTING

**Project:** AI-Based River Cleaning Boat

---

## FIRMWARE WILL NOT COMPILE

| Symptom | Likely cause | Fix |
|---|---|---|
| `esp_camera.h: No such file` | Wrong board / framework | Use `board = esp32cam` with `framework = arduino` for the CAM |
| `undefined reference to ledc...` | Arduino core missing | Ensure `framework = arduino` in platformio.ini |
| Build succeeds for one project only | Each firmware is a separate PlatformIO project | Build inside each `firmware/*` folder separately |

## FLASHING PROBLEMS

| Symptom | Fix |
|---|---|
| `failed to connect to ESP32: Timed out` | Board not in bootloader mode. ESP32-CAM: GPIO0->GND, press Reset. DevKit: hold BOOT, press EN, release BOOT. |
| Wrong serial port | `pio device monitor -l`; set `upload_port` |
| Brownout during flash | Power from a steady 5 V supply; use a good USB cable |
| No serial output after flash | Baud 115200; GPIO0 not tied to GND in run mode; press Reset |
| Port opens, but esptool/pyserial fails `PermissionError(13) ... A device attached to the system is not functioning` (Error 31) | Wedged CP210x (COM8). PnP still shows Status=OK — don't trust it. Fix: run `pnputil /restart-device "USB\VID_10C4&PID_EA60\0001"` **elevated** (UAC prompt) or unplug/replug the USB. Avoid rapid successive open/close probes; they re-trigger the wedge. |
| esptool dies mid-write (`Lost connection` / `Write timeout` at any %) | Single-pass writes of the ~1 MB app image die after ~15-40 s on this CP210x (small regions always succeed). Restart the device (above), then flash `firmware.bin` in **4 chunked passes** at 115200: offsets `0x10000`, `0x50000`, `0x90000`, `0xD0000` with sizes 262144/262144/262144/291568 — one `write-flash` per chunk, each hash-verifies itself. |
| After flashing the CAM, its port floods `[ESPNOW] channel 1 -> 6, link rebuilt` thousands of times (megabytes in seconds) | Wedged post-flash state. Press **Reset** on the CAM (or pulse RTS via capture_com_reset.py). A clean boot shows the `ets ... POWERON_RESET` banner, `[CAM] joined '<ssid>' ip=...`, and exactly **one** resync line. |

## COMMUNICATION PROBLEMS (ESP-NOW link)

| Symptom | Fix |
|---|---|
| `comms_ok:false`, `rx_bytes` stuck at 0 | Board not initialising WiFi/ESP-NOW (boot log should show `[MAIN] esp-now link ready mac=...` / `[CAM] esp-now link up mac=...`). Also check both boards are flashed with the ESP-NOW firmware (they must match). |
| `rx_bytes` climbs but `rx_errors` climbs too | Frames arriving but failing parse. Check `perr` (last parse result: 3=BAD_END, 6=BAD_CHECKSUM) and `dbg` (raw datagram hex — must start `7E01...` and end `7F`). |
| `drops` climbing | RX ring buffer overflowing — loop() blocked too long; check for long blocking calls. |
| Link works then dies | Wi-Fi channel drift or radio init order; both sides hardcode channel 1. Power-cycle both boards; check 5 V rail for brownouts. |
| One `[ESPNOW] channel 1 -> 6, link rebuilt` line after the CAM/main joins a router | **Expected.** The radio follows the router's channel; ESP-NOW re-homes once and reception continues (`comms_ok` stays true). Only a line repeating every second forever is a fault (see FLASHING PROBLEMS row). |
| Motors keep moving after link loss | Should not happen — comms loss forces safe stop. If it does, check safety manager and that `_motors->stop()` runs. |

## MOTOR / POWER PROBLEMS

| Symptom | Fix |
|---|---|
| Motors don't move | Check ENA/ENB **jumper caps are removed** (else full-on, not PWM); IN1..IN4 wired per PIN_MAPPING; driver +12V connected to fused battery; never power motors from ESP32 3.3 V. |
| ESP32 resets when motors start | Motor startup current dipping the 5 V rail. Add bulk capacitance, separate motor supply, thicker wires, fuse. |
| One motor runs backwards | Flip that motor's wires, or toggle MOTOR_LEFT_INVERT / MOTOR_RIGHT_INVERT. |

## AI / VISION PROBLEMS

| Symptom | Fix |
|---|---|
| `CV detector ready` missing from CAM boot log | Firmware predates the CV detector, or camera init failed (check `Camera init: 0x...`). Rebuild/reflash `firmware/esp32_cam_ai`. |
| Always `NO_DETECTION` | Flat scene (no blob): confidence comes from measured contrast/area/fill, so water alone legitimately scores below 70. Point at waste with contrast; optionally lower `CONFIDENCE_THRESHOLD` temporarily for testing. |
| Detection fires but class label looks wrong | Labels are **shape heuristics** (compact→bottle, large/sparse→bag, else debris), not learned recognition — see docs/AI_MODEL.md §2. |
| Camera fails to init | Wrong AI-Thinker pin table; PSRAM setting; insufficient power to CAM. |

## DASHBOARD PROBLEMS

| Symptom | Fix |
|---|---|
| `bridge.py` can't open serial port | Port in use by another monitor; close `pio device monitor`. Correct COM port. |
| Dashboard shows no telemetry | Check `comms_alive` in `/api/telemetry` (bridge serial status), `rx_bytes` (ESP-NOW traffic), and run `pio device monitor` on the main controller to see raw JSON lines. |
| `ModuleNotFoundError: serial` | `pip install pyserial` |
| AI Camera page shows no detections | Expected when the frame is flat water — the on-device detector only locks onto a contrasting blob (≥ ~0.7% of the frame). Point the CAM at waste; check **CAM LINK OK** first. |
| Waste counts never increment | Detection must clear 70% confidence twice in a row (temporal filter). Improve contrast/target size; counts only rise for accepted detections (`bottle_count`/`bag_count`/`debris_count` in telemetry). |
| No MJPEG stream anymore | Intentional — the stream was removed. The CAM is detection-only now; video URLs and `rivercam.local:8080` no longer exist. |

## TDS SENSOR PROBLEMS

| Symptom | Fix |
|---|---|
| `tds_ppm` stuck at 0 with `tds_mv` ≈ 0 | OUT not wired to GPIO34, sensor unpowered, or GND not common. SEN0244 with a dry probe also reads near 0 — put the probe in water. |
| `tds_ppm` shows a small number with nothing in water | Expected: a floating GPIO34 or a dry probe can sit at a steady low millivolts and the curve converts that voltage honestly. Always check `tds_mv` (shown next to ppm): real water typically lifts it well above the ~140 mV dry/floating floor. Confirm wiring by dipping the probe and watching ppm/mV rise. |
| `tds_ppm` = `-1` (`invalid` on the dashboard) | Oversample spread exceeded `TDS_MAX_SPREAD_MV` (100 mV) — input too noisy/unstable: check grounding (motor/GPS noise), probe seating, or shared-GND wiring. |
| `tds_mv` regularly near/above 2400 mV | Output exceeds the 0–2.3 V spec — you likely fitted a **0–5 V** TDS sensor. Disconnect it and add a divider before GPIO34 (ESP32 pins are not 5 V tolerant). |
| ppm plausible but off vs. a reference meter | Calibrate: measure with a reference meter, set `TDS_CALIBRATION` in boat_config.h (scale factor, datasheet curve default 1.0). |

## GENERAL SAFETY

- If anything smells hot or smokes, disconnect battery immediately.
- Never assume a pin rating; verify with a multimeter and datasheet.
- The firmware defaults motors OFF. If motors move unexpectedly, disconnect power and re-check safety logic before proceeding.
