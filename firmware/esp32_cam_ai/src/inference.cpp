#include "inference.h"
#include <string.h>

Inference::Inference()
    : _status(INF_STATUS_INITIALIZING), _loaded(false),
      _inferences_run(0), _inference_errors(0) {}

Inference::~Inference() {}

bool Inference::begin() {
    /* TODO: load the trained INT8 model from assets/model/ here once a
     * model file is provided. Allocate a static tensor arena sized to the
     * model. Until then, report MODEL_NOT_LOADED so the main controller
     * treats the vision unit as unhealthy and never acts on a detection. */
    _loaded = false;
    _status = INF_STATUS_MODEL_NOT_LOADED;
    return false;
}

inference_status_t Inference::run(const int8_t *input, size_t input_len,
                                  ai_detection_payload_t *result) {
    if (!result) return INF_STATUS_INFERENCE_ERROR;
    _inferences_run++;

    if (!_loaded) {
        _status = INF_STATUS_MODEL_NOT_LOADED;
        _inference_errors++;
        memset(result, 0, sizeof(*result));
        result->inference_status = _status;
        result->confidence = 0;
        return _status;
    }

    /* TODO: invoke the real model interpreter here. Populate result:
     *   result->class_id, result->confidence (0..100),
     *   result->bbox_x/y/w/h (0..1000), result->center_x (-500..+500)
     * Set _status = INF_STATUS_OK or INF_STATUS_INFERENCE_ERROR.
     */
    _status = INF_STATUS_INFERENCE_ERROR;
    _inference_errors++;
    memset(result, 0, sizeof(*result));
    result->inference_status = _status;
    result->confidence = 0;
    return _status;
}
