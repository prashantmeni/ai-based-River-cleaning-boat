#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>
#include "boat_config.h"

/* Motor abstraction: navigation code calls move()/stop(); it does not
 * reference GPIO numbers directly. */
class MotorDriver {
public:
    MotorDriver();
    ~MotorDriver();

    /* Initialise driver pins + standby. Returns true on success. */
    bool begin();

    /* Set left/right motor. pwm 0..255, dir -1=reverse 0=stop 1=forward.
     * Applies direction inversion from config. Safe: clamping included. */
    void set_motors(int8_t left_dir, uint8_t left_pwm,
                    int8_t right_dir, uint8_t right_pwm);

    /* Convenience motions. */
    void forward(uint8_t pwm);
    void backward(uint8_t pwm);
    void turn_left(uint8_t pwm);
    void turn_right(uint8_t pwm);
    void stop();

    /* Emergency disable: outputs off, standby off. */
    void disable();

    bool enabled() const { return _enabled; }

    /* Last commanded values (for telemetry). */
    int8_t  last_left_dir()  const { return _last_left_dir; }
    uint8_t last_left_pwm()  const { return _last_left_pwm; }
    int8_t  last_right_dir() const { return _last_right_dir; }
    uint8_t last_right_pwm() const { return _last_right_pwm; }

private:
    void apply(int8_t dir, uint8_t pwm, uint8_t pwm_gpio, uint8_t dir_gpio,
               bool invert);
    bool _enabled;
    int8_t _last_left_dir, _last_right_dir;
    uint8_t _last_left_pwm, _last_right_pwm;
};

#endif /* MOTOR_H */
