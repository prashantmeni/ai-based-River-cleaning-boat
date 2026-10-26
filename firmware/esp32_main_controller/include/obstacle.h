#ifndef OBSTACLE_H
#define OBSTACLE_H

#include <Arduino.h>
#include "boat_config.h"

/* Ultrasonic obstacle sensor driver with filtering, validity checks,
 * hysteresis, and left/right assessment.
 *
 * NOTE: ESP32 Echo pins must not receive 5 V directly. Use a voltage
 * divider (1k + 2k) on 5 V echo modules before the GPIO.
 */
class ObstacleSensors {
public:
    ObstacleSensors();
    ~ObstacleSensors();

    bool begin();

    /* Trigger + read both sensors. Returns true if both readings valid. */
    bool read();

    /* Distance in cm, or -1 if invalid. */
    float left_cm()  const { return _left_valid  ? _left_cm  : -1.0f; }
    float right_cm() const { return _right_valid ? _right_cm : -1.0f; }
    bool left_valid()  const { return _left_valid; }
    bool right_valid() const { return _right_valid; }

    /* Obstacle assessment. */
    bool left_obstacle()  const { return _left_valid  && _left_cm  < OBSTACLE_STOP_CM; }
    bool right_obstacle() const { return _right_valid && _right_cm < OBSTACLE_STOP_CM; }
    bool left_warn()  const { return _left_valid  && _left_cm  < OBSTACLE_WARN_CM; }
    bool right_warn() const { return _right_valid && _right_cm < OBSTACLE_WARN_CM; }
    bool both_blocked() const { return left_obstacle() && right_obstacle(); }
    bool any_invalid() const { return !_left_valid || !_right_valid; }

    /* Which side is clearer (larger distance). -1 if both invalid. */
    int clearer_side() const;

private:
    float read_one(uint8_t trig, uint8_t echo);
    float _left_cm, _right_cm;
    bool  _left_valid, _right_valid;
    uint32_t _last_read_ms;
};

#endif /* OBSTACLE_H */
