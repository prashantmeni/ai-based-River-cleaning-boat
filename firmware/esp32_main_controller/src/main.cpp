/* ========================================================================
 * Standard ESP32 main controller — entry point.
 *
 * Receives AI detections from the ESP32-CAM over validated serial,
 * runs the autonomous navigation state machine, controls motors/conveyor,
 * reads ultrasonic + GPS, enforces safety (emergency stop, watchdog,
 * comms-loss safe state), and exposes telemetry.
 *
 * No AI inference runs here. Motors default OFF at boot.
 * ======================================================================== */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "boat_config.h"
#include "motor.h"
#include "obstacle.h"
#include "gps.h"
#include "tds.h"
#include "conveyor.h"
#include "safety.h"
#include "serial_rx.h"
#include "navigation.h"
#include "telemetry.h"
#include "espnow_link.h"
#include "ws_gateway.h"

/* --- Hardware instances --- */
static MotorDriver g_motors;
static ObstacleSensors g_obs;
static GPS g_gps;
static TdsSensor g_tds;
static Conveyor g_conveyor;
static Safety g_safety;
static SerialRx g_rx;                   /* ESP-NOW link <- ESP32-CAM */
static Navigation g_nav;
static Telemetry g_telemetry;

/* --- Serial callbacks --- */
static void on_ai_detection(const ai_detection_payload_t *det) {
    g_nav.on_detection(det);
}
static void on_ai_no_detection(const ai_no_detection_payload_t *nd) {
    g_nav.on_no_detection(nd);
}
static void on_ai_health(const ai_health_payload_t *h) {
    g_nav.on_health(h);
}

static void on_ai_diag_command(uint8_t cmd, uint8_t value, uint16_t duration_ms) {
    Serial.print("[MAIN] diag cmd="); Serial.print(cmd);
    Serial.print(" val="); Serial.print(value);
    Serial.print(" dur="); Serial.println(duration_ms);
    g_nav.on_diag_command(cmd, value, duration_ms);
}

static WiFiClient s_wifi_client;
static PubSubClient s_mqtt_client(s_wifi_client);
static uint32_t s_last_wifi_log_ms = 0;
static uint32_t s_last_mqtt_attempt_ms = 0;
static uint32_t s_last_mqtt_pub_err_ms = 0;

/* Deferred MQTT command ack: the callback only records the result and
 * maintain_mqtt() publishes it after loop() returns (PubSubClient publish
 * is not re-entrant safe from inside the callback). */
static struct {
    bool pending;
    char id[24];
    bool ok;
    unsigned int cmd;
} s_pending_ack = {false, "", false, 0};

static void publish_mqtt_ack() {
    if (!s_pending_ack.pending || !s_mqtt_client.connected()) return;
    char ack[160];
    int n;
    if (s_pending_ack.ok) {
        n = snprintf(ack, sizeof(ack),
                     "{\"type\":\"ack\",\"id\":\"%s\",\"status\":\"ok\",\"cmd\":%u}",
                     s_pending_ack.id, s_pending_ack.cmd);
    } else {
        n = snprintf(ack, sizeof(ack),
                     "{\"type\":\"ack\",\"id\":\"%s\",\"status\":\"rejected\","
                     "\"cmd\":%u,\"reason\":\"invalid command\"}",
                     s_pending_ack.id, s_pending_ack.cmd);
    }
    s_pending_ack.pending = false;
    if (n > 0 && n < (int)sizeof(ack)) {
        s_mqtt_client.publish(MQTT_ACK_TOPIC, ack);
    }
}

/* Commands may carry a client correlation id ("#<id> CMD:<cmd>,<val>,<dur>")
 * exactly like the WebSocket gateway accepts; every accepted or rejected
 * command is acked on MQTT_ACK_TOPIC so clients never see a false timeout. */
static void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    if (strcmp(topic, MQTT_COMMAND_TOPIC) != 0) {
        return;
    }
    char buf[96];
    unsigned int len = (length < sizeof(buf) - 1) ? length : (sizeof(buf) - 1);
    memcpy(buf, payload, len);
    buf[len] = '\0';

    Serial.print("[MQTT] Received command payload: ");
    Serial.println(buf);

    char id[24] = "";
    char* body = buf;
    if (buf[0] == '#') {
        char* sp = strchr(buf, ' ');
        if (sp != NULL && sp > buf + 1) {
            size_t idlen = (size_t)(sp - buf - 1);
            if (idlen > sizeof(id) - 1) idlen = sizeof(id) - 1;
            memcpy(id, buf + 1, idlen);
            id[idlen] = '\0';
            body = sp + 1;
        }
    }

    unsigned int cmd = 0, val = 0, dur = 0;
    bool ok = strncmp(body, "CMD:", 4) == 0 &&
              sscanf(body + 4, "%u,%u,%u", &cmd, &val, &dur) == 3 &&
              cmd >= DIAG_CMD_STOP && cmd <= DIAG_CMD_AUTONOMOUS_OFF;
    if (ok) {
        g_nav.on_diag_command((uint8_t)cmd, (uint8_t)val, (uint16_t)dur);
        Serial.println("[MQTT] Command executed successfully.");
    } else {
        Serial.println("[MQTT] Command rejected (invalid payload).");
    }
    s_pending_ack.pending = true;
    memcpy(s_pending_ack.id, id, sizeof(id));
    s_pending_ack.ok = ok;
    s_pending_ack.cmd = cmd;
}

static void maintain_mqtt() {
    if (!s_mqtt_client.connected()) {
        if (millis() - s_last_mqtt_attempt_ms < 3000) return;
        s_last_mqtt_attempt_ms = millis();
        Serial.print("[MQTT] Attempting connection to broker.hivemq.com...");
        // Create random client ID
        String clientId = "RiverCleanBoat-" + String(random(0xffff), HEX);
        if (s_mqtt_client.connect(clientId.c_str())) {
            Serial.println(" connected!");
            s_mqtt_client.subscribe(MQTT_COMMAND_TOPIC);
            Serial.print("[MQTT] Subscribed to topic: ");
            Serial.println(MQTT_COMMAND_TOPIC);
        } else {
            Serial.print(" failed, rc=");
            Serial.print(s_mqtt_client.state());
            Serial.println(" try again in 5 seconds");
        }
    }
    s_mqtt_client.loop();
    publish_mqtt_ack();
}
static uint16_t g_tx_seq = 0;
static uint32_t g_last_telemetry_ms = 0;
static const uint32_t TELEMETRY_INTERVAL_MS = 500;

/* Dashboard manual commands arrive as ASCII lines over USB serial:
 *   CMD:<cmd>,<value>,<duration_ms>\n
 * Commands are bounded (duration defaults to 1 s inside navigation). */
static void poll_usb_commands() {
    static char line[64];
    static size_t len = 0;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n') {
            if (len > 0) {
                line[len] = '\0';
                if (strncmp(line, "CMD:", 4) == 0) {
                    unsigned int cmd = 0, val = 0, dur = 0;
                    if (sscanf(line + 4, "%u,%u,%u", &cmd, &val, &dur) == 3 &&
                        cmd >= DIAG_CMD_STOP && cmd <= DIAG_CMD_AUTONOMOUS_OFF) {
                        g_nav.on_diag_command((uint8_t)cmd, (uint8_t)val,
                                              (uint16_t)dur);
                    }
                }
                len = 0;
            }
        } else if (len < sizeof(line) - 1) {
            line[len++] = c;
        } else {
            len = 0;    /* oversized line: drop */
        }
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }
    Serial.println("[MAIN] booting standard ESP32 controller");

    pinMode(STATUS_LED_RED_GPIO, OUTPUT);
    pinMode(STATUS_LED_GREEN_GPIO, OUTPUT);
    pinMode(STATUS_LED_BLUE_GPIO, OUTPUT);
    digitalWrite(STATUS_LED_RED_GPIO, LOW);
    digitalWrite(STATUS_LED_GREEN_GPIO, LOW);
    digitalWrite(STATUS_LED_BLUE_GPIO, LOW);

    /* Safety first: motors/conveyor OFF until explicitly enabled. */
    g_safety.begin();
    g_motors.begin();      /* leaves outputs off, standby ON after init */
    g_conveyor.begin();    /* OFF */
    /* GPS init first: its TX pin (GPIO13) is shared with the right
     * ultrasonic TRIG, so obs.begin() runs after it to re-claim GPIO13
     * as a plain output. GPS is receive-only (NMEA on GPIO4); no wire
     * is needed on its TX side. */
    g_gps.begin(Serial1, GPS_BAUD, GPS_UART_RX_GPIO, GPS_UART_TX_GPIO);
    g_obs.begin();
    g_tds.begin(TDS_ADC_GPIO);   /* analog TDS (SEN0244) on ADC1 */

    /* Wire navigation dependencies. */
    g_nav.attach(&g_motors, &g_obs, &g_conveyor, &g_safety, &g_rx);
    g_nav.set_autonomous(false);   /* start in IDLE/manual, not auto */

    /* Wire serial callbacks. */
    g_rx.on_detection(on_ai_detection);
    g_rx.on_no_detection(on_ai_no_detection);
    g_rx.on_health(on_ai_health);
    g_rx.on_diag_command(on_ai_diag_command);
    g_rx.begin(DASH_AP_SSID, DASH_AP_PASS);   /* ESP-NOW + dashboard SoftAP */
    
    /* Optional: Connect main controller to home/office router in STA mode
     * while maintaining the SoftAP for the dashboard & ESP32-CAM. */
    #ifdef ROUTER_SSID
    if (String(ROUTER_SSID).length() > 0 && String(ROUTER_SSID) != "YOUR_HOME_SSID") {
        WiFi.begin(ROUTER_SSID, ROUTER_PASS);
        Serial.print("[MAIN] Connecting to router: ");
        Serial.println(ROUTER_SSID);
    }
    #endif
    {
        char mac[20];
        espnow_link_mac_str(mac, sizeof(mac));
        if (g_rx.link_ok()) {
            Serial.print("[MAIN] esp-now link ready mac=");
            Serial.println(mac);
            Serial.print("[MAIN] dashboard AP '" DASH_AP_SSID "' ip=");
            Serial.print(WiFi.softAPIP());
            Serial.print(" ws://<ip>:");
            Serial.println(DASH_WS_PORT);
        } else {
            Serial.println("[MAIN] esp-now INIT FAILED");
        }
    }

    /* WebSocket gateway: commands route through the same validation path
     * as USB/dashboard diagnostics (nav.on_diag_command). */
    ws_gateway_set_command_handler(
        [](uint8_t c, uint8_t v, uint16_t d) { g_nav.on_diag_command(c, v, d); });
    ws_gateway_begin();

    /* MQTT setup */
    s_mqtt_client.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    s_mqtt_client.setCallback(mqtt_callback);
    s_mqtt_client.setBufferSize(1024);   /* telemetry frame ~500B > default 256B */

    /* Telemetry. */
    g_telemetry.attach(&g_nav, &g_motors, &g_obs, &g_gps, &g_rx,
                       &g_conveyor, &g_safety, &g_tds);

    Serial.println("[MAIN] init complete. Autonomous disabled. Motors OFF.");
}

static bool s_mqtt_was_connected = false;

static void update_status_indicators() {
    bool internet_ok = (WiFi.status() == WL_CONNECTED && s_mqtt_client.connected());
    bool ready = (g_safety.fault_flags() == 0 && g_gps.has_fix() && g_rx.comms_alive());

    // Blue LED: blinking when connecting, solid when connected
    static uint32_t last_blink_ms = 0;
    static bool blue_state = false;
    if (!internet_ok) {
        if (millis() - last_blink_ms >= 500) {
            last_blink_ms = millis();
            blue_state = !blue_state;
            digitalWrite(STATUS_LED_BLUE_GPIO, blue_state ? HIGH : LOW);
        }
        s_mqtt_was_connected = false;
    } else {
        digitalWrite(STATUS_LED_BLUE_GPIO, HIGH);
        if (!s_mqtt_was_connected) {
            s_mqtt_was_connected = true;
            digitalWrite(BUZZER_GPIO, HIGH);
            delay(150);
            digitalWrite(BUZZER_GPIO, LOW);
        }
    }

    // Red vs Green LEDs
    if (!ready) {
        digitalWrite(STATUS_LED_RED_GPIO, HIGH);
        digitalWrite(STATUS_LED_GREEN_GPIO, LOW);
    } else {
        digitalWrite(STATUS_LED_RED_GPIO, LOW);
        digitalWrite(STATUS_LED_GREEN_GPIO, HIGH);
    }

    // Bin full buzzer alarm: if ultrasonic debris distance < threshold, blow buzzer
    bool bin_full = (g_obs.left_valid() && g_obs.left_cm() > 0.0f && g_obs.left_cm() < BIN_FULL_THRESHOLD_CM);
    if (bin_full && !g_safety.in_emergency()) {
        digitalWrite(BUZZER_GPIO, HIGH);
    } else if (!g_safety.in_emergency() && s_mqtt_was_connected) {
        // Keep buzzer low unless emergency or just connected beep
        digitalWrite(BUZZER_GPIO, LOW);
    }
}

void loop() {
    uint32_t now = millis();

    update_status_indicators();

    poll_usb_commands();    /* dashboard manual commands over USB */
    ws_gateway_update();    /* WebSocket clients + command handling */
    if (WiFi.status() == WL_CONNECTED) {
        maintain_mqtt();
    } else if (now - s_last_wifi_log_ms >= 2000) {
        s_last_wifi_log_ms = now;
        Serial.print("[WIFI] status=");
        Serial.print((int)WiFi.status());
        Serial.print(" ssid=");
        Serial.println(ROUTER_SSID);
    }
    g_gps.update();
    g_tds.update();             /* TDS sample every TDS_UPDATE_MS */
    g_nav.update();          /* state machine + safety + motors */
    g_conveyor.update();
    g_safety.reset_watchdog();

    /* Periodic telemetry back to CAM / serial debug. */
    if (now - g_last_telemetry_ms >= TELEMETRY_INTERVAL_MS) {
        g_last_telemetry_ms = now;
        uint8_t buf[1024];   /* JSON reaches ~600B once rx dbg fills */
        int n = g_telemetry.build_status_frame(buf, sizeof(buf), &g_tx_seq);
        if (n > 0) {
            Serial.write(buf, (size_t)n);              /* USB bridge */
            ws_gateway_on_telemetry(buf, (size_t)n);   /* WebSocket */
            if (s_mqtt_client.connected()) {
                if (!s_mqtt_client.publish(MQTT_TELEMETRY_TOPIC, buf, (size_t)n) &&
                    now - s_last_mqtt_pub_err_ms >= 5000) {
                    s_last_mqtt_pub_err_ms = now;
                    Serial.print("[MQTT] publish FAILED, size=");
                    Serial.println(n);
                }
            }
        }
        g_telemetry.debug_print();
    }

    /* Non-blocking: no long delays. */
}
