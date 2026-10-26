#include <Arduino.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include "config_main.h"
#include "link.h"
#include "state_machine.h"
#include "motors.h"
#include "conveyor.h"
#include "ultrasonic.h"
#include "gps.h"
#include "basket.h"
#include "buzzer.h"

static bool auto_active = false;
static bool estop_latched = false;
static bool sonar_fault = false;
static bool was_link_up = false;
static uint32_t sonar_invalid_since = 0;
static float obs_l = -1, obs_r = -1;
static int stats_att = 0;
static int stats_cfm = 0;
static boat_state_t prev_state = ST_IDLE;
static float fill_at_collect = -1;
static char fault_str[RB_FAULTMSG_MAX] = "";

static uint32_t t_sens = 0, t_gps = 0, t_hb = 0;

static const char* basket_name(basket_state_t b) {
    switch (b) {
        case BASKET_EMPTY: return "EMPTY";
        case BASKET_FILLING: return "FILLING";
        case BASKET_NEAR_FULL: return "NEAR_FULL";
        case BASKET_FULL: return "FULL";
        default: return "SENSOR_FAULT";
    }
}

static void apply_manual(int dir) {
    auto_active = false;
    switch (dir) {
        case 1: motors_set(SEARCH_PWM, SEARCH_PWM); break;
        case 2: motors_set(-SEARCH_PWM, -SEARCH_PWM); break;
        case 3: motors_set(-TURN_PWM, TURN_PWM); break;
        case 4: motors_set(TURN_PWM, -TURN_PWM); break;
        default: motors_stop(); break;
    }
}

static void handle_cmds(void) {
    rb_cmd_t c;
    while (link_take_cmd(&c)) {
        if (!strcmp(c.action, "auto_on")) {
            auto_active = true;
            Serial.println("[cmd] auto_on");
        } else if (!strcmp(c.action, "auto_off")) {
            auto_active = false;
            motors_stop();
            conveyor_stop();
            Serial.println("[cmd] auto_off");
        } else if (!strcmp(c.action, "fwd")) apply_manual(1);
        else if (!strcmp(c.action, "rev")) apply_manual(2);
        else if (!strcmp(c.action, "left")) apply_manual(3);
        else if (!strcmp(c.action, "right")) apply_manual(4);
        else if (!strcmp(c.action, "stopm")) apply_manual(0);
        else if (!strcmp(c.action, "conveyor")) {
            if (conveyor_is_running()) conveyor_stop();
            else conveyor_start();
        } else if (!strcmp(c.action, "reset")) {
            stats_att = 0;
            stats_cfm = 0;
            fill_at_collect = -1;
            Serial.println("[cmd] counters reset");
        } else if (!strcmp(c.action, "clear")) {
            estop_latched = false;
            fault_str[0] = '\0';
            sm_clear_fault();
            Serial.println("[cmd] fault cleared");
        }
    }
}

static void poll_estop(void) {
    if (PIN_ESTOP < 0) return;
    if (digitalRead(PIN_ESTOP) == LOW && !estop_latched) {
        estop_latched = true;
        sm_emergency_stop();
        buzzer_beep(300);
        Serial.println("[estop] engaged");
    }
}

static void poll_battery(void) {
#if BATTERY_SENSE_ENABLED
    static uint32_t t_bat = 0;
    uint32_t now = millis();
    if (now - t_bat < 2000) return;
    t_bat = now;
    uint32_t mv = analogReadMilliVolts(PIN_BATTERY_ADC);
    float v = (mv / 1000.0f) * BATTERY_DIVIDER_RATIO;
    if (v < BATTERY_LOW_V && v > 1.0f) {
        Serial.printf("[batt] LOW %.2fV -> safe stop\n", v);
        auto_active = false;
        motors_stop();
        conveyor_stop();
        snprintf(fault_str, sizeof(fault_str), "BATTERY_LOW");
    }
#else
    (void)0;
#endif
}

static void update_fault_str(bool link_ok) {
    if (estop_latched) {
        snprintf(fault_str, sizeof(fault_str), "ESTOP");
        return;
    }
    if (!link_ok) {
        snprintf(fault_str, sizeof(fault_str), "LINK_LOST");
        return;
    }
    if (!link_cam_flag()) {
        const char* cf = link_cam_fault_msg();
        snprintf(fault_str, sizeof(fault_str), "%s",
                 (cf && cf[0]) ? cf : "CAM_AI");
        return;
    }
    if (sonar_fault && auto_active) {
        snprintf(fault_str, sizeof(fault_str), "SONAR");
        return;
    }
    if (sm_state() == ST_FAULT_STOP) {
        snprintf(fault_str, sizeof(fault_str), "FAULT");
        return;
    }
    fault_str[0] = '\0';
}

static void publish(bool link_ok, bool det_fresh) {
    rb_state_t s = {};
    snprintf(s.st, sizeof(s.st), "%s", sm_state_name(sm_state()));
    snprintf(s.basket, sizeof(s.basket), "%s", basket_name(basket_state()));
    s.fill = basket_fill_percent();
    s.obs_l = obs_l;
    s.obs_r = obs_r;
    s.lat = gps_get()->lat;
    s.lon = gps_get()->lon;
    s.fix = gps_get()->has_fix ? 1 : 0;
    s.sats = (int)gps_get()->satellites;
    s.conv = (int)conveyor_state();
    s.att = stats_att;
    s.cfm = stats_cfm;
    snprintf(s.fault, sizeof(s.fault), "%s", fault_str);
    (void)link_ok;
    (void)det_fresh;
    link_set_state(&s);
    link_set_hb(sm_state_name(sm_state()), estop_latched ? 1 : 0);
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.printf("\n[boot] reset_reason=%d free_heap=%u role=MAIN_CTRL\n",
                  (int)esp_reset_reason(), (unsigned)ESP.getFreeHeap());
    uint32_t t0 = millis();

    esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
    esp_task_wdt_add(NULL);

    link_init();      Serial.printf("[init] link   %ums\n", (unsigned)(millis()-t0));
    sm_init();        Serial.printf("[init] sm     %ums\n", (unsigned)(millis()-t0));
    motors_init();     Serial.printf("[init] motors %ums\n", (unsigned)(millis()-t0));
    conveyor_init();   Serial.printf("[init] conv   %ums\n", (unsigned)(millis()-t0));
    ultrasonic_init(); Serial.printf("[init] us     %ums\n", (unsigned)(millis()-t0));
    gps_init();        Serial.printf("[init] gps    %ums\n", (unsigned)(millis()-t0));
    basket_init();     Serial.printf("[init] basket %ums\n", (unsigned)(millis()-t0));
    buzzer_init();     Serial.printf("[init] buzzer %ums\n", (unsigned)(millis()-t0));
    if (PIN_ESTOP >= 0) {
        pinMode(PIN_ESTOP, INPUT_PULLUP);
        Serial.println("[init] estop input armed");
    }

    Serial.println("[selftest] main controller ready; awaiting CAM link");
}

void loop() {
    uint32_t now = millis();
    esp_task_wdt_reset();
    buzzer_update(now);

    handle_cmds();
    poll_estop();
    poll_battery();

    // ---- sensors ----
    if (now - t_sens >= SENSOR_PERIOD_MS) {
        t_sens = now;
        us_reading_t r1 = {-1, false};
        us_reading_t r2 = {-1, false};
        us_reading_t r3 = {-1, false};
#if US1_ENABLED
        r1 = ultrasonic_read_filtered(PIN_US1_TRIG, PIN_US1_ECHO, 3);
#endif
#if US2_ENABLED
        r2 = ultrasonic_read_filtered(PIN_US2_TRIG, PIN_US2_ECHO, 3);
#endif
#if US3_ENABLED
        r3 = ultrasonic_read_filtered(PIN_US3_TRIG, PIN_US3_ECHO, 3);
#endif
        obs_l = r1.valid ? r1.cm : -1;
        obs_r = r2.valid ? r2.cm : -1;
        basket_update(r3);

        bool any = r1.valid || r2.valid;
        if (any) {
            sonar_fault = false;
            sonar_invalid_since = 0;
        } else {
            if (sonar_invalid_since == 0) sonar_invalid_since = now;
            else if (now - sonar_invalid_since >= SONAR_INVALID_FAULT_MS)
                sonar_fault = true;
        }
    }

    // ---- GPS ----
    if (now - t_gps >= GPS_PERIOD_MS) {
        t_gps = now;
        gps_update(now);
    }

    // ---- link ----
    bool link_ok_prev = link_up(now);
    rb_det_t det = {};
    bool det_fresh = link_get_det(&det) && link_det_fresh(now);
    handle_cmds(); // pick up cmds that arrived mid-pump (link_loop called below)
    link_loop(now);
    bool link_ok = link_up(now);
    if (link_ok && !link_ok_prev && sm_state() == ST_FAULT_STOP &&
        !estop_latched) {
        sm_clear_fault();
        Serial.println("[link] recovered -> IDLE");
    }
    if (!link_ok) {
        motors_stop();
        conveyor_stop();
    }
    handle_cmds();

    update_fault_str(link_ok);

    // ---- state machine (auto mode) ----
    if (auto_active && link_ok && !estop_latched) {
        bool cam_ok = link_cam_flag();
        bool ai_ok = link_cam_flag() && link_ai_flag();
        int bs = (int)basket_state();
        if (det_fresh) {
            sm_update(now, cam_ok, ai_ok, det.cls, det.conf,
                      (target_dir_t)det.dir, obs_l, obs_r, bs);
        } else {
            sm_update(now, cam_ok, ai_ok, -1, 0, DIR_CENTER,
                      obs_l, obs_r, bs);
        }
        boat_state_t cur = sm_state();
        if (cur == ST_COLLECT && prev_state != ST_COLLECT) {
            stats_att++;
            fill_at_collect = basket_fill_percent();
        }
        if (cur == ST_VERIFY_COLLECTION &&
            prev_state != ST_VERIFY_COLLECTION) {
            float delta = basket_fill_percent() - fill_at_collect;
            if (fill_at_collect >= 0 && delta >= CONFIRM_FILL_DELTA_PCT)
                stats_cfm++;
            fill_at_collect = -1;
        }
        prev_state = cur;

        if (sonar_fault) motors_stop();
    } else if (!auto_active) {
        if (!link_ok) { /* already stopped above */ }
    }

    // ---- conveyor bookkeeping ----
    conveyor_update(now);

    // ---- publish to CAM ----
    publish(link_ok, det_fresh);

    // ---- heartbeat to serial ----
    if (now - t_hb >= 1000) {
        t_hb = now;
        const rb_err_t* e = link_errors();
        Serial.printf("[hb] up=%us state=%s auto=%d link=%d det=%d sonar=%d "
                      "estop=%d att=%d cfm=%d gps=%d obs=%.0f/%.0f "
                      "rx=%lu e(json=%lu ver=%lu typ=%lu fld=%lu os=%lu dup=%lu) "
                      "heap=%u\n",
            (unsigned)(now / 1000), sm_state_name(sm_state()),
            (int)auto_active, (int)link_ok, (int)det_fresh,
            (int)sonar_fault, (int)estop_latched,
            stats_att, stats_cfm, (int)gps_get()->has_fix,
            obs_l, obs_r, (unsigned long)link_rx_valid(),
            (unsigned long)e->bad_json, (unsigned long)e->bad_version,
            (unsigned long)e->unknown_type, (unsigned long)e->bad_field,
            (unsigned long)e->oversize, (unsigned long)e->dup_seq,
            (unsigned)ESP.getFreeHeap());
    }

    delay(CONTROL_LOOP_MS / 2);
}
