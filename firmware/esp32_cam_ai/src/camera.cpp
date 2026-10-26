#include "camera.h"

/* AI-Thinker ESP32-CAM camera pinout (hardcoded, since the PlatformIO
 * espressif32 package does not expose the Y*_GPIO_NUM macros). */
#define CAM_PIN_D0      4
#define CAM_PIN_D1      5
#define CAM_PIN_D2      18
#define CAM_PIN_D3      19
#define CAM_PIN_D4      21
#define CAM_PIN_D5      36
#define CAM_PIN_D6      39
#define CAM_PIN_D7      34
#define CAM_PIN_XCLK    0
#define CAM_PIN_PCLK    22
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_SDA     26
#define CAM_PIN_SCL     27
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET  -1

Camera::Camera()
    : _work_buf(nullptr), _work_buf_size(0),
      _width(0), _height(0), _frames_captured(0) {
    _last_error[0] = '\0';
    _initialized = false;

    _config.ledc_channel = LEDC_CHANNEL_0;
    _config.ledc_timer = LEDC_TIMER_0;
    _config.pin_d0 = CAM_PIN_D0;
    _config.pin_d1 = CAM_PIN_D1;
    _config.pin_d2 = CAM_PIN_D2;
    _config.pin_d3 = CAM_PIN_D3;
    _config.pin_d4 = CAM_PIN_D4;
    _config.pin_d5 = CAM_PIN_D5;
    _config.pin_d6 = CAM_PIN_D6;
    _config.pin_d7 = CAM_PIN_D7;
    _config.pin_xclk = CAM_PIN_XCLK;
    _config.pin_pclk = CAM_PIN_PCLK;
    _config.pin_vsync = CAM_PIN_VSYNC;
    _config.pin_href = CAM_PIN_HREF;
    _config.pin_sccb_sda = CAM_PIN_SDA;
    _config.pin_sccb_scl = CAM_PIN_SCL;
    _config.pin_pwdn = CAM_PIN_PWDN;
    _config.pin_reset = CAM_PIN_RESET;
    _config.xclk_freq_hz = CAM_XCLK_FREQ_HZ;
    _config.pixel_format = CAM_PIXEL_FORMAT;
    _config.frame_size = CAM_FRAME_SIZE;
    _config.jpeg_quality = CAM_JPEG_QUALITY;
    _config.fb_count = CAM_FB_COUNT;
    _config.fb_location = CAMERA_FB_IN_DRAM;
    _config.grab_mode = CAMERA_GRAB_LATEST;
}

Camera::~Camera() {
    if (_work_buf) { free(_work_buf); _work_buf = nullptr; }
}

bool Camera::begin() {
    esp_err_t err = esp_camera_init(&_config);
    if (err != ESP_OK) {
        snprintf(_last_error, sizeof(_last_error),
                 "camera init failed 0x%X", (unsigned)err);
        _initialized = false;
        return false;
    }
    _initialized = true;
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_framesize(s, (framesize_t)CAM_FRAME_SIZE);
        s->set_pixformat(s, CAM_PIXEL_FORMAT);
    }

    /* Allocate a reused working buffer for preprocessing. */
    _work_buf_size = (size_t)MODEL_INPUT_WIDTH * MODEL_INPUT_HEIGHT *
                     MODEL_INPUT_CHANNELS;
    _work_buf = (uint8_t *)malloc(_work_buf_size);
    if (!_work_buf) {
        snprintf(_last_error, sizeof(_last_error), "work buffer alloc failed");
        _initialized = false;
        return false;
    }

    _width = MODEL_INPUT_WIDTH;
    _height = MODEL_INPUT_HEIGHT;
    return true;
}

bool Camera::capture(uint8_t **frame_buf, size_t *frame_len) {
    if (!_initialized) { snprintf(_last_error, sizeof(_last_error), "camera not initialized"); return false; }

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) { snprintf(_last_error, sizeof(_last_error), "frame buffer get failed"); return false; }

    /* The driver returns JPEG or raw depending on format. With grayscale
     * raw format, fb->buf is the Y plane. Copy into the reused work buffer. */
    size_t copy_len = fb->len;
    if (copy_len > _work_buf_size) copy_len = _work_buf_size;
    memcpy(_work_buf, fb->buf, copy_len);

    *frame_buf = _work_buf;
    *frame_len = copy_len;
    _width = fb->width;
    _height = fb->height;
    _frames_captured++;

    esp_camera_fb_return(fb);
    _last_error[0] = '\0';
    return true;
}
