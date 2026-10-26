# AI-Based River Cleaning Boat

**ESP32-CAM (AI vision) + standard ESP32 (navigation & control)**  
Independent academic prototype — offline, local AI, two-controller architecture.

## Repository contents

```
docs/                     PROJECT_AUDIT, architecture, pin mapping, wiring,
                          protocol, AI model, state machine, build/flash,
                          testing, troubleshooting, user guide
firmware/esp32_cam_ai/    Controller A: camera, TinyML inference, serial TX
firmware/esp32_main_controller/  Controller B: UART RX, motors, ultrasonic,
                          GPS, conveyor, navigation state machine, safety
shared/protocol/protocol.h Shared framed serial protocol (checksum + sequence)
dashboard/                index.html + bridge.py (localhost telemetry UI)
tests/                    Host-side Python tests (protocol + navigation)
assets/model/             Place your trained INT8 model here (not committed)
```

## Getting started

1. Read docs/PROJECT_AUDIT.md — the repository started empty; this is a from-scratch build.
2. Verify hardware pins/ratings in docs/PIN_MAPPING.md before powering up.
3. Build & flash both controllers (docs/BUILD_AND_FLASH.md).
4. Run the dashboard: `python dashboard/bridge.py --port <COM>`, open http://localhost:8080.
5. Run host tests: `python tests/test_protocol.py` and `python tests/test_navigation.py`.

## Safety

- Motors default OFF at boot; never automatic motion on startup.
- 5 V sensor signals level-shifted to 3.3 V before ESP32 GPIOs.
- Communication loss forces a safe stop.
- First tests in a controlled tank; boat not seaworthy.

## License / attribution

Independent project. Not connected to any company, startup, or prior project.
