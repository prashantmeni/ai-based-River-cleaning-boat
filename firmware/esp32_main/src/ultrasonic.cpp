#include "ultrasonic.h"
#include "config_main.h"
#include <Arduino.h>

void ultrasonic_init(void) {
    // pins set per-read to allow sharing/compilation simplicity
}

static inline void pulse_trig(int trig) {
    digitalWrite(trig, LOW);
    delayMicroseconds(2);
    digitalWrite(trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig, LOW);
}

us_reading_t ultrasonic_read(int trig_pin, int echo_pin) {
    us_reading_t r = { -1.0f, false };
    pinMode(trig_pin, OUTPUT);
    pinMode(echo_pin, INPUT);
    pulse_trig(trig_pin);

    uint32_t t0 = micros();
    // wait echo high
    while (digitalRead(echo_pin) == LOW) {
        if (micros() - t0 > US_TIMEOUT_US) return r;
    }
    uint32_t t1 = micros();
    while (digitalRead(echo_pin) == HIGH) {
        if (micros() - t1 > US_TIMEOUT_US) return r;
    }
    uint32_t us = micros() - t1;
    float cm = us / 58.0f;
    if (cm < US_MIN_VALID_CM || cm > US_MAX_VALID_CM) return r;
    r.cm = cm;
    r.valid = true;
    return r;
}

us_reading_t ultrasonic_read_filtered(int trig_pin, int echo_pin, int n) {
    float acc = 0; int cnt = 0; us_reading_t last = { -1.0f, false };
    for (int i = 0; i < n; i++) {
        us_reading_t r = ultrasonic_read(trig_pin, echo_pin);
        if (r.valid) { acc += r.cm; cnt++; last = r; }
        delay(10);
    }
    if (cnt == 0) return last;
    last.cm = acc / cnt;
    last.valid = true;
    return last;
}
