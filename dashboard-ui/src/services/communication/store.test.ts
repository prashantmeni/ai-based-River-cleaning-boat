import { beforeEach, describe, expect, it } from "vitest";
import { TelemetryStore } from "./store";
import type { Telemetry } from "./types";

function t(over: Partial<Telemetry> = {}): Telemetry {
  return {
    type: "telemetry",
    protocol_version: 1,
    device_id: "riverclean_main",
    state: "IDLE",
    comms_ok: true,
    estop: false,
    autonomous: false,
    detection: false,
    confidence: 0,
    class_id: 0,
    center_x: 48,
    left_cm: 140,
    right_cm: 140,
    gps_fix: false,
    latitude: 0,
    longitude: 0,
    satellites: 0,
    gps_age_ms: 0,
    collection_count: 0,
    conveyor: "OFF",
    fault_flags: 0,
    uptime_ms: 1000,
    left_pwm: 0,
    right_pwm: 0,
    left_dir: 0,
    right_dir: 0,
    rx_bytes: 0,
    rx_errors: 0,
    perr: 0,
    drops: 0,
    dbg: "",
    ...over,
  };
}

describe("TelemetryStore", () => {
  let store: TelemetryStore;

  beforeEach(() => {
    store = new TelemetryStore();
  });

  it("starts offline with no telemetry", () => {
    const s = store.getSnapshot();
    expect(s.latest).toBeNull();
    expect(s.connection.status).toBe("offline");
    expect(s.connection.telemetryCount).toBe(0);
  });

  it("pushes telemetry and marks live status online while fresh", () => {
    const now = 1_000_000;
    store.setWsStatus("open", "ws://test", now);
    store.pushTelemetry(t(), "live", now);
    const s = store.getSnapshot();
    expect(s.latest?.state).toBe("IDLE");
    expect(s.connection.status).toBe("online");
    expect(s.connection.telemetryCount).toBe(1);
    expect(s.version).toBeGreaterThan(0);
  });

  it("turns stale when no samples arrive within 3 s", () => {
    const now = 1_000_000;
    store.setWsStatus("open", "ws://test", now);
    store.pushTelemetry(t(), "live", now);
    store.tick(now + 2999);
    expect(store.getSnapshot().connection.status).toBe("online");
    store.tick(now + 3001);
    expect(store.getSnapshot().connection.status).toBe("stale");
  });

  it("reports offline when the socket closes", () => {
    const now = 5;
    store.pushTelemetry(t(), "live", now);
    store.setWsStatus("closed", "ws://test", now);
    expect(store.getSnapshot().connection.status).toBe("offline");
  });

  it("keeps demo source labelled and forces demo status", () => {
    const now = 42;
    store.pushTelemetry(t(), "demo", now);
    const s = store.getSnapshot();
    expect(s.connection.source).toBe("demo");
    expect(s.connection.status).toBe("demo");
  });

  it("caps the series ring buffer at 300 samples", () => {
    for (let i = 0; i < 350; i++) store.pushTelemetry(t(), "live", i);
    expect(store.getSnapshot().series).toHaveLength(300);
    expect(store.getSnapshot().series[0].at).toBe(50);
  });

  it("records detection events and dedupes rapid repeats", () => {
    const now = 1000;
    store.pushTelemetry(t({ detection: true, class_id: 2, confidence: 80 }), "live", now);
    store.pushTelemetry(t({ detection: true, class_id: 2, confidence: 81 }), "live", now + 100);
    expect(store.getSnapshot().detections).toHaveLength(1);
    store.pushTelemetry(t({ detection: true, class_id: 3, confidence: 90 }), "live", now + 900);
    expect(store.getSnapshot().detections).toHaveLength(2);
    expect(store.getSnapshot().detections[0].label).toBe("General debris");
  });

  it("tracks parse errors and raw message timestamps", () => {
    store.noteParseError("invalid JSON");
    store.noteMessage(777);
    const c = store.getSnapshot().connection;
    expect(c.parseErrors).toBe(1);
    expect(c.lastParseError).toBe("invalid JSON");
    expect(c.lastMessageAt).toBe(777);
  });

  it("notifies subscribers on every commit and supports unsubscribe", () => {
    let calls = 0;
    const un = store.subscribe(() => calls++);
    store.pushTelemetry(t(), "live", 1);
    un();
    store.pushTelemetry(t(), "live", 2);
    expect(calls).toBe(1);
  });
});
