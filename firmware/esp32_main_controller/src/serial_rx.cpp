#include <WiFi.h>     /* explicit: lets LDF link the framework WiFi library */
#include "serial_rx.h"
#include "espnow_link.h"
#include <Arduino.h>

/* ---------------------------------------------------------------------
 * Ring buffer: written by the ESP-NOW receive callback (WiFi task,
 * core 0), drained by update() (loop task, core 1). A short critical
 * section protects head/tail; parsing stays on the loop task.
 * --------------------------------------------------------------------- */
#define RX_RING_SIZE 512u

static portMUX_TYPE s_rx_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t s_ring[RX_RING_SIZE];
static volatile size_t s_head = 0;
static volatile size_t s_tail = 0;
static volatile uint32_t s_ring_drops = 0;
static uint8_t s_dbg[20];
static volatile uint8_t s_dbg_n = 0;

static void espnow_recv_cb(const esp_now_recv_info_t *info,
                           const uint8_t *data, int len) {
    (void)info;
    if (!data || len <= 0) return;
    portENTER_CRITICAL(&s_rx_mux);
    /* Keep a raw copy of the most recent datagram head for diagnostics. */
    s_dbg_n = (uint8_t)((len > 20) ? 20 : len);
    memcpy(s_dbg, data, s_dbg_n);
    for (int i = 0; i < len; i++) {
        size_t next = (s_head + 1) % RX_RING_SIZE;
        if (next == s_tail) {
            s_ring_drops++;          /* should never happen at ~40B/150ms */
            break;
        }
        s_ring[s_head] = data[i];
        s_head = next;
    }
    portEXIT_CRITICAL(&s_rx_mux);
}

uint32_t SerialRx::drops() const { return s_ring_drops; }

SerialRx::SerialRx()
    : _cb_det(nullptr), _cb_nd(nullptr), _cb_health(nullptr),
      _cb_diag(nullptr), _comms_alive(false), _link_ok(false),
      _last_frame_ms(0), _error_count(0), _rx_bytes(0), _last_err(0),
      _dbg_n(0) {
    memset(_dbg, 0, sizeof(_dbg));
}

void SerialRx::begin(const char *ap_ssid, const char *ap_pass) {
    protocol_rx_init(&_rx);
    _link_ok = espnow_link_begin(ap_ssid, ap_pass);
    if (_link_ok) {
        esp_now_register_recv_cb(espnow_recv_cb);
    }
}

/* Publish the raw datagram snapshot to instance members (loop task). */
static void sync_dbg(uint8_t *dst, uint8_t *n_out) {
    *n_out = s_dbg_n;
    memcpy(dst, s_dbg, s_dbg_n);
}

void SerialRx::update() {
    /* Follow Wi-Fi channel drift (STA joins the router on its channel;
     * ESP-NOW home channel must be rebuilt there — IDFGH-17728). */
    if (espnow_link_resync()) {
        esp_now_register_recv_cb(espnow_recv_cb);
    }
    sync_dbg(_dbg, &_dbg_n);
    /* Drain the ring in chunks, then parse on the loop task. */
    for (;;) {
        uint8_t chunk[128];
        size_t n = 0;
        portENTER_CRITICAL(&s_rx_mux);
        while (n < sizeof(chunk) && s_tail != s_head) {
            chunk[n++] = s_ring[s_tail];
            s_tail = (s_tail + 1) % RX_RING_SIZE;
        }
        portEXIT_CRITICAL(&s_rx_mux);
        if (n == 0) break;

        for (size_t i = 0; i < n; i++) {
            _rx_bytes++;
            protocol_frame_t frame;
            protocol_parse_result_t r = protocol_rx_push(&_rx, chunk[i], &frame);
            if (r == PROTOCOL_PARSE_OK) {
                _comms_alive = true;
                _last_frame_ms = millis();
                switch (frame.type) {
                    case PROTOCOL_MSG_AI_DETECTION: {
                        ai_detection_payload_t det;
                        if (protocol_parse_ai_detection_payload(frame.payload,
                                                                frame.payload_length,
                                                                &det) == 0 && _cb_det) {
                            _cb_det(&det);
                        } else { _error_count++; _last_err = 0xA1; }
                        break;
                    }
                    case PROTOCOL_MSG_AI_NO_DETECTION: {
                        if (frame.payload_length >= sizeof(ai_no_detection_payload_t) && _cb_nd) {
                            ai_no_detection_payload_t nd;
                            memcpy(&nd, frame.payload, sizeof(nd));
                            _cb_nd(&nd);
                        } else { _error_count++; _last_err = 0xA2; }
                        break;
                    }
                    case PROTOCOL_MSG_AI_HEALTH: {
                        if (frame.payload_length >= sizeof(ai_health_payload_t) && _cb_health) {
                            ai_health_payload_t h;
                            memcpy(&h, frame.payload, sizeof(h));
                            _cb_health(&h);
                        } else { _error_count++; _last_err = 0xA3; }
                        break;
                    }
                    case PROTOCOL_MSG_DIAG_COMMAND: {
                        if (frame.payload_length >= sizeof(diag_command_payload_t) && _cb_diag) {
                            diag_command_payload_t c;
                            memcpy(&c, frame.payload, sizeof(c));
                            _cb_diag(c.cmd, c.value, c.duration_ms);
                        } else { _error_count++; _last_err = 0xA4; }
                        break;
                    }
                    default:
                        break;
                }
            } else if (r != PROTOCOL_PARSE_INCOMPLETE) {
                _error_count++;
                _last_err = (uint8_t)r;
            }
        }
    }
    /* Comms considered alive only if a frame arrived recently. */
    if (_comms_alive && (millis() - _last_frame_ms > 2000)) {
        _comms_alive = false;
    }
}

void SerialRx::on_detection(DetectionCb cb)  { _cb_det = cb; }
void SerialRx::on_no_detection(NoDetectionCb cb) { _cb_nd = cb; }
void SerialRx::on_health(HealthCb cb) { _cb_health = cb; }
void SerialRx::on_diag_command(DiagCommandCb cb) { _cb_diag = cb; }
