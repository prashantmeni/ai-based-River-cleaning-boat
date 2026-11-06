#include <Arduino.h>
#include <WebSocketsServer.h>
#include <string.h>
#include <stdio.h>
#include "boat_config.h"
#include "protocol.h"
#include "espnow_link.h"
#include "ws_gateway.h"

static WebSocketsServer s_ws(DASH_WS_PORT);
static WsCommandFn s_cmd_fn = nullptr;
static volatile bool s_has_clients = false;

void ws_gateway_set_command_handler(WsCommandFn fn) { s_cmd_fn = fn; }

/* Keep command ids identifier-safe: [A-Za-z0-9_-], max 23 chars. */
static void sanitize_id(const char *in, char *out, size_t out_size) {
    size_t j = 0;
    for (size_t i = 0; in && in[i] && j + 1 < out_size; i++) {
        char c = in[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_') {
            out[j++] = c;
        }
    }
    out[j] = '\0';
}

static void send_ack(uint8_t num, const char *id, const char *status,
                     int cmd, const char *reason) {
    char buf[192];
    snprintf(buf, sizeof(buf),
             "{\"type\":\"ack\",\"id\":\"%s\",\"status\":\"%s\",\"cmd\":%d,"
             "\"reason\":\"%s\"}",
             id, status, cmd, reason ? reason : "");
    s_ws.sendTXT(num, buf);
}

/* Text message: optional "#<id> " prefix + "CMD:<cmd>,<value>,<duration>". */
static void handle_text(uint8_t num, const uint8_t *payload, size_t len) {
    char line[96];
    size_t n = (len < sizeof(line) - 1) ? len : (sizeof(line) - 1);
    memcpy(line, payload, n);
    line[n] = '\0';

    char id[24] = "";
    char *body = line;
    if (line[0] == '#') {
        char *sp = strchr(line, ' ');
        if (sp) {
            size_t idlen = (size_t)(sp - line - 1);
            if (idlen > sizeof(id) - 1) idlen = sizeof(id) - 1;
            memcpy(id, line + 1, idlen);
            id[idlen] = '\0';
            body = sp + 1;
        }
    }
    char safe_id[24];
    sanitize_id(id, safe_id, sizeof(safe_id));

    if (strncmp(body, "CMD:", 4) == 0) {
        unsigned int cmd = 0, val = 0, dur = 0;
        if (sscanf(body + 4, "%u,%u,%u", &cmd, &val, &dur) == 3 &&
            cmd >= DIAG_CMD_STOP && cmd <= DIAG_CMD_AUTONOMOUS_OFF &&
            val <= 255 && dur <= 60000 && s_cmd_fn) {
            s_cmd_fn((uint8_t)cmd, (uint8_t)val, (uint16_t)dur);
            send_ack(num, safe_id, "ok", (int)cmd, "");
            return;
        }
        send_ack(num, safe_id, "rejected", 0, "invalid command");
        return;
    }
    send_ack(num, safe_id, "rejected", 0, "unknown message");
}

static void on_ws_event(uint8_t num, WStype_t type, uint8_t *payload,
                        size_t len) {
    switch (type) {
        case WStype_CONNECTED: {
            s_has_clients = true;
            char hello[224];
            snprintf(hello, sizeof(hello),
                     "{\"type\":\"hello\",\"protocol_version\":1,"
                     "\"device_id\":\"riverclean_main\",\"ws_port\":%d,"
                     "\"espnow_channel\":%d,\"server\":\"riverclean-ws\"}",
                     DASH_WS_PORT, ESPNOW_CHANNEL);
            s_ws.sendTXT(num, hello);
            break;
        }
        case WStype_DISCONNECTED:
            s_has_clients = (s_ws.connectedClients() > 0);
            break;
        case WStype_TEXT:
            handle_text(num, payload, len);
            break;
        default:
            break;
    }
}

void ws_gateway_begin() {
    s_ws.begin();
    s_ws.onEvent(on_ws_event);
}

void ws_gateway_update() {
    s_ws.loop();
}

void ws_gateway_on_telemetry(const uint8_t *json, size_t len) {
    if (s_ws.connectedClients() == 0 || !json || len == 0) return;
    s_ws.broadcastTXT(json, len);
}

bool ws_gateway_has_clients() {
    return s_ws.connectedClients() > 0;
}
