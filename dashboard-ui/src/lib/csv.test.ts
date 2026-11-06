import { describe, expect, it } from "vitest";
import { buildCsv, telemetryToCsvRow } from "./csv";
import type { Telemetry } from "../services/communication";

const T: Telemetry = {
  type: "telemetry",
  protocol_version: 1,
  device_id: "riverclean_main",
  state: "COLLECT",
  comms_ok: true,
  estop: false,
  autonomous: true,
  detection: true,
  confidence: 88,
  class_id: 3,
  center_x: 48,
  left_cm: 30.5,
  right_cm: 31,
  gps_fix: true,
  latitude: 17.385,
  longitude: 78.4867,
  satellites: 12,
  gps_age_ms: 300,
  collection_count: 7,
  conveyor: "RUNNING",
  fault_flags: 0,
  uptime_ms: 9999,
  left_pwm: 120,
  right_pwm: 120,
  left_dir: 1,
  right_dir: 1,
  rx_bytes: 1234,
  rx_errors: 0,
  perr: 0,
  drops: 0,
  dbg: "AABB",
};

describe("csv export", () => {
  it("emits a row with the canonical header", () => {
    const row = telemetryToCsvRow(T, 1_700_000_000_000, "live");
    const cells = row.split(",");
    expect(cells[2]).toBe("COLLECT");
    expect(cells[6]).toBe("true");
    expect(cells[7]).toBe("88");
    expect(cells[cells.length - 1]).toBe("live");
    const csv = buildCsv([row]);
    expect(csv.startsWith("timestamp_iso,wall_time,state,")).toBe(true);
    expect(csv.trimEnd().split("\n")).toHaveLength(2);
  });

  it("quotes values containing commas/quotes", () => {
    const row = telemetryToCsvRow(
      { ...T, dbg: 'has "quote",comma' },
      1_700_000_000_000,
      "demo",
    );
    expect(row).toContain('"has ""quote"",comma"');
  });
});
