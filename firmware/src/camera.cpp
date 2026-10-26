#include "camera.h"
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <esp_camera.h>
#include <img_converters.h>
#include <esp_heap_caps.h>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Primary: RGB565 QVGA in PSRAM (this board's sensor cannot output JPEG —
// esp32-camera reports "JPEG format is not supported on this sensor", so
// the stream path software-encodes with frame2jpg() in main.cpp).
// Fallback: JPEG QQVGA in DRAM (PSRAM-less clone boards).
static bool g_jpeg_fallback = false;
static uint8_t* g_rgb888 = nullptr;   // decode scratch for fallback path

// Streaming gate: while true only the stream task grabs buffers; the AI
// path (camera_crop_resize) returns false so inference pauses.
static volatile bool g_streaming = false;
static SemaphoreHandle_t g_cam_mutex = nullptr;

static camera_config_t make_config(bool jpeg_fallback) {
    camera_config_t c = {};
    c.ledc_channel = LEDC_CHANNEL_0;
    c.ledc_timer   = LEDC_TIMER_0;
    c.pin_d0       = 5;
    c.pin_d1       = 18;
    c.pin_d2       = 19;
    c.pin_d3       = 21;
    c.pin_d4       = 36;
    c.pin_d5       = 39;
    c.pin_d6       = 34;
    c.pin_d7       = 35;
    c.pin_xclk     = 0;
    c.pin_pclk     = 22;
    c.pin_vsync    = 25;
    c.pin_href     = 23;
    c.pin_sccb_sda = 26;
    c.pin_sccb_scl = 27;
    c.pin_pwdn     = 32;
    c.pin_reset    = -1;
    c.xclk_freq_hz = CAM_XCLK_FREQ;
    c.grab_mode    = CAMERA_GRAB_LATEST;
    c.jpeg_quality = 12;
    if (jpeg_fallback) {
        c.pixel_format = PIXFORMAT_JPEG;
        c.frame_size   = FRAMESIZE_QQVGA;   // 160x120, ~8-15KB JPEG, DRAM-safe
        c.fb_location  = CAMERA_FB_IN_DRAM;
        c.fb_count     = 1;
    } else {
        c.pixel_format = CAM_PIXEL_FORMAT;  // RGB565
        c.frame_size   = CAM_FRAME_SIZE;    // QVGA
        c.fb_location  = CAMERA_FB_IN_PSRAM;
        c.fb_count     = 1;
    }
    return c;
}

static void apply_sensor_tweaks(void) {
    sensor_t* s = esp_camera_sensor_get();
    if (!s) return;
    s->set_brightness(s, 0);
    s->set_contrast(s, 1);
    s->set_saturation(s, 0);
    s->set_whitebal(s, 1);
    s->set_exposure_ctrl(s, 1);
}

bool camera_init(void) {
    g_cam_mutex = xSemaphoreCreateMutex();
    if (!g_cam_mutex) return false;
    camera_config_t cfg = make_config(false);
    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        Serial.printf("[cam] PSRAM RGB565 init failed (0x%x); psramFound=%d spiram_free=%u\n",
                      (unsigned)err, (int)psramFound(),
                      (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        camera_config_t cfg2 = make_config(true);
        err = esp_camera_init(&cfg2);
        if (err != ESP_OK) {
            Serial.printf("[cam] DRAM JPEG init also failed (0x%x)\n", (unsigned)err);
            return false;
        }
        g_jpeg_fallback = true;
        g_rgb888 = (uint8_t*)malloc(160 * 120 * 3);
        if (!g_rgb888) return false;
        Serial.println("[cam] using JPEG QQVGA DRAM fallback");
    }
    if (!esp_camera_sensor_get()) return false;
    apply_sensor_tweaks();
    return true;
}

bool camera_capture(void) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return false;
    esp_camera_fb_return(fb);
    return true;
}

static inline uint8_t r565_r(uint16_t p) { return (uint8_t)(((p >> 11) & 0x1F) << 3); }
static inline uint8_t r565_g(uint16_t p) { return (uint8_t)(((p >> 5) & 0x3F) << 2); }
static inline uint8_t r565_b(uint16_t p) { return (uint8_t)((p & 0x1F) << 3); }

// generic crop+resize from an RGB888 source into dst (size*size*3 bytes)
static bool crop_resize_rgb888(const uint8_t* rgb, int W, int H,
                               float fx, float fy, float fw, float fh,
                               uint8_t* dst, int size) {
    int x0 = (int)(fx * W), y0 = (int)(fy * H);
    int x1 = (int)((fx + fw) * W), y1 = (int)((fy + fh) * H);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;
    int cw = x1 - x0, ch = y1 - y0;
    if (cw <= 0 || ch <= 0) return false;
    for (int y = 0; y < size; y++) {
        int sy = y0 + (y * ch) / size;
        const uint8_t* row = rgb + (size_t)sy * W * 3;
        for (int x = 0; x < size; x++) {
            int sx = x0 + (x * cw) / size;
            memcpy(dst + (y * size + x) * 3, row + (size_t)sx * 3, 3);
        }
    }
    return true;
}

bool camera_crop_resize(float fx, float fy, float fw, float fh,
                        uint8_t* dst, int size) {
    if (!g_cam_mutex) return false;
    xSemaphoreTake(g_cam_mutex, portMAX_DELAY);
    if (g_streaming) {           // buffers belong to the stream task
        xSemaphoreGive(g_cam_mutex);
        return false;
    }
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb || !fb->buf) {
        if (fb) esp_camera_fb_return(fb);
        xSemaphoreGive(g_cam_mutex);
        return false;
    }
    bool ok = false;
    if (fb->format == PIXFORMAT_RGB565) {
        int W = fb->width, H = fb->height;
        int x0 = (int)(fx * W), y0 = (int)(fy * H);
        int x1 = (int)((fx + fw) * W), y1 = (int)((fy + fh) * H);
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > W) x1 = W;
        if (y1 > H) y1 = H;
        int cw = x1 - x0, ch = y1 - y0;
        if (cw > 0 && ch > 0) {
            const uint16_t* src = (const uint16_t*)fb->buf;
            for (int y = 0; y < size; y++) {
                int sy = y0 + (y * ch) / size;
                const uint16_t* row = src + (size_t)sy * W;
                for (int x = 0; x < size; x++) {
                    int sx = x0 + (x * cw) / size;
                    uint16_t p = row[sx];
                    uint8_t* d = dst + (y * size + x) * 3;
                    d[0] = r565_r(p);
                    d[1] = r565_g(p);
                    d[2] = r565_b(p);
                }
            }
            ok = true;
        }
    } else if (fb->format == PIXFORMAT_JPEG && g_rgb888) {
        if (fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, g_rgb888)) {
            ok = crop_resize_rgb888(g_rgb888, fb->width, fb->height,
                                    fx, fy, fw, fh, dst, size);
        }
    }
    esp_camera_fb_return(fb);
    xSemaphoreGive(g_cam_mutex);
    return ok;
}

int camera_frame_width(void)  { return g_jpeg_fallback ? 160 : 320; }
int camera_frame_height(void) { return g_jpeg_fallback ? 120 : 240; }

// MJPEG streaming: grab one raw frame while streaming; main.cpp encodes
// it (frame2jpg) or passes JPEG straight through on fallback boards.
camera_fb_t* camera_jpeg_get(void) {
    if (!g_streaming) return nullptr;
    return esp_camera_fb_get();
}

void camera_jpeg_return(camera_fb_t* fb) {
    if (fb) esp_camera_fb_return(fb);
}

bool camera_streaming(void) { return g_streaming; }

void camera_stream_begin(void) {
    if (!g_cam_mutex) return;
    xSemaphoreTake(g_cam_mutex, portMAX_DELAY);
    g_streaming = true;
    xSemaphoreGive(g_cam_mutex);
    Serial.println("[stream] on (RGB565 + software JPEG); AI paused");
}

void camera_stream_end(void) {
    if (!g_cam_mutex) return;
    xSemaphoreTake(g_cam_mutex, portMAX_DELAY);
    g_streaming = false;
    xSemaphoreGive(g_cam_mutex);
    Serial.println("[stream] closed; AI resumed");
}
