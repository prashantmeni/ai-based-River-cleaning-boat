#ifndef SERIAL_RX_H
#define SERIAL_RX_H

#include <stdint.h>
#include "protocol.h"

/* Receiver for the main controller <- ESP32-CAM link (ESP-NOW wireless).
 *
 * The ESP-NOW receive callback runs in the WiFi task and only copies bytes
 * into a ring buffer; update() (loop task) drains the ring, feeds the
 * framed-protocol parser, and dispatches callbacks so navigation code is
 * never touched from interrupt/WiFi context.
 */
class SerialRx {
public:
    typedef void (*DetectionCb)(const ai_detection_payload_t *);
    typedef void (*NoDetectionCb)(const ai_no_detection_payload_t *);
    typedef void (*HealthCb)(const ai_health_payload_t *);
    typedef void (*DiagCommandCb)(uint8_t cmd, uint8_t value, uint16_t duration_ms);

    SerialRx();

    /* Bring up WiFi + ESP-NOW receive path. ap_ssid != nullptr also starts
     * the dashboard SoftAP gateway (same channel as ESP-NOW). */
    void begin(const char *ap_ssid = nullptr, const char *ap_pass = nullptr);

    /* Call regularly from loop. Drains ring, parses frames, dispatches. */
    void update();

    void on_detection(DetectionCb cb);
    void on_no_detection(NoDetectionCb cb);
    void on_health(HealthCb cb);
    void on_diag_command(DiagCommandCb cb);

    bool comms_alive() const { return _comms_alive; }
    bool link_ok() const { return _link_ok; }
    uint32_t last_frame_ms() const { return _last_frame_ms; }
    uint16_t error_count() const { return _error_count; }
    uint32_t rx_bytes() const { return _rx_bytes; }
    uint8_t last_err() const { return _last_err; }
    uint8_t dbg_n() const { return _dbg_n; }
    const uint8_t *dbg() const { return _dbg; }
    uint32_t drops() const;

private:
    protocol_rx_t _rx;
    DetectionCb _cb_det;
    NoDetectionCb _cb_nd;
    HealthCb _cb_health;
    DiagCommandCb _cb_diag;
    bool _comms_alive;
    bool _link_ok;
    uint32_t _last_frame_ms;
    uint16_t _error_count;
    uint32_t _rx_bytes;
    uint8_t _last_err;
    uint8_t _dbg[20];
    uint8_t _dbg_n;
};

#endif /* SERIAL_RX_H */
