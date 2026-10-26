#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_camera.h"

bool camera_init(void);
// Capture one frame into the driver's internal buffer (RGB565).
bool camera_capture(void);
// Copy a resized RGB888 crop of the frame into dst (size*size*3 bytes).
// fx/fy/fw/fh are fractions of the full frame (0..1).
bool camera_crop_resize(float fx, float fy, float fw, float fh,
                        uint8_t* dst, int size);
int camera_frame_width(void);
int camera_frame_height(void);

// MJPEG streaming: call camera_stream_begin once when a client connects
// (switches sensor to JPEG QVGA, pauses AI grabs), then camera_jpeg_get/
// camera_jpeg_return per frame, then camera_stream_end when done.
camera_fb_t* camera_jpeg_get(void);
void camera_jpeg_return(camera_fb_t* fb);
void camera_stream_begin(void);
void camera_stream_end(void);
bool camera_streaming(void);
