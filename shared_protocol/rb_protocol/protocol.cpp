#include "protocol.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static const char* const RB_CMD_ALLOW[] = {
    "auto_on", "auto_off", "fwd", "rev", "left", "right",
    "stopm", "conveyor", "reset", "clear"};

static bool allowed_action(const char* a) {
    for (size_t i = 0; i < sizeof(RB_CMD_ALLOW) / sizeof(RB_CMD_ALLOW[0]); i++)
        if (!strcmp(a, RB_CMD_ALLOW[i])) return true;
    return false;
}

const char* const RB_CLASS_NAMES[RB_CLASS_COUNT] = {
    "plastic_bottle",
    "plastic_wrapper",
    "leaf_or_organic_debris",
    "other_visible_waste",
    "background_or_no_waste",
};

void rb_framer_init(rb_framer_t* f) {
    f->len = 0;
    f->dropping = false;
    f->buf[0] = '\0';
}

int rb_framer_feed(rb_framer_t* f, char c) {
    if (c == '\r') return 0;
    if (c == '\n') {
        if (f->dropping) {
            f->dropping = false;
            f->len = 0;
            return 0;
        }
        if (f->len == 0) return 0;
        f->buf[f->len] = '\0';
        f->len = 0;
        return 1;
    }
    if (f->dropping) return 0;
    if (f->len >= RB_MAX_MSG_LEN) {
        f->dropping = true;
        return -1;
    }
    f->buf[f->len++] = c;
    return 0;
}

void rb_seq_gate_init(rb_seq_gate_t* g) {
    memset(g, 0, sizeof(*g));
}

static bool seq_ok(rb_seq_gate_t* g, rb_msg_type_t t, uint32_t seq) {
    if (!g) return true;
    int i = (int)t;
    if (g->have[i] && g->last_seq[i] == seq) return false;
    g->have[i] = true;
    g->last_seq[i] = seq;
    return true;
}

rb_msg_type_t rb_msg_type(const char* line) {
    JsonDocument doc;
    if (deserializeJson(doc, line)) return RB_MSG_NONE;
    if (!doc.is<JsonObjectConst>()) return RB_MSG_NONE;
    const char* t = doc["type"];
    if (!t) return RB_MSG_UNKNOWN;
    if (!strcmp(t, "hb")) return RB_MSG_HB;
    if (!strcmp(t, "det")) return RB_MSG_DET;
    if (!strcmp(t, "state")) return RB_MSG_STATE;
    if (!strcmp(t, "cmd")) return RB_MSG_CMD;
    if (!strcmp(t, "fault")) return RB_MSG_FAULT;
    return RB_MSG_UNKNOWN;
}

static bool is_num(JsonVariantConst v) {
    return v.is<double>() || v.is<long>() || v.is<unsigned long>();
}
static bool is_int(JsonVariantConst v) {
    return v.is<long>() || v.is<unsigned long>();
}

static int emit(char* out, size_t cap, const char* fmt, ...) {
    if (!out || cap == 0) return -1;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out, cap, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= cap || n > RB_MAX_MSG_LEN) return -1;
    return n;
}

int rb_build_hb_cam(char* out, size_t cap, const rb_hb_cam_t* m) {
    if (!m || m->cam < 0 || m->cam > 1 || m->ai < 0 || m->ai > 1 ||
        m->th < 0 || m->th > 2) return -1;
    return emit(out, cap,
        "{\"v\":%d,\"type\":\"hb\",\"ms\":%lu,\"cam\":%d,\"ai\":%d,"
        "\"temp\":%.1f,\"th\":%d,\"seq\":%lu}",
        RB_PROTO_V, (unsigned long)m->ms, m->cam, m->ai,
        (double)m->temp, m->th, (unsigned long)m->seq);
}

int rb_build_det(char* out, size_t cap, const rb_det_t* m) {
    if (!m || m->cls < 0 || m->cls >= RB_CLASS_COUNT) return -1;
    if (m->conf < 0.0f || m->conf > 1.0f || m->dir < -1 || m->dir > 1)
        return -1;
    return emit(out, cap,
        "{\"v\":%d,\"type\":\"det\",\"cls\":%d,\"name\":\"%s\","
        "\"conf\":%.3f,\"dir\":%d,\"det\":%s,\"seq\":%lu}",
        RB_PROTO_V, m->cls, RB_CLASS_NAMES[m->cls], (double)m->conf,
        m->dir, m->det ? "true" : "false", (unsigned long)m->seq);
}

int rb_build_fault(char* out, size_t cap, const rb_fault_t* m) {
    if (!m || m->cam < 0 || m->cam > 1 || m->ai < 0 || m->ai > 1) return -1;
    if (strlen(m->msg) >= RB_FAULTMSG_MAX) return -1;
    return emit(out, cap,
        "{\"v\":%d,\"type\":\"fault\",\"cam\":%d,\"ai\":%d,"
        "\"msg\":\"%s\",\"seq\":%lu}",
        RB_PROTO_V, m->cam, m->ai, m->msg, (unsigned long)m->seq);
}

int rb_build_hb_main(char* out, size_t cap, const rb_hb_main_t* m) {
    if (!m || m->estop < 0 || m->estop > 1) return -1;
    if (strlen(m->state) >= RB_TEXT_MAX) return -1;
    return emit(out, cap,
        "{\"v\":%d,\"type\":\"hb\",\"ms\":%lu,\"state\":\"%s\","
        "\"estop\":%d,\"seq\":%lu}",
        RB_PROTO_V, (unsigned long)m->ms, m->state, m->estop,
        (unsigned long)m->seq);
}

int rb_build_state(char* out, size_t cap, const rb_state_t* m) {
    if (!m) return -1;
    if (strlen(m->st) >= RB_TEXT_MAX || strlen(m->basket) >= RB_TEXT_MAX ||
        strlen(m->fault) >= RB_FAULTMSG_MAX) return -1;
    if (m->fill < 0 || m->fill > 100 || m->fix < 0 || m->fix > 1 ||
        m->sats < 0 || m->sats > 31 || m->conv < 0 || m->conv > 3 ||
        m->att < 0 || m->cfm < 0) return -1;
    return emit(out, cap,
        "{\"v\":%d,\"type\":\"state\",\"st\":\"%s\",\"basket\":\"%s\","
        "\"fill\":%.1f,\"obs_l\":%.1f,\"obs_r\":%.1f,"
        "\"lat\":%.6f,\"lon\":%.6f,\"fix\":%d,\"sats\":%d,"
        "\"conv\":%d,\"att\":%d,\"cfm\":%d,\"fault\":\"%s\",\"seq\":%lu}",
        RB_PROTO_V, m->st, m->basket, (double)m->fill,
        (double)m->obs_l, (double)m->obs_r, m->lat, m->lon,
        m->fix, m->sats, m->conv, m->att, m->cfm, m->fault,
        (unsigned long)m->seq);
}

int rb_build_cmd(char* out, size_t cap, const rb_cmd_t* m) {
    if (!m || strlen(m->action) >= RB_TEXT_MAX) return -1;
    if (!allowed_action(m->action)) return -1;
    return emit(out, cap, "{\"v\":%d,\"type\":\"cmd\",\"action\":\"%s\",\"seq\":%lu}",
                RB_PROTO_V, m->action, (unsigned long)m->seq);
}

static rb_status_t begin_parse(const char* line, JsonDocument& doc,
                               const char* want_type, rb_seq_gate_t* g,
                               rb_msg_type_t t) {
    if (deserializeJson(doc, line)) return RB_ERR_JSON;
    if (!doc.is<JsonObjectConst>()) return RB_ERR_JSON;
    if (!is_int(doc["v"]) || (int)doc["v"].as<long>() != RB_PROTO_V)
        return RB_ERR_VERSION;
    const char* ty = doc["type"];
    if (!ty || strcmp(ty, want_type)) return RB_ERR_TYPE;
    if (!is_int(doc["seq"])) return RB_ERR_FIELD;
    if (!seq_ok(g, t, doc["seq"].as<uint32_t>())) return RB_ERR_DUP;
    return RB_OK;
}

static bool str_field(JsonDocument& doc, const char* key, char* dst,
                      size_t dstsz) {
    const char* p = doc[key];
    if (!p) return false;
    size_t n = strlen(p);
    if (n >= dstsz) return false;
    memcpy(dst, p, n + 1);
    return true;
}

static bool range_f(JsonDocument& doc, const char* key, float lo, float hi,
                    float* out) {
    JsonVariantConst v = doc[key];
    if (!is_num(v)) return false;
    float f = v.as<float>();
    if (!(f >= lo && f <= hi)) return false;
    *out = f;
    return true;
}

static bool range_i(JsonDocument& doc, const char* key, long lo, long hi,
                    int* out) {
    JsonVariantConst v = doc[key];
    if (!is_int(v)) return false;
    long l = v.as<long>();
    if (l < lo || l > hi) return false;
    *out = (int)l;
    return true;
}

rb_status_t rb_parse_hb_cam(const char* line, rb_hb_cam_t* out,
                            rb_seq_gate_t* g) {
    if (!out) return RB_ERR_FIELD;
    JsonDocument doc;
    rb_status_t st = begin_parse(line, doc, "hb", g, RB_MSG_HB);
    if (st != RB_OK) return st;
    int cam, ai, th;
    float temp;
    if (!range_i(doc, "cam", 0, 1, &cam)) return RB_ERR_FIELD;
    if (!range_i(doc, "ai", 0, 1, &ai)) return RB_ERR_FIELD;
    if (!range_i(doc, "th", 0, 2, &th)) return RB_ERR_FIELD;
    if (!range_f(doc, "temp", -40, 120, &temp)) return RB_ERR_FIELD;
    if (!doc["ms"].is<long>() && !doc["ms"].is<unsigned long>())
        return RB_ERR_FIELD;
    out->ms = doc["ms"].as<uint32_t>();
    out->cam = cam;
    out->ai = ai;
    out->temp = temp;
    out->th = th;
    out->seq = doc["seq"].as<uint32_t>();
    return RB_OK;
}

rb_status_t rb_parse_det(const char* line, rb_det_t* out, rb_seq_gate_t* g) {
    if (!out) return RB_ERR_FIELD;
    JsonDocument doc;
    rb_status_t st = begin_parse(line, doc, "det", g, RB_MSG_DET);
    if (st != RB_OK) return st;
    int cls, dir;
    float conf;
    if (!range_i(doc, "cls", 0, RB_CLASS_COUNT - 1, &cls))
        return RB_ERR_FIELD;
    if (!range_f(doc, "conf", 0.0f, 1.0f, &conf)) return RB_ERR_FIELD;
    if (!range_i(doc, "dir", -1, 1, &dir)) return RB_ERR_FIELD;
    if (!doc["det"].is<bool>()) return RB_ERR_FIELD;
    if (!str_field(doc, "name", out->name, sizeof(out->name)))
        return RB_ERR_FIELD;
    if (strcmp(out->name, RB_CLASS_NAMES[cls])) return RB_ERR_FIELD;
    out->cls = cls;
    out->conf = conf;
    out->dir = dir;
    out->det = doc["det"].as<bool>();
    out->seq = doc["seq"].as<uint32_t>();
    return RB_OK;
}

rb_status_t rb_parse_fault(const char* line, rb_fault_t* out,
                           rb_seq_gate_t* g) {
    if (!out) return RB_ERR_FIELD;
    JsonDocument doc;
    rb_status_t st = begin_parse(line, doc, "fault", g, RB_MSG_FAULT);
    if (st != RB_OK) return st;
    int cam, ai;
    if (!range_i(doc, "cam", 0, 1, &cam)) return RB_ERR_FIELD;
    if (!range_i(doc, "ai", 0, 1, &ai)) return RB_ERR_FIELD;
    if (!str_field(doc, "msg", out->msg, sizeof(out->msg)))
        return RB_ERR_FIELD;
    for (const char* p = out->msg; *p; p++) {
        char c = *p;
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
        if (!ok) return RB_ERR_FIELD;
    }
    out->cam = cam;
    out->ai = ai;
    out->seq = doc["seq"].as<uint32_t>();
    return RB_OK;
}

rb_status_t rb_parse_hb_main(const char* line, rb_hb_main_t* out,
                             rb_seq_gate_t* g) {
    if (!out) return RB_ERR_FIELD;
    JsonDocument doc;
    rb_status_t st = begin_parse(line, doc, "hb", g, RB_MSG_HB);
    if (st != RB_OK) return st;
    int estop;
    if (!range_i(doc, "estop", 0, 1, &estop)) return RB_ERR_FIELD;
    if (!str_field(doc, "state", out->state, sizeof(out->state)))
        return RB_ERR_FIELD;
    if (!doc["ms"].is<long>() && !doc["ms"].is<unsigned long>())
        return RB_ERR_FIELD;
    out->ms = doc["ms"].as<uint32_t>();
    out->estop = estop;
    out->seq = doc["seq"].as<uint32_t>();
    return RB_OK;
}

rb_status_t rb_parse_state(const char* line, rb_state_t* out,
                           rb_seq_gate_t* g) {
    if (!out) return RB_ERR_FIELD;
    JsonDocument doc;
    rb_status_t st = begin_parse(line, doc, "state", g, RB_MSG_STATE);
    if (st != RB_OK) return st;
    float fill, ol, orr;
    int fix, sats, conv, att, cfm;
    if (!str_field(doc, "st", out->st, sizeof(out->st))) return RB_ERR_FIELD;
    if (!str_field(doc, "basket", out->basket, sizeof(out->basket)))
        return RB_ERR_FIELD;
    if (!range_f(doc, "fill", 0, 100, &fill)) return RB_ERR_FIELD;
    if (!range_f(doc, "obs_l", -1, 400, &ol)) return RB_ERR_FIELD;
    if (!range_f(doc, "obs_r", -1, 400, &orr)) return RB_ERR_FIELD;
    if (!range_i(doc, "fix", 0, 1, &fix)) return RB_ERR_FIELD;
    if (!range_i(doc, "sats", 0, 31, &sats)) return RB_ERR_FIELD;
    if (!range_i(doc, "conv", 0, 3, &conv)) return RB_ERR_FIELD;
    if (!range_i(doc, "att", 0, 1000000, &att)) return RB_ERR_FIELD;
    if (!range_i(doc, "cfm", 0, 1000000, &cfm)) return RB_ERR_FIELD;
    if (!str_field(doc, "fault", out->fault, sizeof(out->fault)))
        return RB_ERR_FIELD;
    JsonVariantConst jlat = doc["lat"];
    JsonVariantConst jlon = doc["lon"];
    if (!is_num(jlat) || !is_num(jlon)) return RB_ERR_FIELD;
    double lat = jlat.as<double>();
    double lon = jlon.as<double>();
    if (!(lat >= -90 && lat <= 90) || !(lon >= -180 && lon <= 180))
        return RB_ERR_FIELD;
    out->fill = fill;
    out->obs_l = ol;
    out->obs_r = orr;
    out->lat = lat;
    out->lon = lon;
    out->fix = fix;
    out->sats = sats;
    out->conv = conv;
    out->att = att;
    out->cfm = cfm;
    out->seq = doc["seq"].as<uint32_t>();
    return RB_OK;
}

rb_status_t rb_parse_cmd(const char* line, rb_cmd_t* out, rb_seq_gate_t* g) {
    if (!out) return RB_ERR_FIELD;
    JsonDocument doc;
    rb_status_t st = begin_parse(line, doc, "cmd", g, RB_MSG_CMD);
    if (st != RB_OK) return st;
    if (!str_field(doc, "action", out->action, sizeof(out->action)))
        return RB_ERR_FIELD;
    if (!allowed_action(out->action)) return RB_ERR_FIELD;
    out->seq = doc["seq"].as<uint32_t>();
    return RB_OK;
}
