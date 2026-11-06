#ifndef BOAT_CONFIG_H
#define BOAT_CONFIG_H

/* ========================================================================
 * Standard ESP32 main controller configuration.
 * Edit pin numbers and thresholds to match your verified hardware.
 * ======================================================================== */

/* --- Link to ESP32-CAM: ESP-NOW wireless (see shared/protocol/espnow_link.h) ---
 * Formerly UART2 on GPIO16/17 — now unused; no signal wires between the
 * boards. Only per-board power is required. ESP-NOW runs on 2.4 GHz
 * channel 1, broadcast, no pairing, no router. */
#define ESPNOW_CHANNEL             1

/* --- Dashboard gateway: WPA2 SoftAP + WebSocket server (same radio, ch.1) ---
 * The laptop joins DASH_AP_SSID and opens ws://192.168.4.1:81.
 * Password must be >= 8 chars (WPA2); change before deploying. */
#define DASH_AP_SSID               "RiverClean-Boat"
#define DASH_AP_PASS               "riverclean2026"
#define DASH_WS_PORT               81

/* --- Optional Router connection for Dual AP+STA mode ---
 * Leave blank ("") if you want AP-only mode. Fill in your router details
 * if you want the boat to join your Wi-Fi while still hosting its own AP. */
#define ROUTER_SSID                "Laxmi_Venkateshwar_Krupa"
#define ROUTER_PASS                "Menis@2020"

/* --- Free Public MQTT Broker (broker.hivemq.com) --- */
#define MQTT_BROKER_HOST           "broker.hivemq.com"
#define MQTT_BROKER_PORT           1883
#define MQTT_TELEMETRY_TOPIC       "riverclean/boat/telemetry"
#define MQTT_COMMAND_TOPIC         "riverclean/boat/command"
#define MQTT_ACK_TOPIC             "riverclean/boat/ack"
#define MQTT_WIFI_CONFIG_TOPIC     "riverclean/boat/wifi_config"

/* --- Motor driver: HW-095 (L298N dual H-bridge, 2 A/channel) ---
 * L298N interface: ENx = speed PWM (remove the ENA/ENB jumper caps!),
 * IN1..IN4 = two direction inputs per channel. There is NO STBY pin
 * (earlier drafts assumed a TB6612).
 *
 * GPIO12 (MTDI) MUST NOT be used here: it is the VDD_SDIO strapping
 * pin. When the motor driver is connected its input circuit pulls
 * GPIO12 high at power-on, the flash regulator starts at 1.8 V, and
 * the chip crash-loops with boot:0x33 / "Detected size(2k) ... Probe
 * failed". IN2 therefore lives on GPIO16 (free, non-strapping;
 * formerly UART2 to the CAM, now unused with ESP-NOW). */
#define MOTOR_LEFT_ENA_GPIO        25   /* ENA: left speed (PWM) */
#define MOTOR_LEFT_IN1_GPIO        26   /* IN1: left direction A */
#define MOTOR_LEFT_IN2_GPIO        16   /* IN2: left direction B (was 12: strapping!) */
#define MOTOR_RIGHT_ENB_GPIO       27   /* ENB: right speed (PWM) */
#define MOTOR_RIGHT_IN3_GPIO       14   /* IN3: right direction A */
#define MOTOR_RIGHT_IN4_GPIO       19   /* IN4: right direction B */

/* Direction inversion: set to 1 if a motor runs backwards for "forward". */
#define MOTOR_LEFT_INVERT          0
#define MOTOR_RIGHT_INVERT         0

#define MAX_PWM                    255
#define MAX_STEER_PWM              120
#define APPROACH_SPEED_PWM         80
#define SEARCH_SPEED_PWM           60
#define MIN_MOTOR_PWM              40   /* below this, motor may not turn */

/* --- Ultrasonic sensors --- */
#define US_LEFT_TRIG_GPIO          32
#define US_LEFT_ECHO_GPIO          33   /* via 5V->3.3V divider */
#define US_RIGHT_TRIG_GPIO         13   /* shared with GPS_UART_TX_GPIO:
                                           GPS is RX-only, obs.begin()
                                           re-claims this pin as GPIO */
#define US_RIGHT_ECHO_GPIO         15   /* via 5V->3.3V divider */

#define US_TIMEOUT_US              30000   /* 30 ms echo timeout */
#define OBSTACLE_WARN_CM           30
#define OBSTACLE_STOP_CM           15
#define US_HYSTERESIS_CM           3
#define BIN_FULL_THRESHOLD_CM      12.0f  /* collection bin full if debris distance < 12cm */

/* --- GPS (UART1) --- */
#define GPS_UART_RX_GPIO           4    /* <- GPS TX (NMEA @9600) */
#define GPS_UART_TX_GPIO           13   /* not wired; shared w/ US right TRIG */
#define GPS_BAUD                   9600

/* --- Analog TDS meter: DFRobot Gravity SEN0244 (0..2.3 V output) ---
 * SEN0244 runs on 3.3 V and its output is 3.3 V-safe: wire OUT straight to
 * GPIO34 (ADC1_CH6, input-only). NEVER feed >3.6 V into an ESP32 pin — a
 * 0..5 V TDS sensor needs a 1k/2k divider first (see PIN_MAPPING.md).
 * TDS_CALIBRATION: scale vs. a reference TDS meter (1.0 = datasheet curve). */
#define TDS_ADC_GPIO               34   /* ADC1_CH6 */
#define TDS_CALIBRATION            1.0f
#define TDS_SAMPLE_COUNT           8    /* oversample per reading (>=3) */
#define TDS_UPDATE_MS              1000 /* sample cadence */
#define TDS_PPM_MAX                3000
#define TDS_MAX_SPREAD_MV          100  /* oversample spread above this =
                                          floating/unconnected -> ppm = -1 */

/* --- Conveyor: second HW-095 (L298N) channel ---
 * One direction only (debris onto the boat): IN1=HIGH, IN2=LOW fixed,
 * speed via EN PWM. Remove the EN jumper cap on this board too. */
#define CONVEYOR_EN_GPIO           23   /* EN: speed (PWM) */
#define CONVEYOR_IN1_GPIO          22   /* IN1: direction A (tied HIGH) */
#define CONVEYOR_IN2_GPIO          21   /* IN2: direction B (tied LOW) */
#define CONVEYOR_RUN_MS            5000
#define CONVEYOR_SPEED_PWM         150

/* --- Buzzer & Status LEDs --- */
#define BUZZER_GPIO                5
#define STATUS_LED_RED_GPIO        17   /* Red: Boat not ready / Fault */
#define STATUS_LED_GREEN_GPIO      18   /* Green: Boat ready / All systems nominal */
#define STATUS_LED_BLUE_GPIO       2    /* Blue: Internet connecting (blink) / Connected (solid) */

/* --- Navigation thresholds --- */
#define CONFIDENCE_THRESHOLD       70       /* 0..100 */
#define DETECTION_FRESHNESS_MS     1000
#define CONSECUTIVE_DETECTIONS     2
#define STEERING_DEADBAND          80       /* of 1000 */
#define STEERING_GAIN              50       /* pwm per 1000 offset */
#define COMMS_TIMEOUT_MS           2000
#define TARGET_LOST_SEARCH_MS      10000

/* --- Per-class waste counting (telemetry "bottle/bag/debris_count") --- */
#define WASTE_GAP_RECOUNT_MS       4000  /* same class after this gap = new item */
#define WASTE_COUNT_COOLDOWN_MS    1500  /* min spacing between any two counts */

#endif /* BOAT_CONFIG_H */
