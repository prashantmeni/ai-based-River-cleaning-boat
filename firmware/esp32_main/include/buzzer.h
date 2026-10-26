#pragma once
#include <stdint.h>

void buzzer_init(void);
void buzzer_beep(uint32_t dur_ms);
void buzzer_update(uint32_t now_ms);
