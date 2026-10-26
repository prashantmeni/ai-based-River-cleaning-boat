# COMPONENT LIST

**Project:** AI-Based River Cleaning Boat  
**Target budget:** ₹3,000–₹6,000  
**Status:** PLANNED — verify quantities, ratings, and prices before purchase.

---

## 1. CONTROLLERS

| Qty | Component | Role | Est. cost (₹) | Verified? |
|---|---|---|---|---|
| 1 | AI-Thinker ESP32-CAM + OV2640 | AI vision unit (Controller A) | ~500 | No |
| 1 | ESP32-CAM programmer module | Flashing Controller A | ~150 | No |
| 1 | Standard ESP32 DevKit V1 (30-pin) | Main control unit (Controller B) | ~250 | No |

## 2. MOTION

| Qty | Component | Role | Est. cost (₹) | Verified? |
|---|---|---|---|---|
| 2 | DC gear motors | Propulsion (left/right) | ~300 | No |
| 1 | Dual-channel motor driver (e.g., L298N / L298D / TB6612) | Propulsion motor control | ~150 | No |
| 2 | Low-speed DC geared motors | Conveyor belt drive | ~300 | No |
| 1 | Motor driver for conveyor (if not shared) | Conveyor motor control | ~150 | No (may not be owned) |

> NOTE: A dual-channel driver has only two channels. Two propulsion motors use
> both channels. A conveyor motor requires a THIRD channel or a separate
> driver. If no separate driver is owned, the conveyor cannot be driven
> independently — document this and either buy a cheap driver (e.g. L9110S,
> ~₹60) or omit conveyor automation for the first prototype.

## 3. SENSORS

| Qty | Component | Role | Est. cost (₹) | Verified? |
|---|---|---|---|---|
| 2 | Ultrasonic distance modules (e.g., HC-SR04) | Obstacle avoidance (L/R) | ~200 | No |
| 1 | GPS module (e.g., NEO-6M / NEO-M8N) | Position telemetry | ~300 | No |

## 4. ACTUATORS / INDICATORS

| Qty | Component | Role | Est. cost (₹) | Verified? |
|---|---|---|---|---|
| 1 | Buzzer (3.3 V / 5 V active) | Fault & event audio | ~30 | No |
| 1 | Collection basket / tray | Hold collected waste | ~100 | No (DIY) |

## 5. POWER

| Qty | Component | Role | Est. cost (₹) | Verified? |
|---|---|---|---|---|
| 1–2 | Li-ion cells (18650) | Main energy store | ~200 | No |
| 1 | Li-ion charging module (TP4056) | Charging | ~50 | No |
| 1 | 5 V DC-DC buck converter | 5 V rail for ESP32s/sensors | ~80 | No |
| 1 | Inline fuse + main switch | Battery protection | ~40 | No |
| 1 | Li-ion protection BMS | Cell protection | ~80 | No |

## 6. MECHANICAL / MISCELLANEOUS

| Qty | Component | Role | Est. cost (₹) | Verified? |
|---|---|---|---|---|
| 1 | Thick thermocol sheet | Hull | ~100 | No (DIY/scrap) |
| 2–4 | Plastic bottles | Additional flotation | ~0 | scrap |
| 1 | Conveyor belt + frame | Waste collection | ~150 | No (DIY) |
| 1 | Enclosure / waterproof box | Electronics protection | ~150 | No |
| 1 | Wires, connectors, heat shrink | Interconnects | ~100 | No |
| 1 | Screws, standoffs, zip ties | Assembly | ~50 | No |

## 7. ESTIMATED TOTAL

≈ ₹3,200–₹4,200 (excluding optional conveyor driver, BMS, and enclosure).
Within the ₹3,000–₹6,000 budget if built with scrap hull and minimal parts.

## 8. UNVERIFIED / MISSING SPECIFICATIONS

- Motor driver model and current rating (critical for motor safety)
- DC gear motor nominal voltage and stall current
- Ultrasonic module model and Echo voltage (level-shifting decision)
- GPS module model and baud rate
- Battery cell count and whether a BMS is present
- Buck converter current capacity

Mark each `Verified? = No` item as confirmed before ordering/wiring.
