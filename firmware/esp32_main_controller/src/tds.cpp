#include "tds.h"
#include "boat_config.h"

#if TDS_SAMPLE_COUNT < 3
#error "TDS_SAMPLE_COUNT must be >= 3 (min/max are dropped)"
#endif

void TdsSensor::begin(uint8_t pin) {
    _pin = pin;
    pinMode(pin, INPUT);
    /* 11 dB attenuation -> ~3.1 V full scale; SEN0244 tops out at 2.3 V. */
    analogSetPinAttenuation(pin, ADC_11db);
    _last_ms = millis();   /* first sample one interval after boot */
}

void TdsSensor::update() {
    uint32_t now = millis();
    if (now - _last_ms < TDS_UPDATE_MS) return;
    _last_ms = now;

    /* Oversample, drop min/max, average the rest (noise rejection). */
    uint32_t sum = 0;
    uint16_t mn = 0xFFFF, mx = 0;
    for (uint8_t i = 0; i < TDS_SAMPLE_COUNT; i++) {
        uint16_t v = (uint16_t)analogReadMilliVolts(_pin);
        sum += v;
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        delayMicroseconds(200);
    }
    uint16_t avg = (uint16_t)((sum - mn - mx) / (TDS_SAMPLE_COUNT - 2));
    _mv = avg;

    /* Unconnected/floating input wanders far more than a real SEN0244
     * output. Report invalid rather than converting noise to "ppm". */
    if ((uint16_t)(mx - mn) > TDS_MAX_SPREAD_MV) {
        _valid = false;
        _ppm = -1;
        return;
    }

    /* Manufacturer cubic transfer function (v in volts). */
    float v = avg / 1000.0f;
    float ppm = (133.42f * v * v * v - 255.86f * v * v + 857.39f * v)
                * TDS_CALIBRATION;
    if (ppm < 0.0f) ppm = 0.0f;
    if (ppm > (float)TDS_PPM_MAX) ppm = (float)TDS_PPM_MAX;
    _ppm = (int16_t)(ppm + 0.5f);
    _valid = true;
}
