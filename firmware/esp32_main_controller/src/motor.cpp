#include "motor.h"

MotorDriver::MotorDriver()
    : _enabled(false), _last_left_dir(0), _last_right_dir(0),
      _last_left_pwm(0), _last_right_pwm(0) {}

MotorDriver::~MotorDriver() { disable(); }

bool MotorDriver::begin() {
    pinMode(MOTOR_LEFT_ENA_GPIO, OUTPUT);
    pinMode(MOTOR_LEFT_IN1_GPIO, OUTPUT);
    pinMode(MOTOR_LEFT_IN2_GPIO, OUTPUT);
    pinMode(MOTOR_RIGHT_ENB_GPIO, OUTPUT);
    pinMode(MOTOR_RIGHT_IN3_GPIO, OUTPUT);
    pinMode(MOTOR_RIGHT_IN4_GPIO, OUTPUT);

    /* Safe default: enables low (outputs off), inputs low. */
    analogWrite(MOTOR_LEFT_ENA_GPIO, 0);
    analogWrite(MOTOR_RIGHT_ENB_GPIO, 0);
    digitalWrite(MOTOR_LEFT_IN1_GPIO, LOW);
    digitalWrite(MOTOR_LEFT_IN2_GPIO, LOW);
    digitalWrite(MOTOR_RIGHT_IN3_GPIO, LOW);
    digitalWrite(MOTOR_RIGHT_IN4_GPIO, LOW);

    /* Only enable after successful init. */
    _enabled = true;
    stop();
    return true;
}

/* L298N (HW-095) channel: IN_A/IN_B select direction, ENA carries PWM.
 * dir  1 -> IN_A=1 IN_B=0 (forward)
 * dir -1 -> IN_A=0 IN_B=1 (reverse)
 * dir  0 -> ENA=0 (outputs high-Z, coast) + inputs low. */
void MotorDriver::apply(int8_t dir, uint8_t pwm, uint8_t ena_gpio,
                        uint8_t in_a_gpio, uint8_t in_b_gpio, bool invert) {
    if (!_enabled) { pwm = 0; dir = 0; }
    if (dir == 0) {
        analogWrite(ena_gpio, 0);
        digitalWrite(in_a_gpio, LOW);
        digitalWrite(in_b_gpio, LOW);
        return;
    }
    bool forward = (dir > 0);
    if (invert) forward = !forward;
    digitalWrite(in_a_gpio, forward ? HIGH : LOW);
    digitalWrite(in_b_gpio, forward ? LOW : HIGH);
    analogWrite(ena_gpio, pwm);
}

void MotorDriver::set_motors(int8_t left_dir, uint8_t left_pwm,
                             int8_t right_dir, uint8_t right_pwm) {
    /* uint8_t is 0..255; it can never exceed MAX_PWM(255). */
    _last_left_dir = left_dir;
    _last_right_dir = right_dir;
    _last_left_pwm = (left_dir == 0) ? 0 : left_pwm;
    _last_right_pwm = (right_dir == 0) ? 0 : right_pwm;
    apply(left_dir, left_pwm, MOTOR_LEFT_ENA_GPIO, MOTOR_LEFT_IN1_GPIO,
          MOTOR_LEFT_IN2_GPIO, MOTOR_LEFT_INVERT != 0);
    apply(right_dir, right_pwm, MOTOR_RIGHT_ENB_GPIO, MOTOR_RIGHT_IN3_GPIO,
          MOTOR_RIGHT_IN4_GPIO, MOTOR_RIGHT_INVERT != 0);
}

void MotorDriver::forward(uint8_t pwm) {
    set_motors(1, pwm, 1, pwm);
}
void MotorDriver::backward(uint8_t pwm) {
    set_motors(-1, pwm, -1, pwm);
}
void MotorDriver::turn_left(uint8_t pwm) {
    set_motors(-1, pwm, 1, pwm);
}
void MotorDriver::turn_right(uint8_t pwm) {
    set_motors(1, pwm, -1, pwm);
}
void MotorDriver::stop() {
    set_motors(0, 0, 0, 0);
}
void MotorDriver::disable() {
    _enabled = false;
    analogWrite(MOTOR_LEFT_ENA_GPIO, 0);
    analogWrite(MOTOR_RIGHT_ENB_GPIO, 0);
    digitalWrite(MOTOR_LEFT_IN1_GPIO, LOW);
    digitalWrite(MOTOR_LEFT_IN2_GPIO, LOW);
    digitalWrite(MOTOR_RIGHT_IN3_GPIO, LOW);
    digitalWrite(MOTOR_RIGHT_IN4_GPIO, LOW);
    _last_left_dir = _last_right_dir = 0;
    _last_left_pwm = _last_right_pwm = 0;
}
