#include <WiFi.h>     /* explicit: lets LDF link the framework WiFi library */
#include "serial_tx.h"
#include "espnow_link.h"
#include "protocol.h"

static void (*s_wifi_config_cb)(const char *ssid, const char *password) = nullptr;
static void (*s_flash_cb)(uint8_t state) = nullptr;

static void espnow_recv_cb(const esp_now_recv_info_t *info,
                           const uint8_t *data, int len) {
    (void)info;
    if (!data || len <= 0) return;

    /* Only handle frames with a valid type byte at offset 2. */
    if (data[2] != PROTOCOL_MSG_WIFI_CONFIG && data[2] != PROTOCOL_MSG_CAM_FLASH)
        return;

    /* Payload starts at offset 7 (START+VER+TYPE+SEQ2+LEN2). */
    const uint8_t *payload = data + 7;
    size_t payload_len = (size_t)data[5] | ((size_t)data[6] << 8);

    if (data[2] == PROTOCOL_MSG_WIFI_CONFIG) {
        if (!s_wifi_config_cb || payload_len < 64) return;
        char ssid[32] = "";
        char pass[32] = "";
        if (protocol_parse_wifi_config_payload(payload, payload_len,
                                               ssid, pass, sizeof(ssid)) == 0) {
            s_wifi_config_cb(ssid, pass);
        }
    } else if (data[2] == PROTOCOL_MSG_CAM_FLASH) {
        if (!s_flash_cb || payload_len < 1) return;
        s_flash_cb(payload[0]);
    }
}

SerialTx::SerialTx() : _link_ok(false), _tx_errors(0) {}

bool SerialTx::begin() {
    _link_ok = espnow_link_begin();
    if (_link_ok) {
        esp_now_register_recv_cb(espnow_recv_cb);
    }
    return _link_ok;
}

void SerialTx::set_wifi_config_callback(void (*cb)(const char *, const char *)) {
    s_wifi_config_cb = cb;
}

void SerialTx::set_flash_callback(void (*cb)(uint8_t state)) {
    s_flash_cb = cb;
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
     * Re-register the receive callback after resync since esp_now_deinit
     * clears it. */
    espnow_link_resync();
    esp_now_register_recv_cb(espnow_recv_cb);
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
