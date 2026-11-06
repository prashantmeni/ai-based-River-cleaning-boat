#ifndef STREAM_SERVER_H
#define STREAM_SERVER_H

/* ========================================================================
 * MJPEG streaming HTTP server for the ESP32-CAM AI vision unit.
 *
 * Runs on the ESP-IDF httpd server (own FreeRTOS task), so the main loop
 * keeps running inference while a browser watches /stream.
 *
 * Endpoints (port CAM_STREAM_PORT):
 *   GET /         tiny HTML status page (IP, MAC, uptime, camera state)
 *   GET /stream   multipart/x-mixed-replace MJPEG (grayscale JPEG frames)
 *
 * Network contract: the CAM first tries to join the main controller's
 * SoftAP (CAM_STA_SSID, channel ESPNOW_CHANNEL) so the dashboard laptop
 * reaches the stream over the SAME Wi-Fi network it uses for the
 * WebSocket gateway. If the AP is not found, a fallback SoftAP
 * (CAM_FALLBACK_AP_SSID) is started for stream-only use. ESP-NOW keeps
 * working in both cases (single radio, fixed channel 1).
 * ======================================================================== */

#include <Arduino.h>
#include <stdint.h>

/* Start the HTTP server. Returns true when listening.
 * Call AFTER the camera initialised and Wi-Fi has an IP (STA or SoftAP). */
bool stream_server_begin();

/* true once httpd is running. */
bool stream_server_running();

#endif /* STREAM_SERVER_H */
