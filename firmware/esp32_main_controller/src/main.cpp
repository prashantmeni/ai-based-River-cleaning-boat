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
#include "boat_config.h"
#include "motor.h"
#include "obstacle.h"
#include "gps.h"
#include "conveyor.h"
#include "safety.h"
#include "serial_rx.h"
#include "navigation.h"
#include "telemetry.h"

/* --- Hardware instances --- */
static MotorDriver g_motors;
static ObstacleSensors g_obs;
static GPS g_gps;
static Conveyor g_conveyor;
static Safety g_safety;
static SerialRx g_rx(Serial);            /* UART0 <- ESP32-CAM */
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

static uint16_t g_tx_seq = 0;
static uint32_t g_last_telemetry_ms = 0;
static const uint32_t TELEMETRY_INTERVAL_MS = 500;

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }
    Serial.println("[MAIN] booting standard ESP32 controller");

    /* Safety first: motors/conveyor OFF until explicitly enabled. */
    g_safety.begin();
    g_motors.begin();      /* leaves outputs off, standby ON after init */
    g_conveyor.begin();    /* OFF */
    g_obs.begin();
    g_gps.begin(Serial2, GPS_BAUD, GPS_UART_RX_GPIO, GPS_UART_TX_GPIO);

    /* Wire navigation dependencies. */
    g_nav.attach(&g_motors, &g_obs, &g_conveyor, &g_safety, &g_rx);
    g_nav.set_autonomous(false);   /* start in IDLE/manual, not auto */

    /* Wire serial callbacks. */
    g_rx.on_detection(on_ai_detection);
    g_rx.on_no_detection(on_ai_no_detection);
    g_rx.on_health(on_ai_health);
    g_rx.on_diag_command(on_ai_diag_command);
    g_rx.begin(MAIN_UART_BAUD);

    /* Telemetry. */
    g_telemetry.attach(&g_nav, &g_motors, &g_obs, &g_gps, &g_rx,
                       &g_conveyor, &g_safety);

    Serial.println("[MAIN] init complete. Autonomous disabled. Motors OFF.");
}

void loop() {
    uint32_t now = millis();

    g_gps.update();
    g_nav.update();          /* state machine + safety + motors */
    g_conveyor.update();
    g_safety.reset_watchdog();

    /* Periodic telemetry back to CAM / serial debug. */
    if (now - g_last_telemetry_ms >= TELEMETRY_INTERVAL_MS) {
        g_last_telemetry_ms = now;
        uint8_t buf[128];
        int n = g_telemetry.build_status_frame(buf, sizeof(buf), &g_tx_seq);
        if (n > 0) {
            Serial.write(buf, (size_t)n);
        }
        g_telemetry.debug_print();
    }

    /* Non-blocking: no long delays. */
}
