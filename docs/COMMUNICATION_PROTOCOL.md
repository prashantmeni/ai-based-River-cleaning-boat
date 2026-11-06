# COMMUNICATION PROTOCOL

**Project:** AI-Based River Cleaning Boat  
**Link:** ESP32-CAM (AI vision unit) <-> standard ESP32 (main control unit)  
**Physical layer:** ESP-NOW wireless (2.4 GHz, channel 1, broadcast, no router)  
**Transport:** one protocol frame per ESP-NOW datagram (frames <= 73 bytes, datagram limit 250)  
**Document:** docs/COMMUNICATION_PROTOCOL.md  
**Header:** shared/protocol/protocol.h  
**Link helpers:** shared/protocol/espnow_link.h

---

## 1. OVERVIEW

The two microcontrollers communicate over a wireless ESP-NOW link (no
router, no pairing, no wires between the boards). The ESP32-CAM is the
**sender of AI detection messages**; the standard ESP32 is the
**receiver and actuator controller**. Both sides may send status/fault
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

## 2. PHYSICAL LAYER (ESP-NOW)

| Property | Value |
|---|---|
| Radio | ESP-NOW (2.4 GHz Wi-Fi radio, no AP/router needed for the link itself) |
| Channel | 1 on both boards at boot (set with `esp_wifi_set_channel`); re-homed by `espnow_link_resync()` when a STA join moves the radio (see below) |
| Addressing | Broadcast `FF:FF:FF:FF:FF:FF` (no MAC configuration) |
| Encryption | None (trusted local link on the hull) |
| Modem sleep | Disabled (`WiFi.setSleep(false)`) |
| Payload limit | 250 bytes/datagram — each frame travels as one datagram |
| Inter-board wires | **None** — only each board's own power |

UART0 on each board is used **only for local debug output / flashing**
(CAM: `[CAM]` prints on its USB adapter; MAIN: JSON telemetry + dashboard
commands on the USB port). No signal wire exists between the boards, so
neither board's boot-strapping pins are affected by the link.

**Channel following:** the link needs no router, but if a board's STA joins
one (the CAM for video — cascade in `network_bringup()`, or the main board's
STA mode), the radio moves to the router's channel. `espnow_link_resync()`
detects the mismatch (`get_channel != s_home_ch`, throttled to 1/s), re-homes
ESP-NOW to the new channel, and logs one
`[ESPNOW] channel X -> Y, link rebuilt` line — verified live when the router
moved from channel 1 to 6 with reception continuing (`comms_ok` stayed true).

Board power: each ESP32 is powered from the 5 V buck; grounds should
still be common across the hull for sensor/motor logic.

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

Maximum payload: 64 bytes. Maximum frame: 73 bytes (9 bytes overhead).

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

Note: with no trained model present, `class_id`/`confidence` come from the
on-device CV shape heuristic (`inference.cpp`, see docs/AI_MODEL.md §2) —
best-effort labels, honestly measured confidence. The frame format and
thresholds are model-agnostic.

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
| Link silence > timeout | Mark AI comms lost, stop target-seeking, safe stop |
| ESP32-CAM reboot | Sequence resets; main controller resyncs on next valid frame |
| Main controller reboot | Re-initialise all outputs to OFF before enabling anything |

---

## 9. CLOUD MQTT COMMAND + ACK PATH (dashboard / app)

The laptop dashboard and phone app reach the main controller over the public
cloud MQTT path (HiveMQ WSS). This is separate from the ESP-NOW link above.

| Topic | Direction | Payload |
|---|---|---|
| `riverclean/boat/telemetry` | MAIN -> broker | telemetry JSON (§5 equivalent, same schema as USB) |
| `riverclean/boat/command` | client -> broker -> MAIN | command text (below) |
| `riverclean/boat/ack` | MAIN -> broker | ack JSON (below) |

**Command text** — both forms are accepted (identical semantics to the
WebSocket gateway):

```
CMD:<cmd>,<value>,<duration_ms>            # plain (Flutter app)
#<id> CMD:<cmd>,<value>,<duration_ms>      # with correlation id (dashboard)
```

`<cmd>` must be `DIAG_CMD_STOP(1)`..`DIAG_CMD_AUTONOMOUS_OFF(8)`.
Anything else is rejected but still acked, so clients never hang.

**Ack JSON** (published for every accepted *and* rejected command):

```json
{"type":"ack","id":"cWEB1","status":"ok","cmd":1}
{"type":"ack","id":"cWEB2","status":"rejected","cmd":99,"reason":"invalid command"}
```

The id is echoed verbatim (empty string when the payload had no `#id`
prefix). The ack is built inside the PubSubClient callback but published from
`maintain_mqtt()` after `loop()` returns (PubSubClient publish is not
re-entrant safe inside its own callback).

**Verified live:** `#cWEB1 CMD:1,0,0` -> ok ack (this exact form silently
failed before the fix — the callback required the payload to start with
`CMD:` and the dashboard never received acks, showing a false timeout);
garbage payload and out-of-range cmd -> rejected ack with reason; plain
`CMD:1,0,0` -> ok ack.

---

## 10. SOURCE

- Message structures and wire encoding: `shared/protocol/protocol.h`
- ESP32-CAM transmitter: `firmware/esp32_cam_ai/src/`
- Standard ESP32 receiver: `firmware/esp32_main_controller/src/`
