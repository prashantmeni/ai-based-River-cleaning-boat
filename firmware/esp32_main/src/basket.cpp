#include "basket.h"
#include "config_main.h"

static basket_state_t g_state = BASKET_EMPTY;
static float g_pct = 0;

void basket_init(void) {
    g_state = BASKET_EMPTY;
    g_pct = 0;
}

void basket_update(us_reading_t r) {
    if (!r.valid || r.cm < BASKET_MIN_VALID_CM || r.cm > BASKET_MAX_VALID_CM) {
        g_state = BASKET_SENSOR_FAULT;
        return;
    }
    // closer reading = fuller basket
    float span = BASKET_EMPTY_CM - BASKET_FULL_CM;
    float pct = (BASKET_EMPTY_CM - r.cm) / span * 100.0f;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    g_pct = pct;

    if (g_state == BASKET_SENSOR_FAULT) g_state = BASKET_EMPTY; // recovered

    if (r.cm <= BASKET_FULL_CM) {
        g_state = BASKET_FULL;
    } else if (r.cm <= BASKET_NEAR_FULL_CM) {
        g_state = BASKET_NEAR_FULL;
        if (g_pct > 5) g_state = BASKET_NEAR_FULL;
    } else {
        g_state = (g_pct > 2) ? BASKET_FILLING : BASKET_EMPTY;
    }
}

basket_state_t basket_state(void) { return g_state; }
float basket_fill_percent(void) { return g_pct; }
