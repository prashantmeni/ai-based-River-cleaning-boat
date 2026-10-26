#include "serial_rx.h"

SerialRx::SerialRx(HardwareSerial &port) : _port(port) {
    _cb_det = nullptr; _cb_nd = nullptr; _cb_health = nullptr;
    _cb_diag = nullptr;
    _comms_alive = false; _last_frame_ms = 0; _error_count = 0;
}

void SerialRx::begin(uint32_t baud) {
    _port.begin(baud);
    protocol_rx_init(&_rx);
}

void SerialRx::update() {
    while (_port.available()) {
        uint8_t b = (uint8_t)_port.read();
        protocol_frame_t frame;
        protocol_parse_result_t r = protocol_rx_push(&_rx, b, &frame);
        if (r == PROTOCOL_PARSE_OK) {
            _comms_alive = true;
            _last_frame_ms = millis();
            switch (frame.type) {
                case PROTOCOL_MSG_AI_DETECTION: {
                    ai_detection_payload_t det;
                    if (protocol_parse_ai_detection_payload(frame.payload,
                                                            frame.payload_length,
                                                            &det) == 0 && _cb_det) {
                        _cb_det(&det);
                    } else { _error_count++; }
                    break;
                }
                case PROTOCOL_MSG_AI_NO_DETECTION: {
                    if (frame.payload_length >= sizeof(ai_no_detection_payload_t) && _cb_nd) {
                        ai_no_detection_payload_t nd;
                        memcpy(&nd, frame.payload, sizeof(nd));
                        _cb_nd(&nd);
                    } else { _error_count++; }
                    break;
                }
                case PROTOCOL_MSG_AI_HEALTH: {
                    if (frame.payload_length >= sizeof(ai_health_payload_t) && _cb_health) {
                        ai_health_payload_t h;
                        memcpy(&h, frame.payload, sizeof(h));
                        _cb_health(&h);
                    } else { _error_count++; }
                    break;
                }
                case PROTOCOL_MSG_DIAG_COMMAND: {
                    if (frame.payload_length >= sizeof(diag_command_payload_t) && _cb_diag) {
                        diag_command_payload_t c;
                        memcpy(&c, frame.payload, sizeof(c));
                        _cb_diag(c.cmd, c.value, c.duration_ms);
                    } else { _error_count++; }
                    break;
                }
                default:
                    break;
            }
        } else if (r != PROTOCOL_PARSE_INCOMPLETE) {
            _error_count++;
        }
    }
    /* Comms considered alive only if a frame arrived recently. */
    if (_comms_alive && (millis() - _last_frame_ms > 2000)) {
        _comms_alive = false;
    }
}

void SerialRx::on_detection(DetectionCb cb)  { _cb_det = cb; }
void SerialRx::on_no_detection(NoDetectionCb cb) { _cb_nd = cb; }
void SerialRx::on_health(HealthCb cb) { _cb_health = cb; }
void SerialRx::on_diag_command(DiagCommandCb cb) { _cb_diag = cb; }
