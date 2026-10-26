# TROUBLESHOOTING

**Project:** AI-Based River Cleaning Boat

---

## FIRMWARE WILL NOT COMPILE

| Symptom | Likely cause | Fix |
|---|---|---|
| `esp_camera.h: No such file` | Wrong board / framework | Use `board = esp32cam` with `framework = arduino` for the CAM |
| `undefined reference to ledc...` | Arduino core missing | Ensure `framework = arduino` in platformio.ini |
| Build succeeds for one project only | Each firmware is a separate PlatformIO project | Build inside each `firmware/*` folder separately |

## FLASHING PROBLEMS

| Symptom | Fix |
|---|---|
| `failed to connect to ESP32: Timed out` | Board not in bootloader mode. ESP32-CAM: GPIO0->GND, press Reset. DevKit: hold BOOT, press EN, release BOOT. |
| Wrong serial port | `pio device monitor -l`; set `upload_port` |
| Brownout during flash | Power from a steady 5 V supply; use a good USB cable |
| No serial output after flash | Baud 115200; GPIO0 not tied to GND in run mode; press Reset |

## COMMUNICATION PROBLEMS

| Symptom | Fix |
|---|---|
| Main controller never sees AI frames | TX/RX swapped? CAM GPIO1 -> MAIN GPIO3, CAM GPIO3 -> MAIN GPIO1. Common GND required. Baud 115200 both sides. |
| Checksum errors / garbled frames | Loose wiring; reduce baud; keep wires short; common ground. |
| Motors keep moving after disconnect | Should not happen — comms loss forces safe stop. If it does, check safety manager and that `_motors->stop()` runs. |

## MOTOR / POWER PROBLEMS

| Symptom | Fix |
|---|---|
| Motors don't move | Check STBY pin is HIGH; driver VMOT connected to battery (fused); PWM pins correct. Never power motors from ESP32 3.3 V. |
| ESP32 resets when motors start | Motor startup current dipping the 5 V rail. Add bulk capacitance, separate motor supply, thicker wires, fuse. |
| One motor runs backwards | Flip that motor's wires, or toggle MOTOR_LEFT_INVERT / MOTOR_RIGHT_INVERT. |

## AI / VISION PROBLEMS

| Symptom | Fix |
|---|---|
| `MODEL_NOT_LOADED` | No model file wired into inference.cpp yet. Expected until you add a trained model to assets/model/. |
| Always `NO_DETECTION` | Confidence threshold too high, or model not trained for your waste classes. Lower threshold temporarily for testing. |
| Camera fails to init | Wrong AI-Thinker pin table; PSRAM setting; insufficient power to CAM. |

## DASHBOARD PROBLEMS

| Symptom | Fix |
|---|---|
| `bridge.py` can't open serial port | Port in use by another monitor; close `pio device monitor`. Correct COM port. |
| Dashboard shows no telemetry | Main ESP32 not sending MAIN_STATUS; check serial link and `pio device monitor` on main controller. |
| `ModuleNotFoundError: serial` | `pip install pyserial` |

## GENERAL SAFETY

- If anything smells hot or smokes, disconnect battery immediately.
- Never assume a pin rating; verify with a multimeter and datasheet.
- The firmware defaults motors OFF. If motors move unexpectedly, disconnect power and re-check safety logic before proceeding.
