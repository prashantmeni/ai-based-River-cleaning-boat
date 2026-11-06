#include <WiFi.h>     /* explicit: lets LDF link the framework WiFi library */
#include "serial_tx.h"
#include "espnow_link.h"

SerialTx::SerialTx() : _link_ok(false), _tx_errors(0) {}

bool SerialTx::begin() {
    _link_ok = espnow_link_begin();
    return _link_ok;
}

bool SerialTx::_send_frame(uint8_t type, const uint8_t *payload,
                           uint16_t payload_len, uint16_t *sequence) {
    if (!sequence || !_link_ok) return false;
    int n = protocol_serialize_frame(type, *sequence, payload, payload_len,
                                     _wire, sizeof(_wire));
    if (n < 0) return false;
    (*sequence)++;
    /* Follow Wi-Fi channel drift before transmitting (the CAM joins the
     * dashboard SoftAP, whose channel follows the router — IDFGH-17728).
     * No receive callback exists on this board, so nothing to re-register. */
    espnow_link_resync();
    if (!espnow_link_send(_wire, (size_t)n)) {
        _tx_errors++;
        return false;
    }
    return true;
}

bool SerialTx::send_detection(const ai_detection_payload_t *det,
                              uint16_t *sequence) {
    uint8_t payload[64];
    size_t len = protocol_build_ai_detection_payload(det, payload);
    return _send_frame(PROTOCOL_MSG_AI_DETECTION, payload, (uint16_t)len, sequence);
}

bool SerialTx::send_no_detection(const ai_no_detection_payload_t *nd,
                                 uint16_t *sequence) {
    size_t len = sizeof(ai_no_detection_payload_t);
    return _send_frame(PROTOCOL_MSG_AI_NO_DETECTION,
                       reinterpret_cast<const uint8_t *>(nd),
                       (uint16_t)len, sequence);
}

bool SerialTx::send_health(const ai_health_payload_t *h,
                           uint16_t *sequence) {
    size_t len = sizeof(ai_health_payload_t);
    return _send_frame(PROTOCOL_MSG_AI_HEALTH,
                       reinterpret_cast<const uint8_t *>(h),
                       (uint16_t)len, sequence);
}

void SerialTx::flush() {
    /* ESP-NOW: frames are handed to the radio driver as whole datagrams. */
}
