# BUILD AND FLASHING

**Project:** AI-Based River Cleaning Boat  
**Build system:** PlatformIO ( Arduino framework for ESP32 )  
**Controllers:** ESP32-CAM (Controller A) and standard ESP32 (Controller B)

---

## 1. REQUIRED DEVELOPMENT TOOLS

| Tool | Purpose |
|---|---|
| Python 3 | PlatformIO runs on Python |
| PlatformIO Core (`pio`) | Build & flash both firmwares |
| A serial/USB cable | Connect each ESP32 to the laptop |
| (Optional) `pip install pyserial` | For the dashboard bridge |

Install PlatformIO:
```
pip install platformio
```

## 2. REQUIRED LIBRARIES

Both firmwares use only the **Arduino-ESP32 framework** (built into PlatformIO's
`espressif32` platform). No external Arduino libraries are required.

- `esp_camera.h` — provided by arduino-esp32 (ESP32-CAM camera driver).
- `HardwareSerial`, `ledc`, `pulseIn` — Arduino core.

## 3. REQUIRED MODEL FILES

No trained model is committed to the repository (see `.gitignore` and
`docs/AI_MODEL.md`). Until you provide one, the ESP32-CAM reports
`INF_STATUS_MODEL_NOT_LOADED` and the main controller treats the vision unit
as unhealthy.

To enable real detection:
1. Train or obtain a small INT8-quantized waste model.
2. Place it in `assets/model/` (not committed by default; use Git LFS if you
   want to version it).
3. Wire it into `firmware/esp32_cam_ai/src/inference.cpp`.
4. Update `docs/AI_MODEL.md` with real input dimensions, class labels, and
   measured RAM usage.

## 4. PROJECT STRUCTURE

```
firmware/esp32_cam_ai/      Controller A — AI vision (ESP32-CAM)
firmware/esp32_main_controller/  Controller B — navigation & control (ESP32)
shared/protocol/protocol.h  Shared framed serial protocol
dashboard/                  index.html + bridge.py (telemetry dashboard)
tests/                      Host-side Python tests
docs/                       All documentation
```

## 5. BUILD & FLASH — ESP32-CAM (Controller A)

```
cd firmware/esp32_cam_ai
pio run                    # compile
pio run -t upload          # flash
pio device monitor         # view serial logs (115200 baud)
```

**Flashing the ESP32-CAM:** Use the ESP32-CAM programmer module. To enter
upload mode, connect **GPIO0 to GND**, then press/release the **Reset** button
(or power-cycle). After upload, disconnect GPIO0 from GND for normal operation.

Select the correct serial port in PlatformIO (`pio device monitor -l` to list,
or set `upload_port` in `platformio.ini`).

## 6. BUILD & FLASH — STANDARD ESP32 (Controller B)

```
cd firmware/esp32_main_controller
pio run                    # compile
pio run -t upload          # flash
pio device monitor         # view serial logs (115200 baud)
```

Standard ESP32 DevKit boards usually enter upload mode automatically via DTR/RTS.
If not, hold **BOOT** and press **EN/Reset**, then release BOOT.

## 7. SELECTING THE SERIAL PORT

List ports:
```
pio device monitor -l
```
Then choose the right one. On Windows ports look like `COM3`; on Linux
`/dev/ttyUSB0`; on macOS `/dev/cu.usbserial-...`.

You can set it per project in `platformio.ini`:
```
upload_port = COM3
monitor_port = COM3
```

## 8. MONITORING SERIAL LOGS

```
pio device monitor
```
Baud: 115200 for both controllers.

## 9. TESTING COMMUNICATION BETWEEN CONTROLLERS

1. Flash both firmwares.
2. Connect ESP32-CAM GPIO1 (TX) -> main ESP32 GPIO3 (RX).
   Connect ESP32-CAM GPIO3 (RX) -> main ESP32 GPIO1 (TX).
   Connect GND to GND.
3. Open two serial monitors (one per controller) at 115200 baud.
4. You should see the ESP32-CAM send AI_NO_DETECTION / AI_HEALTH frames and the
   main ESP32 reply with MAIN_STATUS frames.
5. Communication loss (disconnect UART) must make the main controller report
   `comms_ok=false` and stop motors.

## 10. CONFIGURING WI-FI FOR DASHBOARD MONITORING (OPTIONAL)

The essential boat functions do **not** use Wi-Fi. The dashboard is optional
and runs on a laptop.

The dashboard bridge (`dashboard/bridge.py`) reads the main ESP32 serial port
and serves telemetry over HTTP on localhost:

```
pip install pyserial
python dashboard/bridge.py --port COM3 --baud 115200 --http-port 8080
```
Then open `http://localhost:8080`.

No Wi-Fi is configured on the ESP32s by default, so the two controllers never
create conflicting network configurations. If you later add ESP32 Web Server
or ESP-NOW, configure only the main controller as an access point and keep the
ESP32-CAM offline.

## 11. RECOVERING FROM COMMON FLASHING FAILURES

| Problem | Fix |
|---|---|
| `failed to connect` / `timed out` | Put board in upload mode (GPIO0->GND for CAM; BOOT button for DevKit), check port & baud |
| `esptool not found` | `pip install platformio --upgrade` |
| Brownout / reset during flash | Power ESP32-CAM from a stable 5 V source, not the programmer's 3.3 V alone |
| Serial monitor shows garbage | Confirm baud 115200, correct port, and that only one monitor is open |
| Upload succeeds but no output | Check GPIO0 is not held at GND in normal operation; press Reset |

## 12. RUNNING THE DASHBOARD

```
python dashboard/bridge.py --port <main-esp32-port> --baud 115200
open http://localhost:8080
```

The dashboard is read-only telemetry plus bounded manual commands (each auto-expires
after ~1 s). It is not a safety controller.

## 13. IMPORTANT — HARDWARE NOT YET VALIDATED

This documentation has not been verified against physical hardware. Pin numbers,
motor driver ratings, and power ratings in docs/PIN_MAPPING.md and
docs/COMPONENT_LIST.md are planned values marked `TODO (verify)`. Confirm with a
multimeter and datasheets before applying motor power.
