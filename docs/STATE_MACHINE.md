# STATE MACHINE

**Project:** AI-Based River Cleaning Boat  
**Controller:** Standard ESP32 (Main Control Unit)  
**Status:** IMPLEMENTED (firmware), documented here.

---

## 1. STATES

| State | Meaning |
|---|---|
| BOOT | Initialising peripherals; motors OFF |
| SELF_TEST | Running hardware self-checks |
| IDLE | Waiting; no motion, no collection |
| SEARCH | Actively looking for a target (slow search motion OK) |
| TARGET_DETECTED | Valid fresh detection received; confirming target |
| ALIGN | Steering toward target using center_x |
| APPROACH | Moving slowly toward target; monitoring obstacles |
| COLLECT | Conveyor ON; conservative forward motion |
| VERIFY_COLLECTION | Post-collection check (timed/attempted) |
| TARGET_LOST | Target disappeared; bounded re-search |
| OBSTACLE_AVOIDANCE | Obstacle too close; steering away or stopping |
| RETURN_OR_STOP | Return to start / stop (not implemented without GPS waypoints) |
| FAULT | Recoverable fault; motors disabled |
| EMERGENCY_STOP | Immediate disable of all outputs; latched until reset |

## 2. TRANSITIONS (summary)

```
BOOT -> SELF_TEST -> IDLE
IDLE --(start autonomous)--> SEARCH
SEARCH --(fresh valid detection x N)--> TARGET_DETECTED
TARGET_DETECTED --(aligned)--> ALIGN
ALIGN --(centered + close)--> APPROACH
APPROACH --(at target)--> COLLECT
COLLECT --(time/completion)--> VERIFY_COLLECTION
VERIFY_COLLECTION --(done)--> SEARCH
* --(target lost)--> TARGET_LOST
TARGET_LOST --(timeout)--> SEARCH or IDLE
* --(obstacle too close)--> OBSTACLE_AVOIDANCE
OBSTACLE_AVOIDANCE --(clear)--> previous state
* --(comms lost / fault / e-stop)--> FAULT / EMERGENCY_STOP
```

Obstacle avoidance preempts nearly all motion states. Communication loss
preempts target-tracking states.

## 3. STATE BEHAVIOURS

### SEARCH
- Run camera/UART reception.
- Wait for fresh, valid detections.
- Slow search motion within safe limits (optional).
- Do not drive motors if safety checks fail.

### TARGET_DETECTED
- Validate confidence >= threshold.
- Confirm class is a supported waste class.
- Reject stale detections (freshness window).
- Require consecutive valid detections before tracking.

### ALIGN
- Use center_x: negative => steer left, positive => steer right, near zero =>
  centred.
- Configurable steering gain, deadband, max PWM.
- Account for physical motor arrangement (direction inversion setting).

### APPROACH
- Move slowly toward target.
- Continuously monitor obstacle sensors.
- Continuously validate AI messages (freshness).
- Stop or reacquire if target disappears.
- Do not infer physical distance from image coordinates.

### COLLECT
- Activate conveyor motor.
- Propulsion at conservative configurable speed.
- Stop after configurable run time or completion condition.
- No indefinite conveyor operation.
- Record as "attempted / timed collection" unless a verification sensor
  is installed.

### VERIFY_COLLECTION
- No full-basket or waste-verify sensor installed by default.
- Record event as attempted/timed collection.
- Extend later with a load/IR sensor if added.

### TARGET_LOST
- Stop aggressive tracking.
- Bounded re-search time; then safe state.

### OBSTACLE_AVOIDANCE
- Ultrasonic L/R with filtering + validity checks.
- Left/right assessment to pick safer steering direction.
- Stop if both directions unsafe.
- Recovery timeout before resuming.

### EMERGENCY_STOP
- Immediately PWM=0, conveyor=OFF.
- Latched; cleared only by explicit reset after safety check.

### FAULT
- Motors disabled. Fault code reported via MAIN_STATUS / buzzer.

## 4. CONFIGURABLE PARAMETERS

| Parameter | Default |
|---|---|
| CONFIDENCE_THRESHOLD | 70 |
| DETECTION_FRESHNESS_MS | 1000 |
| CONSECUTIVE_DETECTIONS | 2 |
| STEERING_DEADBAND | 80 (of 1000) |
| STEERING_GAIN | 0.5 |
| MAX_STEER_PWM | 120 |
| APPROACH_SPEED_PWM | 80 |
| SEARCH_SPEED_PWM | 60 |
| CONVEYOR_RUN_MS | 5000 |
| OBSTACLE_WARN_CM | 30 |
| OBSTACLE_STOP_CM | 15 |
| COMMS_TIMEOUT_MS | 2000 |

All in `shared/configuration/` (create `boat_config.h` before build).

## 5. SAFE DEFAULTS

- All PWM outputs = 0 at boot and on any fault.
- Conveyor = OFF at boot.
- Navigation = IDLE until explicitly started.
- No automatic motion after boot.
