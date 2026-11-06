# AI-Based River Cleaning Boat

**ESP32-CAM (AI vision) + standard ESP32 (navigation & control)**  
Independent academic prototype — offline, local AI, two-controller architecture.

## Repository contents

```
docs/                     PROJECT_AUDIT, architecture, pin mapping, wiring,
                          protocol, AI model, state machine, build/flash,
                          testing, troubleshooting, final report
firmware/esp32_cam_ai/    Controller A: camera, TinyML inference, ESP-NOW TX
firmware/esp32_main_controller/  Controller B: ESP-NOW RX, motors, ultrasonic,
                          GPS, conveyor, navigation state machine, safety,
                          WebSocket dashboard gateway (port 81)
shared/protocol/          protocol.h (frame format) + espnow_link.h (ESP-NOW
                          helpers + optional SoftAP), shared by both firmwares
dashboard-ui/             PRIMARY dashboard: React + TypeScript + Vite +
                           Tailwind SPA over WebSocket (5 pages, demo mode)
dashboard/                LEGACY fallback: index.html + bridge.py (USB serial
                          → localhost HTTP telemetry UI)
tests/                    Host-side Python tests (protocol + navigation)
assets/model/             Place your trained INT8 model here (not committed)
```

## Getting started

1. Read docs/PROJECT_AUDIT.md — the repository started empty; this is a from-scratch build.
2. Verify hardware pins/ratings in docs/PIN_MAPPING.md before powering up.
3. Build & flash both controllers (docs/BUILD_AND_FLASH.md).
   - CAM flash needs `esptool --no-stub` on its COM port; kill `bridge.py`
     before flashing the main board (it holds the port).
4. **Dashboard (primary):** power the main board, join its Wi-Fi AP
   (`RiverClean-Boat` — the laptop loses internet while connected), then:
   ```
   cd dashboard-ui
   npm install
   npm run dev        # http://localhost:5173 → auto-connects ws://192.168.4.1:81
   ```
   No hardware handy? Use **Demo mode** (top bar) — fully labelled simulation.
   Details: `dashboard-ui/README.md`.
5. **Dashboard (legacy fallback):** `python dashboard/bridge.py --port COM8`,
   open http://localhost:8080.
6. Run host tests: `python tests/test_protocol.py` (7/7),
   `python tests/test_navigation.py` (5/5), `cd dashboard-ui && npm run test`
   (51/51), plus `npm run lint` / `npm run build`.

## Communication at a glance

- **ESP32-CAM ↔ ESP32:** wireless ESP-NOW only (Wi-Fi channel 1, broadcast,
  one framed packet per datagram, checksummed). No signal wires between
  boards; UART0 on both is debug output.
- **ESP32 ↔ browser:** WPA2 SoftAP on the ESP32 (same radio, same channel 1,
  IP 192.168.4.1) with a WebSocket server on port 81 — commands with ids and
  firmware acks, telemetry JSON at ~5 Hz.
- **USB/UART0 ↔ laptop:** JSON telemetry + diag commands for the legacy
  `bridge.py` path and debug consoles (COM ports).

## Safety

- Motors default OFF at boot; never automatic motion on startup.
- The **firmware is the safety controller**: E-STOP, fault latches and
  command guards run on the ESP32 — the browser only sends requests.
- 5 V sensor signals level-shifted to 3.3 V before ESP32 GPIOs.
- Communication loss latches a fault and forces a safe stop.
- First tests in a controlled tank; boat not seaworthy.

## Known limitations

- No trained AI model ships in the repo (`INF_STATUS_MODEL_NOT_LOADED`
  stub) — see docs/AI_MODEL.md.
- ESP32-CAM firmware serves a live grayscale MJPEG stream on port 8080
  (`http://rivercam.local:8080/stream`, or the CAM's printed IP) while it
  keeps doing AI inference and sending ESP-NOW detection frames. It joins
  the boat Wi-Fi automatically, or hosts its own `RiverClean-CAM` fallback
  AP if the main board is down.
- The main board (COM8) has exhibited intermittent full silences requiring
  physical bench intervention — see docs/FINAL_REPORT.md §11.

## License / attribution

Independent project. Not connected to any company, startup, or prior project.
