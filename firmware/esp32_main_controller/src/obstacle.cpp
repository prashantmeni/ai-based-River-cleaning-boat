#include "obstacle.h"

ObstacleSensors::ObstacleSensors()
    : _left_cm(-1), _right_cm(-1), _left_valid(false), _right_valid(false),
      _last_read_ms(0) {}

ObstacleSensors::~ObstacleSensors() {}

bool ObstacleSensors::begin() {
    pinMode(US_LEFT_TRIG_GPIO, OUTPUT);
    pinMode(US_LEFT_ECHO_GPIO, INPUT);
    pinMode(US_RIGHT_TRIG_GPIO, OUTPUT);
    pinMode(US_RIGHT_ECHO_GPIO, INPUT);
    digitalWrite(US_LEFT_TRIG_GPIO, LOW);
    digitalWrite(US_RIGHT_TRIG_GPIO, LOW);
    return true;
}

float ObstacleSensors::read_one(uint8_t trig, uint8_t echo) {
    digitalWrite(trig, LOW);
    delayMicroseconds(2);
    digitalWrite(trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig, LOW);

    uint32_t dur = pulseIn(echo, HIGH, US_TIMEOUT_US);
    if (dur == 0) return -1.0f;   /* timeout / invalid */
    /* Distance cm = duration_us / 58 (approx for HC-SR04). */
    float cm = dur / 58.0f;
    if (cm < 2.0f || cm > 400.0f) return -1.0f;  /* out of sane range */
    return cm;
}

bool ObstacleSensors::read() {
    uint32_t now = millis();
    if (now - _last_read_ms < 60) { /* limit to ~16 Hz */
        /* return last values */
        return _left_valid && _right_valid;
    }
    _last_read_ms = now;

    float l = read_one(US_LEFT_TRIG_GPIO, US_LEFT_ECHO_GPIO);
    float r = read_one(US_RIGHT_TRIG_GPIO, US_RIGHT_ECHO_GPIO);

    _left_valid = (l >= 0);
    _right_valid = (r >= 0);
    _left_cm = _left_valid ? l : -1.0f;
    _right_cm = _right_valid ? r : -1.0f;
    return _left_valid && _right_valid;
}

int ObstacleSensors::clearer_side() const {
    if (!_left_valid && !_right_valid) return -1;
    if (!_left_valid) return 1;   /* right clearer by default */
    if (!_right_valid) return 0;
    return (_right_cm >= _left_cm) ? 1 : 0;
}
