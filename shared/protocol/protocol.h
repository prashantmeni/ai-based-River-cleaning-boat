#ifndef RIVER_CLEANING_BOAT_PROTOCOL_H
#define RIVER_CLEANING_BOAT_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ========================================================================
 * Shared serial protocol between ESP32-CAM (AI vision unit) and
 * standard ESP32 (main control unit).
 *
 * Frame format (all bytes, little-endian multi-byte fields):
 *
 *   START(0x7E) | VERSION | TYPE | SEQUENCE(2) | PAYLOAD_LENGTH(2) |
 *   PAYLOAD(N) | CHECKSUM | END(0x7F)
 *
 * CHECKSUM = 8-bit sum of bytes from VERSION through last payload byte,
 *            taken modulo 256.
 *
 * A frame is rejected if: wrong start/end byte, payload length mismatch,
 * checksum mismatch, unknown type, or length > PROTOCOL_MAX_PAYLOAD.
 * ======================================================================== */

#define PROTOCOL_START_BYTE  0x7Eu
#define PROTOCOL_END_BYTE    0x7Fu
#define PROTOCOL_VERSION     0x01u

#define PROTOCOL_MAX_PAYLOAD 64u
#define PROTOCOL_FRAME_OVERHEAD 7u   /* START+VER+TYPE+SEQ2+LEN2+CHK+END */
#define PROTOCOL_MAX_FRAME  (PROTOCOL_FRAME_OVERHEAD + PROTOCOL_MAX_PAYLOAD)

#define PROTOCOL_SEQUENCE_STALE_LIMIT 255u

/* -----------------------------------------------------------------------
 * Message types
 * ----------------------------------------------------------------------- */
typedef enum {
    PROTOCOL_MSG_AI_DETECTION      = 0x01,
    PROTOCOL_MSG_AI_NO_DETECTION   = 0x02,
    PROTOCOL_MSG_AI_HEALTH         = 0x03,
    PROTOCOL_MSG_MAIN_STATUS       = 0x10,
    PROTOCOL_MSG_DIAG_COMMAND      = 0x20,
    PROTOCOL_MSG_FAULT_REPORT      = 0x30,
} protocol_msg_type_t;

/* -----------------------------------------------------------------------
 * Inference status codes
 * ----------------------------------------------------------------------- */
typedef enum {
    INF_STATUS_OK                 = 0x00,
    INF_STATUS_MODEL_NOT_LOADED   = 0x01,
    INF_STATUS_CAMERA_ERROR       = 0x02,
    INF_STATUS_INFERENCE_ERROR    = 0x03,
    INF_STATUS_INITIALIZING       = 0x04,
} inference_status_t;

/* -----------------------------------------------------------------------
 * Payload structures (packed, little-endian)
 * ----------------------------------------------------------------------- */
typedef struct __attribute__((packed)) {
    uint8_t  class_id;
    uint8_t  confidence;               /* 0..100 percent                    */
    int16_t  bbox_x;                   /* 0..1000 normalized                */
    int16_t  bbox_y;
    int16_t  bbox_w;
    int16_t  bbox_h;
    int16_t  center_x;                 /* -500..+500 relative to image centre */
    uint32_t timestamp_ms;             /* CAM local ms since boot           */
    uint8_t  camera_health;
    uint8_t  inference_status;
} ai_detection_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t  reason;
    uint32_t timestamp_ms;
    uint8_t  camera_health;
    uint8_t  inference_status;
} ai_no_detection_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t  camera_health;
    uint8_t  inference_status;
    uint8_t  frames_captured;
    uint8_t  inferences_run;
    uint16_t inference_errors;
    uint32_t uptime_ms;
} ai_health_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t  nav_state;
    uint8_t  motor_left_pwm;
    uint8_t  motor_right_pwm;
    int8_t   motor_left_dir;
    int8_t   motor_right_dir;
    uint8_t  conveyor_state;
    uint8_t  comm_health;
    uint8_t  fault_flags;
    uint16_t collection_count;
    uint32_t uptime_ms;
} main_status_payload_t;

/* -----------------------------------------------------------------------
 * Diagnostic command payload (PROTOCOL_MSG_DIAG_COMMAND)
 * Commands: 1=stop, 2=forward, 3=turn-left, 4=turn-right,
 *           5=conveyor-on, 6=conveyor-off, 7=autonomous-on, 8=autonomous-off
 * ----------------------------------------------------------------------- */
typedef struct __attribute__((packed)) {
    uint8_t  cmd;
    uint8_t  value;          /* PWM or parameter */
    uint16_t duration_ms;    /* bounded command duration (0 = indefinite until stop) */
} diag_command_payload_t;

#define DIAG_CMD_STOP             1
#define DIAG_CMD_FORWARD          2
#define DIAG_CMD_TURN_LEFT        3
#define DIAG_CMD_TURN_RIGHT       4
#define DIAG_CMD_CONVEYOR_ON      5
#define DIAG_CMD_CONVEYOR_OFF     6
#define DIAG_CMD_AUTONOMOUS_ON    7
#define DIAG_CMD_AUTONOMOUS_OFF   8

/* -----------------------------------------------------------------------
 * Wire frame
 * ----------------------------------------------------------------------- */
typedef struct {
    uint8_t  start_byte;
    uint8_t  version;
    uint8_t  type;
    uint16_t sequence;
    uint16_t payload_length;
    uint8_t  payload[PROTOCOL_MAX_PAYLOAD];
    uint8_t  checksum;
    uint8_t  end_byte;
} protocol_frame_t;

/* -----------------------------------------------------------------------
 * Parse result
 * ----------------------------------------------------------------------- */
typedef enum {
    PROTOCOL_PARSE_OK               = 0,
    PROTOCOL_PARSE_INCOMPLETE       = 1,
    PROTOCOL_PARSE_BAD_START        = 2,
    PROTOCOL_PARSE_BAD_END          = 3,
    PROTOCOL_PARSE_BAD_VERSION      = 4,
    PROTOCOL_PARSE_BAD_LENGTH       = 5,
    PROTOCOL_PARSE_BAD_CHECKSUM     = 6,
    PROTOCOL_PARSE_UNKNOWN_TYPE     = 7,
    PROTOCOL_PARSE_BUFFER_OVERFLOW  = 8,
} protocol_parse_result_t;

typedef struct {
    uint8_t  buf[PROTOCOL_MAX_FRAME];
    size_t   len;
    size_t   expected_len;
    uint8_t  start_seen;
} protocol_rx_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Compute 8-bit checksum over [buf, len). */
static inline uint8_t protocol_compute_checksum(const uint8_t *buf, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum = (uint8_t)(sum + buf[i]);
    }
    return sum;
}

/* Serialise a frame into a flat wire buffer. Returns bytes written, or
 * negative on error. */
static inline int protocol_serialize_frame(uint8_t type, uint16_t sequence,
        const uint8_t *payload, uint16_t payload_len,
        uint8_t *out_buf, size_t out_buf_size) {
    if (payload_len > PROTOCOL_MAX_PAYLOAD) return -1;
    size_t frame_len = PROTOCOL_FRAME_OVERHEAD + payload_len;
    if (out_buf_size < frame_len) return -2;

    size_t i = 0;
    out_buf[i++] = PROTOCOL_START_BYTE;
    out_buf[i++] = PROTOCOL_VERSION;
    out_buf[i++] = type;
    out_buf[i++] = (uint8_t)(sequence & 0xFF);
    out_buf[i++] = (uint8_t)((sequence >> 8) & 0xFF);
    out_buf[i++] = (uint8_t)(payload_len & 0xFF);
    out_buf[i++] = (uint8_t)((payload_len >> 8) & 0xFF);
    memcpy(out_buf + i, payload, payload_len);
    i += payload_len;

    /* checksum over VERSION..payload (bytes 1 .. 6+payload_len-1) */
    uint8_t chk = protocol_compute_checksum(out_buf + 1, i - 1);
    out_buf[i++] = chk;
    out_buf[i++] = PROTOCOL_END_BYTE;
    return (int)frame_len;
}

/* Initialise a streaming receiver. */
static inline void protocol_rx_init(protocol_rx_t *rx) {
    rx->len = 0;
    rx->expected_len = 0;
    rx->start_seen = 0;
    memset(rx->buf, 0, sizeof(rx->buf));
}

/* Push one received byte. Returns PROTOCOL_PARSE_OK when a complete valid
 * frame is available in *frame. */
static inline protocol_parse_result_t protocol_rx_push(protocol_rx_t *rx,
        uint8_t b, protocol_frame_t *frame) {
    /* Waiting for start byte */
    if (!rx->start_seen) {
        if (b == PROTOCOL_START_BYTE) {
            rx->start_seen = 1;
            rx->len = 0;
            rx->buf[rx->len++] = b;
        }
        return PROTOCOL_PARSE_INCOMPLETE;
    }

    /* Already saw start; accumulate */
    if (rx->len >= PROTOCOL_MAX_FRAME) {
        rx->start_seen = 0;
        return PROTOCOL_PARSE_BUFFER_OVERFLOW;
    }
    rx->buf[rx->len++] = b;

    /* Need at least START+VER+TYPE+SEQ2+LEN2 = 7 bytes to know payload len */
    if (rx->len < 7) return PROTOCOL_PARSE_INCOMPLETE;

    /* Validate version (byte index 1) */
    if (rx->buf[1] != PROTOCOL_VERSION) {
        rx->start_seen = 0;
        return PROTOCOL_PARSE_BAD_VERSION;
    }

    uint16_t payload_len = (uint16_t)rx->buf[5] | ((uint16_t)rx->buf[6] << 8);
    if (payload_len > PROTOCOL_MAX_PAYLOAD) {
        rx->start_seen = 0;
        return PROTOCOL_PARSE_BAD_LENGTH;
    }
    rx->expected_len = (size_t)(PROTOCOL_FRAME_OVERHEAD + payload_len);

    if (rx->len < rx->expected_len) return PROTOCOL_PARSE_INCOMPLETE;

    /* Full frame received */
    if (rx->buf[rx->expected_len - 1] != PROTOCOL_END_BYTE) {
        rx->start_seen = 0;
        return PROTOCOL_PARSE_BAD_END;
    }

    uint8_t type = rx->buf[2];
    if (type != PROTOCOL_MSG_AI_DETECTION &&
        type != PROTOCOL_MSG_AI_NO_DETECTION &&
        type != PROTOCOL_MSG_AI_HEALTH &&
        type != PROTOCOL_MSG_MAIN_STATUS &&
        type != PROTOCOL_MSG_DIAG_COMMAND &&
        type != PROTOCOL_MSG_FAULT_REPORT) {
        rx->start_seen = 0;
        return PROTOCOL_PARSE_UNKNOWN_TYPE;
    }

    uint8_t chk = protocol_compute_checksum(rx->buf + 1, rx->expected_len - 3);
    if (chk != rx->buf[rx->expected_len - 2]) {
        rx->start_seen = 0;
        return PROTOCOL_PARSE_BAD_CHECKSUM;
    }

    /* Populate frame */
    frame->start_byte = rx->buf[0];
    frame->version = rx->buf[1];
    frame->type = rx->buf[2];
    frame->sequence = (uint16_t)rx->buf[3] | ((uint16_t)rx->buf[4] << 8);
    frame->payload_length = payload_len;
    memcpy(frame->payload, rx->buf + 7, payload_len);
    frame->checksum = rx->buf[rx->expected_len - 2];
    frame->end_byte = rx->buf[rx->expected_len - 1];

    rx->start_seen = 0;
    rx->len = 0;
    return PROTOCOL_PARSE_OK;
}

/* Build an AI detection payload into a byte buffer. Returns bytes written. */
static inline size_t protocol_build_ai_detection_payload(
        const ai_detection_payload_t *det, uint8_t *out) {
    size_t i = 0;
    out[i++] = det->class_id;
    out[i++] = det->confidence;
    out[i++] = (uint8_t)(det->bbox_x & 0xFF);
    out[i++] = (uint8_t)((det->bbox_x >> 8) & 0xFF);
    out[i++] = (uint8_t)(det->bbox_y & 0xFF);
    out[i++] = (uint8_t)((det->bbox_y >> 8) & 0xFF);
    out[i++] = (uint8_t)(det->bbox_w & 0xFF);
    out[i++] = (uint8_t)((det->bbox_w >> 8) & 0xFF);
    out[i++] = (uint8_t)(det->bbox_h & 0xFF);
    out[i++] = (uint8_t)((det->bbox_h >> 8) & 0xFF);
    out[i++] = (uint8_t)(det->center_x & 0xFF);
    out[i++] = (uint8_t)((det->center_x >> 8) & 0xFF);
    out[i++] = (uint8_t)(det->timestamp_ms & 0xFF);
    out[i++] = (uint8_t)((det->timestamp_ms >> 8) & 0xFF);
    out[i++] = (uint8_t)((det->timestamp_ms >> 16) & 0xFF);
    out[i++] = (uint8_t)((det->timestamp_ms >> 24) & 0xFF);
    out[i++] = det->camera_health;
    out[i++] = det->inference_status;
    return i;
}

/* Parse an AI detection payload from bytes. Returns 0 on success. */
static inline int protocol_parse_ai_detection_payload(
        const uint8_t *in, size_t len, ai_detection_payload_t *det) {
    if (len < sizeof(ai_detection_payload_t)) return -1;
    size_t i = 0;
    det->class_id = in[i++];
    det->confidence = in[i++];
    det->bbox_x = (int16_t)((uint16_t)in[i++] | ((uint16_t)in[i++] << 8));
    det->bbox_y = (int16_t)((uint16_t)in[i++] | ((uint16_t)in[i++] << 8));
    det->bbox_w = (int16_t)((uint16_t)in[i++] | ((uint16_t)in[i++] << 8));
    det->bbox_h = (int16_t)((uint16_t)in[i++] | ((uint16_t)in[i++] << 8));
    det->center_x = (int16_t)((uint16_t)in[i++] | ((uint16_t)in[i++] << 8));
    det->timestamp_ms = (uint32_t)in[i++] | ((uint32_t)in[i++] << 8) |
                        ((uint32_t)in[i++] << 16) | ((uint32_t)in[i++] << 24);
    det->camera_health = in[i++];
    det->inference_status = in[i++];
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* RIVER_CLEANING_BOAT_PROTOCOL_H */
