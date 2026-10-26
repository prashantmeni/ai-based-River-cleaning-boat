#include "buzzer.h"
#include "config_main.h"
#include <Arduino.h>

static uint32_t g_on_until;

void buzzer_init(void) {
    g_on_until = 0;
    if (PIN_BUZZER >= 0) {
        pinMode(PIN_BUZZER, OUTPUT);
        digitalWrite(PIN_BUZZER, LOW);
        Serial.println("[buzzer] ready");
    } else {
        Serial.println("[buzzer] not fitted (PIN_BUZZER -1) - stub disabled");
    }
}

void buzzer_beep(uint32_t dur_ms) {
    if (PIN_BUZZER < 0) return;
    digitalWrite(PIN_BUZZER, HIGH);
    g_on_until = millis() + dur_ms;
}

void buzzer_update(uint32_t now_ms) {
    if (PIN_BUZZER < 0) return;
    if (g_on_until && now_ms >= g_on_until) {
        digitalWrite(PIN_BUZZER, LOW);
        g_on_until = 0;
    }
}
