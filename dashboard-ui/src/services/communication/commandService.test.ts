import { describe, expect, it } from "vitest";
import { formatCommand, sendCommand, nextCommandId } from "./commandService";
import { DIAG_CMD } from "./types";
import { connectionManager } from "./connectionManager";

describe("formatCommand", () => {
  it("produces the exact frame ws_gateway.cpp parses", () => {
    const frame = formatCommand({
      cmd: DIAG_CMD.FORWARD,
      value: 150,
      durationMs: 600,
      id: "cABC",
    });
    expect(frame).toBe("#cABC CMD:2,150,600");
  });

  it("clamps value to 0..255 and duration to 0..60000", () => {
    expect(formatCommand({ cmd: 2, value: 9999, durationMs: 999999, id: "x" })).toBe(
      "#x CMD:2,255,60000",
    );
    expect(formatCommand({ cmd: 3, value: -5, durationMs: -10, id: "y" })).toBe(
      "#y CMD:3,0,0",
    );
  });

  it("emits valid ids", () => {
    const id = nextCommandId();
    expect(id).toMatch(/^[A-Za-z0-9_-]{1,23}$/);
    expect(formatCommand({ cmd: 1, id }).startsWith(`#${id} CMD:1,`)).toBe(true);
  });

  it("handles non-finite inputs safely", () => {
    expect(formatCommand({ cmd: 4, value: NaN, durationMs: NaN, id: "z" })).toBe(
      "#z CMD:4,0,0",
    );
  });
});

describe("sendCommand", () => {
  it("never reports success when the socket is down", async () => {
    // connectionManager is fresh in tests: not connected.
    expect(connectionManager.isConnected()).toBe(false);
    const r = await sendCommand({ cmd: DIAG_CMD.FORWARD, value: 100, durationMs: 500 });
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.reason).toBe("not_connected");
  });
});
