#ifndef BOAT_CONFIG_H
#define BOAT_CONFIG_H

/* ========================================================================
 * Standard ESP32 main controller configuration.
 * Edit pin numbers and thresholds to match your verified hardware.
 * ======================================================================== */

/* --- Serial link from ESP32-CAM (UART0) --- */
#define MAIN_UART_BAUD             115200
#define MAIN_UART_RX_GPIO          3   /* <- CAM TX */
#define MAIN_UART_TX_GPIO          1   /* -> CAM RX */

/* --- Motor driver (dual channel) --- */
#define MOTOR_LEFT_PWM_GPIO        25
#define MOTOR_LEFT_DIR_GPIO        26
#define MOTOR_RIGHT_PWM_GPIO       27
#define MOTOR_RIGHT_DIR_GPIO       14
#define MOTOR_STBY_GPIO            12   /* driver standby/enable */

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
#define US_RIGHT_TRIG_GPIO         25   /* TODO: resolve pin conflicts */
#define US_RIGHT_ECHO_GPIO         26   /* via 5V->3.3V divider */

#define US_TIMEOUT_US              30000   /* 30 ms echo timeout */
#define OBSTACLE_WARN_CM           30
#define OBSTACLE_STOP_CM           15
#define US_HYSTERESIS_CM           3

/* --- GPS (UART2) --- */
#define GPS_UART_RX_GPIO           16   /* <- GPS TX */
#define GPS_UART_TX_GPIO           17   /* -> GPS RX */
#define GPS_BAUD                   9600

/* --- Conveyor --- */
#define CONVEYOR_PWM_GPIO          14   /* TODO: avoid conflict with motors */
#define CONVEYOR_DIR_GPIO          27   /* TODO: avoid conflict */
#define CONVEYOR_RUN_MS            5000
#define CONVEYOR_SPEED_PWM         150

/* --- Buzzer --- */
#define BUZZER_GPIO                5

/* --- Navigation thresholds --- */
#define CONFIDENCE_THRESHOLD       70       /* 0..100 */
#define DETECTION_FRESHNESS_MS     1000
#define CONSECUTIVE_DETECTIONS     2
#define STEERING_DEADBAND          80       /* of 1000 */
#define STEERING_GAIN              50       /* pwm per 1000 offset */
#define COMMS_TIMEOUT_MS           2000
#define TARGET_LOST_SEARCH_MS      10000

#endif /* BOAT_CONFIG_H */
