#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"

void link_init(void);
void link_loop(uint32_t now_ms);
bool link_up(uint32_t now_ms);
bool link_det_fresh(uint32_t now_ms);
bool link_get_det(rb_det_t* out);
bool link_cam_flag(void);
bool link_ai_flag(void);
const char* link_cam_fault_msg(void);
void link_set_hb(const char* state, int estop);
void link_set_state(const rb_state_t* s);
uint32_t link_rx_valid(void);
const rb_err_t* link_errors(void);
bool link_take_cmd(rb_cmd_t* out);
