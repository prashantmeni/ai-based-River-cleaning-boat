#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <Arduino.h>
#include "protocol.h"

/* Builds MAIN_STATUS telemetry payload and sends it back to the ESP32-CAM
 * (optional) / exposes fields for the dashboard. */
class Telemetry {
public:
    Telemetry();
    void attach(class Navigation *nav, class MotorDriver *motors,
                class ObstacleSensors *obs, class GPS *gps,
                class SerialRx *rx, class Conveyor *conv,
                class Safety *safety);

    /* Encode a MAIN_STATUS frame into the wire buffer. Returns bytes. */
    int build_status_frame(uint8_t *out, size_t out_size, uint16_t *sequence);

    /* Human-readable one-line summary for serial debug. */
    void debug_print() const;

private:
    class Navigation *_nav;
    class MotorDriver *_motors;
    class ObstacleSensors *_obs;
    class GPS *_gps;
    class SerialRx *_rx;
    class Conveyor *_conv;
    class Safety *_safety;
};

#endif /* TELEMETRY_H */

