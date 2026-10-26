#include "conveyor.h"
#include "config_main.h"
#include <Arduino.h>

// Conveyor via L298N #2 channel A: ENA=PIN_C_ENA (PWM on/off+speed);
// IN1/IN2 hardwired on the breadboard (IN1=5V, IN2=GND), forward only.

static conveyor_state_t g_state = CONVEYOR_OFF;
static uint32_t g_start_ms = 0;
static bool g_running = false;
static bool g_ok = false;

void conveyor_init(void) {
    g_state = CONVEYOR_OFF;
    g_running = false;
    ledcSetup(2, MOTOR_PWM_FREQ_HZ, MOTOR_PWM_BITS);
    ledcAttachPin(PIN_C_ENA, 2);
    ledcWrite(2, 0);
    g_ok = true;
    Serial.println("[conv] L298N #2 chA ready (ENA=GPIO4, IN tied)");
}

void conveyor_start(void) {
    g_start_ms = millis();
    g_running = true;
    g_state = CONVEYOR_RUN;
    if (g_ok) ledcWrite(2, CONVEYOR_PWM);
}

void conveyor_stop(void) {
    g_running = false;
    if (g_state == CONVEYOR_RUN) g_state = CONVEYOR_OFF;
    if (g_ok) ledcWrite(2, 0);
}

void conveyor_update(uint32_t now_ms) {
    if (!g_running) return;
    if (now_ms - g_start_ms > COLLECT_DURATION_MS && g_state == CONVEYOR_RUN) {
        g_state = CONVEYOR_TIMEOUT;
        conveyor_stop();
        g_state = CONVEYOR_TIMEOUT;
    }
    if (now_ms - g_start_ms > CONVEYOR_TIMEOUT_MS) {
        conveyor_stop();
        g_state = CONVEYOR_FAULT;
    }
}

conveyor_state_t conveyor_state(void) { return g_state; }
bool conveyor_is_running(void) { return g_running; }
