import { describe, expect, it } from "vitest";
import { parseTelemetry } from "./telemetryParser";

const VALID = JSON.stringify({
  type: "telemetry",
  protocol_version: 1,
  device_id: "riverclean_main",
  state: "SEARCH",
  comms_ok: true,
  estop: false,
  autonomous: true,
  detection: true,
  confidence: 82,
  class_id: 1,
  center_x: 51,
  left_cm: 140.5,
  right_cm: -1,
  gps_fix: true,
  latitude: 17.385,
  longitude: 78.4867,
  satellites: 11,
  gps_age_ms: 420,
  collection_count: 3,
  conveyor: "RUNNING",
  fault_flags: 0,
  uptime_ms: 123456,
  left_pwm: 90,
  right_pwm: 88,
  left_dir: 1,
  right_dir: 1,
  rx_bytes: 48123,
  rx_errors: 0,
  perr: 0,
  drops: 0,
  dbg: "7EC71F7E",
});

describe("parseTelemetry", () => {
  it("parses a complete valid firmware frame", () => {
    const r = parseTelemetry(VALID);
    expect(r.ok).toBe(true);
    if (!r.ok) return;
    expect(r.message.type).toBe("telemetry");
    if (r.message.type !== "telemetry") return;
    expect(r.message.state).toBe("SEARCH");
    expect(r.message.confidence).toBe(82);
    expect(r.message.left_cm).toBeCloseTo(140.5);
    expect(r.message.right_cm).toBe(-1);
    expect(r.message.conveyor).toBe("RUNNING");
    expect(r.message.protocol_version).toBe(1);
  });

  it("normalises unknown states to UNKNOWN but keeps the frame", () => {
    const raw = JSON.parse(VALID) as Record<string, unknown>;
    raw.state = "SOMETHING_NEW";
    const r = parseTelemetry(JSON.stringify(raw));
    expect(r.ok).toBe(true);
    if (!r.ok || r.message.type !== "telemetry") return;
    expect(r.message.state).toBe("UNKNOWN");
  });

  it("rejects an unsupported protocol version", () => {
    const raw = JSON.parse(VALID) as Record<string, unknown>;
    raw.protocol_version = 2;
    const r = parseTelemetry(JSON.stringify(raw));
    expect(r.ok).toBe(false);
  });

  it("rejects a foreign device_id", () => {
    const raw = JSON.parse(VALID) as Record<string, unknown>;
    raw.device_id = "someone_elses_board";
    const r = parseTelemetry(JSON.stringify(raw));
    expect(r.ok).toBe(false);
  });

  it("rejects missing critical fields instead of defaulting them", () => {
    const raw = JSON.parse(VALID) as Record<string, unknown>;
    delete raw.comms_ok;
    const r = parseTelemetry(JSON.stringify(raw));
    expect(r.ok).toBe(false);
  });

  it("rejects invalid JSON, non-objects, unknown types and empty input", () => {
    expect(parseTelemetry("{nope").ok).toBe(false);
    expect(parseTelemetry("[1,2,3]").ok).toBe(false);
    expect(parseTelemetry('"hello"').ok).toBe(false);
    expect(parseTelemetry("").ok).toBe(false);
    expect(parseTelemetry(JSON.stringify({ type: "weather" })).ok).toBe(false);
  });

  it("rejects oversized frames", () => {
    expect(parseTelemetry("x".repeat(5000)).ok).toBe(false);
  });

  it("parses hello frames from the gateway", () => {
    const r = parseTelemetry(
      JSON.stringify({
        type: "hello",
        protocol_version: 1,
        device_id: "riverclean_main",
        ws_port: 81,
        espnow_channel: 1,
        server: "WebSocketsServer",
      }),
    );
    expect(r.ok).toBe(true);
    if (!r.ok || r.message.type !== "hello") return;
    expect(r.message.ws_port).toBe(81);
    expect(r.message.espnow_channel).toBe(1);
  });

  it("parses acks and maps non-ok status to rejected", () => {
    const ok = parseTelemetry(
      JSON.stringify({ type: "ack", id: "c1", status: "ok", cmd: 2 }),
    );
    expect(ok.ok).toBe(true);
    if (ok.ok && ok.message.type === "ack") {
      expect(ok.message.status).toBe("ok");
      expect(ok.message.cmd).toBe(2);
    }
    const bad = parseTelemetry(
      JSON.stringify({ type: "ack", id: "c2", status: "nope", cmd: 1, reason: "latched" }),
    );
    if (bad.ok && bad.message.type === "ack") {
      expect(bad.message.status).toBe("rejected");
      expect(bad.message.reason).toBe("latched");
    } else {
      expect.fail("ack not parsed");
    }
  });
});
