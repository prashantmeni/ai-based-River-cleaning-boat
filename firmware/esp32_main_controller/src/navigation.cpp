#include "navigation.h"
#include "motor.h"
#include "obstacle.h"
#include "conveyor.h"
#include "safety.h"
#include "serial_rx.h"
#include "protocol.h"

Navigation::Navigation()
    : _motors(nullptr), _obs(nullptr), _conv(nullptr), _safety(nullptr),
      _rx(nullptr), _state(NAV_BOOT), _autonomous(false),
      _state_enter_ms(0), _last_detection_ms(0), _consec_detections(0),
      _have_detection(false), _manual_end_ms(0),
      _manual_left_dir(0), _manual_left_pwm(0),
      _manual_right_dir(0), _manual_right_pwm(0) {
    memset(&_last_detection, 0, sizeof(_last_detection));
}

Navigation::~Navigation() {}

void Navigation::attach(MotorDriver *motors, ObstacleSensors *obs,
                        Conveyor *conv, Safety *safety, SerialRx *rx) {
    _motors = motors; _obs = obs; _conv = conv; _safety = safety; _rx = rx;
}

const char *Navigation::state_name() const {
    switch (_state) {
        case NAV_BOOT: return "BOOT";
        case NAV_SELF_TEST: return "SELF_TEST";
        case NAV_IDLE: return "IDLE";
        case NAV_SEARCH: return "SEARCH";
        case NAV_TARGET_DETECTED: return "TARGET_DETECTED";
        case NAV_ALIGN: return "ALIGN";
        case NAV_APPROACH: return "APPROACH";
        case NAV_COLLECT: return "COLLECT";
        case NAV_VERIFY_COLLECT: return "VERIFY_COLLECT";
        case NAV_TARGET_LOST: return "TARGET_LOST";
        case NAV_OBSTACLE_AVOID: return "OBSTACLE_AVOID";
        case NAV_FAULT: return "FAULT";
        case NAV_EMERGENCY_STOP: return "EMERGENCY_STOP";
        default: return "UNKNOWN";
    }
}

void Navigation::transition(nav_state_t next) {
    _state = next;
    _state_enter_ms = millis();
}

void Navigation::manual_stop() {
    _manual_end_ms = 0;
    if (_motors) _motors->stop();
    if (_conv) _conv->stop();
}

void Navigation::manual_forward(uint8_t pwm, uint32_t duration_ms) {
    _manual_end_ms = millis() + duration_ms;
    _manual_left_dir = 1; _manual_left_pwm = pwm;
    _manual_right_dir = 1; _manual_right_pwm = pwm;
    _autonomous = false;
}

void Navigation::manual_turn(int8_t dir, uint8_t pwm, uint32_t duration_ms) {
    _manual_end_ms = millis() + duration_ms;
    if (dir > 0) { _manual_left_dir = 1; _manual_right_dir = -1; }
    else if (dir < 0) { _manual_left_dir = -1; _manual_right_dir = 1; }
    else { _manual_left_dir = 0; _manual_right_dir = 0; }
    _manual_left_pwm = pwm; _manual_right_pwm = pwm;
    _autonomous = false;
}

void Navigation::conveyor_on() {
    if (_conv) _conv->start_collection();
    _autonomous = false;
}
void Navigation::conveyor_off() {
    if (_conv) _conv->stop();
    _autonomous = false;
}

void Navigation::set_autonomous(bool on) {
    _autonomous = on;
    _manual_end_ms = 0;
    if (!on && _motors) _motors->stop();
}

void Navigation::on_detection(const ai_detection_payload_t *det) {
    if (!det) return;
    _last_detection = *det;
    _last_detection_ms = millis();
    _have_detection = true;
    if (det->confidence >= CONFIDENCE_THRESHOLD &&
        det->inference_status == INF_STATUS_OK) {
        _consec_detections++;
    } else {
        _consec_detections = 0;
    }
}

void Navigation::on_no_detection(const ai_no_detection_payload_t *nd) {
    (void)nd;
    _consec_detections = 0;
    /* Do NOT clear _have_detection immediately; freshness window decides. */
}

void Navigation::on_health(const ai_health_payload_t *h) {
    (void)h;
    /* Could log camera/inference health for telemetry. */
}

void Navigation::on_diag_command(uint8_t cmd, uint8_t value, uint16_t duration_ms) {
    /* All dashboard commands are bounded: a duration of 0 defaults to a
     * short timeout so motors never run indefinitely. */
    if (duration_ms == 0) duration_ms = 1000;
    _autonomous = false;
    _manual_end_ms = millis() + duration_ms;
    _manual_left_dir = 0; _manual_left_pwm = 0;
    _manual_right_dir = 0; _manual_right_pwm = 0;

    switch (cmd) {
        case DIAG_CMD_STOP:
            if (_motors) _motors->stop();
            break;
        case DIAG_CMD_FORWARD:
            _manual_left_dir = 1; _manual_left_pwm = value;
            _manual_right_dir = 1; _manual_right_pwm = value;
            if (_motors) _motors->forward(value);
            break;
        case DIAG_CMD_TURN_LEFT:
            _manual_left_dir = -1; _manual_left_pwm = value;
            _manual_right_dir = 1; _manual_right_pwm = value;
            if (_motors) _motors->turn_left(value);
            break;
        case DIAG_CMD_TURN_RIGHT:
            _manual_left_dir = 1; _manual_left_pwm = value;
            _manual_right_dir = -1; _manual_right_pwm = value;
            if (_motors) _motors->turn_right(value);
            break;
        case DIAG_CMD_CONVEYOR_ON:
            if (_conv) _conv->start_collection();
            break;
        case DIAG_CMD_CONVEYOR_OFF:
            if (_conv) _conv->stop();
            break;
        case DIAG_CMD_AUTONOMOUS_ON:
            set_autonomous(true);
            break;
        case DIAG_CMD_AUTONOMOUS_OFF:
            set_autonomous(false);
            if (_motors) _motors->stop();
            break;
        default:
            break;
    }
}

void Navigation::update() {
    uint32_t now = millis();

    /* Manual command expiry: bounded, never indefinite. */
    if (_manual_end_ms != 0 && now >= _manual_end_ms) {
        _manual_end_ms = 0;
        if (_motors) _motors->stop();
        _autonomous = false;
    }

    /* Safety faults preempt everything except emergency stop. */
    if (_safety && (_safety->in_emergency() ||
                    _safety->fault_flags() != 0)) {
        if (_state != NAV_EMERGENCY_STOP && _state != NAV_FAULT) {
            transition(_safety->in_emergency() ? NAV_EMERGENCY_STOP : NAV_FAULT);
        }
        if (_motors) _motors->stop();
        if (_conv) _conv->stop();
        if (_rx) _rx->update(); /* keep draining serial */
        return;
    }

    /* Always drain incoming AI serial messages (non-blocking). */
    if (_rx) _rx->update();

    /* Comms-loss handling: invalidate stale detections. */
    bool comms_ok = _rx && _rx->comms_alive();
    if (!comms_ok) {
        if (now - _last_detection_ms > COMMS_TIMEOUT_MS) {
            _have_detection = false;
        }
    }

    /* Staleness: a detection older than FRESHNESS_MS is not valid. */
    if (_have_detection &&
        (now - _last_detection_ms > DETECTION_FRESHNESS_MS)) {
        _have_detection = false;
    }

    /* Obstacle assessment (only if sensors valid). */
    bool obstacle_stop = false;
    if (_obs) {
        _obs->read();
        if (_obs->any_invalid()) {
            obstacle_stop = true;   /* don't move blindly on bad sensors */
            if (_safety) _safety->set_sensors_bad(true);
        } else {
            if (_safety) _safety->set_sensors_bad(false);
            if (_obs->both_blocked()) obstacle_stop = true;
        }
    }

    /* --- State machine --- */
    switch (_state) {
        case NAV_BOOT:
            transition(NAV_SELF_TEST);
            break;

        case NAV_SELF_TEST:
            /* Peripherals already initialised in setup. Move to IDLE. */
            transition(NAV_IDLE);
            break;

        case NAV_IDLE:
            if (_motors) _motors->stop();
            if (_conv) _conv->update();
            if (_autonomous) transition(NAV_SEARCH);
            break;

        case NAV_SEARCH:
            if (!comms_ok || !_have_detection) {
                /* Safe search: slow turn or hold position. */
                if (_motors && !obstacle_stop) _motors->turn_left(SEARCH_SPEED_PWM);
                else if (_motors) _motors->stop();
                if (_conv) _conv->stop();
                break;
            }
            /* Fresh detection present -> confirm target. */
            if (_consec_detections >= CONSECUTIVE_DETECTIONS) {
                transition(NAV_TARGET_DETECTED);
            }
            break;

        case NAV_TARGET_DETECTED: {
            if (!_have_detection) {
                transition(NAV_TARGET_LOST);
                break;
            }
            if (_last_detection.confidence < CONFIDENCE_THRESHOLD) {
                transition(NAV_SEARCH);
                break;
            }
            /* Decide alignment from center_x (-500..+500). */
            int16_t cx = _last_detection.center_x;
            if (cx > STEERING_DEADBAND) transition(NAV_ALIGN);
            else if (cx < -STEERING_DEADBAND) transition(NAV_ALIGN);
            else transition(NAV_APPROACH);
            break;
        }

        case NAV_ALIGN: {
            if (!_have_detection) { transition(NAV_TARGET_LOST); break; }
            if (obstacle_stop) { transition(NAV_OBSTACLE_AVOID); break; }
            int16_t cx = _last_detection.center_x;
            int8_t steer = 0;
            uint8_t pwm = 0;
            if (cx > STEERING_DEADBAND) {
                steer = 1;   /* turn right */
                pwm = (uint8_t)(STEERING_GAIN * cx / 1000);
            } else if (cx < -STEERING_DEADBAND) {
                steer = -1;  /* turn left */
                pwm = (uint8_t)(STEERING_GAIN * (-cx) / 1000);
            }
            if (pwm > MAX_STEER_PWM) pwm = MAX_STEER_PWM;
            if (pwm < MIN_MOTOR_PWM && pwm != 0) pwm = MIN_MOTOR_PWM;
            if (_motors) {
                if (steer > 0) _motors->turn_right(pwm);
                else if (steer < 0) _motors->turn_left(pwm);
                else _motors->stop();
            }
            if (steer == 0) transition(NAV_APPROACH);
            break;
        }

        case NAV_APPROACH: {
            if (!_have_detection) { transition(NAV_TARGET_LOST); break; }
            if (obstacle_stop) { transition(NAV_OBSTACLE_AVOID); break; }
            if (_motors && !obstacle_stop) _motors->forward(APPROACH_SPEED_PWM);
            /* Re-align if target drifts off centre. */
            int16_t cx = _last_detection.center_x;
            if (cx > STEERING_DEADBAND || cx < -STEERING_DEADBAND)
                transition(NAV_ALIGN);
            break;
        }

        case NAV_COLLECT: {
            if (_conv && !_conv->running()) _conv->start_collection();
            if (_motors && !obstacle_stop) _motors->forward(APPROACH_SPEED_PWM / 2);
            else if (_motors) _motors->stop();
            /* Time-based completion handled by conveyor.update(). */
            if (_conv && _conv->state() == CONVEYOR_OFF &&
                _conv->collection_count() > 0) {
                transition(NAV_VERIFY_COLLECT);
            }
            break;
        }

        case NAV_VERIFY_COLLECT:
            if (_conv) _conv->update();
            /* No full-basket sensor: record as attempted/timed collection. */
            transition(NAV_SEARCH);
            break;

        case NAV_TARGET_LOST:
            if (_motors) _motors->stop();
            if (_conv) _conv->stop();
            if (_have_detection) {
                transition(NAV_SEARCH);
            } else if (now - _state_enter_ms > TARGET_LOST_SEARCH_MS) {
                transition(NAV_IDLE);
            }
            break;

        case NAV_OBSTACLE_AVOID:
            if (_motors) _motors->stop();
            if (_obs && !_obs->any_invalid() && !_obs->both_blocked()) {
                int side = _obs->clearer_side();
                if (_motors) {
                    if (side == 1) _motors->turn_left(SEARCH_SPEED_PWM);
                    else if (side == 0) _motors->turn_right(SEARCH_SPEED_PWM);
                }
            }
            if (!obstacle_stop) {
                /* Recover to previous behaviour. */
                if (_have_detection) transition(NAV_SEARCH);
                else transition(NAV_IDLE);
            }
            break;

        case NAV_FAULT:
            if (_motors) _motors->stop();
            if (_conv) _conv->stop();
            break;

        case NAV_EMERGENCY_STOP:
            if (_motors) _motors->disable();
            if (_conv) _conv->stop();
            break;

        default:
            if (_motors) _motors->stop();
            break;
    }

    apply_motors_for_state();
}

void Navigation::apply_motors_for_state() {
    /* Manual override takes priority when active and not autonomous. */
    if (_manual_end_ms != 0 && millis() < _manual_end_ms && !_autonomous) {
        if (_motors) _motors->set_motors(_manual_left_dir, _manual_left_pwm,
                                         _manual_right_dir, _manual_right_pwm);
        return;
    }
    /* State machine already applied motors inside the switch; this is a
     * hook for any post-processing. Motors remain at last commanded value
     * until next update. */
    (void)_motors;
}

