#ifndef AI_CONFIG_H
#define AI_CONFIG_H

/* ========================================================================
 * ESP32-CAM AI vision unit configuration.
 * Edit values to match your trained model and hardware.
 * ======================================================================== */

/* --- Debug console (UART0 on ESP32-CAM: GPIO1 TX, GPIO3 RX) ---
 * Link to the main controller uses ESP-NOW wireless (espnow_link.h);
 * UART0 is debug output only. */
#define CAM_UART_BAUD              115200
#define CAM_UART_PORT              Serial

/* --- Camera --- */
#define CAM_FRAME_WIDTH            160
#define CAM_FRAME_HEIGHT           120
#define CAM_PIXEL_FORMAT           PIXFORMAT_GRAYSCALE  /* Compatible with GC2145 / M21-45 when driver accepts it */
#define CAM_FRAME_SIZE             FRAMESIZE_QQVGA      /* 160x120 (fits DRAM without PSRAM) */
#define CAM_JPEG_QUALITY           12
#define CAM_FB_COUNT               2                    /* 2 buffers: inference loop + MJPEG stream task */
#define CAM_XCLK_FREQ_HZ           20000000

/* --- Wi-Fi / MJPEG streaming ---
 * The CAM first tries the site router (same SSID the main controller's
 * STA uses) so the dashboard laptop reaches the stream (port 8080) over
 * the home/office network while cloud MQTT stays up. If the router is
 * not reachable it joins the main controller's dashboard SoftAP (video
 * only when the laptop is on that AP), and finally starts a fallback
 * SoftAP. ESP-NOW follows the active channel via espnow_link_resync(). */
#define CAM_ROUTER_SSID            "Laxmi_Venkateshwar_Krupa"               /* same as main ROUTER_SSID */
#define CAM_ROUTER_PASS            "Menis@2020"          /* same as main ROUTER_PASS */
#define CAM_ROUTER_TIMEOUT_MS      15000
#define CAM_STA_SSID               "RiverClean-Boat"    /* main's DASH_AP_SSID */
#define CAM_STA_PASS               "riverclean2026"     /* main's DASH_AP_PASS */
#define CAM_STA_TIMEOUT_MS         15000
#define CAM_FALLBACK_AP_SSID       "RiverClean-CAM"
#define CAM_FALLBACK_AP_PASS       "riverclean2026"
#define CAM_MDNS_HOSTNAME          "rivercam"           /* http://rivercam.local:8080/stream */
#define CAM_STREAM_PORT            8080
#define CAM_STREAM_FRAME_DELAY_MS  66                   /* ~15 fps cap */

/* --- AI model --- */
#define MODEL_INPUT_WIDTH          96
#define MODEL_INPUT_HEIGHT         96
#define MODEL_INPUT_CHANNELS       1                    /* grayscale */
#define MODEL_CONFIDENCE_THRESHOLD 70                   /* 0..100 */
#define CONSECUTIVE_DETECTIONS     2                    /* temporal filter */
#define INFERENCE_INTERVAL_MS      150                  /* ms between runs */

/* --- Health reporting --- */
#define HEALTH_REPORT_INTERVAL_MS  2000

/* --- Detection classes (PLACEHOLDER — replace with trained labels) --- */
#define CLASS_BACKGROUND           0
#define CLASS_PLASTIC_BOTTLE       1
#define CLASS_PLASTIC_BAG          2
#define CLASS_GENERAL_DEBRIS       3
#define NUM_CLASSES                4

/* --- Serial status LED --- */
#define STATUS_LED_GPIO            33   /* ESP32-CAM built-in LED */

#endif /* AI_CONFIG_H */
