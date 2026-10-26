#pragma once
// ============================================================
// River Cleaning Boat - standard ESP32 main controller config
// Board: ESP32-WROOM-32 DevKit (ESP32 dev board from parts list)
// Vision/AI parameters live in firmware/include/config.h (CAM).
// Pin table + power: docs/wiring.md (dual-controller section).
// ============================================================

// ---- Inter-controller UART link (docs/communication_protocol.md) ----
#define PIN_LINK_RX 26   // main RX <- ESP32-CAM TX (GPIO14)
#define PIN_LINK_TX 27   // main TX -> ESP32-CAM RX (GPIO13)

// ---- GPS (UART2; 16/17 unavailable on this module's board) ----
#define PIN_GPS_RX 36    // main RX <- GPS TX (input-only pin, no strap risk)
#define PIN_GPS_TX 15    // main TX -> GPS RX (strap pin: TX idle-high = normal boot)
#define GPS_BAUD 9600

// ---- Ultrasonic sensors (HC-SR04; ECHO through 1k/2k divider!) ----
#define US1_ENABLED 1    // front-left obstacle
#define PIN_US1_TRIG 32
#define PIN_US1_ECHO 33
#define US2_ENABLED 1    // front-right obstacle
#define PIN_US2_TRIG 25
#define PIN_US2_ECHO 34
#define US3_ENABLED 1    // basket fill (dedicated transducer)
#define PIN_US3_TRIG 13
#define PIN_US3_ECHO 35

// ---- Motor driver: L298N dual H-bridge #1 (drive L/R), 12-bit LEDC ----
// ENA/ENB = speed (PWM), IN pins = direction. NOT PCA9685 (absent from
// parts list; PCA9685 driver retired - see docs/audit-report.md).
#define PIN_M_ENA 18     // left  speed
#define PIN_M_IN1 19
#define PIN_M_IN2 21
#define PIN_M_ENB 22     // right speed
#define PIN_M_IN3 23
#define PIN_M_IN4 14
#define MOTOR_PWM_FREQ_HZ 5000
#define MOTOR_PWM_BITS 12

// ---- Conveyor: L298N dual H-bridge #2, channel A ----
// Speed/on-off only: ENA from GPIO4 (jumper REMOVED); IN1/IN2 hardwired
// on the breadboard (IN1=+5V, IN2=GND) for fixed forward direction.
// Avoids strap pins GPIO2/GPIO5 as outputs.
#define PIN_C_ENA 4
#define CONV_IN1_TIE 1    // documentation: wired HIGH
#define CONV_IN2_TIE 0    // documentation: wired LOW

// ---- Buzzer / indicators (buzzer NOT in parts list: disabled stub) ----
#define PIN_BUZZER -1
#define PIN_STATUS_LED -1

// ---- Free spares (main board): 5,12,15 (strapping: boot-safe only as
// inputs with care), 12/15/2/5 not assigned; 1,3 = USB console ----

// ---- Battery sense (battery spec unknown from parts list: disabled) ----
#define BATTERY_SENSE_ENABLED 0
#define PIN_BATTERY_ADC 36
#define BATTERY_DIVIDER_RATIO 2.0f   // TODO: set when cell count known
#define BATTERY_LOW_V 6.6f           // TODO: set when cell count known

// ---- E-stop input (no button in parts list: software e-stop only) ----
#define PIN_ESTOP -1

// ---- Navigation / motors (PWM scale 0..4095, kept from single-board
// design so the state machine constants change by zero) ----
#define PROP_PWM_MIN 0
#define PROP_PWM_MAX 4095
#define APPROACH_PWM 2200
#define SEARCH_PWM 1800
#define TURN_PWM 2400
#define MOTOR_CMD_TIMEOUT_MS 8000

// ---- Conveyor ----
#define CONVEYOR_PWM 2800
#define COLLECT_DURATION_MS 4000
#define CONVEYOR_TIMEOUT_MS 8000

// ---- Ultrasonic ----
#define US_TIMEOUT_US 25000
#define OBSTACLE_STOP_CM 40
#define OBSTACLE_TURN_CM 60
#define US_MIN_VALID_CM 2
#define US_MAX_VALID_CM 400
#define SONAR_REQUIRED_FOR_AUTO 1    // AUTO refuses motion if both sonars
                                     // invalid > 3 s (p2 safety rule)

// ---- Basket fill ----
#define BASKET_EMPTY_CM 30.0f
#define BASKET_FULL_CM 8.0f
#define BASKET_NEAR_FULL_CM 12.0f
#define BASKET_MIN_VALID_CM 2.0f
#define BASKET_MAX_VALID_CM 60.0f
#define CONFIRM_FILL_DELTA_PCT 5.0f  // attempt -> confirmed needs +5% fill

// ---- Detection gate on received dets (MUST equal CAM
// firmware/include/config.h DETECT_CONF_THRESHOLD - protocol test enforces) ----
#define DETECT_CONF_THRESHOLD 0.55f
#define DETECT_PERSIST_FRAMES 3

// ---- GPS route log ----
#define ROUTE_LOG_MAX 128

// ---- Timing ----
#define CONTROL_LOOP_MS 100
#define SENSOR_PERIOD_MS 200
#define GPS_PERIOD_MS 1000
#define MAIN_STATE_PERIOD_MS 500

// ---- Bench diagnostics (set 0 for field builds) ----
// LINK_USB_MIRROR: also send protocol lines on the USB console and accept
// injected protocol lines from it (mocked-link testing without wires).
// LINK_RAW_SNIFF: echo raw UART1 RX bytes to the console (physical probe).
#define LINK_USB_MIRROR 0
#define LINK_RAW_SNIFF 0
#define SONAR_INVALID_FAULT_MS 3000

// ---- Safety ----
#define WATCHDOG_TIMEOUT_S 30
