#include "conveyor.h"

Conveyor::Conveyor()
    : _state(CONVEYOR_OFF), _start_ms(0), _collection_count(0),
      _pwm_enabled(false) {}

Conveyor::~Conveyor() { stop(); }

bool Conveyor::begin() {
    pinMode(CONVEYOR_PWM_GPIO, OUTPUT);
    pinMode(CONVEYOR_DIR_GPIO, OUTPUT);
    digitalWrite(CONVEYOR_DIR_GPIO, HIGH);  /* fixed direction */
    analogWrite(CONVEYOR_PWM_GPIO, 0);
    _state = CONVEYOR_OFF;
    return true;
}

void Conveyor::set_pwm(uint8_t pwm) {
    if (_state == CONVEYOR_FAULT) { pwm = 0; }
    analogWrite(CONVEYOR_PWM_GPIO, pwm);
    _pwm_enabled = (pwm > 0);
}

void Conveyor::start_collection() {
    if (_state == CONVEYOR_FAULT) return;
    _state = CONVEYOR_STARTING;
    _start_ms = millis();
}

void Conveyor::stop() {
    _state = CONVEYOR_OFF;
    _start_ms = 0;
    set_pwm(0);
}

void Conveyor::update() {
    uint32_t now = millis();
    switch (_state) {
        case CONVEYOR_STARTING:
            set_pwm(CONVEYOR_SPEED_PWM);
            _state = CONVEYOR_RUNNING;
            _start_ms = now;
            break;
        case CONVEYOR_RUNNING:
            if (now - _start_ms >= CONVEYOR_RUN_MS) {
                _state = CONVEYOR_STOPPING;
                _start_ms = now;
                _collection_count++;   /* timed completion = attempted collection */
            }
            break;
        case CONVEYOR_STOPPING:
            set_pwm(0);
            _state = CONVEYOR_OFF;
            break;
        case CONVEYOR_OFF:
            set_pwm(0);
            break;
        case CONVEYOR_FAULT:
            set_pwm(0);
            break;
    }
}
