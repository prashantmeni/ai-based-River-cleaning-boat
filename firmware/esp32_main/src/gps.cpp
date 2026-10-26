#include "gps.h"
#include "config_main.h"
#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

static TinyGPSPlus gps;
static HardwareSerial GpsSerial(2); // UART2 on PIN_GPS_RX/PIN_GPS_TX
static gps_fix_t g_fix = {};
static gps_fix_t g_route[ROUTE_LOG_MAX];
static int g_route_n = 0;
static uint32_t g_last_add_ms = 0;

void gps_init(void) {
    GpsSerial.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
    g_fix.has_fix = false;
    g_route_n = 0;
}

void gps_feed(char c) { gps.encode(c); }

void gps_update(uint32_t now_ms) {
    while (GpsSerial.available()) {
        gps.encode((char)GpsSerial.read());
    }
    if (gps.location.isUpdated()) {
        g_fix.has_fix = gps.location.isValid() && gps.location.age() < 5000;
        g_fix.lat = gps.location.lat();
        g_fix.lon = gps.location.lng();
        g_fix.speed_kmh = gps.speed.isValid() ? gps.speed.kmph() : 0;
        g_fix.course_deg = gps.course.isValid() ? gps.course.deg() : 0;
        g_fix.satellites = gps.satellites.isValid() ? gps.satellites.value() : 0;
        g_fix.last_update_ms = now_ms;

        // bounded route log, one point per second max
        if (g_fix.has_fix && now_ms - g_last_add_ms >= 1000) {
            if (g_route_n < ROUTE_LOG_MAX) {
                g_route[g_route_n++] = g_fix;
            } else {
                // shift to keep memory bounded (oldest dropped)
                memmove(&g_route[0], &g_route[1], sizeof(gps_fix_t) * (ROUTE_LOG_MAX - 1));
                g_route[ROUTE_LOG_MAX - 1] = g_fix;
            }
            g_last_add_ms = now_ms;
        }
    }
}

const gps_fix_t* gps_get(void) { return &g_fix; }
int gps_route_count(void) { return g_route_n; }
const gps_fix_t* gps_route(void) { return g_route; }
