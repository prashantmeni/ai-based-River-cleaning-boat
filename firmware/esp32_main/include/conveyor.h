#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CONVEYOR_OFF = 0,
    CONVEYOR_RUN,
    CONVEYOR_TIMEOUT,
    CONVEYOR_FAULT,
} conveyor_state_t;

void conveyor_init(void);
void conveyor_start(void);
void conveyor_stop(void);
// Call periodically; auto-stops after timeout.
void conveyor_update(uint32_t now_ms);
conveyor_state_t conveyor_state(void);
bool conveyor_is_running(void);
