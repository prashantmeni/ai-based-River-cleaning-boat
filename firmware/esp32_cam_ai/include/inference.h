#ifndef INFERENCE_H
#define INFERENCE_H

#include <stdint.h>
#include <stddef.h>
#include "ai_config.h"
#include "shared/protocol/protocol.h"

/* Inference engine interface for the ESP32-CAM.
 *
 * The concrete backend (TensorFlow Lite Micro / ESP-NN / custom) is
 * intentionally a stub here because no trained model file exists in the
 * repository. When a model is added to assets/model/, wire it into
 * inference.cpp and update docs/AI_MODEL.md.
 */
class Inference {
public:
    Inference();
    ~Inference();

    /* Load the model from flash/filesystem. Returns true on success. */
    bool begin();

    /* Run inference on preprocessed input (int8, HWC). Fills result.
     * Returns inference_status_t. */
    inference_status_t run(const int8_t *input, size_t input_len,
                           ai_detection_payload_t *result);

    inference_status_t status() const { return _status; }

private:
    inference_status_t _status;
    bool _loaded;
    uint32_t _inferences_run;
    uint16_t _inference_errors;
};

#endif /* INFERENCE_H */
