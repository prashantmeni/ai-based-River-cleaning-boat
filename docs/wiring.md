# Wiring — River Cleaning Boat (Dual-Controller: ESP32-CAM + ESP32)

> **PIN MAP IS PROVISIONAL.** Verify against actual modules before wiring
> motors/batteries. Sources of truth: `firmware/include/config.h` (CAM) and
> `firmware/esp32_main/include/config_main.h` (main controller).
>
> This is the **P2 dual-board architecture**: the AI-Thinker ESP32-CAM does
> vision only (camera, AI, dashboard, Wi-Fi stream); a standard ESP32 dev
> board runs the boat (sensors, motors, conveyor, state machine, GPS). The
> two link over UART1 with the validated NDJSON protocol
> (`shared_protocol/rb_protocol`, `docs/communication_protocol.md`).

## Controller A — AI-Thinker ESP32-CAM (vision)

ESP32-D0WD-V3, OV2640, 4MB PSRAM. Flashed over the CH340 adapter on
GPIO1/GPIO3 (UART0, also serial debug at 115200).

| Function | GPIO | Notes |
|---|---|---|
| Link TX → main ESP32 RX | 14 | UART1 TX, 115200 8N1. Non-strap pin (SD-slot pin 14 — SD unused) |
| Link RX ← main ESP32 TX | 13 | UART1 RX (SD-slot pin 13 — SD unused) |
| Camera XCLK | 0 | 20 MHz, fixed AI-Thinker wiring (strapping pin — camera supplies clock) |
| Camera data/control | 5, 16–19, 21, 23, 25–27, 32–39 | AI-Thinker camera slot (fixed) |
| Console / flashing | 1, 3 | UART0, untouched |

- **GPIO16 is PSRAM CS — never usable.**
- Strapping pins on this board: 0, 2, 12, 15 (all reserved for camera/boot).
- Nothing else hangs off the CAM: no motors, GPS, sonar, I2C. The link pins
  deliberately avoid 0/2/12/15 so the board boots and flashes normally.
- Retired from P1: I2C→PCA9685 (13/14 reused for the UART link).

## Controller B — Standard ESP32 dev board (boat control)

Board: `esp32dev` (PlatformIO env `esp32main`).

### UART links

| Function | GPIO | Notes |
|---|---|---|
| Link RX ← CAM TX (GPIO14) | 26 | UART1 RX |
| Link TX → CAM RX (GPIO13) | 27 | UART1 TX |
| GPS RX ← GPS module TX | 16 | UART2 RX (TinyGPSPlus) |
| GPS TX → GPS module RX | 17 | UART2 TX |
| Console / flashing | 1, 3 | UART0, untouched |

- UART1 RX/TX are crossed to the CAM's TX/RX above; **common ground is
  mandatory**, both UARTs are 3.3 V TTL (never 5 V).
- GPS module TX level: NEO-6M-class boards are 3.3 V logic — verify yours;
  fit a 1k/2k divider if the module's TX is 5 V.

### Ultrasonic sensors (HC-SR04-style, 3×)

| Sensor | TRIG | ECHO | Notes |
|---|---|---|---|
| US1 — front-left obstacle | 32 | 33 | |
| US2 — front-right obstacle | 25 | 34 | ECHO input-only pin, OK for echo |
| US3 — basket fill | 13 | 35 | ECHO input-only pin, OK for echo |

- **Every ECHO pin needs a 1k/2k level divider (5 V → 3.3 V)** — ECHO is 5 V
  logic. Two resistors per sensor on the breadboard.
- TRIG inputs are safe driven at 3.3 V.
- If no sensor is fitted, reads time out invalid; after
  `SONAR_INVALID_FAULT_MS` (3 s) auto mode is blocked (manual still works).

### Motors — L298N dual H-bridge #1 (left/right drive)

| L298N pin | ESP32 GPIO | Notes |
|---|---|---|
| ENA | 18 | 12-bit LEDC PWM @ 5 kHz (speed) — remove the ENA jumper cap |
| IN1 | 19 | Direction |
| IN2 | 21 | Direction |
| ENB | 22 | 12-bit LEDC PWM @ 5 kHz (speed) — remove the ENB jumper cap |
| IN3 | 23 | Direction |
| IN4 | 14 | Direction |
| +5V | 5 V rail | Logic supply (board's onboard 5 V regulator OUT, jumper removed) |
| GND | common GND | With battery − and ESP32 GND |
| +12V/Vs | motor battery + | Size for 4× geared motor stall current |

- PWM duty is 0..4095 (`PROP_PWM_MAX`), so state-machine speed constants
  (`SEARCH_PWM 1800`, `APPROACH_PWM 2200`, `TURN_PWM 2400`) are unchanged
  from P1.
- ESCs not used in this build (geared DC motors per parts list).

### Conveyor — L298N #2 channel A (GPIO4 single-PWM trick)

| L298N #2 pin | Connection | Notes |
|---|---|---|
| ENA | ESP32 GPIO4 | PWM on/off+speed only; **remove the ENA jumper** |
| IN1 | hardwired to +5 V | Logic high |
| IN2 | hardwired to GND | Logic low → direction fixed forward |
| IN3/IN4/ENB | unused (ch. B spare) | |
| +12V/Vs | motor battery + | Same rail as #1 or separate by current needs |

- Rationale: one spare PWM pin drives the whole conveyor; L298N #2's jumpers
  set fixed-forward logic. GPIO4 is the onboard-LED pin on many dev boards —
  harmless at PWM frequencies; verify yours is not a boot strap (it is not).
- Speed: `CONVEYOR_PWM 2800/4095`; run duration `COLLECT_DURATION_MS`,
  hard fault at `CONVEYOR_TIMEOUT_MS`.

### GPS / misc / power

| Function | GPIO / rail | Notes |
|---|---|---|
| Battery ADC | disabled | `BATTERY_SENSE_ENABLED 0` — cell count unknown; enable + divider when spec'd |
| E-stop input | disabled | `PIN_ESTOP -1` — button absent from parts list; software e-stop (cmd/`emergency_stop`) only |
| Buzzer | disabled | `PIN_BUZZER -1` — not in parts list; stub only |
| Spare GPIO | 2, 5, 12, 15 | **Strapping pins** — only for low-at-boot peripherals |
| Spare GPIO | 36, 39 | Input-only |
| Buck converter #1 | battery + → 5 V | ESP32-CAM + ESP32 + sonar + link logic |
| Buck converter #2 | battery + → L298N 5V rail option | Or use L298N onboard 5 V output |
| Solar + relay + charging module | battery management | Per hardware-inventory |

### Power rules

- **Common ground across both ESP32s, both L298Ns, GPS, sonar, battery.**
- Brown-out risk is real: power the ESP32-CAM from a BEC ≥ 2 A, separate
  from motor rails where possible (camera + TFLM draw spikes).
- Ultrasonic ECHO is 5 V logic — never feed it raw into an ESP32 GPIO.
- The two boards talk only over UART — never power one board from another's
  3.3 V pin.

## Safety

- Watchdog: 30 s (`WATCHDOG_TIMEOUT_S`) on the main controller; the CAM
  keeps its AI-loop watchdog. Link loss > 3 s stops all actuators and latches
  a safe stop (auto-recover clears on link return).
- Motors/conveyor stop on: `FAULT_STOP`, battery low (when enabled), e-stop
  (when fitted), link loss, auto-off, command timeout semantics
  (`cmd_manual` disables auto).
- Bench tests: props removed or hull restrained; flash only with `keeper2`
  stopped (`taskkill /F /IM python.exe` after checking the log, or kill by
  PID) — the keeper holds COM3.
