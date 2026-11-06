import { FAULT, type NavState } from "../services/communication";

export interface FaultInfo {
  bit: number;
  label: string;
  severity: "critical" | "warning";
}

const ALL: FaultInfo[] = [
  { bit: FAULT.COMMS_LOST, label: "CAM link lost", severity: "critical" },
  { bit: FAULT.OBSTACLE, label: "Obstacle stop", severity: "warning" },
  { bit: FAULT.SENSOR_BAD, label: "Sensor invalid", severity: "warning" },
  { bit: FAULT.GPS_NO_FIX, label: "GPS no fix", severity: "warning" },
  { bit: FAULT.WATCHDOG, label: "Watchdog reset", severity: "critical" },
];

export function decodeFaults(flags: number): FaultInfo[] {
  if (!Number.isFinite(flags) || flags <= 0) return [];
  return ALL.filter((f) => (flags & f.bit) !== 0);
}

export function hasCriticalFault(flags: number): boolean {
  return decodeFaults(flags).some((f) => f.severity === "critical");
}

export type RiskLevel = "normal" | "attention" | "critical";

/**
 * Status risk ladder (spec): critical for fault/e-stop/comms loss/EMERGENCY
 * states; attention for stale link, no GPS fix while navigating, obstacle.
 */
export function riskLevel(input: {
  state: NavState;
  faultFlags: number;
  estop: boolean;
  commsOk: boolean;
  connectionStale: boolean;
}): RiskLevel {
  if (input.estop) return "critical";
  if (input.state === "EMERGENCY_STOP" || input.state === "FAULT") return "critical";
  if (hasCriticalFault(input.faultFlags)) return "critical";
  if (input.connectionStale) return "critical";
  if (!input.commsOk) return "attention";
  if ((input.faultFlags & (FAULT.OBSTACLE | FAULT.GPS_NO_FIX | FAULT.SENSOR_BAD)) !== 0)
    return "attention";
  return "normal";
}

export const STATE_COLOR: Record<NavState, "accent" | "secondary" | "warning" | "critical"> = {
  BOOT: "secondary",
  SELF_TEST: "secondary",
  IDLE: "accent",
  SEARCH: "secondary",
  TARGET_DETECTED: "accent",
  ALIGN: "accent",
  APPROACH: "accent",
  COLLECT: "accent",
  VERIFY_COLLECT: "accent",
  TARGET_LOST: "warning",
  OBSTACLE_AVOID: "warning",
  FAULT: "critical",
  EMERGENCY_STOP: "critical",
  UNKNOWN: "warning",
};

/** Same semantics, expressed in StatusPill tone names. */
export const STATE_PILL: Record<NavState, "ok" | "warn" | "crit" | "info"> = {
  BOOT: "info",
  SELF_TEST: "info",
  IDLE: "ok",
  SEARCH: "info",
  TARGET_DETECTED: "ok",
  ALIGN: "ok",
  APPROACH: "ok",
  COLLECT: "ok",
  VERIFY_COLLECT: "ok",
  TARGET_LOST: "warn",
  OBSTACLE_AVOID: "warn",
  FAULT: "crit",
  EMERGENCY_STOP: "crit",
  UNKNOWN: "warn",
};

/** Human-readable description for each firmware state (for Help/Overview). */
export const STATE_DESCRIPTIONS: Record<NavState, string> = {
  BOOT: "Firmware boot — hardware not yet initialised.",
  SELF_TEST: "Power-on self-test of sensors, motors and safety interlocks.",
  IDLE: "Motors stopped; waiting for an operator command or autonomy enable.",
  SEARCH: "Autonomous scan pattern while looking for detected debris.",
  TARGET_DETECTED: "Debris locked — holding it in frame.",
  ALIGN: "Steering to centre the target for a straight approach.",
  APPROACH: "Driving toward the target at approach speed.",
  COLLECT: "Conveyor running to pull debris from the water.",
  VERIFY_COLLECT: "Checking the collection completed before returning to search.",
  TARGET_LOST: "Locked target left the camera — reacquiring or re-searching.",
  OBSTACLE_AVOID: "Ultrasonic obstacle detected — evasive manoeuvre in progress.",
  FAULT: "Safety fault latched; motors stopped until faults clear.",
  EMERGENCY_STOP: "Emergency stop latched; motors and conveyor off.",
  UNKNOWN: "State not recognised by this dashboard version.",
};
