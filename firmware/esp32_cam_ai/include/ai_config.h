#ifndef AI_CONFIG_H
#define AI_CONFIG_H

/* ========================================================================
 * ESP32-CAM AI vision unit configuration.
 * Edit values to match your trained model and hardware.
 * ======================================================================== */

/* --- Serial link to main controller --- */
#define CAM_UART_BAUD              115200
#define CAM_UART_PORT              Serial   /* UART0 on ESP32-CAM (GPIO1 TX, GPIO3 RX) */

/* --- Camera --- */
#define CAM_FRAME_WIDTH            96
#define CAM_FRAME_HEIGHT           96
#define CAM_PIXEL_FORMAT           PIXFORMAT_GRAYSCALE  /* see esp_camera.h */
#define CAM_FRAME_SIZE             FRAMESIZE_96X96      /* must be supported */
#define CAM_JPEG_QUALITY           12
#define CAM_FB_COUNT               1                    /* reuse single buffer */
#define CAM_XCLK_FREQ_HZ           20000000

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
