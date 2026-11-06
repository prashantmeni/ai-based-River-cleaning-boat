# PIN MAPPING

**Project:** AI-Based River Cleaning Boat  
**Status:** As implemented in `firmware/esp32_main_controller/include/boat_config.h`
and `firmware/esp32_cam_ai/include/ai_config.h`. Values marked `TODO` are
unverified against the physical hardware.

---

## 1. ESP32-CAM (AI-Thinker, AI vision unit)

The ESP32-CAM has fixed onboard connections for the camera and flash.
The link to the main controller is **ESP-NOW wireless** — no signal
wires leave this board.

| Function | GPIO | Notes |
|---|---|---|
| UART0 TX (debug console) | GPIO1 | `[CAM]` prints on USB adapter only |
| UART0 RX (debug console) | GPIO3 | Unused; flashing requires GPIO0 low |
| Link to main controller | — | **ESP-NOW wireless, channel 1, no wires** |
| Camera OV2640 data | GPIO0,2,4,5,12,13,15,17 | Fixed onboard, do not repurpose |
| Camera XCLK | GPIO0 | Fixed onboard |
| Camera PCLK | GPIO5 | Fixed onboard |
| Camera VSYNC | GPIO25 | Fixed onboard |
| Camera HREF | GPIO23 | Fixed onboard |
| Camera SIOC/SIOD | GPIO27/GPIO26 | Fixed onboard |
| Camera PWDN / RESET | GPIO32 / GPIO16 | Fixed onboard |
| Flash (GPIO4) | GPIO4 | Shared with camera data — avoid |
| GPIO16 (UART2 RX) | GPIO16 | Also camera reset; avoid |
| GPIO17 (UART2 TX) | GPIO17 | Also camera data; avoid |
| Built-in LED | GPIO33 | Status LED (`STATUS_LED_GPIO`) |
| SD / flash pins | GPIO2,4,12,13,14,15 | Shared; not used for UART |

Boot-strapping pins on ESP32-CAM: GPIO0, GPIO2, GPIO5, GPIO12 must not be
driven externally at boot in a way that forces an unwanted boot mode.

> **WARNING:** Do not repurpose camera pins. Do not connect 5 V to any ESP32-CAM
> GPIO. The ESP32-CAM has no PSRAM on the common AI-Thinker variant; keep the
> model small.

---

## 2. STANDARD ESP32 DEV BOARD (main control unit)

| Function | GPIO | Notes |
|---|---|---|
| Link to ESP32-CAM | — | **ESP-NOW wireless, channel 1 — no signal wires** |
| USB serial (UART0) | GPIO1/GPIO3 | JSON telemetry out + `CMD:` commands in |
| Motor driver ENA (left PWM) | GPIO25 | PWM, 0..255 — **remove ENA jumper cap** |
| Motor driver IN1 (left dir A) | GPIO26 | L298N direction input |
| Motor driver IN2 (left dir B) | GPIO16 | L298N direction input (moved off strapping pin GPIO12 — `MOTOR_LEFT_IN2_GPIO`; **TODO (verify)** which pin is physically wired) |
| Motor driver ENB (right PWM) | GPIO27 | PWM — **remove ENB jumper cap** |
| Motor driver IN3 (right dir A) | GPIO14 | L298N direction input |
| Motor driver IN4 (right dir B) | GPIO19 | L298N direction input |
| Ultrasonic LEFT trig | GPIO32 | 3.3 V trigger |
| Ultrasonic LEFT echo | GPIO33 | via level divider (5 V -> 3.3 V) |
| Ultrasonic RIGHT trig | GPIO13 | shared with GPS TX (GPS is RX-only) |
| Ultrasonic RIGHT echo | GPIO15 | via level divider (5 V -> 3.3 V) |
| Conveyor motor EN (PWM) | GPIO23 | **remove EN jumper cap** on 2nd HW-095 |
| Conveyor motor IN1 | GPIO22 | fixed HIGH (one direction) |
| Conveyor motor IN2 | GPIO21 | fixed LOW |
| GPS UART RX | GPIO4 | <- GPS TX (NMEA @9600) |
| GPS UART TX | GPIO13 | not wired; pin re-claimed by ultrasonic RIGHT trig |
| TDS meter (SEN0244 OUT) | GPIO34 | ADC1_CH6, input-only; 0–2.3 V direct — **never >3.6 V** (5 V sensors need a divider) |
| Buzzer | GPIO5 | 3.3 V driven or via transistor |
| Status LED red | GPIO17 | Solid = boat not ready (fault / no GPS fix / CAM link down) |
| Status LED green | GPIO18 | Solid = boat ready (was listed as optional E-STOP input — re-purposed; **TODO (verify)** LED wiring) |
| Status LED blue | GPIO2 | Blinking = connecting to internet, solid = connected (strapping pin; LED load is safe) |
| Emergency-stop input | — | Not implemented; GPIO18 went to the green status LED. Re-add on a free pin if wanted. |

### Motor driver: HW-095 (L298N dual H-bridge)
The driver is an **HW-095 board built around the L298N chip** (2 A/channel,
5–35 V motor supply, 5 V logic). The firmware drives it as ENx (PWM speed)
+ IN1..IN4 (direction pair per channel). There is **no STBY pin** — earlier
firmware drafts assumed a TB6612; the code now matches the L298N.

### Pin sharing note
GPIO13 carries both the ultrasonic RIGHT TRIG output and the unused GPS
UART TX function. `GPS_UART_RX_GPIO=4` is claimed first at boot, then
`obs.begin()` re-claims GPIO13 as a plain output. GPS is receive-only in
this design, so no GPS TX wire exists.

---

## 3. MOTOR DRIVER (HW-095 / L298N dual H-bridge)

| Function | Connection |
|---|---|
| +12V / VMOT | Battery via fuse (6–35 V per L298N; match motor rating) |
| 5V | Logic supply (see note) |
| GND | Common with ESP32 and battery |
| ENA | ESP32 GPIO25 (PWM) — **jumper cap removed** |
| IN1 | ESP32 GPIO26 |
| IN2 | ESP32 GPIO12 |
| ENB | ESP32 GPIO27 (PWM) — **jumper cap removed** |
| IN3 | ESP32 GPIO14 |
| IN4 | ESP32 GPIO19 |
| OUT1/OUT2 | Left propulsion motor |
| OUT3/OUT4 | Right propulsion motor |

Notes:
- **Remove the ENA/ENB jumper caps** or speed control is ignored (pins
  forced high → full throttle).
- The HW-095's on-board 5 V regulator (78M05) can power its logic when
  VMOT ≤ 12 V — do not feed 5 V into the 5V pin while the jumper is on.
- L298N drops ~2 V per bridge; size the motor supply accordingly.
- Driver current rating: 2 A/channel max. Motor stall current is
  **unknown** — verify before wiring. Do not power motors from ESP32
  3.3 V or 5 V regulator pins.
- A **second HW-095** drives the conveyor: EN=GPIO23, IN1=GPIO22 (HIGH),
  IN2=GPIO21 (LOW) — fixed direction, PWM speed, EN jumper removed.

---

## 4. ULTRASONIC SENSORS (5 V echo — level shift required)

Most common ultrasonic modules (e.g., HC-SR04) drive Echo at 5 V. The ESP32
GPIOs are 3.3 V and are not 5 V tolerant.

| Signal | Wiring |
|---|---|
| VCC | 5 V |
| GND | Common GND |
| Trig | ESP32 GPIO (3.3 V drive is sufficient to trigger) |
| Echo | Voltage divider: 1 kΩ (Echo->GPIO) + 2 kΩ (GPIO->GND) gives ~3.3 V |

Do **not** connect Echo directly to an ESP32 GPIO if the module is 5 V.

---

## 5. GPS MODULE

| Function | Connection |
|---|---|
| VCC | 5 V or 3.3 V per module |
| GND | Common GND |
| TX | ESP32 GPIO4 (GPS UART RX) |
| RX | not wired (GPS is receive-only in this design) |

Baud rate: 9600 (`GPS_BAUD` in boat_config.h).

---

## 5b. TDS METER (DFRobot Gravity SEN0244)

| Function | Connection |
|---|---|
| VCC | 3.3 V (sensor is specified for 3.3 V) |
| GND | Common GND |
| OUT | ESP32 **GPIO34** (ADC1_CH6, input-only) — direct, no divider |

Notes:
- Output range 0–2.3 V → safe for the ESP32 ADC at 11 dB attenuation
  (firmware sets this in `TdsSensor::begin()`).
- **Never connect a 0–5 V sensor output directly.** If you substitute one,
  add the 1 kΩ/2 kΩ divider from §4 first.
- Firmware exposes `tds_ppm` (converted, clamped 0–3000) **and** `tds_mv`
  (raw averaged millivolts) in telemetry — check `tds_mv` when validating
  wiring; calibrate with `TDS_CALIBRATION` against a reference meter.

---

## 6. POWER

| Rail | Source | Notes |
|---|---|---|
| Battery | Li-ion cell(s) | Main fuse + switch first |
| 5 V buck | Battery -> 5 V | Powers ESP32s (via their 5 V-in), sensors |
| ESP32 3.3 V | Onboard regulator from 5 V | Powers ESP32 logic |
| Motor supply | Battery (fused) | NOT from ESP32 regulator |

Li-ion cells must not be connected in arbitrary series/parallel. Use a
protection BMS and a proper Li-ion charger. Do not assume a basic charging
module is a BMS.

---

## 7. UNVERIFIED SPECIFICATIONS (verify before power-up)

- Motor driver: HW-095 (L298N) identified — logic 5 V (3.3 V inputs OK);
  current rating 2 A/channel; **motor stall current still unknown**
- DC gear motor nominal voltage, stall current
- Ultrasonic module model and echo voltage
- GPS module model and baud rate
- Battery cell count / nominal voltage; BMS presence
- 5 V buck converter current capacity

Mark every unverified value `TODO (verify)` in the build guide and confirm
with a multimeter before energising motors.

---

## 8. GROUNDING RULE

A single common ground reference must exist between the ESP32-CAM, the main
ESP32, the motor driver logic, sensors, and the 5 V buck converter. Motor
high-current return should flow directly back to the battery, not through
signal ground where avoidable.
