# Hardware inventory

Status legend: **PRESENT** = physically here and verified · **ON PARTS LIST** =
listed in `Components list.txt`; fitment not confirmed yet · **ABSENT** =
required by design, not obtainable from the parts list (honest gap) ·
**RETIRED** = removed from the design after audit.

## Development host

| Item | Detail | Status |
|---|---|---|
| PC | Windows 10 Pro x64, Intel i5, 6 GB RAM | PRESENT |
| Toolchain | PlatformIO (espressif32@6.7.0), Python 3.12, pyserial | PRESENT |
| Serial port | COM3 (CH340 USB-UART) — **board currently unplugged** | PRESENT (intermittent) |

## Fitted and verified on the bench (P1 evidence)

| Item | Detail | Status |
|---|---|---|
| Board | AI-Thinker **ESP32-CAM**, ESP32-D0WD-V3, chip rev v3.1, MAC 38:3e:51:c3:e9:bc | PRESENT |
| Camera | OV2640, QVGA 320×240 RGB565, XCLK 20 MHz | PRESENT |
| PSRAM | 4 MB (spiram_free ≈ 4.19 MB at boot) | PRESENT |
| Flash | "huge_app" partition app slot (model + firmware) | PRESENT |
| USB adapter | CH340 (on cam board) → COM3, 115200 baud | PRESENT |
| Power | USB 5 V for bench tests | PRESENT |

Serial evidence: `[boot] ... psram=1`, `[cam] init OK`, `[ai] model ready`,
`[selftest] camera+ai ready`.

## P2 dual-controller hardware

| Item | Design intent | Config reference | Status |
|---|---|---|---|
| **Standard ESP32 dev board** | Main boat controller (env `esp32main`) | `firmware/esp32_main/platformio.ini` | **ABSENT** — firmware compile-verified only; blocks HIL scenarios 2–7 |
| UART link wire (3.3 V TTL + common GND) | CAM GPIO14/13 ↔ main GPIO26/27 | `docs/wiring.md` | ON PARTS LIST (dupont wires) — not wired |
| 2× L298N H-bridge | Drive ×4 motors (ch A/B) + conveyor (ch A) | `config_main.h` pins 18/19/21/22/23/14 + GPIO4 | ON PARTS LIST — not fitted (replaces retired PCA9685) |
| HC-SR04 ultrasonic ×3 | Front-left, front-right, basket fill | TRIG 32/25/13, ECHO 33/34/35 | ON PARTS LIST — each ECHO needs 1k/2k divider |
| 4× DC geared motors | Differential drive (2 per side) | L298N #1 | ON PARTS LIST — not fitted |
| Conveyor motor | Sweeps waste into basket | L298N #2 ENA (GPIO4) | ON PARTS LIST — not fitted |
| GPS module (NMEA 9600) | Route logging | main UART2 RX 16 / TX 17 | ON PARTS LIST — firmware parses, `gps_fix=false` without module |
| Buck converter ×2 | 5 V logic rail / motor-logic rail | `docs/wiring.md` Power | ON PARTS LIST |
| Solar + relay + charging module | Field power management | — | ON PARTS LIST — not integrated in firmware |
| Battery pack | Field power | ADC disabled (`BATTERY_SENSE_ENABLED 0`) | **Spec unknown** (cell count/voltage) — firmware waits for spec |
| TDS water-quality sensor | (not in current firmware design) | — | ON PARTS LIST — **not integrated**, no driver written |
| Hull / float body | Buoyancy, conveyor mount, basket | — | Not listed — still required (Level 8/11) |
| Breadboard, wires, switch | Assembly | — | ON PARTS LIST |

## Retired / absent — honest gaps

| Item | Reason |
|---|---|
| PCA9685 16-ch PWM | **Not in parts list**; never hardware-validated in P1 (`PCA9685 NOT found`) — replaced by L298N GPIO/LEDC control |
| ESCs + propellers | Parts list specifies DC geared motors, not ESCs |
| Buzzer | **Not in parts list** — `PIN_BUZZER -1`, stub only |
| Emergency-stop button | **Not in parts list** — `PIN_ESTOP -1`, software e-stop only |
| I2C expander | Not needed — US3 got real pins on the main controller |

## Sensor/hardware test blocking summary

- Levels 7 (sensors) and 8 (motors) remain **BLOCKED** — fittings not
  confirmed; additionally the main ESP32 board itself is absent.
- Basket fill reports `SENSOR_FAULT` **until the sensor is physically
  fitted** (pin is now allocated — no longer disabled by design).
- Reproducible procedures: `docs/water-test.md` §1; HIL plan status:
  `docs/testing_plan.md`.
