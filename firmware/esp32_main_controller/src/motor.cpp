#include "motor.h"

MotorDriver::MotorDriver()
    : _enabled(false), _last_left_dir(0), _last_right_dir(0),
      _last_left_pwm(0), _last_right_pwm(0) {}

MotorDriver::~MotorDriver() { disable(); }

bool MotorDriver::begin() {
    pinMode(MOTOR_LEFT_PWM_GPIO, OUTPUT);
    pinMode(MOTOR_LEFT_DIR_GPIO, OUTPUT);
    pinMode(MOTOR_RIGHT_PWM_GPIO, OUTPUT);
    pinMode(MOTOR_RIGHT_DIR_GPIO, OUTPUT);
    pinMode(MOTOR_STBY_GPIO, OUTPUT);

    /* Safe default: standby off, all outputs low. */
    digitalWrite(MOTOR_STBY_GPIO, LOW);
    analogWrite(MOTOR_LEFT_PWM_GPIO, 0);
    analogWrite(MOTOR_RIGHT_PWM_GPIO, 0);

    /* Only enable after successful init. */
    _enabled = true;
    digitalWrite(MOTOR_STBY_GPIO, HIGH);
    stop();
    return true;
}

void MotorDriver::apply(int8_t dir, uint8_t pwm, uint8_t pwm_gpio,
                        uint8_t dir_gpio, bool invert) {
    if (!_enabled) { pwm = 0; }
    bool forward = (dir > 0);
    if (invert) forward = !forward;
    digitalWrite(dir_gpio, forward ? HIGH : LOW);
    analogWrite(pwm_gpio, (dir == 0) ? 0 : pwm);
}

void MotorDriver::set_motors(int8_t left_dir, uint8_t left_pwm,
                             int8_t right_dir, uint8_t right_pwm) {
    /* uint8_t is 0..255; it can never exceed MAX_PWM(255). */
    _last_left_dir = left_dir;
    _last_right_dir = right_dir;
    _last_left_pwm = (left_dir == 0) ? 0 : left_pwm;
    _last_right_pwm = (right_dir == 0) ? 0 : right_pwm;
    apply(left_dir, left_pwm, MOTOR_LEFT_PWM_GPIO, MOTOR_LEFT_DIR_GPIO,
          MOTOR_LEFT_INVERT != 0);
    apply(right_dir, right_pwm, MOTOR_RIGHT_PWM_GPIO, MOTOR_RIGHT_DIR_GPIO,
          MOTOR_RIGHT_INVERT != 0);
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
    digitalWrite(MOTOR_STBY_GPIO, LOW);
    analogWrite(MOTOR_LEFT_PWM_GPIO, 0);
    analogWrite(MOTOR_RIGHT_PWM_GPIO, 0);
    digitalWrite(MOTOR_LEFT_DIR_GPIO, LOW);
    digitalWrite(MOTOR_RIGHT_DIR_GPIO, LOW);
    _last_left_dir = _last_right_dir = 0;
    _last_left_pwm = _last_right_pwm = 0;
}
