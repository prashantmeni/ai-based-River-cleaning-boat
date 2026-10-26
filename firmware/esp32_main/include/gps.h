#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool  has_fix;
    double lat;
    double lon;
    float  speed_kmh;
    float  course_deg;
    uint32_t satellites;
    uint32_t last_update_ms;
} gps_fix_t;

void gps_init(void);
// Feed one byte (call from loop or serial task).
void gps_feed(char c);
// Poll: reads available UART bytes internally.
void gps_update(uint32_t now_ms);
const gps_fix_t* gps_get(void);
int gps_route_count(void);
// Returns pointer to log entries; count via gps_route_count().
const gps_fix_t* gps_route(void);
