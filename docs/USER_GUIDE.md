# USER GUIDE

**Project:** AI-Based River Cleaning Boat Using ESP32-CAM and ESP32  
**Status:** Prototype — for controlled tank testing only. Not seaworthy.

---

## 1. WHAT THE BOAT DOES

- The ESP32-CAM looks at the water, runs a local AI model, and reports whether
  it sees waste (and where in the frame).
- The standard ESP32 receives that report, steers the boat toward the waste,
  avoids obstacles with ultrasonic sensors, and runs a conveyor to collect it.
- GPS reports position for telemetry (no return-to-home in this prototype).
- Everything works offline — no internet or cloud required.

---

## 2. BEFORE FIRST POWER-UP (READ FIRST)

1. Verify every pin in docs/PIN_MAPPING.md against your actual hardware.
2. Confirm the motor driver voltage matches your motors and battery.
3. Confirm ultrasonic Echo is level-shifted (5 V -> 3.3 V) before the ESP32.
4. Install an inline fuse and main switch on the battery positive rail.
5. Keep batteries and electronics in a waterproof enclosure above the waterline.
6. Do **not** power motors from ESP32 GPIO pins.

---

## 3. SOFTWARE SETUP

```
pip install platformio pyserial

# Build & flash Controller A (ESP32-CAM)
cd firmware/esp32_cam_ai
pio run -t upload

# Build & flash Controller B (standard ESP32)
cd ../esp32_main_controller
pio run -t upload
```

See docs/BUILD_AND_FLASH.md for full details.

---

## 4. RUNNING THE DASHBOARD (OPTIONAL)

```
python dashboard/bridge.py --port COM3 --baud 115200
open http://localhost:8080
```

The dashboard shows boat state, AI detection, obstacles, GPS, motors, conveyor,
and faults. It also offers bounded manual test commands (each expires after ~1 s).

The dashboard is monitoring only. The boat is safe with the dashboard disconnected.

---

## 5. MODES

| Mode | Behaviour |
|---|---|
| IDLE | No motion, no collection. Default after boot. |
| SEARCH | Slowly looking for a detected waste target. |
| TARGET_DETECTED / ALIGN / APPROACH | Steering toward and approaching a detected target. |
| COLLECT | Conveyor on; slow forward motion to collect. |
| VERIFY_COLLECT | Timed collection cycle complete (recorded as attempted). |
| TARGET_LOST | Target gone; bounded re-search then idle. |
| OBSTACLE_AVOID | Obstacle too close; stop or steer away. |
| FAULT / EMERGENCY_STOP | All outputs disabled. |

Start autonomous mode via the dashboard ("AUTO ON") or keep it in IDLE/manual.

---

## 6. SAFETY RULES

- Motors are OFF after boot and on any fault/comms loss.
- Manual dashboard commands auto-expire (never run indefinitely).
- The boat must not move blindly if ultrasonic sensors fail.
- First tests: in a tank, boat restrained, props off the ground.
- Do not test in strong currents or open water.

---

## 7. WHAT IS NOT IMPLEMENTED (PROTOTYPE LIMITS)

- No return-to-home or waypoint navigation (GPS provides position only).
- No full-basket detection (no verification sensor installed).
- No live camera stream on the dashboard (shown as unavailable honestly).
- No cloud/AI API (all inference is local on the ESP32-CAM).
- No trained waste model included — you must provide one.

See docs/PROJECT_AUDIT.md for the full honest status.
