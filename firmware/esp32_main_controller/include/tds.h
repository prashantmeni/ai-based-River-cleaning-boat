#ifndef TDS_H
#define TDS_H

#include <Arduino.h>

/* Analog TDS meter (DFRobot Gravity SEN0244, 0..2.3 V output) on ADC1.
 * Oversamples the pin every TDS_UPDATE_MS. A reading whose oversample
 * spread exceeds TDS_MAX_SPREAD_MV is an unconnected/floating input and
 * is reported as INVALID (ppm = -1) — never converted into a plausible
 * number. Raw averaged millivolts are always exposed for wiring checks. */
class TdsSensor {
public:
    void begin(uint8_t pin);
    void update();                     /* throttled: samples every TDS_UPDATE_MS */
    int16_t ppm() const { return _ppm; }       /* -1 = invalid/no reading yet */
    uint16_t millivolts() const { return _mv; } /* raw averaged mV */
    bool valid() const { return _valid; }
    uint32_t last_read_ms() const { return _last_ms; }

private:
    uint8_t _pin = 0;
    int16_t _ppm = -1;
    uint16_t _mv = 0;
    bool _valid = false;
    uint32_t _last_ms = 0;
};

#endif /* TDS_H */
