#ifndef SERIAL_TX_H
#define SERIAL_TX_H

#include <Arduino.h>
#include "protocol.h"
    /* Transmitter for the ESP32-CAM -> main controller link (ESP-NOW wireless).
     * Wraps protocol encoding and sends each complete frame as one ESP-NOW
     * broadcast datagram. UART0 (GPIO1/3) remains free for the debug console.
     */
    class SerialTx {
    public:
        SerialTx();

        /* Bring up WiFi + ESP-NOW. Returns true when ready to send. */
        bool begin();

        /* Send an AI detection. Returns true if handed to the radio. */
        bool send_detection(const ai_detection_payload_t *det, uint16_t *sequence);

        /* Send a no-detection message. */
        bool send_no_detection(const ai_no_detection_payload_t *nd,
                               uint16_t *sequence);

        /* Send an AI health heartbeat. */
        bool send_health(const ai_health_payload_t *h, uint16_t *sequence);

        /* Set a callback for incoming Wi-Fi config frames (PROTOCOL_MSG_WIFI_CONFIG)
         * sent by the main controller over ESP-NOW. The callback receives the
         * ssid and password and should reconnect Wi-Fi STA to the new network. */
        void set_wifi_config_callback(void (*cb)(const char *ssid,
                                                 const char *password));

        /* Kept for API compatibility: ESP-NOW is packet-based, nothing to flush. */
        void flush();

        bool link_ok() const { return _link_ok; }
        uint32_t tx_errors() const { return _tx_errors; }

private:
    bool _send_frame(uint8_t type, const uint8_t *payload, uint16_t payload_len,
                     uint16_t *sequence);
    bool _link_ok;
    uint32_t _tx_errors;
    uint8_t _wire[PROTOCOL_MAX_FRAME];
};

#endif /* SERIAL_TX_H */
