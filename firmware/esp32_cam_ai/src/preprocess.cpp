#include "preprocess.h"

bool preprocess_frame(const uint8_t *in_buf, int in_w, int in_h,
                      int8_t *out, size_t out_size) {
    if (!in_buf || !out) return false;
    size_t needed = (size_t)MODEL_INPUT_WIDTH * MODEL_INPUT_HEIGHT *
                    (size_t)MODEL_INPUT_CHANNELS;
    if (out_size < needed) return false;
    if (in_w <= 0 || in_h <= 0) return false;

    /* Nearest-neighbor resize to MODEL_INPUT_WIDTH x MODEL_INPUT_HEIGHT.
     * Grayscale: one channel. Value copied as-is (already 0..255). */
    for (int y = 0; y < MODEL_INPUT_HEIGHT; y++) {
        int sy = (y * in_h) / MODEL_INPUT_HEIGHT;
        for (int x = 0; x < MODEL_INPUT_WIDTH; x++) {
            int sx = (x * in_w) / MODEL_INPUT_WIDTH;
            size_t src_idx = (size_t)sy * (size_t)in_w + (size_t)sx;
            size_t dst_idx = (size_t)y * MODEL_INPUT_WIDTH + (size_t)x;
            out[dst_idx] = (int8_t)in_buf[src_idx];
        }
    }
    return true;
}
