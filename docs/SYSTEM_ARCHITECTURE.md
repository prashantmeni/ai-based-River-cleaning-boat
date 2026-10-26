# SYSTEM ARCHITECTURE

**Project:** AI-Based River Cleaning Boat  
**Status:** PLANNED  
**Related docs:** docs/PIN_MAPPING.md, docs/WIRING_DIAGRAM.md,
docs/COMMUNICATION_PROTOCOL.md, docs/AI_MODEL.md, docs/STATE_MACHINE.md

---

## 1. HIGH-LEVEL DIAGRAM

```
   [ESP32-CAM: AI Vision Unit]  UART  [ESP32: Main Control Unit]
   ---------------------------   ---   -------------------------
   OV2640 camera                          Dual motor driver
   TinyML inference                       2x DC gear motors
   Structured serial output               Ultrasonic L/R
                                          GPS module
                                           Conveyor motor
                                           Buzzer
                                             |
                                             v
                                         Dashboard (optional Wi-Fi / serial)
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
- Structured serial output: AI_DETECTION / AI_NO_DETECTION / AI_HEALTH
- Serial diagnostics
- Never drives motors. Never makes safety decisions.

### Controller B — Standard ESP32 (Main Control Unit)
- UART reception, validation, checksum, sequence handling
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

### Dashboard (`dashboard/`)
- Web UI (single HTML/JS file) served optionally by ESP32 or run on laptop
- reads serial telemetry via a serial-to-web bridge or ESP32 web server

---

## 4. COMMUNICATION

Framed, checksummed, sequenced UART at 115200 baud. See
docs/COMMUNICATION_PROTOCOL.md.

Main controller invalidates stale detections. On communication loss it
stops target-seeking and enters a safe state.

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
