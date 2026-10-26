#include "link.h"
#include "config_main.h"
#include <Arduino.h>
#include <HardwareSerial.h>

static HardwareSerial LinkSerial(1);
static rb_framer_t g_fr;
static rb_framer_t g_fr_usb;
static rb_seq_gate_t g_rx_gate;
static rb_err_t g_err;
static uint32_t g_tx_seq;
static uint32_t g_last_rx_ms;
static bool g_have_rx;
static uint32_t g_rx_valid;
static rb_det_t g_det;
static bool g_have_det;
static uint32_t g_det_rx_ms;
static int g_cam_flag;
static int g_ai_flag;
static char g_cam_fault[RB_FAULTMSG_MAX];
static char g_hb_state[RB_TEXT_MAX];
static int g_hb_estop;
static bool g_hb_dirty;
static rb_state_t g_state;
static bool g_state_dirty;
static uint32_t g_last_hb_tx;
static uint32_t g_last_state_tx;
static rb_cmd_t g_cmd;
static bool g_cmd_pending;

void link_init(void) {
    rb_framer_init(&g_fr);
    rb_framer_init(&g_fr_usb);
    rb_seq_gate_init(&g_rx_gate);
    memset(&g_err, 0, sizeof(g_err));
    g_tx_seq = 0;
    g_last_rx_ms = 0;
    g_have_rx = false;
    g_rx_valid = 0;
    g_have_det = false;
    g_det_rx_ms = 0;
    g_cam_flag = 0;
    g_ai_flag = 0;
    g_cam_fault[0] = '\0';
    g_hb_state[0] = '\0';
    g_hb_estop = 0;
    g_hb_dirty = false;
    g_state_dirty = false;
    g_last_hb_tx = 0;
    g_last_state_tx = 0;
    g_cmd_pending = false;
    LinkSerial.begin(RB_BAUD, SERIAL_8N1, PIN_LINK_RX, PIN_LINK_TX);
    Serial.printf("[link] uart up TX=%d RX=%d baud=%d\n",
                  PIN_LINK_TX, PIN_LINK_RX, RB_BAUD);
}

static void count_status(rb_status_t st) {
    switch (st) {
        case RB_ERR_JSON:    g_err.bad_json++; break;
        case RB_ERR_VERSION: g_err.bad_version++; break;
        case RB_ERR_TYPE:    g_err.unknown_type++; break;
        case RB_ERR_FIELD:   g_err.bad_field++; break;
        case RB_ERR_DUP:     g_err.dup_seq++; break;
        default: break;
    }
}

static void handle_line(const char* line, uint32_t now_ms) {
    rb_msg_type_t t = rb_msg_type(line);
    if (t == RB_MSG_NONE) {
        g_err.bad_json++;
        return;
    }
    rb_status_t st = RB_ERR_TYPE;
    if (t == RB_MSG_HB) {
        rb_hb_cam_t m;
        st = rb_parse_hb_cam(line, &m, &g_rx_gate);
        if (st == RB_OK) {
            g_cam_flag = m.cam;
            g_ai_flag = m.ai;
            g_last_rx_ms = now_ms;
            g_have_rx = true;
            g_rx_valid++;
        }
    } else if (t == RB_MSG_DET) {
        rb_det_t m;
        st = rb_parse_det(line, &m, &g_rx_gate);
        if (st == RB_OK) {
            g_det = m;
            g_have_det = true;
            g_det_rx_ms = now_ms;
            g_last_rx_ms = now_ms;
            g_have_rx = true;
            g_rx_valid++;
        }
    } else if (t == RB_MSG_FAULT) {
        rb_fault_t m;
        st = rb_parse_fault(line, &m, &g_rx_gate);
        if (st == RB_OK) {
            snprintf(g_cam_fault, sizeof(g_cam_fault), "%s", m.msg);
            g_cam_flag = m.cam;
            g_ai_flag = m.ai;
            g_last_rx_ms = now_ms;
            g_have_rx = true;
            g_rx_valid++;
        }
    } else if (t == RB_MSG_CMD) {
        rb_cmd_t m;
        st = rb_parse_cmd(line, &m, &g_rx_gate);
        if (st == RB_OK) {
            g_cmd = m;
            g_cmd_pending = true;
            g_last_rx_ms = now_ms;
            g_have_rx = true;
            g_rx_valid++;
        }
    }
    count_status(st);
}

void link_loop(uint32_t now_ms) {
    while (LinkSerial.available()) {
        char c = (char)LinkSerial.read();
#if LINK_RAW_SNIFF
        Serial.write(c);
#endif
        int r = rb_framer_feed(&g_fr, c);
        if (r == -1) {
            g_err.oversize++;
            continue;
        }
        if (r != 1) continue;
        handle_line(g_fr.buf, now_ms);
    }
#if LINK_USB_MIRROR
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r') continue;
        int r = rb_framer_feed(&g_fr_usb, c);
        if (r == -1) continue;
        if (r != 1) continue;
        handle_line(g_fr_usb.buf, now_ms);
    }
#endif

    if (g_last_hb_tx == 0 || now_ms - g_last_hb_tx >= 1000) {
        g_last_hb_tx = now_ms;
        g_hb_dirty = true;
    }
    if (g_hb_dirty) {
        g_hb_dirty = false;
        rb_hb_main_t hb = {};
        hb.ms = now_ms;
        snprintf(hb.state, sizeof(hb.state), "%s",
                 g_hb_state[0] ? g_hb_state : "BOOT");
        hb.estop = g_hb_estop;
        hb.seq = g_tx_seq++;
        char buf[RB_MAX_LINE];
        int n = rb_build_hb_main(buf, sizeof(buf), &hb);
        if (n > 0) {
            LinkSerial.write((const uint8_t*)buf, (size_t)n);
            LinkSerial.write('\n');
#if LINK_USB_MIRROR
            Serial.write((const uint8_t*)buf, (size_t)n);
            Serial.write('\n');
#endif
        }
    }
    if (g_last_state_tx == 0 || now_ms - g_last_state_tx >= MAIN_STATE_PERIOD_MS) {
        g_last_state_tx = now_ms;
        g_state_dirty = true;
    }
    if (g_state_dirty && g_state.st[0]) {
        g_state_dirty = false;
        g_state.seq = g_tx_seq++;
        char buf[RB_MAX_LINE];
        int n = rb_build_state(buf, sizeof(buf), &g_state);
        if (n > 0) {
            LinkSerial.write((const uint8_t*)buf, (size_t)n);
            LinkSerial.write('\n');
#if LINK_USB_MIRROR
            Serial.write((const uint8_t*)buf, (size_t)n);
            Serial.write('\n');
#endif
#if LINK_RAW_SNIFF
            static uint32_t stx_log;
            if (now_ms - stx_log >= 2000) {
                stx_log = now_ms;
                Serial.printf("[stx] ok n=%d\n", n);
            }
#endif
        } else {
#if LINK_RAW_SNIFF
            Serial.printf("[stx] BUILD_FAIL n=%d st='%s' fill=%.1f sats=%d conv=%d\n",
                          n, g_state.st, (double)g_state.fill,
                          g_state.sats, g_state.conv);
#endif
        }
    } else if (g_state_dirty) {
#if LINK_RAW_SNIFF
        static uint32_t st_empty_log;
        if (now_ms - st_empty_log >= 2000) {
            st_empty_log = now_ms;
            Serial.println("[stx] st-empty");
        }
#endif
    }
}

bool link_up(uint32_t now_ms) {
    return g_have_rx && (now_ms - g_last_rx_ms) < RB_LINK_TIMEOUT_MS;
}

bool link_det_fresh(uint32_t now_ms) {
    return g_have_det && link_up(now_ms) &&
           (now_ms - g_det_rx_ms) < RB_DET_STALE_MS;
}

bool link_get_det(rb_det_t* out) {
    if (!out || !g_have_det) return false;
    *out = g_det;
    return true;
}

bool link_cam_flag(void) { return g_cam_flag != 0; }
bool link_ai_flag(void) { return g_ai_flag != 0; }
const char* link_cam_fault_msg(void) { return g_cam_fault; }

void link_set_hb(const char* state, int estop) {
    snprintf(g_hb_state, sizeof(g_hb_state), "%s", state ? state : "?");
    g_hb_estop = estop;
}

void link_set_state(const rb_state_t* s) {
    if (!s) return;
    g_state = *s;
}

uint32_t link_rx_valid(void) { return g_rx_valid; }
const rb_err_t* link_errors(void) { return &g_err; }

bool link_take_cmd(rb_cmd_t* out) {
    if (!out || !g_cmd_pending) return false;
    *out = g_cmd;
    g_cmd_pending = false;
    return true;
}
