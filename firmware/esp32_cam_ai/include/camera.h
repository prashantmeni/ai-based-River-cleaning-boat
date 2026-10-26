#ifndef CAMERA_H
#define CAMERA_H

#include <Arduino.h>
#include "esp_camera.h"
#include "ai_config.h"

/* Camera health state. */
class Camera {
public:
    Camera();
    ~Camera();

    /* Initialise the OV2640. Returns true on success. */
    bool begin();

    /* Capture a frame into an internal reused buffer.
     * Returns true if a valid frame is available. frame_buf/frame_len are
     * set to the internal buffer (do not free). */
    bool capture(uint8_t **frame_buf, size_t *frame_len);

    /* Return last capture status / error message. */
    const char *last_error() const { return _last_error; }
    bool ok() const { return _initialized; }

    /* Sensor dimensions of last captured frame. */
    int width() const { return _width; }
    int height() const { return _height; }

private:
    camera_config_t _config;
    bool _initialized;
    uint8_t *_work_buf;      /* reused working buffer (grayscale) */
    size_t _work_buf_size;
    int _width, _height;
    char _last_error[96];
    uint32_t _frames_captured;
};

#endif /* CAMERA_H */
