#ifndef SAFETY_H
#define SAFETY_H

#include <Arduino.h>
#include "boat_config.h"

/* Fault bitmask. */
enum FaultBit : uint8_t {
    FAULT_COMMS_LOST   = 1 << 0,
    FAULT_OBSTACLE     = 1 << 1,
    FAULT_SENSOR_BAD   = 1 << 2,
    FAULT_GPS_NO_FIX   = 1 << 3,   /* advisory only, not fatal */
    FAULT_WATCHDOG     = 1 << 4,
};

class Safety {
public:
    Safety();
    ~Safety();

    void begin();
    void reset_watchdog();

    /* Emergency stop: latched until clear_fault(). Disables everything. */
    void emergency_stop();
    void clear_fault(uint8_t bits);

    uint8_t fault_flags() const { return _faults; }
    bool in_emergency() const { return _emergency; }

    bool comms_ok()      const { return !(_faults & FAULT_COMMS_LOST); }
    bool obstacle_stop() const { return !(_faults & FAULT_OBSTACLE); }
    bool sensors_ok()    const { return !(_faults & FAULT_SENSOR_BAD); }

    void set_comms_lost(bool lost);
    void set_obstacle_stop(bool stop);
    void set_sensors_bad(bool bad);

private:
    uint8_t _faults;
    bool _emergency;
    uint32_t _wd_last_ms;
};

#endif /* SAFETY_H */
