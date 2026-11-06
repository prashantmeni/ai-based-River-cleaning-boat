/* ========================================================================
 * ESP32-CAM AI vision unit — main entry point.
 *
 * Captures camera frames, preprocesses them to 96x96 grayscale, runs the
 * on-device computer-vision detector (blob segmentation + shape-based
 * classification), applies confidence + temporal filtering, and transmits
 * structured detection/no-detection/health messages over ESP-NOW to the
 * main ESP32 controller.
 *
 * Also joins the site router (or the dashboard SoftAP / a fallback AP)
 * for time sync / diagnostics. There is NO video stream: detections and
 * health only.
 *
 * No inference runs on the main controller.
 * ======================================================================== */

#include <Arduino.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <WiFi.h>
#include "ai_config.h"
#include "camera.h"
#include "preprocess.h"
#include "inference.h"
#include "detection.h"
#include "serial_tx.h"
#include "stream_server.h"
#include "protocol.h"
#include "espnow_link.h"

static Camera g_camera;
static Inference g_inference;
static SerialTx g_serial;

static uint16_t g_tx_seq = 0;
static uint32_t g_last_inference_ms = 0;
static uint32_t g_last_health_ms = 0;

/* Wi-Fi config received from the main controller over ESP-NOW.
 * Reconnect STA to the provisioned network so both boards stay on the
 * same router. The stream server keeps running on all interfaces. */
static void on_wifi_config(const char *ssid, const char *password) {
    Serial.print("[CAM] Wi-Fi config received: SSID='");
    Serial.print(ssid);
    Serial.println("'");
    WiFi.begin(ssid, password);
}

static void print_status(const char *msg) {
    CAM_UART_PORT.print("[CAM] ");
    CAM_UART_PORT.println(msg);
}

/* Join the site router first, then the main controller's SoftAP, then
 * start a local fallback AP. ESP-NOW was already started and follows the
 * active channel. Wi-Fi is kept only for network presence — detections
 * travel over ESP-NOW. */
static bool try_join(const char *ssid, const char *pass, uint32_t timeout_ms) {
    WiFi.begin(ssid, pass);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - t0 < timeout_ms) {
        delay(250);
    }
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        return false;
    }
    Serial.print("[CAM] joined '");
    Serial.print(ssid);
    Serial.print("' ip=");
    Serial.println(WiFi.localIP());
    return true;
}

static void network_bringup() {
    WiFi.setAutoReconnect(true);
    bool joined = try_join(CAM_ROUTER_SSID, CAM_ROUTER_PASS,
                           CAM_ROUTER_TIMEOUT_MS);
    if (!joined) {
        joined = try_join(CAM_STA_SSID, CAM_STA_PASS, CAM_STA_TIMEOUT_MS);
    }

    if (!joined) {
        Serial.print("[CAM] no STA found — fallback AP '"
                     CAM_FALLBACK_AP_SSID "'");
        if (WiFi.softAP(CAM_FALLBACK_AP_SSID, CAM_FALLBACK_AP_PASS,
                        ESPNOW_CHANNEL, 0, 4)) {
            Serial.print(" ip=");
            Serial.println(WiFi.softAPIP());
        } else {
            print_status("fallback AP FAILED — offline, ESP-NOW still active");
            return;
        }
    }
}

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout detector

    pinMode(STATUS_LED_GPIO, OUTPUT);
    digitalWrite(STATUS_LED_GPIO, LOW);

    CAM_UART_PORT.begin(CAM_UART_BAUD);
    print_status("booting ESP32-CAM AI vision unit");

    /* Camera init. If it fails, keep reporting camera_health != 0. */
    if (!g_camera.begin()) {
        print_status(g_camera.last_error());
    } else {
        print_status("camera initialized");
    }

    /* On-device CV detector (no model file required). */
    if (!g_inference.begin()) {
        print_status("CV detector FAILED to start");
    } else {
        print_status("CV detector ready (blob + shape classifier)");
    }

    if (g_serial.begin()) {
        char mac[20];
        espnow_link_mac_str(mac, sizeof(mac));
        Serial.print("[CAM] esp-now link up mac=");
        Serial.println(mac);
        g_serial.set_wifi_config_callback(on_wifi_config);
    } else {
        print_status("esp-now link FAILED");
    }

    network_bringup();

    /* Start the MJPEG HTTP stream server (/stream on port 8080). */
    if (!stream_server_begin()) {
        print_status("stream server FAILED to start");
    }

    Serial.println("[CAM] init complete.");
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

