#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"

typedef enum {
    ST_BOOT = 0,
    ST_SELF_TEST,
    ST_IDLE,
    ST_SEARCH,
    ST_DETECT,
    ST_ALIGN,
    ST_APPROACH,
    ST_COLLECT,
    ST_VERIFY_COLLECTION,
    ST_RETURN_OR_STOP,
    ST_FAULT_STOP,
} boat_state_t;

void sm_init(void);
boat_state_t sm_state(void);
const char* sm_state_name(boat_state_t s);
// Drive one control tick with fresh inputs.
void sm_update(uint32_t now_ms,
               bool camera_ok,
               bool ai_ok,
               int ai_class, float ai_conf, target_dir_t dir,
               float obstacle_left_cm, float obstacle_right_cm,
               int basket_state);
void sm_emergency_stop(void);
void sm_clear_fault(void);
