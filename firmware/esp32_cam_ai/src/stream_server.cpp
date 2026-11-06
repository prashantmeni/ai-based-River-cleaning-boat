#include "stream_server.h"
#include "ai_config.h"

#include <WiFi.h>
#include <esp_camera.h>
#include <img_converters.h>
#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define STREAM_BOUNDARY "frame"
#define STREAM_CONTENT_TYPE \
    "multipart/x-mixed-replace;boundary=" STREAM_BOUNDARY
#define STREAM_PART_HEADER \
    "--" STREAM_BOUNDARY "\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n"

static httpd_handle_t g_stream_httpd = nullptr;
static volatile int g_stream_clients = 0;
static portMUX_TYPE s_stream_mux = portMUX_INITIALIZER_UNLOCKED;

bool stream_server_running() {
    return g_stream_httpd != nullptr;
}

static esp_err_t status_handler(httpd_req_t *req) {
    char page[768];
    IPAddress ip = WiFi.getMode() == WIFI_AP ? WiFi.softAPIP() : WiFi.localIP();
    int n = snprintf(page, sizeof(page),
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>RiverClean CAM</title></head><body "
        "style='font-family:monospace;background:#0B1220;color:#D7E2F0;"
        "padding:24px'>"
        "<h2 style='color:#19C7A1'>RiverClean AI vision unit</h2>"
        "<p>IP: %s<br>MAC: %s<br>Uptime: %lus<br>Stream clients: %d</p>"
        "<p><a style='color:#38BDF8' href='/stream'>/stream</a> (MJPEG, "
        "grayscale)</p></body></html>",
        ip.toString().c_str(), WiFi.macAddress().c_str(),
        (unsigned long)(millis() / 1000), (int)g_stream_clients);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, page, n);
}

static esp_err_t stream_handler(httpd_req_t *req) {
    esp_err_t err = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (err != ESP_OK) return err;
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control",
                       "no-store, no-cache, must-revalidate, max-age=0");

    portENTER_CRITICAL(&s_stream_mux);
    g_stream_clients++;
    portEXIT_CRITICAL(&s_stream_mux);

    while (true) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("[CAM][stream] fb get failed");
            err = ESP_FAIL;
            break;
        }

        uint8_t *jpg_buf = nullptr;
        size_t jpg_len = 0;
        bool ok = frame2jpg(fb, CAM_JPEG_QUALITY, &jpg_buf, &jpg_len);
        esp_camera_fb_return(fb);
        if (!ok || !jpg_buf || jpg_len == 0) {
            Serial.println("[CAM][stream] jpeg encode failed");
            err = ESP_FAIL;
            break;
        }

        char part[96];
        int plen = snprintf(part, sizeof(part), STREAM_PART_HEADER,
                            (unsigned)jpg_len);
        err = httpd_resp_send_chunk(req, part, plen);
        if (err == ESP_OK)
            err = httpd_resp_send_chunk(req, (const char *)jpg_buf, jpg_len);
        if (err == ESP_OK)
            err = httpd_resp_send_chunk(req, "\r\n", 2);
        free(jpg_buf);
        if (err != ESP_OK) break;   /* client disconnected */

        /* ~CAM_STREAM_FPS cap; also yields to other tasks. */
        vTaskDelay(pdMS_TO_TICKS(CAM_STREAM_FRAME_DELAY_MS));
    }

    portENTER_CRITICAL(&s_stream_mux);
    g_stream_clients--;
    portEXIT_CRITICAL(&s_stream_mux);
    return err;
}

bool stream_server_begin() {
    if (g_stream_httpd) return true;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = CAM_STREAM_PORT;
    config.ctrl_port = CAM_STREAM_PORT + 1;
    config.max_open_sockets = 3;
    config.backlog_conn = 2;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&g_stream_httpd, &config) != ESP_OK) {
        Serial.println("[CAM][stream] httpd start failed");
        g_stream_httpd = nullptr;
        return false;
    }

    static const httpd_uri_t status_uri = {
        .uri = "/", .method = HTTP_GET, .handler = status_handler,
        .user_ctx = nullptr};
    static const httpd_uri_t stream_uri = {
        .uri = "/stream", .method = HTTP_GET, .handler = stream_handler,
        .user_ctx = nullptr};
    httpd_register_uri_handler(g_stream_httpd, &status_uri);
    httpd_register_uri_handler(g_stream_httpd, &stream_uri);

    Serial.print("[CAM][stream] http server on port ");
    Serial.println(CAM_STREAM_PORT);
    return true;
}
