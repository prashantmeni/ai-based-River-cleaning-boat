import {
  CONVEYOR_STATES,
  NAV_STATES,
  PROTOCOL_VERSION,
  type AckMessage,
  type HelloMessage,
  type IncomingMessage,
  type NavState,
  type Telemetry,
} from "./types";

export type ParseResult =
  | { ok: true; message: IncomingMessage }
  | { ok: false; error: string };

function isFiniteNumber(v: unknown): v is number {
  return typeof v === "number" && Number.isFinite(v);
}

function asBool(v: unknown, fallback = false): boolean {
  return typeof v === "boolean" ? v : fallback;
}

function asInt(v: unknown, fallback = 0): number {
  return typeof v === "number" && Number.isFinite(v) ? Math.trunc(v) : fallback;
}

function asFloat(v: unknown, fallback: number): number {
  return isFiniteNumber(v) ? v : fallback;
}

function asString(v: unknown, fallback = ""): string {
  return typeof v === "string" ? v : fallback;
}

function normalizeState(v: unknown): NavState {
  const s = asString(v, "UNKNOWN").toUpperCase();
  return (NAV_STATES as string[]).includes(s) ? (s as NavState) : "UNKNOWN";
}

function normalizeConveyor(v: unknown): Telemetry["conveyor"] {
  const s = asString(v, "OFF").toUpperCase();
  return (CONVEYOR_STATES as string[]).includes(s)
    ? (s as Telemetry["conveyor"])
    : "OFF";
}

function parseHello(o: Record<string, unknown>): HelloMessage {
  return {
    type: "hello",
    protocol_version: asInt(o.protocol_version, 0),
    device_id: asString(o.device_id),
    ws_port: asInt(o.ws_port, 0),
    espnow_channel: asInt(o.espnow_channel, 0),
    server: asString(o.server),
  };
}

function parseAck(o: Record<string, unknown>): AckMessage {
  const status = asString(o.status) === "ok" ? "ok" : "rejected";
  return {
    type: "ack",
    id: asString(o.id),
    status,
    cmd: asInt(o.cmd, 0),
    reason: asString(o.reason),
  };
}

/**
 * Parse one WebSocket text frame from the main controller.
 * Strict: unknown types and wrong protocol versions are rejected so bad
 * data never reaches application state.
 */
export function parseTelemetry(raw: string): ParseResult {
  if (typeof raw !== "string" || raw.length === 0) {
    return { ok: false, error: "empty frame" };
  }
  if (raw.length > 4096) {
    return { ok: false, error: "frame too large" };
  }
  let obj: unknown;
  try {
    obj = JSON.parse(raw);
  } catch {
    return { ok: false, error: "invalid JSON" };
  }
  if (obj === null || typeof obj !== "object" || Array.isArray(obj)) {
    return { ok: false, error: "not an object" };
  }
  const o = obj as Record<string, unknown>;
  const type = typeof o.type === "string" ? o.type : "";

  if (type === "hello") return { ok: true, message: parseHello(o) };
  if (type === "ack") return { ok: true, message: parseAck(o) };

  if (type === "telemetry") {
    if (o.protocol_version !== PROTOCOL_VERSION) {
      return { ok: false, error: "unsupported protocol version" };
    }
    if (o.device_id !== "riverclean_main") {
      return { ok: false, error: "unexpected device_id" };
    }
    // Required primitives — absence marks the frame invalid, never defaults
    // live hardware readings silently.
    if (typeof o.state !== "string") return { ok: false, error: "missing state" };
    if (typeof o.comms_ok !== "boolean") return { ok: false, error: "missing comms_ok" };
    if (typeof o.estop !== "boolean") return { ok: false, error: "missing estop" };
    if (typeof o.autonomous !== "boolean") return { ok: false, error: "missing autonomous" };
    if (typeof o.gps_fix !== "boolean") return { ok: false, error: "missing gps_fix" };
    if (!isFiniteNumber(o.left_cm) || !isFiniteNumber(o.right_cm)) {
      return { ok: false, error: "missing ultrasonic readings" };
    }
    if (!isFiniteNumber(o.fault_flags) || !isFiniteNumber(o.uptime_ms)) {
      return { ok: false, error: "missing fault/uptime" };
    }

    const t: Telemetry = {
      type: "telemetry",
      protocol_version: PROTOCOL_VERSION,
      device_id: "riverclean_main",
      state: normalizeState(o.state),
      comms_ok: o.comms_ok,
      estop: o.estop,
      autonomous: o.autonomous,
      detection: asBool(o.detection),
      confidence: asInt(o.confidence, 0),
      class_id: asInt(o.class_id, 0),
      center_x: asInt(o.center_x, 0),
      left_cm: asFloat(o.left_cm, -1),
      right_cm: asFloat(o.right_cm, -1),
      gps_fix: o.gps_fix,
      latitude: asFloat(o.latitude, 0),
      longitude: asFloat(o.longitude, 0),
      satellites: asInt(o.satellites, 0),
      gps_age_ms: asInt(o.gps_age_ms, 0),
      collection_count: asInt(o.collection_count, 0),
      conveyor: normalizeConveyor(o.conveyor),
      fault_flags: asInt(o.fault_flags, 0),
      uptime_ms: asInt(o.uptime_ms, 0),
      left_pwm: asInt(o.left_pwm, 0),
      right_pwm: asInt(o.right_pwm, 0),
      left_dir: asInt(o.left_dir, 0),
      right_dir: asInt(o.right_dir, 0),
      rx_bytes: asInt(o.rx_bytes, 0),
      rx_errors: asInt(o.rx_errors, 0),
      perr: asInt(o.perr, 0),
      drops: asInt(o.drops, 0),
      dbg: asString(o.dbg),
      bottle_count: asInt(o.bottle_count, 0),
      bag_count: asInt(o.bag_count, 0),
      debris_count: asInt(o.debris_count, 0),
      tds_ppm: asInt(o.tds_ppm, -1),
      tds_mv: asInt(o.tds_mv, 0),
    };
    return { ok: true, message: t };
  }

  return { ok: false, error: `unknown message type '${type}'` };
}
