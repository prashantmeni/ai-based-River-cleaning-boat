# River Cleaning Boat — Authoritative Circuit Diagram & Wiring Guide

This document defines the complete physical wiring and pin assignments for the AI-Based River Cleaning Boat system, matching the firmware configuration (`boat_config.h`) and system documentation.

---

## 1. System Power Architecture

```
[ Li-ion Battery 12V ]
       │
       ├──[ Main Fuse & Switch ] ───► [ Motor Driver 1 (L298N) VMOT ]
       │                         └───► [ Motor Driver 2 (HW-095) VMOT ]
       │
       └──[ Buck Converter 5V ] ────► 5V DC Rail
                                      ├── ESP32 DevKit Vin / 5V
                                      ├── HC-SR04 VCC (5V)
                                      ├── GPS VCC (3.3V/5V)
                                      ├── TDS Sensor VCC (3.3V)
                                      └── Status LEDs & Buzzer VCC
```
* **Critical Rule:** All grounds (battery negative, buck output GND, motor driver GNDs, sensor GNDs, ESP32 GND) must be tied to a single **Common Ground**.

---

## 2. Pin Mapping Table (Main ESP32 DevKit)

| Component | Component Pin | ESP32 GPIO | Electrical / Notes |
|---|---|---|---|
| **Propulsion Motor Driver (Left)** | ENA | `GPIO 25` | PWM Speed Control (ENA jumper removed) |
| | IN1 | `GPIO 26` | Direction 1 |
| | IN2 | `GPIO 16` | Direction 2 (Moved off strapping pin GPIO12) |
| **Propulsion Motor Driver (Right)** | ENB | `GPIO 27` | PWM Speed Control (ENB jumper removed) |
| | IN3 | `GPIO 14` | Direction 1 |
| | IN4 | `GPIO 19` | Direction 2 |
| **Conveyor Motor Driver** | EN (PWM) | `GPIO 23` | Conveyor Speed Control |
| | IN1 | `GPIO 22` | Tied `HIGH` |
| | IN2 | `GPIO 21` | Tied `LOW` |
| **Ultrasonic Left (HC-SR04)** | Trig | `GPIO 32` | Direct 3.3V/5V compatible |
| | Echo | `GPIO 33` | **Via Voltage Divider** (1kΩ series + 2kΩ to GND) |
| **Ultrasonic Right (HC-SR04)** | Trig | `GPIO 13` | Direct |
| | Echo | `GPIO 15` | **Via Voltage Divider** (1kΩ series + 2kΩ to GND) |
| **TDS Meter (DFRobot SEN0244)**| Analog Out | `GPIO 34` | ADC1 Channel 6 (0–2.3V input, VCC = 3.3V) |
| **GPS Module (NEO-6M)** | TX | `GPIO 4` | UART1 RX, 9600 baud NMEA (GGA/RMC) |
| **Red LED (Not Ready)** | Anode (+) | `GPIO 17` | Current-limiting resistor (330Ω) to GND |
| **Green LED (Ready)** | Anode (+) | `GPIO 18` | Current-limiting resistor (330Ω) to GND |
| **Blue LED (WiFi/Cloud)** | Anode (+) | `GPIO 2` | Built-in or external with 330Ω resistor |
| **Buzzer** | Positive (+) | `GPIO 5` | Active/passive buzzer with transistor driver |
| **ESP32-CAM** | Wireless | ESP-NOW | Channel 1 wireless telemetry/detection link (no physical wires to main board) |

---

## 3. Visual SVG Diagram
An interactive and clean visual schematic has been generated and saved to:
`assets/circuit_diagram.svg`
