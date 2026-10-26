#ifndef SERIAL_TX_H
#define SERIAL_TX_H

#include <HardwareSerial.h>
#include "protocol.h"

/* Transmitter for the ESP32-CAM -> main controller serial link.
 * Wraps protocol encoding and sends over a HardwareSerial port.
 */
class SerialTx {
public:
    SerialTx(HardwareSerial &port);
    void begin(uint32_t baud);

    /* Send an AI detection. Returns true if written. */
    bool send_detection(const ai_detection_payload_t *det, uint16_t *sequence);

    /* Send a no-detection message. */
    bool send_no_detection(const ai_no_detection_payload_t *nd, uint16_t *sequence);

    /* Send an AI health heartbeat. */
    bool send_health(const ai_health_payload_t *h, uint16_t *sequence);

    /* Flush the transmit buffer. */
    void flush();

private:
    HardwareSerial &_port;
    bool _send_frame(uint8_t type, const uint8_t *payload, uint16_t payload_len,
                     uint16_t *sequence);
    uint8_t _wire[128];
};

#endif /* SERIAL_TX_H */

