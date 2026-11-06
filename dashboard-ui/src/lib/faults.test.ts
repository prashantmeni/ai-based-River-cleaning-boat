import { describe, expect, it } from "vitest";
import { decodeFaults, hasCriticalFault, riskLevel, STATE_DESCRIPTIONS } from "./faults";
import { FAULT, NAV_STATES } from "../services/communication";

describe("decodeFaults", () => {
  it("decodes individual and combined bits", () => {
    expect(decodeFaults(0)).toEqual([]);
    expect(decodeFaults(FAULT.COMMS_LOST)).toHaveLength(1);
    expect(decodeFaults(FAULT.COMMS_LOST)[0].label).toBe("CAM link lost");
    const both = decodeFaults(FAULT.COMMS_LOST | FAULT.OBSTACLE);
    expect(both).toHaveLength(2);
    expect(both.map((f) => f.bit)).toEqual([FAULT.COMMS_LOST, FAULT.OBSTACLE]);
  });

  it("identifies critical faults", () => {
    expect(hasCriticalFault(FAULT.COMMS_LOST)).toBe(true);
    expect(hasCriticalFault(FAULT.WATCHDOG)).toBe(true);
    expect(hasCriticalFault(FAULT.OBSTACLE)).toBe(false);
    expect(hasCriticalFault(0)).toBe(false);
  });

  it("tolerates garbage input", () => {
    expect(decodeFaults(Number.NaN)).toEqual([]);
    expect(decodeFaults(-1)).toEqual([]);
  });
});

describe("riskLevel", () => {
  const base = {
    state: "IDLE" as const,
    faultFlags: 0,
    estop: false,
    commsOk: true,
    connectionStale: false,
  };

  it("is normal in the happy path", () => {
    expect(riskLevel(base)).toBe("normal");
  });

  it("is critical for e-stop, fault state and latched critical faults", () => {
    expect(riskLevel({ ...base, estop: true })).toBe("critical");
    expect(riskLevel({ ...base, state: "EMERGENCY_STOP" })).toBe("critical");
    expect(riskLevel({ ...base, state: "FAULT" })).toBe("critical");
    expect(riskLevel({ ...base, faultFlags: FAULT.COMMS_LOST })).toBe("critical");
    expect(riskLevel({ ...base, connectionStale: true })).toBe("critical");
  });

  it("is attention for cam link down and advisory faults", () => {
    expect(riskLevel({ ...base, commsOk: false })).toBe("attention");
    expect(riskLevel({ ...base, faultFlags: FAULT.OBSTACLE })).toBe("attention");
    expect(riskLevel({ ...base, faultFlags: FAULT.GPS_NO_FIX })).toBe("attention");
  });

  it("e-stop outranks stale connection", () => {
    expect(riskLevel({ ...base, estop: true, connectionStale: true })).toBe("critical");
  });
});

describe("state documentation", () => {
  it("documents every navigation state", () => {
    for (const s of NAV_STATES) {
      expect(STATE_DESCRIPTIONS[s]).toBeTruthy();
      expect(STATE_DESCRIPTIONS[s].length).toBeGreaterThan(10);
    }
  });
});
