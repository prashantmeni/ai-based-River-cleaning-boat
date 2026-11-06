/* Strongly typed mirrors of the ACTUAL firmware message schema.
 *
 * Source of truth:
 *   firmware/esp32_main_controller/src/telemetry.cpp  (telemetry JSON)
 *   firmware/esp32_main_controller/src/ws_gateway.cpp (hello + ack)
 *   firmware/esp32_main_controller/src/navigation.cpp (state names)
 *   firmware/esp32_cam_ai/include/ai_config.h         (model classes)
 *   firmware/esp32_main_controller/include/safety.h    (fault bits)
 * Never invent fields the firmware does not send.
 */

export const PROTOCOL_VERSION = 1 as const;
export const DEVICE_ID = "riverclean_main" as const;

/** Exact strings produced by Navigation::state_name(). */
export type NavState =
  | "BOOT"
  | "SELF_TEST"
  | "IDLE"
  | "SEARCH"
  | "TARGET_DETECTED"
  | "ALIGN"
  | "APPROACH"
  | "COLLECT"
  | "VERIFY_COLLECT"
  | "TARGET_LOST"
  | "OBSTACLE_AVOID"
  | "FAULT"
  | "EMERGENCY_STOP"
  | "UNKNOWN";

export const NAV_STATES: NavState[] = [
  "BOOT",
  "SELF_TEST",
  "IDLE",
  "SEARCH",
  "TARGET_DETECTED",
  "ALIGN",
  "APPROACH",
  "COLLECT",
  "VERIFY_COLLECT",
  "TARGET_LOST",
  "OBSTACLE_AVOID",
  "FAULT",
  "EMERGENCY_STOP",
  "UNKNOWN",
];

export type ConveyorState = "OFF" | "STARTING" | "RUNNING" | "STOPPING" | "FAULT";

export const CONVEYOR_STATES: ConveyorState[] = [
  "OFF",
  "STARTING",
  "RUNNING",
  "STOPPING",
  "FAULT",
];

/** Telemetry JSON line from telemetry.cpp (one object per WS text frame). */
export interface Telemetry {
  type: "telemetry";
  protocol_version: 1;
  device_id: "riverclean_main";
  state: NavState;
  /** ESP-NOW link to ESP32-CAM alive (frames within last 2 s). */
  comms_ok: boolean;
  /** Firmware-latched remote emergency stop. */
  estop: boolean;
  autonomous: boolean;
  /** Valid detection present in nav (freshness window). */
  detection: boolean;
  confidence: number;
  class_id: number;
  center_x: number;
  /** cm; -1 means INVALID sensor reading (never treat as distance). */
  left_cm: number;
  right_cm: number;
  gps_fix: boolean;
  latitude: number;
  longitude: number;
  satellites: number;
  gps_age_ms: number;
  collection_count: number;
  conveyor: ConveyorState;
  fault_flags: number;
  uptime_ms: number;
  left_pwm: number;
  right_pwm: number;
  left_dir: number;
  right_dir: number;
  rx_bytes: number;
  rx_errors: number;
  perr: number;
  drops: number;
  dbg: string;
  /** Per-class waste counts (telemetry.cpp). Optional so older firmware
   *  frames without them stay valid — never fabricated by the UI. */
  bottle_count?: number;
  bag_count?: number;
  debris_count?: number;
  /** Analog TDS (SEN0244): ppm — −1 means invalid/unavailable (floating
   *  input or older firmware that omits the field); raw sensor millivolts
   *  always accompany it for wiring checks (telemetry.cpp). */
  tds_ppm?: number;
  tds_mv?: number;
}

/** WStype_CONNECTED hello frame (ws_gateway.cpp). */
export interface HelloMessage {
  type: "hello";
  protocol_version: number;
  device_id: string;
  ws_port: number;
  espnow_channel: number;
  server: string;
}

export type AckStatus = "ok" | "rejected";

/** Command acknowledgement (ws_gateway.cpp send_ack). */
export interface AckMessage {
  type: "ack";
  id: string;
  status: AckStatus;
  cmd: number;
  reason?: string;
}

export type IncomingMessage = Telemetry | HelloMessage | AckMessage;

/** Diagnostic command codes (shared/protocol/protocol.h). */
export const DIAG_CMD = {
  STOP: 1,
  FORWARD: 2,
  TURN_LEFT: 3,
  TURN_RIGHT: 4,
  CONVEYOR_ON: 5,
  CONVEYOR_OFF: 6,
  AUTONOMOUS_ON: 7,
  AUTONOMOUS_OFF: 8,
} as const;

export type DiagCmdCode = (typeof DIAG_CMD)[keyof typeof DIAG_CMD];

/** Fault bits from safety.h. */
export const FAULT = {
  COMMS_LOST: 1 << 0,
  OBSTACLE: 1 << 1,
  SENSOR_BAD: 1 << 2,
  GPS_NO_FIX: 1 << 3,
  WATCHDOG: 1 << 4,
} as const;

/** Model labels from ai_config.h — the ONLY supported classes. */
export const CLASS_LABELS: Record<number, string> = {
  0: "Background",
  1: "Plastic bottle",
  2: "Plastic bag",
  3: "General debris",
};

export function classLabel(classId: number): string {
  return CLASS_LABELS[classId] ?? "Unknown";
}

export type ConnectionState =
  | "connecting"
  | "online"
  | "stale"
  | "offline"
  | "demo";

export interface DetectionEvent {
  at: number;
  classId: number;
  label: string;
  confidence: number;
  centerX: number;
  source: "live" | "demo";
}
