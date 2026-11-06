# SYSTEM ARCHITECTURE

**Project:** AI-Based River Cleaning Boat  
**Status:** PLANNED  
**Related docs:** docs/PIN_MAPPING.md, docs/WIRING_DIAGRAM.md,
docs/COMMUNICATION_PROTOCOL.md, docs/AI_MODEL.md, docs/STATE_MACHINE.md

---

## 1. HIGH-LEVEL DIAGRAM

```
   [ESP32-CAM: AI Vision Unit] ESP-NOW  [ESP32: Main Control Unit]
   ---------------------------   ---   -------------------------
   OV2640 camera                          Dual motor driver
   TinyML inference                       2x DC gear motors
   Detection frames (wireless)            Ultrasonic L/R
                                           GPS module
                                            Conveyor motor
                                            Buzzer
                                              |
                                              v
                                          Dashboard (laptop serial bridge)
```

- Controller A (ESP32-CAM) owns the camera and ALL AI inference.
- Controller B (standard ESP32) owns motors, sensors, GPS, conveyor,
  navigation state machine, and safety.
- No cloud, no laptop inference, no internet required for autonomy.

---

## 2. RESPONSIBILITY SPLIT

### Controller A — ESP32-CAM (AI Vision Unit)
- Camera capture + frame buffering (reuse frame buffers)
- Image preprocessing (resize/grayscale/normalize) tuned to the model
- Local TinyML inference (INT8, small input)
- Confidence thresholding
- Temporal filtering (consecutive-detection count)
- Structured output: AI_DETECTION / AI_NO_DETECTION / AI_HEALTH (ESP-NOW)
- Debug console on UART0 (USB adapter)
- Never drives motors. Never makes safety decisions.

### Controller B — Standard ESP32 (Main Control Unit)
- ESP-NOW reception, validation, checksum, sequence handling
- Differential motor drive (PWM + direction), safe OFF defaults
- Ultrasonic distance + filtering + invalid-reading detection
- GPS NMEA parsing + fix validity
- Conveyor state machine with timeout
- Autonomous navigation state machine (SEE docs/STATE_MACHINE.md)
- Safety manager: watchdog, emergency stop, comms-loss safe state
- Buzzer control
- Telemetry to dashboard (optional local Wi-Fi or serial)

---

## 3. SOFTWARE LAYERS (per controller)

### ESP32-CAM firmware (`firmware/esp32_cam_ai/`)
- `camera.cpp` — OV2640 init, frame capture
- `preprocess.cpp` — resize/grayscale/normalize for model input
- `inference.cpp` — model load + run (TFLM / ESP-NN stub)
- `detection.cpp` — confidence threshold, temporal filter, result struct
- `serial.cpp` — protocol framing + transmit
- `main.cpp` — setup/loop, non-blocking inference scheduler

### Standard ESP32 firmware (`firmware/esp32_main_controller/`)
- `serial_rx.cpp` — protocol receiver
- `motor.cpp` — motor abstraction + PWM/direction
- `navigation.cpp` — state machine
- `obstacle.cpp` — ultrasonic driver + filtering
- `gps.cpp` — NMEA parser
- `conveyor.cpp` — conveyor state machine
- `safety.cpp` — watchdog, fault flags, emergency stop
- `telemetry.cpp` — optional dashboard reporting
- `main.cpp` — setup/loop, scheduler

### Shared (`shared/`)
- `protocol/protocol.h` — frame format, payload structs, encode/decode
- `protocol/espnow_link.h` — ESP-NOW helpers (broadcast, channel 1) + optional
  WPA2 SoftAP on the main controller

### Dashboard (`dashboard-ui/` — primary)
- React + TypeScript + Vite + Tailwind SPA, 5 pages, offline-first (localStorage)
- Two selectable transports, auto-picked from the Settings endpoint
  (`transportForUrl()` in `communication/connectionManager.ts`): cloud MQTT
  over WSS (`wss://broker.hivemq.com:8884/mqtt` — telemetry in, commands out,
  acks on `riverclean/boat/ack`) **or direct Wi-Fi** to the boat's own
  gateway (`ws://192.168.4.1:81`, no internet; laptop joins the boat AP or
  the same router). Any URL containing `/mqtt` → broker, any other `ws://`
  → direct. Identical JSON schema on both; USB bridge as legacy fallback
- Commands carry client ids and await firmware acks; telemetry schema is a
  mirror of telemetry.cpp (typed in `src/services/communication/types.ts`)

### Dashboard (`dashboard/` — legacy fallback)
- Web UI (single HTML/JS file) + `bridge.py` serial→HTTP bridge for the USB
  path (COM port → http://localhost:8080)

---

## 4. COMMUNICATION

Framed, checksummed, sequenced frames over an ESP-NOW wireless link
(channel 1, broadcast, one frame per datagram). See
docs/COMMUNICATION_PROTOCOL.md. The USB/UART0 serial port on the main
controller is reserved for JSON telemetry and debug.

**Dashboard path:** either cloud MQTT (topics above; commands accepted
with or without a `#<id>` prefix, acked on `riverclean/boat/ack` — see
docs/COMMUNICATION_PROTOCOL.md §9) or direct WebSocket to the boat (same
JSON over plain text frames; the dashboard picks the transport from the
endpoint URL). The main controller also runs
`WIFI_AP_STA` — ESP-NOW stays on channel 1 while a WPA2 SoftAP
("RiverClean-Boat", 192.168.4.1) serves a WebSockets server on port 81
(`ws_gateway.cpp`); the browser sends `[#id] CMD:cmd,value,dur`
text frames and receives hello/ack/telemetry JSON there too. Both radios
share the one ESP32 radio on channel 1 by contract.

Main controller invalidates stale detections. On communication loss it
latches a fault, stops target-seeking and enters a safe state.

---

## 5. SAFETY PRINCIPLES

- Motors default OFF after boot; enabled only after successful init + safety
  checks.
- No automatic motion at boot.
- 5 V sensor signals level-shifted to 3.3 V before ESP32 GPIOs.
- Motors never powered from ESP32 GPIO / regulator pins.
- Emergency stop (firmware) disables propulsion + conveyor immediately.
- Communication loss => safe stop, never blind motion.

---

## 6. OFFLINE OPERATION

AI inference runs entirely on the ESP32-CAM. Navigation, obstacle avoidance,
and motor control are local. The dashboard is optional and may connect over
local Wi-Fi or serial; the boat operates without it.
