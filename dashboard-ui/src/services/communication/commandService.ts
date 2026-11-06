import { connectionManager } from "./connectionManager";
import { DIAG_CMD, type AckMessage, type DiagCmdCode } from "./types";

export interface CommandRequest {
  cmd: DiagCmdCode;
  value?: number;
  durationMs?: number;
  /** Client-generated correlation id, echoed by the firmware. */
  id?: string;
}

export type CommandResult =
  | { ok: true; ack: AckMessage }
  | { ok: false; reason: "not_connected" | "timeout" | "rejected"; detail?: string };

const ACK_TIMEOUT_MS = 3000;

let idCounter = 0;

export function nextCommandId(): string {
  idCounter = (idCounter + 1) % 0xffffff;
  return `c${Date.now().toString(36)}${idCounter.toString(36)}`;
}

/**
 * Wire format consumed by ws_gateway.cpp:
 *   "#<id> CMD:<cmd>,<value>,<duration_ms>"
 * Acks come back as {"type":"ack","id":...,"status":"ok|rejected"}.
 */
export function formatCommand(req: CommandRequest): string {
  const id = req.id ?? nextCommandId();
  const value = clampByte(req.value ?? 0);
  const rawDur = Number(req.durationMs ?? 0);
  const dur = Number.isFinite(rawDur)
    ? Math.max(0, Math.min(60000, Math.trunc(rawDur)))
    : 0;
  return `#${id} CMD:${req.cmd},${value},${dur}`;
}

function clampByte(v: number): number {
  if (!Number.isFinite(v)) return 0;
  return Math.max(0, Math.min(255, Math.trunc(v)));
}

/** Send a command and await its firmware ack (never optimistic). */
export function sendCommand(req: CommandRequest): Promise<CommandResult> {
  const id = req.id ?? nextCommandId();
  const frame = formatCommand({ ...req, id });

  if (!connectionManager.isConnected()) {
    return Promise.resolve({ ok: false, reason: "not_connected" });
  }

  return new Promise<CommandResult>((resolve) => {
    let settled = false;
    const off = connectionManager.onAck((ack) => {
      if (settled || ack.id !== id) return;
      settled = true;
      clearTimeout(timer);
      off();
      if (ack.status === "ok") {
        resolve({ ok: true, ack });
      } else {
        resolve({ ok: false, reason: "rejected", detail: ack.reason });
      }
    });

    const timer = setTimeout(() => {
      if (settled) return;
      settled = true;
      off();
      resolve({ ok: false, reason: "timeout" });
    }, ACK_TIMEOUT_MS);

    if (!connectionManager.send(frame)) {
      settled = true;
      clearTimeout(timer);
      off();
      resolve({ ok: false, reason: "not_connected" });
    }
  });
}

/** Convenience builders used by manual control / automation. */
export const Commands = {
  stop: () => sendCommand({ cmd: DIAG_CMD.STOP, value: 0 }),
  estopLatch: () => sendCommand({ cmd: DIAG_CMD.STOP, value: 1 }),
  estopClear: () => sendCommand({ cmd: DIAG_CMD.STOP, value: 2 }),
  forward: (pwm: number, durationMs: number) =>
    sendCommand({ cmd: DIAG_CMD.FORWARD, value: pwm, durationMs }),
  turnLeft: (pwm: number, durationMs: number) =>
    sendCommand({ cmd: DIAG_CMD.TURN_LEFT, value: pwm, durationMs }),
  turnRight: (pwm: number, durationMs: number) =>
    sendCommand({ cmd: DIAG_CMD.TURN_RIGHT, value: pwm, durationMs }),
  conveyorOn: () => sendCommand({ cmd: DIAG_CMD.CONVEYOR_ON, value: 1 }),
  conveyorOff: () => sendCommand({ cmd: DIAG_CMD.CONVEYOR_OFF, value: 0 }),
  autoOn: () => sendCommand({ cmd: DIAG_CMD.AUTONOMOUS_ON, value: 1 }),
  autoOff: () => sendCommand({ cmd: DIAG_CMD.AUTONOMOUS_OFF, value: 0 }),
} as const;
