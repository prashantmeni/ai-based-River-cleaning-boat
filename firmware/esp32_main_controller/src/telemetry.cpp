#include "telemetry.h"
#include "navigation.h"
#include "motor.h"
#include "obstacle.h"
#include "gps.h"
#include "serial_rx.h"
#include "conveyor.h"
#include "safety.h"
#include "tds.h"

Telemetry::Telemetry() : _nav(nullptr), _motors(nullptr), _obs(nullptr),
                         _gps(nullptr), _rx(nullptr), _conv(nullptr),
                         _safety(nullptr), _tds(nullptr) {}

void Telemetry::attach(Navigation *nav, MotorDriver *motors,
                       ObstacleSensors *obs, GPS *gps, SerialRx *rx,
                       Conveyor *conv, Safety *safety, TdsSensor *tds) {
    _nav = nav; _motors = motors; _obs = obs; _gps = gps;
    _rx = rx; _conv = conv; _safety = safety; _tds = tds;
}

int Telemetry::build_status_frame(uint8_t *out, size_t out_size,
                                  uint16_t *sequence) {
    (void)sequence;
    /* Output clean JSON telemetry line for the dashboard bridge over USB */
    const char *state_str = _nav ? _nav->state_name() : "UNKNOWN";
    bool comms = _rx && _rx->comms_alive();
    uint32_t rx_bytes = _rx ? _rx->rx_bytes() : 0;
    uint32_t rx_errors = _rx ? _rx->error_count() : 0;
    uint8_t l_pwm = _motors ? _motors->last_left_pwm() : 0;
    uint8_t r_pwm = _motors ? _motors->last_right_pwm() : 0;
    int8_t l_dir = _motors ? _motors->last_left_dir() : 0;
    int8_t r_dir = _motors ? _motors->last_right_dir() : 0;
    const char *conv_str = "OFF";
    if (_conv) {
        switch (_conv->state()) {
            case CONVEYOR_OFF: conv_str = "OFF"; break;
            case CONVEYOR_STARTING: conv_str = "STARTING"; break;
            case CONVEYOR_RUNNING: conv_str = "RUNNING"; break;
            case CONVEYOR_STOPPING: conv_str = "STOPPING"; break;
            case CONVEYOR_FAULT: conv_str = "FAULT"; break;
        }
    }
    uint8_t faults = _safety ? _safety->fault_flags() : 0;
    uint16_t coll = _conv ? _conv->collection_count() : 0;
    uint16_t bottles = _nav ? _nav->waste_count(1) : 0;
    uint16_t bags = _nav ? _nav->waste_count(2) : 0;
    uint16_t debris = _nav ? _nav->waste_count(3) : 0;
    uint32_t up = millis();
    float l_cm = (_obs && _obs->left_valid()) ? _obs->left_cm() : -1.0f;
    float r_cm = (_obs && _obs->right_valid()) ? _obs->right_cm() : -1.0f;
    bool gps_fix = _gps ? _gps->has_fix() : false;
    double lat = (_gps && gps_fix) ? _gps->latitude() : 0.0;
    double lon = (_gps && gps_fix) ? _gps->longitude() : 0.0;
    bool det = _nav ? _nav->have_detection() : false;
    uint8_t conf = _nav ? _nav->detection_confidence() : 0;
    uint8_t cid = _nav ? _nav->detection_class() : 0;
    int16_t cx = _nav ? _nav->detection_center_x() : 0;
    bool estop = _safety ? _safety->in_emergency() : false;
    bool auto_on = _nav ? _nav->autonomous() : false;
    uint8_t sats = _gps ? _gps->satellites() : 0;
    uint32_t gps_age = _gps ? _gps->fix_age_ms() : 0;
    int16_t tds_ppm = _tds ? _tds->ppm() : -1;   /* -1 = invalid/unavailable */
    uint16_t tds_mv = _tds ? _tds->millivolts() : 0;
    bool bin_full = (l_cm > 0.0f && l_cm < BIN_FULL_THRESHOLD_CM);
    float bin_dist = l_cm;
    char dbg[41] = {0};
    for (uint8_t i = 0; i < (_rx ? _rx->dbg_n() : 0) && i < 20; i++) {
        snprintf(dbg + (2 * i), 3, "%02X", _rx->dbg()[i]);
    }

    int n = snprintf((char *)out, out_size,
        "{\"type\":\"telemetry\",\"protocol_version\":1,\"device_id\":\"riverclean_main\","
        "\"state\":\"%s\",\"comms_ok\":%s,\"estop\":%s,\"autonomous\":%s,"
        "\"detection\":%s,\"confidence\":%d,\"class_id\":%d,\"center_x\":%d,"
        "\"left_cm\":%.1f,\"right_cm\":%.1f,"
        "\"bin_full\":%s,\"bin_distance_cm\":%.1f,"
        "\"gps_fix\":%s,\"latitude\":%.6f,\"longitude\":%.6f,"
        "\"satellites\":%u,\"gps_age_ms\":%lu,"
        "\"tds_ppm\":%d,\"tds_mv\":%u,"
        "\"collection_count\":%u,\"conveyor\":\"%s\",\"fault_flags\":%u,\"uptime_ms\":%lu,"
        "\"bottle_count\":%u,\"bag_count\":%u,\"debris_count\":%u,"
        "\"left_pwm\":%u,\"right_pwm\":%u,\"left_dir\":%d,\"right_dir\":%d,"
        "\"rx_bytes\":%lu,\"rx_errors\":%lu,\"perr\":%u,\"drops\":%lu,\"dbg\":\"%s\"}\n",
        state_str, comms ? "true" : "false", estop ? "true" : "false",
        auto_on ? "true" : "false",
        det ? "true" : "false", conf, cid, cx,
        l_cm, r_cm, bin_full ? "true" : "false", bin_dist,
        gps_fix ? "true" : "false", lat, lon,
        sats, (unsigned long)gps_age,
        (int)tds_ppm, (unsigned)tds_mv,
        coll, conv_str, faults, (unsigned long)up,
        bottles, bags, debris,
        l_pwm, r_pwm, l_dir, r_dir,
        (unsigned long)rx_bytes, (unsigned long)rx_errors,
        _rx ? _rx->last_err() : 0,
        _rx ? (unsigned long)_rx->drops() : 0UL, dbg
    );

    return (n > 0 && (size_t)n < out_size) ? n : -1;
}

void Telemetry::debug_print() const {
    // Handled by JSON output in build_status_frame
}
