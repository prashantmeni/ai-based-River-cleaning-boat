#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define RB_PROTO_V          1
#define RB_BAUD             115200
#define RB_MAX_LINE         256
#define RB_MAX_MSG_LEN      255
#define RB_LINK_TIMEOUT_MS      3000
#define RB_CAM_LINK_TIMEOUT_MS  10000
#define RB_DET_STALE_MS         4000
#define RB_CLASS_COUNT          5
#define RB_NAME_MAX             32
#define RB_TEXT_MAX             24
#define RB_FAULTMSG_MAX         24

extern const char* const RB_CLASS_NAMES[RB_CLASS_COUNT];

typedef enum { DIR_LEFT = -1, DIR_CENTER = 0, DIR_RIGHT = 1 } target_dir_t;

typedef enum {
    RB_MSG_NONE = 0,
    RB_MSG_HB,
    RB_MSG_DET,
    RB_MSG_STATE,
    RB_MSG_CMD,
    RB_MSG_FAULT,
    RB_MSG_UNKNOWN
} rb_msg_type_t;

typedef enum {
    RB_OK = 0,
    RB_ERR_JSON,
    RB_ERR_VERSION,
    RB_ERR_TYPE,
    RB_ERR_FIELD,
    RB_ERR_DUP
} rb_status_t;

typedef struct {
    uint32_t ms;
    int      cam;
    int      ai;
    float    temp;
    int      th;
    uint32_t seq;
} rb_hb_cam_t;

typedef struct {
    int      cls;
    char     name[RB_NAME_MAX];
    float    conf;
    int      dir;
    bool     det;
    uint32_t seq;
} rb_det_t;

typedef struct {
    int      cam;
    int      ai;
    char     msg[RB_FAULTMSG_MAX];
    uint32_t seq;
} rb_fault_t;

typedef struct {
    uint32_t ms;
    char     state[RB_TEXT_MAX];
    int      estop;
    uint32_t seq;
} rb_hb_main_t;

typedef struct {
    char     st[RB_TEXT_MAX];
    char     basket[RB_TEXT_MAX];
    float    fill;
    float    obs_l;
    float    obs_r;
    double   lat;
    double   lon;
    int      fix;
    int      sats;
    int      conv;
    int      att;
    int      cfm;
    char     fault[RB_FAULTMSG_MAX];
    uint32_t seq;
} rb_state_t;

typedef struct {
    char     action[RB_TEXT_MAX];
    uint32_t seq;
} rb_cmd_t;

typedef struct {
    uint32_t bad_json;
    uint32_t bad_version;
    uint32_t unknown_type;
    uint32_t bad_field;
    uint32_t oversize;
    uint32_t dup_seq;
} rb_err_t;

typedef struct {
    char     buf[RB_MAX_LINE];
    uint16_t len;
    bool     dropping;
} rb_framer_t;

typedef struct {
    uint32_t last_seq[RB_MSG_UNKNOWN + 1];
    bool     have[RB_MSG_UNKNOWN + 1];
} rb_seq_gate_t;

void rb_framer_init(rb_framer_t* f);
int  rb_framer_feed(rb_framer_t* f, char c);
void rb_seq_gate_init(rb_seq_gate_t* g);

rb_msg_type_t rb_msg_type(const char* line);

int  rb_build_hb_cam(char* out, size_t cap, const rb_hb_cam_t* m);
int  rb_build_det(char* out, size_t cap, const rb_det_t* m);
int  rb_build_fault(char* out, size_t cap, const rb_fault_t* m);
int  rb_build_hb_main(char* out, size_t cap, const rb_hb_main_t* m);
int  rb_build_state(char* out, size_t cap, const rb_state_t* m);
int  rb_build_cmd(char* out, size_t cap, const rb_cmd_t* m);

rb_status_t rb_parse_hb_cam(const char* line, rb_hb_cam_t* out, rb_seq_gate_t* g);
rb_status_t rb_parse_det(const char* line, rb_det_t* out, rb_seq_gate_t* g);
rb_status_t rb_parse_fault(const char* line, rb_fault_t* out, rb_seq_gate_t* g);
rb_status_t rb_parse_hb_main(const char* line, rb_hb_main_t* out, rb_seq_gate_t* g);
rb_status_t rb_parse_state(const char* line, rb_state_t* out, rb_seq_gate_t* g);
rb_status_t rb_parse_cmd(const char* line, rb_cmd_t* out, rb_seq_gate_t* g);
