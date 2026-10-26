#ifndef PREPROCESS_H
#define PREPROCESS_H

#include <stdint.h>
#include <stddef.h>
#include "ai_config.h"

/* Preprocess a camera frame into the model input tensor.
 *
 * in_buf    : raw camera pixels (grayscale, width x height)
 * in_w/in_h : source dimensions
 * out       : destination tensor buffer (MODEL_INPUT_WIDTH x MODEL_INPUT_HEIGHT x CHANNELS)
 *
 * Returns true on success. Implemented as resize-by-nearest-neighbor +
 * normalization into int8 [0,255] suitable for an INT8 quantized model.
 */
bool preprocess_frame(const uint8_t *in_buf, int in_w, int in_h,
                      int8_t *out, size_t out_size);

#endif /* PREPROCESS_H */
