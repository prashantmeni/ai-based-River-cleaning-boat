import { afterEach, describe, expect, it } from "vitest";
import { DemoEngine } from "./demoEngine";
import { parseTelemetry } from "../services/communication/telemetryParser";
import { DIAG_CMD, PROTOCOL_VERSION, store } from "../services/communication";

describe("DemoEngine", () => {
  const engine = new DemoEngine();

  afterEach(() => {
    engine.stop();
  });

  it("produces frames that satisfy the real parser every tick", () => {
    engine.start("mission");
    for (let i = 0; i < 40; i++) {
      engine.step();
      const snap = store.getSnapshot();
      expect(snap.connection.source).toBe("demo");
      const parsed = parseTelemetry(JSON.stringify(snap.latest));
      expect(parsed.ok).toBe(true);
    }
  });

  it("labels protocol metadata correctly", () => {
    engine.start("mission");
    engine.step();
    const t = store.getSnapshot().latest!;
    expect(t.protocol_version).toBe(PROTOCOL_VERSION);
    expect(t.device_id).toBe("riverclean_main");
  });

  it("engages autonomy through the mission phases", () => {
    engine.start("mission");
    expect(engine.handleCommand(DIAG_CMD.AUTONOMOUS_ON, 1, 0).ok).toBe(true);
    const seen = new Set<string>();
    for (let i = 0; i < 60; i++) {
      engine.step();
      seen.add(store.getSnapshot().latest!.state);
    }
    expect(seen.has("SEARCH")).toBe(true);
    expect(seen.has("TARGET_DETECTED")).toBe(true);
    expect(seen.has("COLLECT")).toBe(true);
    expect(store.getSnapshot().latest!.autonomous).toBe(true);
  });

  it("latches E-STOP and blocks drive commands until cleared", () => {
    engine.start("mission");
    expect(engine.handleCommand(DIAG_CMD.STOP, 1, 0).ok).toBe(true);
    engine.step();
    expect(store.getSnapshot().latest!.estop).toBe(true);
    expect(store.getSnapshot().latest!.state).toBe("EMERGENCY_STOP");

    const blocked = engine.handleCommand(DIAG_CMD.FORWARD, 150, 500);
    expect(blocked.ok).toBe(false);

    expect(engine.handleCommand(DIAG_CMD.STOP, 2, 0).ok).toBe(true);
    engine.step();
    expect(store.getSnapshot().latest!.estop).toBe(false);
    expect(store.getSnapshot().latest!.state).toBe("IDLE");
    expect(engine.handleCommand(DIAG_CMD.FORWARD, 150, 500).ok).toBe(true);
  });

  it("rejects manual driving while autonomous", () => {
    engine.start("mission");
    engine.handleCommand(DIAG_CMD.AUTONOMOUS_ON, 1, 0);
    const r = engine.handleCommand(DIAG_CMD.FORWARD, 150, 500);
    expect(r.ok).toBe(false);
    expect(r.reason).toContain("autonomous");
  });

  it("injects comms loss in the comms_loss scenario and recovers on clear", () => {
    engine.start("comms_loss");
    engine.handleCommand(DIAG_CMD.AUTONOMOUS_ON, 1, 0);
    for (let i = 0; i < 30; i++) engine.step();
    const down = store.getSnapshot().latest!;
    expect(down.comms_ok).toBe(false);
    expect(down.fault_flags & 1).toBe(1);
    expect(down.state).toBe("FAULT");

    engine.handleCommand(DIAG_CMD.STOP, 2, 0);
    engine.step();
    const up = store.getSnapshot().latest!;
    expect(up.comms_ok).toBe(true);
    expect(up.fault_flags).toBe(0);
    expect(up.state).toBe("IDLE");
  });

  it("increments collection_count through a full mission cycle", () => {
    engine.start("mission");
    engine.handleCommand(DIAG_CMD.AUTONOMOUS_ON, 1, 0);
    let maxCount = 0;
    for (let i = 0; i < 80; i++) {
      engine.step();
      maxCount = Math.max(maxCount, store.getSnapshot().latest!.collection_count);
    }
    expect(maxCount).toBeGreaterThanOrEqual(1);
  });

  it("resets fully on restart", () => {
    engine.start("mission");
    engine.handleCommand(DIAG_CMD.AUTONOMOUS_ON, 1, 0);
    for (let i = 0; i < 30; i++) engine.step();
    engine.start("fault");
    const t = store.getSnapshot().latest!;
    expect(t.autonomous).toBe(false);
    expect(t.estop).toBe(false);
    expect(t.fault_flags).toBe(0);
    expect(t.state).toBe("IDLE");
  });

  it("injects obstacle + sensor faults in the fault scenario", () => {
    engine.start("fault");
    engine.handleCommand(DIAG_CMD.AUTONOMOUS_ON, 1, 0);
    for (let i = 0; i < 16; i++) engine.step();
    const mid = store.getSnapshot().latest!;
    expect(mid.fault_flags & 2).toBe(2);
    for (let i = 0; i < 30; i++) engine.step();
    expect(store.getSnapshot().latest!.fault_flags & 4).toBe(4);
  });
});
