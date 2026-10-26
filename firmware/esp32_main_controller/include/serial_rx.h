#ifndef SERIAL_RX_H
#define SERIAL_RX_H

#include <HardwareSerial.h>
#include "shared/protocol/protocol.h"

/* Receiver for main controller <- ESP32-CAM serial link.
 * Feeds bytes one at a time; dispatches complete frames via callbacks.
 */
class SerialRx {
public:
    typedef void (*DetectionCb)(const ai_detection_payload_t *);
    typedef void (*NoDetectionCb)(const ai_no_detection_payload_t *);
    typedef void (*HealthCb)(const ai_health_payload_t *);
    typedef void (*DiagCommandCb)(uint8_t cmd, uint8_t value, uint16_t duration_ms);

    SerialRx(HardwareSerial &port);
    void begin(uint32_t baud);

    /* Call regularly from loop. Processes available serial bytes. */
    void update();

    void on_detection(DetectionCb cb);
    void on_no_detection(NoDetectionCb cb);
    void on_health(HealthCb cb);
    void on_diag_command(DiagCommandCb cb);

    bool comms_alive() const { return _comms_alive; }
    uint32_t last_frame_ms() const { return _last_frame_ms; }
    uint16_t error_count() const { return _error_count; }

private:
    HardwareSerial &_port;
    protocol_rx_t _rx;
    DetectionCb _cb_det;
    NoDetectionCb _cb_nd;
    HealthCb _cb_health;
    DiagCommandCb _cb_diag;
    bool _comms_alive;
    uint32_t _last_frame_ms;
    uint16_t _error_count;
};

#endif /* SERIAL_RX_H */
