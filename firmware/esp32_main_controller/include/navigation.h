#ifndef NAVIGATION_H
#define NAVIGATION_H

#include <Arduino.h>
#include "protocol.h"
#include "boat_config.h"

/* Navigation state machine states. */
typedef enum {
    NAV_BOOT             = 0,
    NAV_SELF_TEST        = 1,
    NAV_IDLE             = 2,
    NAV_SEARCH           = 3,
    NAV_TARGET_DETECTED  = 4,
    NAV_ALIGN            = 5,
    NAV_APPROACH         = 6,
    NAV_COLLECT          = 7,
    NAV_VERIFY_COLLECT   = 8,
    NAV_TARGET_LOST      = 9,
    NAV_OBSTACLE_AVOID   = 10,
    NAV_FAULT            = 11,
    NAV_EMERGENCY_STOP   = 12,
} nav_state_t;

class MotorDriver;
class ObstacleSensors;
class Conveyor;
class Safety;
class SerialRx;

class Navigation {
public:
    Navigation();
    ~Navigation();

    /* Inject dependencies. */
    void attach(MotorDriver *motors, ObstacleSensors *obs, Conveyor *conv,
                Safety *safety, SerialRx *rx);

    /* Call once per loop iteration. */
    void update();

    nav_state_t state() const { return _state; }
    const char *state_name() const;

    /* Manual override (bounded). */
    void manual_stop();
    void manual_forward(uint8_t pwm, uint32_t duration_ms);
    void manual_turn(int8_t dir, uint8_t pwm, uint32_t duration_ms);
    void conveyor_on();  void conveyor_off();
    void set_autonomous(bool on);

    bool autonomous() const { return _autonomous; }

    /* Serial callbacks (wired in main.cpp). */
    void on_detection(const ai_detection_payload_t *det);
    void on_no_detection(const ai_no_detection_payload_t *nd);
    void on_health(const ai_health_payload_t *h);
    void on_diag_command(uint8_t cmd, uint8_t value, uint16_t duration_ms);

    uint32_t last_detection_age_ms() const {
        return _have_detection ? (uint32_t)(millis() - _last_detection_ms) : 0xFFFFFFFFu;
    }
    bool have_detection() const { return _have_detection; }
    uint8_t detection_confidence() const { return _last_detection.confidence; }
    int16_t detection_center_x() const { return _last_detection.center_x; }
    uint8_t detection_class() const { return _last_detection.class_id; }

private:
    void transition(nav_state_t next);
    void apply_motors_for_state();

    MotorDriver *_motors;
    ObstacleSensors *_obs;
    Conveyor *_conv;
    Safety *_safety;
    SerialRx *_rx;

    nav_state_t _state;
    bool _autonomous;
    uint32_t _state_enter_ms;
    uint32_t _last_detection_ms;
    uint8_t _consec_detections;
    ai_detection_payload_t _last_detection;
    bool _have_detection;
    uint32_t _manual_end_ms;
    int8_t _manual_left_dir; uint8_t _manual_left_pwm;
    int8_t _manual_right_dir; uint8_t _manual_right_pwm;
};

#endif /* NAVIGATION_H */

