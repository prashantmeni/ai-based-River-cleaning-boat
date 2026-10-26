#pragma once
#include <Arduino.h>

// Snapshot the dashboard must display (filled by main loop).
typedef struct {
    const char* state;
    int ai_class;          // -1 none
    float ai_conf;
    const char* region;    // left/center/right/-
    int count_plastic_bottle;
    int count_plastic_wrapper;
    int count_organic;
    int count_other;
    int collection_attempts;
    int collection_confirmed; // always attempts until sensor added
    const char* basket_state;
    float basket_fill_pct;
    float obstacle_left_cm;
    float obstacle_right_cm;
    double lat;
    double lon;
    bool gps_fix;
    int gps_sats;
    const char* conveyor;
    const char* fault;
    bool wifi_on;
    float battery_v;       // -1 if not measured
    float cpu_temp;        // internal TSENS, deg C
    int thermal;           // 0 ok, 1 throttled, 2 AI paused
} dash_snapshot_t;

void dashboard_begin(void);
void dashboard_update(const dash_snapshot_t* snap);
void dashboard_loop(void);
