# PIN MAPPING

**Project:** AI-Based River Cleaning Boat  
**Status:** PLANNED — all pin assignments must be verified against the actual
purchased hardware before powering up. Values marked `TODO` are unverified.

---

## 1. ESP32-CAM (AI-Thinker, AI vision unit)

The ESP32-CAM has fixed onboard connections for the camera and flash. Only a
few GPIOs are safely available for user UART.

| Function | GPIO | Notes |
|---|---|---|
| UART0 TX (to MAIN RX) | GPIO1 | Shared with bootloader serial |
| UART0 RX (from MAIN TX) | GPIO3 | Shared with bootloader serial |
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
| Built-in LED | GPIO33 | Optional status LED |
| SD / flash pins | GPIO2,4,12,13,14,15 | Shared; not used for UART |

**Available for user serial:** GPIO1 (TX) and GPIO3 (RX) via UART0.
Boot-strapping pins on ESP32-CAM: GPIO0, GPIO2, GPIO5, GPIO12 must not be
driven externally at boot in a way that forces an unwanted boot mode.

> **WARNING:** Do not repurpose camera pins. Do not connect 5 V to any ESP32-CAM
> GPIO. The ESP32-CAM has no PSRAM on the common AI-Thinker variant; keep the
> model small.

---

## 2. STANDARD ESP32 DEV BOARD (main control unit)

| Function | GPIO | Notes |
|---|---|---|
| UART0 RX (from CAM TX) | GPIO3 | Main RX <- CAM TX |
| UART0 TX (to CAM RX) | GPIO1 | Main TX -> CAM RX |
| Motor driver LEFT PWM | GPIO25 | PWM, 0..255 |
| Motor driver LEFT DIR | GPIO26 | direction |
| Motor driver RIGHT PWM | GPIO27 | PWM |
| Motor driver RIGHT DIR | GPIO14 | direction |
| Motor driver STBY / ENABLE | GPIO12 | standby pin (pull high to enable) |
| Ultrasonic LEFT trig | GPIO32 | 3.3 V trigger |
| Ultrasonic LEFT echo | GPIO33 | via level divider (5 V -> 3.3 V) |
| Ultrasonic RIGHT trig | GPIO34 | input-only GPIO, OK for echo; use separate trig |
| Ultrasonic RIGHT echo | GPIO35 | input-only, via level divider |
| Conveyor motor PWM | GPIO33 | **conflict** — see note below |
| Conveyor motor DIR | GPIO32 | **conflict** — see note below |
| GPS UART TX | GPIO16 | Main RX <- GPS TX (UART2) |
| GPS UART RX | GPIO17 | Main TX -> GPS RX (UART2) |
| Buzzer | GPIO5 | 3.3 V driven or via transistor |
| Emergency-stop input | GPIO18 | optional, active-low, not implemented by default |

### Pin conflict note
GPIO32/33 were tentatively assigned to both ultrasonic echo and conveyor
control above — this is **not allowed** (one GPIO, two incompatible
functions). Final assignment must resolve this. Example corrected assignment:

- Conveyor PWM: GPIO33
- Conveyor DIR: GPIO32
- Ultrasonic LEFT trig: GPIO25 (shared? no) ...

Because the motor driver already uses GPIO25/26/27/14/12 and ultrasonic needs
two trig + two echo, a clean allocation is:

| Function | GPIO |
|---|---|
| Ultrasonic LEFT trig | GPIO32 |
| Ultrasonic LEFT echo | GPIO33 (via divider) |
| Ultrasonic RIGHT trig | GPIO25 |
| Ultrasonic RIGHT echo | GPIO26 (via divider) |
| Conveyor PWM | GPIO14 |
| Conveyor DIR | GPIO27 |

Motor driver LEFT PWM/DIR then move to GPIO16/17 if GPS uses UART2... These
are tentative. **Verify and finalise in hardware before use.** GPIO34/35/36/39
are input-only and cannot drive PWM/direction outputs.

Do not assign a GPIO to multiple incompatible functions. Input-only GPIOs
(34, 35, 36, 39) must not be used for motor/buzzer outputs.

---

## 3. MOTOR DRIVER (dual-channel, model TBD)

| Function | Connection |
|---|---|
| VMOT / motor supply | Battery via buck / fuse (driver-dependent) |
| VCC / logic supply | 5 V or 3.3 V per driver spec |
| Left PWM | ESP32 GPIO |
| Left DIR | ESP32 GPIO |
| Right PWM | ESP32 GPIO |
| Right DIR | ESP32 GPIO |
| STBY | ESP32 GPIO (active) |
| GND | Common with ESP32 and battery |

Driver current rating and motor stall current are **unknown** — verify before
wiring. Do not power motors from ESP32 3.3 V or 5 V regulator pins.

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
| TX | ESP32 GPIO16 (UART2 RX) |
| RX | ESP32 GPIO17 (UART2 TX) |

Baud rate: typically 9600 (verify with module datasheet).

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

- Motor driver model, logic voltage, current rating
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
