# COMMUNICATION PROTOCOL

**Project:** AI-Based River Cleaning Boat  
**Link:** ESP32-CAM (AI vision unit) <-> standard ESP32 (main control unit)  
**Physical layer:** UART (TTL serial, 3.3 V logic)  
**Baud rate:** 115200 bps (configurable)  
**Document:** docs/COMMUNICATION_PROTOCOL.md  
**Header:** shared/protocol/protocol.h

---

## 1. OVERVIEW

The two microcontrollers communicate over a single UART connection. The
ESP32-CAM is the **sender of AI detection messages**; the standard ESP32 is
the **receiver and actuator controller**. Both sides may send status/fault
messages.

The protocol is deliberately small and dependency-free so it compiles on both
controllers without external libraries.

Key design goals:
- No inference on the main controller (AI stays on ESP32-CAM).
- No internet required for any message.
- Every frame is framed, checksummed, sequenced, and length-checked.
- Malformed frames are rejected; communication failure drives the main
  controller to a safe state.

---

## 2. PHYSICAL CONNECTION

| Signal | ESP32-CAM | Standard ESP32 | Notes |
|---|---|---|---|
| TX | GPIO1 (UART0 TX) | GPIO3 (UART0 RX) | CAM TX -> MAIN RX |
| RX | GPIO3 (UART0 RX) | GPIO1 (UART0 TX) | MAIN TX -> CAM RX |
| GND | GND | GND | Common ground required |
| VCC | 5 V (from buck) | 5 V (from buck) | Separate regulators OK if grounds tied |

Logic levels: both ESP32s are 3.3 V, so no level shifter is needed between
them. Do **not** connect 5 V UART devices directly to an ESP32 GPIO.

Pin numbers are **planned values** and must be verified in
docs/PIN_MAPPING.md before wiring. UART0 on the ESP32-CAM is shared with the
programmer/Serial bootloader; flashing the CAM requires GPIO0 held low.

---

## 3. FRAME FORMAT

All multi-byte integers are little-endian.

```
Offset  Field             Size    Description
0       START_BYTE        1       0x7E (frame start)
1       VERSION           1       0x01 (protocol version)
2       TYPE              1       message type (see section 4)
3-4     SEQUENCE          2       incrementing frame sequence (mod 65536)
5-6     PAYLOAD_LENGTH    2       N, number of payload bytes
7..7+N-1 PAYLOAD          N       message payload
7+N     CHECKSUM          1       sum of bytes [1 .. 6+N-1] mod 256
8+N     END_BYTE          1       0x7F (frame end)
```

Maximum payload: 64 bytes. Maximum frame: 71 bytes.

### Checksum
The checksum is an 8-bit additive checksum over every byte from VERSION
through the last payload byte, inclusive. A receiver recomputes it and
rejects the frame on mismatch.

### Sequence numbers
Each controller increments its own transmit sequence. The receiver uses the
sequence to detect stale or missed frames; a very old sequence relative to
the expected window is treated as stale.

---

## 4. MESSAGE TYPES

| Type (hex) | Name                 | Direction | Purpose |
|---|---|---|---|
| 0x01 | AI_DETECTION      | CAM -> MAIN | A waste object was detected above confidence threshold |
| 0x02 | AI_NO_DETECTION   | CAM -> MAIN | No reliable target in current frame |
| 0x03 | AI_HEALTH         | CAM -> MAIN | Camera + inference health telemetry |
| 0x10 | MAIN_STATUS       | MAIN -> CAM | Controller state, motors, conveyor, faults |
| 0x20 | DIAG_COMMAND      | MAIN -> CAM | Optional diagnostic commands (e.g. resend health) |
| 0x30 | FAULT_REPORT      | MAIN -> CAM | Fault / emergency-stop notification |

A **no-detection message is a valid message**. The absence of any message
is never interpreted as a detection.

---

## 5. PAYLOAD DEFINITIONS

### 5.1 AI_DETECTION (0x01) — 18 bytes

| Field | Size | Meaning |
|---|---|---|
| class_id | u8 | class index (see docs/AI_MODEL.md) |
| confidence | u8 | 0..100 percent |
| bbox_x | i16 | 0..1000 normalized |
| bbox_y | i16 | 0..1000 normalized |
| bbox_w | i16 | 0..1000 normalized |
| bbox_h | i16 | 0..1000 normalized |
| center_x | i16 | -500..+500 horizontal offset from image centre |
| timestamp_ms | u32 | CAM local milliseconds since boot |
| camera_health | u8 | 0 = OK, nonzero = problem |
| inference_status | u8 | see inference_status_t |

`center_x` is the value the main controller uses for differential steering:
negative = target left of centre, positive = target right of centre.

### 5.2 AI_NO_DETECTION (0x02) — 9 bytes

| Field | Size | Meaning |
|---|---|---|
| reason | u8 | 0 = none_above_threshold, 1 = low_confidence, 2 = invalid_frame |
| timestamp_ms | u32 | CAM local ms since boot |
| camera_health | u8 | 0 = OK |
| inference_status | u8 | see inference_status_t |

### 5.3 AI_HEALTH (0x03) — 10 bytes

Periodic heartbeat. Fields: camera_health, inference_status,
frames_captured, inferences_run, inference_errors (u16), uptime_ms (u32).

### 5.4 MAIN_STATUS (0x10) — 14 bytes

| Field | Size | Meaning |
|---|---|---|
| nav_state | u8 | current navigation state (see docs/STATE_MACHINE.md) |
| motor_left_pwm | u8 | 0..255 |
| motor_right_pwm | u8 | 0..255 |
| motor_left_dir | i8 | -1 reverse, 0 stop, 1 forward |
| motor_right_dir | i8 | -1 reverse, 0 stop, 1 forward |
| conveyor_state | u8 | conveyor state (OFF/STARTING/RUNNING/STOPPING/FAULT) |
| comm_health | u8 | 0 = OK, nonzero = degraded |
| fault_flags | u8 | bitmask of active faults |
| collection_count | u16 | collection cycle counter |
| uptime_ms | u32 | MAIN local ms since boot |

### 5.5 DIAG_COMMAND (0x20) — 1 byte

Payload: command code (0x01 = request health, 0x02 = reset inference, ...).

### 5.6 FAULT_REPORT (0x30) — 1 byte

Payload: fault code bitmask.

---

## 6. RECEIVER BEHAVIOUR

- Parse one byte at a time (`protocol_rx_push`).
- Reject frames with bad start/end byte, bad version, bad length,
  bad checksum, unknown type, or buffer overflow.
- On a valid frame, dispatch by type.
- Track last-valid-frame timestamp per message stream.
- If no valid AI frame arrives within `AI_COMM_TIMEOUT_MS`, the main
  controller marks detections as stale and transitions away from target
  tracking toward a safe state.

---

## 7. STALENESS AND SAFE STATE

- A detection is considered **fresh** only if its timestamp (or receipt time)
  is within `DETECTION_FRESHNESS_MS` (default 1000 ms).
- Consecutive valid detections (`CONSECUTIVE_DETECTIONS_REQUIRED`, default 2)
  are required before the main controller commits to target tracking.
- On communication loss: propulsion PWM -> 0, conveyor -> OFF, navigation
  -> SAFE_STOP/FAULT. Motors do **not** continue on stale commands.

---

## 8. ERROR HANDLING SUMMARY

| Condition | Action |
|---|---|
| Bad checksum | Drop frame, increment error counter, keep previous state |
| Bad length / unknown type | Drop frame, do not act on payload |
| UART silence > timeout | Mark AI comms lost, stop target-seeking, safe stop |
| ESP32-CAM reboot | Sequence resets; main controller resyncs on next valid frame |
| Main controller reboot | Re-initialise all outputs to OFF before enabling anything |

---

## 9. SOURCE

- Message structures and wire encoding: `shared/protocol/protocol.h`
- ESP32-CAM transmitter: `firmware/esp32_cam_ai/src/`
- Standard ESP32 receiver: `firmware/esp32_main_controller/src/`
