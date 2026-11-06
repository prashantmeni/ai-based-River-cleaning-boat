#ifndef WS_GATEWAY_H
#define WS_GATEWAY_H

#include <Arduino.h>

/* WebSocket dashboard gateway (main controller).
 *
 * Runs on the WPA2 SoftAP started by espnow_link_begin(DASH_AP_SSID, ...)
 * on the same 2.4 GHz channel as ESP-NOW. Telemetry JSON produced by
 * telemetry.cpp is broadcast verbatim; commands arrive as text lines:
 *
 *   [#<command-id> ]CMD:<cmd>,<value>,<duration_ms>
 *
 * cmd: 1=STOP (value 0 stop, 1 remote E-STOP latch, 2 E-STOP clear),
 *      2=FORWARD, 3=TURN_LEFT, 4=TURN_RIGHT, 5=CONVEYOR_ON,
 *      6=CONVEYOR_OFF, 7=AUTONOMOUS_ON, 8=AUTONOMOUS_OFF.
 *
 * Every command is answered with:
 *   {"type":"ack","id":"...","status":"ok|rejected","cmd":N,"reason":"..."}
 *
 * The browser is NOT the safety controller: navigation/safety still
 * enforce watchdog, comms-loss and E-STOP locally.
 */

typedef void (*WsCommandFn)(uint8_t cmd, uint8_t value, uint16_t duration_ms);

void ws_gateway_set_command_handler(WsCommandFn fn);
void ws_gateway_begin();
void ws_gateway_update();                                  /* ws.loop() */
void ws_gateway_on_telemetry(const uint8_t *json, size_t len);
bool ws_gateway_has_clients();

#endif /* WS_GATEWAY_H */
