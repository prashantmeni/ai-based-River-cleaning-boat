#include "detection.h"

DetectionResult filter_detection(const ai_detection_payload_t *raw,
                                 uint8_t conf_threshold,
                                 uint8_t consec_required) {
    DetectionResult out;
    out.valid = false;
    out.consecutive_count = 0;
    memset(&out.detection, 0, sizeof(out.detection));

    static uint8_t consecutive = 0;

    bool above_threshold = (raw && raw->confidence >= conf_threshold &&
                            raw->inference_status == INF_STATUS_OK);

    if (above_threshold) {
        consecutive++;
        if (consecutive >= consec_required) {
            out.valid = true;
            out.consecutive_count = consecutive;
            if (raw) out.detection = *raw;
        } else {
            out.consecutive_count = consecutive;
        }
    } else {
        consecutive = 0;
        out.consecutive_count = 0;
    }
    return out;
}

ai_no_detection_payload_t make_no_detection(uint8_t reason,
                                            uint32_t timestamp_ms,
                                            uint8_t camera_health,
                                            uint8_t inference_status) {
    ai_no_detection_payload_t p;
    p.reason = reason;
    p.timestamp_ms = timestamp_ms;
    p.camera_health = camera_health;
    p.inference_status = inference_status;
    return p;
}
