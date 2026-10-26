#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float cm;         // measured distance, <0 if invalid
    bool  valid;
} us_reading_t;

void ultrasonic_init(void);
// Blocking read with timeout; returns cm<0 on invalid/timeout.
us_reading_t ultrasonic_read(int trig_pin, int echo_pin);
// Filtered average of N samples.
us_reading_t ultrasonic_read_filtered(int trig_pin, int echo_pin, int n);
