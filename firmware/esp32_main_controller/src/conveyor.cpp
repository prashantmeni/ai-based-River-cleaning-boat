#include "conveyor.h"

Conveyor::Conveyor()
    : _state(CONVEYOR_OFF), _start_ms(0), _collection_count(0),
      _pwm_enabled(false) {}

Conveyor::~Conveyor() { stop(); }

bool Conveyor::begin() {
    pinMode(CONVEYOR_EN_GPIO, OUTPUT);
    pinMode(CONVEYOR_IN1_GPIO, OUTPUT);
    pinMode(CONVEYOR_IN2_GPIO, OUTPUT);
    /* Fixed direction (debris onto the boat): IN1=1, IN2=0. */
    digitalWrite(CONVEYOR_IN1_GPIO, HIGH);
    digitalWrite(CONVEYOR_IN2_GPIO, LOW);
    analogWrite(CONVEYOR_EN_GPIO, 0);
    _state = CONVEYOR_OFF;
    return true;
}

void Conveyor::set_pwm(uint8_t pwm) {
    if (_state == CONVEYOR_FAULT) { pwm = 0; }
    analogWrite(CONVEYOR_EN_GPIO, pwm);
    _pwm_enabled = (pwm > 0);
}

void Conveyor::start_collection() {
    if (_state == CONVEYOR_FAULT) return;
    _state = CONVEYOR_RUNNING;
    set_pwm(CONVEYOR_SPEED_PWM);
}

void Conveyor::stop() {
    _state = CONVEYOR_OFF;
    _start_ms = 0;
    set_pwm(0);
}

void Conveyor::update() {
    switch (_state) {
        case CONVEYOR_RUNNING:
            set_pwm(CONVEYOR_SPEED_PWM);
            break;
        case CONVEYOR_OFF:
        case CONVEYOR_FAULT:
            set_pwm(0);
            break;
        default:
            break;
    }
}
