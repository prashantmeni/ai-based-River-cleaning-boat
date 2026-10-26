#include "telemetry.h"
#include "navigation.h"
#include "motor.h"
#include "obstacle.h"
#include "gps.h"
#include "serial_rx.h"
#include "conveyor.h"
#include "safety.h"
#include "shared/protocol/protocol.h"

Telemetry::Telemetry() : _nav(nullptr), _motors(nullptr), _obs(nullptr),
                         _gps(nullptr), _rx(nullptr), _conv(nullptr),
                         _safety(nullptr) {}

void Telemetry::attach(Navigation *nav, MotorDriver *motors,
                       ObstacleSensors *obs, GPS *gps, SerialRx *rx,
                       Conveyor *conv, Safety *safety) {
    _nav = nav; _motors = motors; _obs = obs; _gps = gps;
    _rx = rx; _conv = conv; _safety = safety;
}

int Telemetry::build_status_frame(uint8_t *out, size_t out_size,
                                  uint16_t *sequence) {
    main_status_payload_t p;
    memset(&p, 0, sizeof(p));
    p.nav_state = (uint8_t)(_nav ? _nav->state() : 0);
    p.collection_count = (_conv ? _conv->collection_count() : 0);
    p.uptime_ms = millis();
    p.fault_flags = (_safety ? _safety->fault_flags() : 0);
    p.comm_health = (_rx && _rx->comms_alive()) ? 0 : 1;

    if (_motors) {
        p.motor_left_pwm = _motors->last_left_pwm();
        p.motor_right_pwm = _motors->last_right_pwm();
        p.motor_left_dir = _motors->last_left_dir();
        p.motor_right_dir = _motors->last_right_dir();
    }
    p.conveyor_state = (_conv ? (uint8_t)_conv->state() : 0);

    size_t len = sizeof(main_status_payload_t);
    return protocol_serialize_frame(PROTOCOL_MSG_MAIN_STATUS, *sequence,
                                    reinterpret_cast<const uint8_t *>(&p),
                                    (uint16_t)len, out, out_size);
}

void Telemetry::debug_print() const {
    Serial.print("[MAIN] state=");
    Serial.print(_nav ? _nav->state_name() : "?");
    Serial.print(" comms=");
    Serial.print(_rx && _rx->comms_alive() ? "ok" : "lost");
    Serial.print(" det=");
    Serial.print(_nav && _nav->have_detection() ? "yes" : "no");
    Serial.print(" conf=");
    Serial.print(_nav ? _nav->detection_confidence() : 0);
    Serial.print(" L=");
    Serial.print(_obs ? _obs->left_cm() : -1);
    Serial.print("cm R=");
    Serial.print(_obs ? _obs->right_cm() : -1);
    Serial.print("cm");
    if (_gps && _gps->has_fix()) {
        Serial.print(" fix lat="); Serial.print(_gps->latitude(), 6);
        Serial.print(" lon="); Serial.print(_gps->longitude(), 6);
    } else {
        Serial.print(" fix=no");
    }
    Serial.println();
}
