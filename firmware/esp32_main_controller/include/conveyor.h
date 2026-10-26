#ifndef CONVEYOR_H
#define CONVEYOR_H

#include <Arduino.h>
#include "boat_config.h"

/* Conveyor control state machine with timeout protection.
 * OFF -> STARTING -> RUNNING -> STOPPING -> (OFF | FAULT)
 */
typedef enum {
    CONVEYOR_OFF        = 0,
    CONVEYOR_STARTING   = 1,
    CONVEYOR_RUNNING    = 2,
    CONVEYOR_STOPPING   = 3,
    CONVEYOR_FAULT      = 4,
} conveyor_state_t;

class Conveyor {
public:
    Conveyor();
    ~Conveyor();

    bool begin();

    /* Start a collection cycle (runs for CONVEYOR_RUN_MS then stops). */
    void start_collection();

    /* Stop immediately. */
    void stop();

    /* Call regularly. Manages state transitions and timeout. */
    void update();

    conveyor_state_t state() const { return _state; }
    bool running() const { return _state == CONVEYOR_RUNNING; }
    bool fault() const { return _state == CONVEYOR_FAULT; }
    uint16_t collection_count() const { return _collection_count; }

private:
    void set_pwm(uint8_t pwm);
    conveyor_state_t _state;
    uint32_t _start_ms;
    uint16_t _collection_count;
    bool _pwm_enabled;
};

#endif /* CONVEYOR_H */
