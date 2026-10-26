#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"

void link_init(void);
void link_loop(uint32_t now_ms);
bool link_up(uint32_t now_ms);
void link_set_status(int cam_ok, int ai_ok, float temp, int th);
bool link_send_det(int cls, float conf, int dir, bool detected);
bool link_send_fault(const char* msg);
bool link_send_cmd(const char* action);
bool link_get_state(rb_state_t* out);
bool link_get_main_hb(rb_hb_main_t* out);
uint32_t link_rx_valid(void);
const rb_err_t* link_errors(void);
const char* link_boat_state(void);
bool link_main_autoflag(void);
