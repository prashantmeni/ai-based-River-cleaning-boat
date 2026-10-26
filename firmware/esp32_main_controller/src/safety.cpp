#include "safety.h"

Safety::Safety() : _faults(0), _emergency(false), _wd_last_ms(0) {}
Safety::~Safety() {}

void Safety::begin() {
    _wd_last_ms = millis();
    pinMode(BUZZER_GPIO, OUTPUT);
    digitalWrite(BUZZER_GPIO, LOW);
}

void Safety::reset_watchdog() {
    _wd_last_ms = millis();
}

void Safety::emergency_stop() {
    _emergency = true;
    digitalWrite(BUZZER_GPIO, HIGH);
}

void Safety::clear_fault(uint8_t bits) {
    _faults = (uint8_t)(_faults & ~bits);
    if (_faults == 0) {
        _emergency = false;
        digitalWrite(BUZZER_GPIO, LOW);
    }
}

void Safety::set_comms_lost(bool lost) {
    if (lost) _faults |= FAULT_COMMS_LOST;
    else _faults = (uint8_t)(_faults & ~FAULT_COMMS_LOST);
}
void Safety::set_obstacle_stop(bool stop) {
    if (stop) _faults |= FAULT_OBSTACLE;
    else _faults = (uint8_t)(_faults & ~FAULT_OBSTACLE);
}
void Safety::set_sensors_bad(bool bad) {
    if (bad) _faults |= FAULT_SENSOR_BAD;
    else _faults = (uint8_t)(_faults & ~FAULT_SENSOR_BAD);
}
