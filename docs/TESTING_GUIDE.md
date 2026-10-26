# TESTING GUIDE

**Project:** AI-Based River Cleaning Boat  
**Convention:** Every test below is marked SIMULATED (host-side, Python),
COMPILED (firmware builds without error), or HARDWARE (requires physical
device). No hardware test is claimed as passed unless actually performed.

---

## TEST GROUP A — EXISTING / REFERENCE AI MODEL

| # | Test | Type | Expected |
|---|---|---|---|
| A1 | Model file exists in `assets/model/` | SIMULATED | File present (or documented as missing) |
| A2 | Model loads in inference runtime | SIMULATED | `begin()` returns true; or `MODEL_NOT_LOADED` reported honestly |
| A3 | Input dimensions match preprocessing | SIMULATED | 96×96 grayscale tensor |
| A4 | Output dimensions / class IDs match labels | SIMULATED | class_id in [0, NUM_CLASSES) |
| A5 | Confidence values valid (0..100) | SIMULATED | 0 <= confidence <= 100 |
| A6 | Memory requirement measured | COMPILED | Tensor arena fits ESP32-CAM RAM |
| A7 | No-detection case works | SIMULATED | AI_NO_DETECTION sent when below threshold |
| A8 | Invalid camera frames rejected | SIMULATED | capture() failure -> AI_NO_DETECTION (reason=invalid) |

Run host protocol tests:
```
python tests/test_protocol.py
```

## TEST GROUP B — CONTROLLER COMMUNICATION

| # | Test | Type | Expected |
|---|---|---|---|
| B1 | Valid AI detection message | SIMULATED | Frame parses OK; detection fields populated |
| B2 | No-detection message | SIMULATED | Valid frame; not treated as detection |
| B3 | Corrupted checksum | SIMULATED | Frame rejected, error counter++ |
| B4 | Invalid message length | SIMULATED | Frame rejected |
| B5 | Unknown message type | SIMULATED | Frame rejected |
| B6 | Stale sequence number | SIMULATED | Main controller marks detection stale |
| B7 | Communication timeout | SIMULATED/HARDWARE | `comms_ok=false`; motors stop |
| B8 | ESP32-CAM reboot | HARDWARE | Main controller resyncs on next valid frame |
| B9 | Main ESP32 reboot | HARDWARE | All outputs OFF until re-init |

Run:
```
python tests/test_protocol.py
```

## TEST GROUP C — NAVIGATION STATE MACHINE

| # | Transition / scenario | Type | Expected |
|---|---|---|---|
| C1 | SEARCH -> TARGET_DETECTED | SIMULATED | After N consecutive valid detections |
| C2 | TARGET_DETECTED -> ALIGN/APPROACH | SIMULATED | Based on center_x deadband |
| C3 | APPROACH -> COLLECT | SIMULATED | At target; conveyor starts |
| C4 | COLLECT -> VERIFY_COLLECT -> SEARCH | SIMULATED | After timed conveyor run |
| C5 | Target disappears | SIMULATED | -> TARGET_LOST -> bounded re-search -> IDLE |
| C6 | Confidence drops | SIMULATED | -> SEARCH |
| C7 | AI controller stops responding | SIMULATED/HARDWARE | -> safe state; motors stop |
| C8 | Obstacle detected | SIMULATED/HARDWARE | -> OBSTACLE_AVOID; slow/stop |
| C9 | Both directions blocked | SIMULATED/HARDWARE | Stop propulsion |
| C10 | GPS fix lost | SIMULATED | Advisory only; local nav unaffected |
| C11 | Invalid sensor readings | SIMULATED/HARDWARE | No blind motion; sensors_ok=false |
| C12 | Emergency stop | SIMULATED/HARDWARE | All outputs OFF immediately |
| C13 | Watchdog trigger | SIMULATED | Fault flag; motors disabled |

Run:
```
python tests/test_navigation.py
```

## TEST GROUP D — MOTORS AND CONVEYOR

| # | Test | Type | Expected |
|---|---|---|---|
| D1 | Motors disabled after boot | HARDWARE | PWM=0 at startup |
| D2 | Motor direction mapping correct | HARDWARE | forward() moves boat forward |
| D3 | Stop command works | HARDWARE | Motors halt |
| D4 | Max PWM respected | HARDWARE | PWM <= MAX_PWM |
| D5 | Conveyor timeout works | HARDWARE | Stops after CONVEYOR_RUN_MS |
| D6 | Communication loss stops motion | HARDWARE | Motors stop on comms loss |
| D7 | Emergency stop disables all outputs | HARDWARE | Motors + conveyor OFF |

**Mark HARDWARE:** requires the physical robot on a bench (wheels off ground).

## TEST GROUP E — DASHBOARD

| # | Test | Type | Expected |
|---|---|---|---|
| E1 | Dashboard starts | SIMULATED | `python dashboard/bridge.py` runs; HTTP 200 |
| E2 | Connection status correct | SIMULATED | Shows connected/disconnected honestly |
| E3 | Missing telemetry shown honestly | SIMULATED | `UNKNOWN`/`-` when no data |
| E4 | Invalid GPS not shown as position | SIMULATED | "NO FIX" until valid fix |
| E5 | Manual commands bounded | SIMULATED | Auto-expire after ~1 s |
| E6 | Expired command stops safely | SIMULATED | Motors stop after timeout |
| E7 | Disconnection visible | SIMULATED | `comms_ok=false` indicator |
| E8 | Fault messages readable | SIMULATED | Fault flags displayed |

Run:
```
python dashboard/bridge.py --port <PORT>
open http://localhost:8080
```

## HOW TO RUN ALL HOST-SIDE TESTS

```
python tests/test_protocol.py
python tests/test_navigation.py
```

No ESP32 toolchain or hardware is required for these.

## HARDWARE TESTS THAT REMAIN OUTSTANDING

All D-group (motors/conveyor) and most C-group/HARDWARE tests require the
physical boat and must be performed in a controlled tank, with the boat
restrained (props/ wheels off the ground) for the first motor tests.
