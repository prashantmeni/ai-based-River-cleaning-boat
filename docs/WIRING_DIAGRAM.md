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
              | GPIO1 TX -------+----| GPIO3 RX      |      |               |
              | GPIO3 RX <-----------+ GPIO1 TX      |      +---------------+
              |               |      |               |
              |               |      +---------------+
              |               |         |   |   |   |
              |               |         v   v   v   v
              |               |   +---------------+
              |               |   | Motor Driver  |
              |               |   | (dual channel)|
              |               |   | VMOT <- bat   |
              |               |   | VCC <- 5 V    |
              |               |   | GND ----------+-- Common GND
              |               |   | L_PWM <- GPIO25 |
              |               |   | L_DIR <- GPIO26 |
              |               |   | R_PWM <- GPIO27 |
              |               |   | R_DIR <- GPIO14 |
              |               |   | STBY  <- GPIO12 |
              |               |   |               |
              |               |   | L_Motor       | R_Motor
              |               |   | (propulsion)  | (propulsion)
              |               |   +---------------+
              |               |
              |               |   +---------------+
              |               |   | Conveyor Motor|
              |               |   | PWM <- GPIO14 |
              |               |   | DIR <- GPIO27 |
              |               |   +---------------+
              |               |         |   |   |
              |               |         v   v   v
              |               |   +---------------+
              |               |   | GPS Module    |
              |               |   | VCC <- 5 V    |
              |               |   | TX -> GPIO16  |
              |               |   | RX <- GPIO17  |
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
| Motor driver logic voltage (3.3 V vs 5 V) | Driver model unknown |
| Motor VMOT voltage vs battery voltage | Battery cell count unknown |
| Conveyor needs separate driver? | Only one dual-channel driver purchased; conveyor motor needs its own driver or shared channel — not yet resolved |
| GPS baud rate | Module unknown |
| Buck converter current for motors + ESP32s | Converter rating unknown |

Do not energise the motor supply until these are confirmed with a multimeter
and the driver/motor datasheets.

---

## 4. GROUNDING & SAFETY NOTES

- One common ground for all logic (ESP32s, driver logic, sensors).
- Motor high-current return runs directly to battery negative.
- Fuse and main switch on the battery positive rail, before the buck.
- Keep motor wiring away from the UART and GPS signal wires.
- Keep batteries and electronics above the waterline in a sealed enclosure.
