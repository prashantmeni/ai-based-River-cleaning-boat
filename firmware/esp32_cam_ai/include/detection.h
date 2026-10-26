#ifndef DETECTION_H
#define DETECTION_H

#include <stdint.h>
#include "shared/protocol/protocol.h"
#include "inference.h"

/* Detection result after thresholding and temporal filtering. */
struct DetectionResult {
    bool valid;                 /* passes confidence + temporal filter   */
    ai_detection_payload_t detection;
    uint8_t consecutive_count;  /* consecutive valid frames so far       */
};

/* Confidence + temporal filter.
 *
 * conf_threshold : 0..100 minimum confidence
 * consec_required: consecutive valid frames required before valid==true
 *
 * Call once per inference cycle. Returns a DetectionResult.
 */
DetectionResult filter_detection(const ai_detection_payload_t *raw,
                                 uint8_t conf_threshold,
                                 uint8_t consec_required);

/* Build a no-detection payload. */
ai_no_detection_payload_t make_no_detection(uint8_t reason,
                                            uint32_t timestamp_ms,
                                            uint8_t camera_health,
                                            uint8_t inference_status);

#endif /* DETECTION_H */
