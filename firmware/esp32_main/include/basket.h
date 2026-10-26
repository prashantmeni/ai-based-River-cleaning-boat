#pragma once
#include <stdbool.h>
#include "ultrasonic.h"

typedef enum {
    BASKET_EMPTY = 0,
    BASKET_FILLING,
    BASKET_NEAR_FULL,
    BASKET_FULL,
    BASKET_SENSOR_FAULT,
} basket_state_t;

void basket_init(void);
// Call with a fresh ultrasonic reading from the basket sensor.
void basket_update(us_reading_t r);
basket_state_t basket_state(void);
float basket_fill_percent(void); // 0..100 estimate
