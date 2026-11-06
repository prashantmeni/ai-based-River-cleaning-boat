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
- The analog TDS meter (DFRobot Gravity SEN0244 → GPIO34) reports water
  total dissolved solids in ppm plus raw sensor millivolts (dashboard
  Sensors panel, Monitor tab) — 1 s cadence.
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

**Modern dashboard (React app):**

```
cd dashboard-ui
npm run dev        # open http://localhost:5173
```

Settings → **Endpoint** picks how the dashboard talks to the boat (the app
chooses the transport automatically):

| Endpoint | Mode | Needs |
|---|---|---|
| `wss://broker.hivemq.com:8884/mqtt` (default) | Cloud MQTT | Internet on the laptop **and** a working router link on the boat |
| `ws://192.168.4.1:81` | **Direct Wi-Fi** to the boat's own gateway | Join the `RiverClean-Boat` Wi-Fi first (no internet while joined), or be on the same router and use the boat's IP |

Any endpoint containing `/mqtt` is treated as a broker; any other `ws://`
endpoint connects straight to the boat. Both carry identical telemetry,
commands, and acks.

**Legacy USB fallback:**

```
python dashboard/bridge.py --port COM8 --baud 115200
open http://localhost:8080
```

The dashboard shows boat state, AI detection, obstacles, GPS, motors, conveyor,
and faults. It also offers bounded manual test commands (each expires after ~1 s).

**Waste detection (no live video):** the ESP32-CAM no longer serves an MJPEG
stream. It captures 160×120 grayscale frames, runs the on-device detector
(blob segmentation + shape classification), and pushes **name · accuracy ·
position** over ESP-NOW to the main controller, which publishes it to the
dashboard (Overview → *Waste separated* counts; AI Camera page → live lock).
Classes: `Plastic bottle`, `Plastic bag`, `General debris`. Counts increment
once per distinct accepted detection per class.

**Status LEDs + buzzer (main controller):**
| Indicator | Meaning |
|---|---|
| Blue blinking | Connecting to the internet (Wi-Fi/MQTT) |
| Blue solid + one beep | Internet connected (beep confirms it) |
| Red solid | Boat **not ready** — fault latched, no GPS fix, or CAM link lost |
| Green solid | Boat **ready** — no faults, GPS fixed, CAM link alive |

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
- No cloud/AI API (all inference is local on the ESP32-CAM).
- No trained waste model included — you must provide one.

See docs/PROJECT_AUDIT.md for the full honest status.
