#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "config.h"

typedef struct {
    int   best_class;     // index into CLASS_NAMES, -1 if none
    float best_conf;      // 0..1
    float region_score[NUM_CLASSES]; // mean softmax over 3 regions (reserved)
    bool  ok;             // inference executed
} ai_result_t;

bool ai_init(void);
// Run inference on an RGB888 buffer of size*size*3 (values 0..255).
bool ai_infer(const uint8_t* rgb, int size, ai_result_t* out);
// Convenience: run on full frame; sets out->best_class/conf.
bool ai_infer_full(ai_result_t* out);
