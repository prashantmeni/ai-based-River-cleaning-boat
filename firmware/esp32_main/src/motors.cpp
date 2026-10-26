#include "motors.h"
#include "config_main.h"
#include <Arduino.h>

// L298N dual H-bridge #1: ENA/ENB = 12-bit LEDC speed, IN pins = direction.
// Signed input (-4095..4095); sign drives direction pins, magnitude drives PWM.

static bool g_ok = false;

static void drive(int in1, int in2, int speed, int ch) {
    if (speed > 0) {
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW);
    } else if (speed < 0) {
        digitalWrite(in1, LOW);
        digitalWrite(in2, HIGH);
    } else {
        digitalWrite(in1, LOW);
        digitalWrite(in2, LOW);
    }
    int mag = speed < 0 ? -speed : speed;
    if (mag > PROP_PWM_MAX) mag = PROP_PWM_MAX;
    ledcWrite(ch, mag);
}

void motors_init(void) {
    pinMode(PIN_M_IN1, OUTPUT);
    pinMode(PIN_M_IN2, OUTPUT);
    pinMode(PIN_M_IN3, OUTPUT);
    pinMode(PIN_M_IN4, OUTPUT);
    digitalWrite(PIN_M_IN1, LOW);
    digitalWrite(PIN_M_IN2, LOW);
    digitalWrite(PIN_M_IN3, LOW);
    digitalWrite(PIN_M_IN4, LOW);
    ledcSetup(0, MOTOR_PWM_FREQ_HZ, MOTOR_PWM_BITS);
    ledcSetup(1, MOTOR_PWM_FREQ_HZ, MOTOR_PWM_BITS);
    ledcAttachPin(PIN_M_ENA, 0);
    ledcAttachPin(PIN_M_ENB, 1);
    ledcWrite(0, 0);
    ledcWrite(1, 0);
    g_ok = true;
    Serial.println("[motors] L298N #1 ready (LEDC ch0/ch1)");
}

void motors_set(int left, int right) {
    if (!g_ok) return;
    if (left  >  PROP_PWM_MAX) left  =  PROP_PWM_MAX;
    if (left  < -PROP_PWM_MAX) left  = -PROP_PWM_MAX;
    if (right >  PROP_PWM_MAX) right =  PROP_PWM_MAX;
    if (right < -PROP_PWM_MAX) right = -PROP_PWM_MAX;
    drive(PIN_M_IN1, PIN_M_IN2, left, 0);
    drive(PIN_M_IN3, PIN_M_IN4, right, 1);
}

void motors_stop(void) {
    if (!g_ok) return;
    drive(PIN_M_IN1, PIN_M_IN2, 0, 0);
    drive(PIN_M_IN3, PIN_M_IN4, 0, 1);
}
