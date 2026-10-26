/* ========================================================================
 * ESP32-CAM AI vision unit — main entry point.
 *
 * Captures camera frames, preprocesses for the (placeholder) TinyML model,
 * runs inference locally, applies confidence + temporal filtering, and
 * transmits structured detection/no-detection/health messages over UART
 * to the main ESP32 controller.
 *
 * No inference runs on the main controller. No network is used.
 * ======================================================================== */

#include <Arduino.h>
#include "ai_config.h"
#include "camera.h"
#include "preprocess.h"
#include "inference.h"
#include "detection.h"
#include "serial_tx.h"
#include "protocol.h"

static Camera g_camera;
static Inference g_inference;
static SerialTx g_serial(CAM_UART_PORT);

static uint16_t g_tx_seq = 0;
static uint32_t g_last_inference_ms = 0;
static uint32_t g_last_health_ms = 0;

static void print_status(const char *msg) {
    CAM_UART_PORT.print("[CAM] ");
    CAM_UART_PORT.println(msg);
}

void setup() {
    pinMode(STATUS_LED_GPIO, OUTPUT);
    digitalWrite(STATUS_LED_GPIO, LOW);

    CAM_UART_PORT.begin(CAM_UART_BAUD);
    while (!CAM_UART_PORT) { /* wait for serial */ }
    print_status("booting ESP32-CAM AI vision unit");

    /* Camera init. If it fails, keep reporting camera_health != 0. */
    if (!g_camera.begin()) {
        print_status(g_camera.last_error());
    } else {
        print_status("camera initialized");
    }

    /* Inference / model load. Expect failure until a model is provided. */
    if (!g_inference.begin()) {
        print_status("model not loaded (expected until assets/model/ provided)");
    } else {
        print_status("model loaded");
    }

    g_serial.begin(CAM_UART_BAUD);
    print_status("serial link started");
}

void loop() {
    uint32_t now = millis();

    /* Run inference at a controlled interval (non-blocking). */
    if (now - g_last_inference_ms >= INFERENCE_INTERVAL_MS) {
        g_last_inference_ms = now;

        uint8_t *fb = nullptr;
        size_t fb_len = 0;
        bool cam_ok = g_camera.capture(&fb, &fb_len);

        uint8_t camera_health = cam_ok ? 0 : 1;

        if (cam_ok && fb_len > 0) {
            static int8_t tensor_buf[96 * 96];  /* MODEL_INPUT_WIDTH*HEIGHT*CHANNELS */
            if (preprocess_frame(fb, g_camera.width(), g_camera.height(),
                                 tensor_buf, sizeof(tensor_buf))) {
                ai_detection_payload_t det;
                memset(&det, 0, sizeof(det));
                det.timestamp_ms = now;
                det.camera_health = camera_health;

                inference_status_t istat = g_inference.run(tensor_buf,
                                                           sizeof(tensor_buf), &det);
                det.inference_status = istat;

                DetectionResult r = filter_detection(&det,
                                                     MODEL_CONFIDENCE_THRESHOLD,
                                                     CONSECUTIVE_DETECTIONS);
                if (r.valid) {
                    digitalWrite(STATUS_LED_GPIO, HIGH);
                    g_serial.send_detection(&r.detection, &g_tx_seq);
                } else {
                    digitalWrite(STATUS_LED_GPIO, LOW);
                    ai_no_detection_payload_t nd = make_no_detection(
                        (istat != INF_STATUS_OK) ? 2 : 1, now,
                        camera_health, istat);
                    g_serial.send_no_detection(&nd, &g_tx_seq);
                }
            } else {
                ai_no_detection_payload_t nd = make_no_detection(2, now,
                    camera_health, INF_STATUS_INFERENCE_ERROR);
                g_serial.send_no_detection(&nd, &g_tx_seq);
            }
        } else {
            ai_no_detection_payload_t nd = make_no_detection(2, now,
                camera_health, INF_STATUS_CAMERA_ERROR);
            g_serial.send_no_detection(&nd, &g_tx_seq);
        }
    }

    /* Periodic health heartbeat. */
    if (now - g_last_health_ms >= HEALTH_REPORT_INTERVAL_MS) {
        g_last_health_ms = now;
        ai_health_payload_t h;
        memset(&h, 0, sizeof(h));
        h.camera_health = g_camera.ok() ? 0 : 1;
        h.inference_status = g_inference.status();
        h.frames_captured = 0;   /* tracked internally; expose if needed */
        h.inferences_run = 0;
        h.inference_errors = 0;
        h.uptime_ms = now;
        g_serial.send_health(&h, &g_tx_seq);
    }

    g_serial.flush();
    /* No blocking delay; loop runs as fast as the inference interval allows. */
}

