#include "link.h"
#include "config.h"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <soc/uart_reg.h>
#include <soc/soc.h>

static HardwareSerial LinkSerial(1);
static rb_framer_t g_fr;
static rb_framer_t g_fr_usb;
static void handle_line(const char* line, uint32_t now_ms);
static rb_seq_gate_t g_rx_gate;
static rb_err_t g_err;
static uint32_t g_tx_seq;
static uint32_t g_last_rx_ms;
static bool g_have_rx;
static uint32_t g_rx_valid;
static rb_state_t g_state;
static bool g_have_state;
static rb_hb_main_t g_mhb;
static bool g_have_mhb;
static int g_cam_ok = 1;
static int g_ai_ok = 1;
static float g_temp = 0;
static int g_th = 0;
static uint32_t g_last_hb_tx;

static void tx_line(const char* buf, int n) {
    if (n <= 0) {
        Serial.println("[link] tx build failed");
        return;
    }
    LinkSerial.write((const uint8_t*)buf, (size_t)n);
    LinkSerial.write('\n');
#if LINK_USB_MIRROR
    Serial.write((const uint8_t*)buf, (size_t)n);
    Serial.write('\n');
#endif
}

void link_init(void) {
    rb_framer_init(&g_fr);
    rb_framer_init(&g_fr_usb);
    rb_seq_gate_init(&g_rx_gate);
    memset(&g_err, 0, sizeof(g_err));
    g_tx_seq = 0;
    g_last_rx_ms = 0;
    g_have_rx = false;
    g_rx_valid = 0;
    g_have_state = false;
    g_have_mhb = false;
    g_last_hb_tx = 0;
    LinkSerial.begin(RB_BAUD, SERIAL_8N1, PIN_LINK_RX, PIN_LINK_TX);
    Serial.printf("[link] uart up TX=%d RX=%d baud=%d\n",
                  PIN_LINK_TX, PIN_LINK_RX, RB_BAUD);
#if LINK_UART1_LOOPBACK_TEST
    SET_PERI_REG_MASK(UART_CONF0_REG(1), UART_LOOPBACK);
    const char* probe =
        "{\"v\":1,\"type\":\"hb\",\"ms\":0,\"state\":\"LB_TEST\","
        "\"estop\":0,\"seq\":9001}\n";
    LinkSerial.print(probe);
    uint32_t t0 = millis();
    while (millis() - t0 < 250) {
        while (LinkSerial.available()) {
            char c = (char)LinkSerial.read();
            int r = rb_framer_feed(&g_fr, c);
            if (r == 1) handle_line(g_fr.buf, millis());
        }
    }
    CLEAR_PERI_REG_MASK(UART_CONF0_REG(1), UART_LOOPBACK);
    if (g_rx_valid > 0) {
        Serial.println("[lb] UART1 loopback PASS (TX->RX peripheral ok)");
    } else {
        Serial.println("[lb] UART1 loopback FAIL (TX->RX)");
    }
    g_rx_valid = 0;
    g_have_rx = false;
    g_last_rx_ms = 0;
    g_have_state = false;
    g_have_mhb = false;
    rb_seq_gate_init(&g_rx_gate);
#endif
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
        rb_hb_main_t m;
        st = rb_parse_hb_main(line, &m, &g_rx_gate);
        if (st == RB_OK) {
            g_mhb = m;
            g_have_mhb = true;
            g_last_rx_ms = now_ms;
            g_have_rx = true;
            g_rx_valid++;
        }
    } else if (t == RB_MSG_STATE) {
        rb_state_t m;
        st = rb_parse_state(line, &m, &g_rx_gate);
        if (st == RB_OK) {
            g_state = m;
            g_have_state = true;
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
        int r = rb_framer_feed(&g_fr, c);
        if (r == -1) {
            g_err.oversize++;
            continue;
        }
        if (r == 1) handle_line(g_fr.buf, now_ms);
    }
#if LINK_USB_MIRROR
    while (Serial.available()) {
        char c = (char)Serial.read();
        int r = rb_framer_feed(&g_fr_usb, c);
        if (r == -1) {
            g_err.oversize++;
            continue;
        }
        if (r == 1) handle_line(g_fr_usb.buf, now_ms);
    }
#endif

    if (g_last_hb_tx == 0 || now_ms - g_last_hb_tx >= 1000) {
        g_last_hb_tx = now_ms;
        rb_hb_cam_t hb = {};
        hb.ms = now_ms;
        hb.cam = g_cam_ok;
        hb.ai = g_ai_ok;
        hb.temp = g_temp;
        hb.th = g_th;
        hb.seq = g_tx_seq++;
        char buf[RB_MAX_LINE];
        tx_line(buf, rb_build_hb_cam(buf, sizeof(buf), &hb));
    }
}

bool link_up(uint32_t now_ms) {
    return g_have_rx && (now_ms - g_last_rx_ms) < RB_CAM_LINK_TIMEOUT_MS;
}

void link_set_status(int cam_ok, int ai_ok, float temp, int th) {
    g_cam_ok = cam_ok;
    g_ai_ok = ai_ok;
    g_temp = temp;
    g_th = th;
}

bool link_send_det(int cls, float conf, int dir, bool detected) {
    rb_det_t d = {};
    d.cls = cls;
    d.conf = conf;
    d.dir = dir;
    d.det = detected;
    d.seq = g_tx_seq++;
    char buf[RB_MAX_LINE];
    int n = rb_build_det(buf, sizeof(buf), &d);
    if (n <= 0) return false;
    tx_line(buf, n);
    return true;
}

bool link_send_fault(const char* msg) {
    rb_fault_t f = {};
    f.cam = g_cam_ok;
    f.ai = g_ai_ok;
    snprintf(f.msg, sizeof(f.msg), "%s", msg ? msg : "fault");
    for (char* p = f.msg; *p; p++)
        if (*p < 'a' || *p > 'z')
            if (!(*p >= '0' && *p <= '9')) *p = '_';
    f.seq = g_tx_seq++;
    char buf[RB_MAX_LINE];
    int n = rb_build_fault(buf, sizeof(buf), &f);
    if (n <= 0) return false;
    tx_line(buf, n);
    return true;
}

bool link_send_cmd(const char* action) {
    rb_cmd_t c = {};
    snprintf(c.action, sizeof(c.action), "%s", action);
    c.seq = g_tx_seq++;
    char buf[RB_MAX_LINE];
    int n = rb_build_cmd(buf, sizeof(buf), &c);
    if (n <= 0) return false;
    tx_line(buf, n);
    return true;
}

bool link_get_state(rb_state_t* out) {
    if (!out || !g_have_state) return false;
    *out = g_state;
    return true;
}

bool link_get_main_hb(rb_hb_main_t* out) {
    if (!out || !g_have_mhb) return false;
    *out = g_mhb;
    return true;
}

uint32_t link_rx_valid(void) { return g_rx_valid; }
const rb_err_t* link_errors(void) { return &g_err; }

const char* link_boat_state(void) {
    if (!link_up(millis())) return "NO_LINK";
    if (g_have_state && g_state.st[0]) return g_state.st;
    if (g_have_mhb && g_mhb.state[0]) return g_mhb.state;
    return "NO_LINK";
}

bool link_main_autoflag(void) {
    if (!link_up(millis())) return false;
    const char* s = link_boat_state();
    return !strcmp(s, "SEARCH") || !strcmp(s, "DETECT") ||
           !strcmp(s, "ALIGN") || !strcmp(s, "APPROACH") ||
           !strcmp(s, "COLLECT") || !strcmp(s, "VERIFY_COLLECTION");
}
