#include "state_machine.h"
#include "config_main.h"
#include "motors.h"
#include "conveyor.h"
#include "basket.h"
#include <Arduino.h>

static boat_state_t g_st = ST_BOOT;
static bool g_estop = false;
static bool g_camera_ok = false;

// target persistence
static int g_persist = 0;
static int g_locked_class = -1;
static target_dir_t g_locked_dir = DIR_CENTER;
static uint32_t g_state_since = 0;
static uint32_t g_last_cmd_ms = 0;

// collection attempt bookkeeping (attempt != success)
typedef struct {
    uint32_t t;
    int cls;
    bool attempt;
    bool confirmed;
} collect_event_t;

void sm_init(void) {
    g_st = ST_SELF_TEST;
    g_estop = false;
    g_persist = 0;
    g_state_since = millis();
    motors_stop();
}

static void enter(boat_state_t s) {
    if (g_st != s) {
        g_st = s;
        g_state_since = millis();
    }
}

boat_state_t sm_state(void) { return g_st; }

const char* sm_state_name(boat_state_t s) {
    switch (s) {
        case ST_BOOT: return "BOOT";
        case ST_SELF_TEST: return "SELF_TEST";
        case ST_IDLE: return "IDLE";
        case ST_SEARCH: return "SEARCH";
        case ST_DETECT: return "DETECT";
        case ST_ALIGN: return "ALIGN";
        case ST_APPROACH: return "APPROACH";
        case ST_COLLECT: return "COLLECT";
        case ST_VERIFY_COLLECTION: return "VERIFY_COLLECTION";
        case ST_RETURN_OR_STOP: return "RETURN_OR_STOP";
        case ST_FAULT_STOP: return "FAULT_STOP";
        default: return "?";
    }
}

void sm_emergency_stop(void) {
    g_estop = true;
    motors_stop();
    conveyor_stop();
    enter(ST_FAULT_STOP);
}

void sm_clear_fault(void) {
    g_estop = false;
    enter(ST_IDLE);
}

static bool obstacle_stop(float l, float r) {
    bool a = l > 0 && l < OBSTACLE_STOP_CM;
    bool b = r > 0 && r < OBSTACLE_STOP_CM;
    return a && b; // stop only if both paths blocked (single-side: turn)
}

static bool obstacle_turn(float l, float r, target_dir_t* out) {
    bool left_blocked = l > 0 && l < OBSTACLE_TURN_CM;
    bool right_blocked = r > 0 && r < OBSTACLE_TURN_CM;
    if (left_blocked && right_blocked) return false;
    if (left_blocked && !right_blocked) { *out = DIR_RIGHT; return true; }
    if (right_blocked && !left_blocked) { *out = DIR_LEFT; return true; }
    return false;
}

void sm_update(uint32_t now_ms,
               bool camera_ok, bool ai_ok,
               int ai_class, float ai_conf, target_dir_t dir,
               float obstacle_left_cm, float obstacle_right_cm,
               int basket_state) {
    g_camera_ok = camera_ok;
    g_last_cmd_ms = now_ms;

    if (g_estop) { motors_stop(); return; }

    // global safety interlocks regardless of state
    if (!camera_ok) { enter(ST_FAULT_STOP); motors_stop(); conveyor_stop(); return; }
    if (basket_state == BASKET_FULL) {
        motors_stop();
        conveyor_stop();
        enter(ST_RETURN_OR_STOP);
        return;
    }

    bool want = (ai_class >= 0 && ai_conf >= DETECT_CONF_THRESHOLD);
    if (want) {
        if (ai_class == g_locked_class && dir == g_locked_dir) {
            if (g_persist < DETECT_PERSIST_FRAMES) g_persist++;
        } else {
            g_locked_class = ai_class;
            g_locked_dir = dir;
            g_persist = 1;
        }
    } else if (g_persist > 0) {
        g_persist--;
    }

    bool locked = g_persist >= DETECT_PERSIST_FRAMES;
    uint32_t dt = now_ms - g_state_since;

    switch (g_st) {
    case ST_BOOT:
        enter(ST_SELF_TEST);
        break;

    case ST_SELF_TEST:
        motors_stop();
        if (camera_ok) enter(ST_IDLE);
        break;

    case ST_IDLE:
        motors_stop();
        if (camera_ok && ai_ok) enter(ST_SEARCH);
        break;

    case ST_SEARCH: {
        // wander slowly; respect obstacles
        target_dir_t t;
        if (obstacle_stop(obstacle_left_cm, obstacle_right_cm)) {
            motors_stop();
        } else if (obstacle_turn(obstacle_left_cm, obstacle_right_cm, &t)) {
            motors_set(t == DIR_LEFT ? -TURN_PWM : TURN_PWM,
                       t == DIR_LEFT ? TURN_PWM : -TURN_PWM);
        } else {
            motors_set(SEARCH_PWM, SEARCH_PWM);
        }
        if (locked) enter(ST_DETECT);
        break;
    }

    case ST_DETECT:
        if (!locked) { enter(ST_SEARCH); break; }
        enter(ST_ALIGN);
        break;

    case ST_ALIGN: {
        if (!locked) { enter(ST_SEARCH); break; }
        // steer toward region
        if (g_locked_dir == DIR_LEFT) motors_set(-TURN_PWM, TURN_PWM);
        else if (g_locked_dir == DIR_RIGHT) motors_set(TURN_PWM, -TURN_PWM);
        else motors_set(APPROACH_PWM / 2, APPROACH_PWM / 2);
        if (g_locked_dir == DIR_CENTER || dt > 4000) enter(ST_APPROACH);
        break;
    }

    case ST_APPROACH: {
        if (!locked) { enter(ST_SEARCH); break; }
        target_dir_t t;
        if (obstacle_stop(obstacle_left_cm, obstacle_right_cm)) {
            motors_stop();
            enter(ST_COLLECT); // close enough: attempt intake
            break;
        }
        if (obstacle_turn(obstacle_left_cm, obstacle_right_cm, &t)) {
            motors_set(t == DIR_LEFT ? -TURN_PWM : TURN_PWM,
                       t == DIR_LEFT ? TURN_PWM : -TURN_PWM);
        } else if (g_locked_dir == DIR_LEFT) {
            motors_set(APPROACH_PWM / 2, APPROACH_PWM);
        } else if (g_locked_dir == DIR_RIGHT) {
            motors_set(APPROACH_PWM, APPROACH_PWM / 2);
        } else {
            motors_set(APPROACH_PWM, APPROACH_PWM);
        }
        // conservative max approach duration
        if (dt > 20000) enter(ST_COLLECT);
        // intake geometry: assume collection zone reached when very close
        // for prototype: fixed timer approach -> collect
        if (dt > 6000) enter(ST_COLLECT);
        break;
    }

    case ST_COLLECT:
        motors_stop();
        if (!conveyor_is_running()) {
            // NOTE: this is a collection ATTEMPT, not a confirmed success.
            conveyor_start();
        }
        conveyor_update(now_ms);
        if (!conveyor_is_running()) {
            enter(ST_VERIFY_COLLECTION);
        }
        break;

    case ST_VERIFY_COLLECTION: {
        // Without a dedicated confirmation sensor, this stage marks the
        // event as 'attempt' only. It may be upgraded if basket fill
        // delta indicates success.
        conveyor_stop();
        // reset persistence to avoid instantly re-locking same target
        g_persist = 0;
        g_locked_class = -1;
        enter(ST_SEARCH);
        break;
    }

    case ST_RETURN_OR_STOP:
        motors_stop();
        // waits for basket recovery (FULL cleared) or operator
        if (basket_state != BASKET_FULL) enter(ST_IDLE);
        break;

    case ST_FAULT_STOP:
        motors_stop();
        conveyor_stop();
        break;
    }
}
