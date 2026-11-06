# WIRING DIAGRAM

**Project:** AI-Based River Cleaning Boat  
**Status:** PLANNED — verify every connection against actual hardware before
power-up. This diagram reflects the pin assignments in docs/PIN_MAPPING.md.

---

## 1. TEXTUAL WIRING DIAGRAM

```
                                   +---------------------+
                                   |   BATTERY (Li-ion)  |
                                   |   + FUSE + SWITCH   |
                                   +----------+----------+
                                              |
                                              v
                                  +---------------------+
                                  | 5 V DC-DC BUCK      |
                                  | (motor + ESP rail)  |
                                  +----------+----------+
                                             |   |   |
                      +--------------------+   |   +--------------------+
                      |                        |                        |
                      v                        v                        v
               +---------------+      +---------------+      +---------------+
               | ESP32-CAM     |      | ESP32 MAIN    |      | Ultrasonic L  |
               | (3.3 V logic) |      | (3.3 V logic) |      | VCC 5 V       |
               | VCC <- 5 V    |      | VCC <- 5 V    |      | GND ----------+-- Common GND
               | GND ----------+------| GND ----------+------| Trig -> GPIO32|
               |               |      |               |      | Echo -> div -> GPIO33
               |      ESP-NOW wireless link (2.4 GHz ch.1)    |               |
               |      NO signal wires between boards         +---------------+
               |               |      |               |
              |               |      +---------------+
              |               |         |   |   |   |
              |               |         v   v   v   v
               |               |   +---------------+
               |               |   | Motor Driver  |
               |               |   | HW-095 (L298N)|
               |               |   | +12V <- bat   |
               |               |   | 5V  <- logic  |
               |               |   | GND ----------+-- Common GND
               |               |   | ENA <- GPIO25 |
               |               |   | IN1 <- GPIO26 |
               |               |   | IN2 <- GPIO12 |
               |               |   | ENB <- GPIO27 |
               |               |   | IN3 <- GPIO14 |
               |               |   | IN4 <- GPIO19 |
               |               |   |               |
               |               |   | L_Motor       | R_Motor
               |               |   | (propulsion)  | (propulsion)
               |               |   +---------------+
              |               |
              |               |   +---------------+
               |               |   | 2nd HW-095    |
               |               |   | (conveyor)    |
               |               |   | EN  <- GPIO23 |
               |               |   | IN1 <- GPIO22 |
               |               |   | IN2 <- GPIO21 |
               |               |   | OUT -> Conveyor Motor
               |               |   +---------------+
               |               |         |   |   |
               |               |         v   v   v
               |               |   +---------------+
               |               |   | GPS Module    |
               |               |   | VCC <- 5 V    |
               |               |   | TX -> GPIO4   |
               |               |   | RX (not wired)|
               |               |   | GND ----------+
              |               |   +---------------+
              |               |
              |               v
              |         +---------------+
              |         | Buzzer <- GPIO5|
              |         +---------------+
              |
              +----> Status LED (ESP32-CAM GPIO33)
```

Legend:
- `->` = signal output from ESP32
- `<-` = signal input to ESP32
- `div` = 1 kΩ + 2 kΩ voltage divider on 5 V Echo to 3.3 V
- The ESP32-CAM <-> MAIN link is **ESP-NOW wireless**: power each board
  from the 5 V buck (common ground), but run **no signal wires** between
  them. UART0 on each board is only for USB flashing/debug.

---

## 2. LEVEL SHIFTING (5 V Echo -> 3.3 V ESP32)

```
Ultrasonic Echo (5 V) ----[1 kΩ]----+----> ESP32 GPIO (3.3 V max)
                                    |
                                   [2 kΩ]
                                    |
                                  GND
```

Result at GPIO ≈ 5 V × 2 kΩ / (1 kΩ + 2 kΩ) ≈ 3.3 V. Safe for ESP32.

---

## 3. CONNECTIONS REQUIRING VERIFICATION

| Item | Why unverified |
|---|---|
| Motor VMOT voltage vs battery voltage | Battery cell count unknown (HW-095 accepts 5–35 V) |
| Motor stall current vs 2 A/channel | Motors unknown — L298N has no current limiting |
| Conveyor driver | Resolved — second HW-095 board owned (channel used for conveyor) |
| GPS baud rate | Module unknown |
| Buck converter current for motors + ESP32s | Converter rating unknown |

Do not energise the motor supply until these are confirmed with a multimeter
and the driver/motor datasheets.

---

## 4. GROUNDING & SAFETY NOTES

- One common ground for all logic (ESP32s, driver logic, sensors).
- Motor high-current return runs directly to battery negative.
- Fuse and main switch on the battery positive rail, before the buck.
- Keep motor wiring away from the GPS signal wire (GPIO4).
- Keep batteries and electronics above the waterline in a sealed enclosure.
