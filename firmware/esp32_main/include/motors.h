#pragma once
#include <stdint.h>
#include <stdbool.h>

void motors_init(void);
// speed: -4095..4095 (signed for direction), channel L/R
void motors_set(int left, int right);
void motors_stop(void);
