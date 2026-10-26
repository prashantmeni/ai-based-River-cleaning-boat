#pragma once
// ============================================================
// River Cleaning Boat - ESP32-CAM vision controller config
// Board: AI-Thinker ESP32-CAM (OV2640 + PSRAM)
// Boat/motor/sensor parameters live in
// firmware/esp32_main/include/config_main.h (dual-controller).
// ============================================================

// ---- Inter-controller UART link (see docs/communication_protocol.md) ----
// GPIO13/14 are microSD-slot pins (SD not used) and non-strapping.
// GPIO16 excluded (PSRAM CS); GPIO12/15 excluded (strapping risk).
#define PIN_LINK_RX 13   // ESP32-CAM RX <- main controller TX
#define PIN_LINK_TX 14   // ESP32-CAM TX -> main controller RX

// ---- Wi-Fi / dashboard ----
#define WIFI_AP_SSID "RiverBot-AP"
#define WIFI_AP_PASS "cleanriver"
#define DASHBOARD_PORT 80

// ---- Camera ----
#define CAMERA_MODEL_AI_THINKER
#define CAM_XCLK_FREQ 20000000
#define CAM_FRAME_SIZE FRAMESIZE_QVGA  // 320x240
#define CAM_PIXEL_FORMAT PIXFORMAT_RGB565
#define INFERENCE_SIZE 96              // model input edge length

// ---- AI model ----
#define NUM_CLASSES 5
static const char* const CLASS_NAMES[NUM_CLASSES] = {
    "plastic_bottle",
    "plastic_wrapper",
    "leaf_or_organic_debris",
    "other_visible_waste",
    "background_or_no_waste",
};
#define DETECT_CONF_THRESHOLD 0.55f
#define REGION_LEFT_END   0.33f
#define REGION_RIGHT_START 0.67f

// ---- Timing ----
#define CONTROL_LOOP_MS 100
// AI invoke ~680 ms; 1500 ms period => ~45% duty (heat guard, see below).
#define AI_PERIOD_MS 1500
#define DASHBOARD_JSON_PERIOD_MS 500
#define LINK_HB_PERIOD_MS 1000

// ---- Bench diagnostics (set 0 for field builds) ----
#define LINK_USB_MIRROR 0          // mirror protocol on USB console + accept injected msgs
#define LINK_UART1_LOOPBACK_TEST 0 // boot-time UART1 TX->RX peripheral self-test

// ---- Safety ----
#define WATCHDOG_TIMEOUT_S 30

// ---- Thermal protection (internal TSENS reads ~10-15 C cooler than the
// AMS1117/camera surface; levels engage at touch-safe die temps.) ----
#define THERMAL_WARN_C          58.0f  // level 1: throttle AI cadence
#define THERMAL_LIMIT_C         70.0f  // level 2: pause AI until it cools
#define THERMAL_RESUME_C        50.0f  // hysteresis: level 0 below this
#define THERMAL_MIN_INTERVAL_MS 2000   // throttled minimum gap between AI passes
