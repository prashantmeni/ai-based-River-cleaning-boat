import { useState } from "react";
import {
  ArrowBigUp,
  ArrowLeft,
  ArrowRight,
  CircleStop,
  Compass,
  Gauge as GaugeIcon,
  Satellite,
  ShieldAlert,
  Waves,
} from "lucide-react";
import { Panel } from "../components/ui/Panel";
import { Metric } from "../components/ui/Metric";
import { Gauge } from "../components/ui/Gauge";
import { StatusPill } from "../components/ui/StatusPill";
import { ConfirmButton, useToast } from "../components/ui/Toast";
import { useBoat } from "../state/useBoat";
import { useTelemetry } from "../state/TelemetryContext";
import { useSettings } from "../state/SettingsContext";
import { STATE_DESCRIPTIONS, STATE_COLOR } from "../lib/faults";
import {
  NAV_STATES,
  DIAG_CMD,
  type NavState,
  type CommandRequest,
} from "../services/communication";
import { formatDistanceCm, formatDuration, formatLatLon } from "../lib/format";

const CHIP_TONE: Record<string, string> = {
  accent: "border-accent/50 bg-accent/10 text-accent",
  secondary: "border-secondary/50 bg-secondary/10 text-secondary",
  warning: "border-warning/50 bg-warning/10 text-warning",
  critical: "border-critical/50 bg-critical/10 text-critical",
};

function useManualCommand() {
  const { send } = useTelemetry();
  const { show } = useToast();

  return async (label: string, req: CommandRequest): Promise<boolean> => {
    const r = await send(req);
    if (r.ok) {
      show(`${label} — acknowledged`, "ok");
      return true;
    }
    if (r.reason === "not_connected") {
      show(`${label} — not sent`, "crit", "WebSocket offline — check the connection.");
    } else if (r.reason === "timeout") {
      show(`${label} — no acknowledgement`, "warn", "Frame sent but the controller did not ack in 3 s.");
    } else {
      show(`${label} — rejected by controller`, "warn", r.detail ?? "Safety rules blocked it.");
    }
    return false;
  };
}

function DistanceBar({ label, cm, side }: { label: string; cm: number; side: "left" | "right" }) {
  const invalid = cm < 0 || !Number.isFinite(cm);
  const pct = invalid ? 0 : Math.max(0, Math.min(100, (cm / 200) * 100));
  const tone =
    invalid ? "text-warning" : cm < 30 ? "text-critical" : cm < 60 ? "text-warning" : "text-accent";
  return (
    <div>
      <div className="mb-1 flex items-center justify-between text-xs">
        <span className="flex items-center gap-1.5 text-t-secondary">
          <Waves className="h-3.5 w-3.5" aria-hidden /> {label}
        </span>
        <span className={`font-mono font-semibold ${tone}`}>
          {formatDistanceCm(cm)}
        </span>
      </div>
      <div className="h-3 overflow-hidden rounded bg-elevated">
        <div
          className={`h-full rounded ${side === "left" ? "bg-secondary" : "bg-accent"} transition-all`}
          style={{ width: `${pct}%` }}
        />
      </div>
    </div>
  );
}

export function DrivePage() {
  const { t, conn, demo, snapshot } = useBoat();
  const { send, estopLatch, estopClear } = useTelemetry();
  const { settings, update } = useSettings();
  const { show } = useToast();
  const runCmd = useManualCommand();
  const [heading, setHeading] = useState<"left" | "right" | null>(null);

  const state: NavState = t?.state ?? "UNKNOWN";
  const autonomous = t?.autonomous ?? false;
  const estop = t?.estop ?? false;
  const latched = estop || state === "EMERGENCY_STOP" || state === "FAULT";
  const canDrive = !latched && !autonomous;

  const drive = async (dir: "fwd" | "left" | "right") => {
    if (!canDrive) return;
    const cmd =
      dir === "fwd" ? DIAG_CMD.FORWARD : dir === "left" ? DIAG_CMD.TURN_LEFT : DIAG_CMD.TURN_RIGHT;
    setHeading(dir === "right" ? "right" : dir === "left" ? "left" : null);
    await runCmd(
      dir === "fwd" ? "Forward" : dir === "left" ? "Turn left" : "Turn right",
      { cmd, value: settings.manualPwm, durationMs: settings.commandDurationMs },
    );
    setHeading(null);
  };

  const stop = () => void runCmd("Stop", { cmd: DIAG_CMD.STOP, value: 0 });

  const runConveyor = async (on: boolean) => {
    if (latched) return;
    await runCmd(
      on ? "Conveyor ON" : "Conveyor OFF",
      { cmd: on ? DIAG_CMD.CONVEYOR_ON : DIAG_CMD.CONVEYOR_OFF, value: on ? 1 : 0 },
    );
  };

  const toggleAuto = async () => {
    if (!autonomous) {
      const ok = await runCmd("Autonomy ON", { cmd: DIAG_CMD.AUTONOMOUS_ON, value: 1 });
      if (ok) show("Autonomous mission engaged", "info", "Motors are under firmware state machine.");
    } else {
      await runCmd("Autonomy OFF", { cmd: DIAG_CMD.AUTONOMOUS_OFF, value: 0 });
    }
  };

  const quality = demo ? "demo" : conn.status === "online" ? "live" : "stale";
  const lastAck = snapshot.connection.telemetryCount;

  const actionBtn = (extra = "") =>
    "btn-secondary h-14 w-full text-sm disabled:opacity-40 " + extra;

  return (
    <div className="space-y-4">
      <header className="flex flex-wrap items-center justify-between gap-3">
        <div>
          <h1 className="text-lg font-bold">Drive</h1>
          <p className="text-xs text-t-secondary">
            Manual control, autonomy and vessel sensors
          </p>
        </div>
        <div className="flex items-center gap-2">
          <StatusPill
            tone={estop ? "crit" : latched ? "crit" : "ok"}
            label={estop ? "E-STOP LATCHED" : latched ? "SAFETY STOP" : "MOTORS SAFE"}
          />
          <StatusPill
            tone={autonomous ? "info" : "idle"}
            label={autonomous ? "AUTO" : "MANUAL"}
          />
        </div>
      </header>

      <div className="grid gap-4 lg:grid-cols-3">
        <Panel title="Manual drive" subtitle="Sends DIAG commands over WebSocket">
          <div className="space-y-3">
            <label className="block">
              <span className="mb-1 flex justify-between text-[11px] uppercase tracking-wider text-t-secondary">
                Throttle <span className="font-mono text-t-primary">{settings.manualPwm}/255</span>
              </span>
              <input
                type="range"
                min={40}
                max={255}
                value={settings.manualPwm}
                onChange={(e) => update({ manualPwm: Number(e.target.value) })}
                className="w-full accent-accent"
                disabled={!canDrive}
              />
            </label>
            <label className="block">
              <span className="mb-1 flex justify-between text-[11px] uppercase tracking-wider text-t-secondary">
                Duration{" "}
                <span className="font-mono text-t-primary">
                  {settings.commandDurationMs === 0
                    ? "latched"
                    : `${settings.commandDurationMs} ms`}
                </span>
              </span>
              <input
                type="range"
                min={0}
                max={4000}
                step={100}
                value={settings.commandDurationMs}
                onChange={(e) => update({ commandDurationMs: Number(e.target.value) })}
                className="w-full accent-accent"
                disabled={!canDrive}
              />
            </label>

            <button
              type="button"
              className={actionBtn()}
              disabled={!canDrive}
              onClick={() => void drive("fwd")}
              title="Drive both motors forward"
            >
              <ArrowBigUp className={`h-5 w-5 ${heading === null ? "" : "opacity-60"}`} />
              Forward {settings.manualPwm}
            </button>
            <div className="grid grid-cols-2 gap-3">
              <button
                type="button"
                className={actionBtn()}
                disabled={!canDrive}
                onClick={() => void drive("left")}
              >
                <ArrowLeft className={`h-5 w-5 ${heading === "left" ? "text-secondary" : ""}`} />
                Turn left
              </button>
              <button
                type="button"
                className={actionBtn()}
                disabled={!canDrive}
                onClick={() => void drive("right")}
              >
                <ArrowRight className={`h-5 w-5 ${heading === "right" ? "text-secondary" : ""}`} />
                Turn right
              </button>
            </div>
            <button type="button" className="btn-warning w-full py-3" onClick={stop}>
              <CircleStop className="h-5 w-5" /> Stop motors
            </button>

            <p className="text-[11px] leading-relaxed text-t-secondary">
              Commands are acknowledged by the firmware (avg &lt; 10 ms local);
              motors always expire to 0 PWM when the bridge times out. Autonomy
              and safety latches override manual input.
            </p>
          </div>
        </Panel>

        <Panel title="State machine" subtitle={`Current: ${state}`}>
          <div className="flex flex-wrap gap-1.5">
            {NAV_STATES.filter((s) => s !== "UNKNOWN").map((s) => (
              <span
                key={s}
                title={STATE_DESCRIPTIONS[s]}
                className={`rounded border px-1.5 py-0.5 text-[10px] font-medium ${
                  CHIP_TONE[STATE_COLOR[s]] ?? CHIP_TONE.warning
                } ${s === state ? "ring-1 ring-t-primary/60" : "opacity-70"}`}
              >
                {s}
              </span>
            ))}
          </div>
          <div className="mt-3 rounded-lg border border-t-border bg-elevated p-3 text-xs leading-relaxed text-t-secondary">
            <span className="font-semibold text-t-primary">{state}: </span>
            {STATE_DESCRIPTIONS[state]}
          </div>

          <div className="mt-3 space-y-2">
            <button
              type="button"
              className={`w-full ${autonomous ? "btn-warning" : "btn-primary"}`}
              onClick={() => void toggleAuto()}
              title={autonomous ? "Return to manual idle" : "Engage autonomous mission"}
            >
              <Compass className="h-4 w-4" />
              {autonomous ? "Disengage autonomy" : "Engage autonomy"}
            </button>
            {autonomous && (
              <p className="text-[11px] text-t-secondary">
                Firmware owns the motors — manual buttons are locked out until
                you disengage.
              </p>
            )}
          </div>
        </Panel>

        <Panel title="Emergency stop" subtitle="Firmware-latched, survives disconnects">
          <div className="space-y-3">
            <div className="flex items-center gap-2 rounded-lg border border-critical/40 bg-critical/10 p-3">
              <ShieldAlert className="h-5 w-5 text-critical" aria-hidden />
              <div className="text-xs">
                <div className="font-semibold text-critical">
                  {estop ? "E-STOP LATCHED" : "Ready to latch"}
                </div>
                <div className="text-t-secondary">
                  {estop
                    ? "Motors + conveyor inhibited until cleared."
                    : "Latching forces NAV_EMERGENCY_STOP on the controller."}
                </div>
              </div>
            </div>

            {estop ? (
              <ConfirmButton
                label="Clear E-STOP"
                confirmLabel="Confirm clear — press again"
                onConfirm={() => {
                  void estopClear().then((r) =>
                    r.ok
                      ? show("E-STOP cleared", "ok")
                      : show("Clear failed", "crit", r.reason),
                  );
                }}
                className="btn-primary w-full"
              />
            ) : (
              <ConfirmButton
                label="ENGAGE E-STOP"
                confirmLabel="Confirm E-STOP — press again"
                onConfirm={() => {
                  void estopLatch().then((r) =>
                    r.ok
                      ? show("Emergency stop latched", "crit", "Firmware will hold until cleared.")
                      : show("E-STOP not acknowledged", "warn", r.reason),
                  );
                }}
                className="btn-danger w-full py-3"
              />
            )}

            <div className="rounded-lg border border-t-border bg-elevated p-3 text-[11px] leading-relaxed text-t-secondary">
              The browser only <b className="text-t-primary">requests</b> the
              stop — the ESP32 safety supervisor latches it independently of
              Wi-Fi or this page staying open.
            </div>

            <button
              type="button"
              className="btn-secondary w-full text-xs"
              onClick={() =>
                void send({ cmd: DIAG_CMD.STOP, value: 0 }).then((r) =>
                  show(
                    r.ok ? "Plain stop acknowledged" : `Stop ${r.reason}`,
                    r.ok ? "ok" : "warn",
                  ),
                )
              }
            >
              <CircleStop className="h-4 w-4" /> Send plain STOP (no latch)
            </button>
          </div>
        </Panel>
      </div>

      <div className="grid gap-4 lg:grid-cols-3">
        <Panel title="Ultrasonic ranging" subtitle="Obstacle sensors — −1 cm means invalid">
          <div className="space-y-4">
            <DistanceBar label="Left sensor" cm={t?.left_cm ?? -1} side="left" />
            <DistanceBar label="Right sensor" cm={t?.right_cm ?? -1} side="right" />
            <div className="flex items-center justify-between text-[11px] text-t-secondary">
              <span>Invalid readings trigger FAULT_SENSOR_BAD in firmware.</span>
              <StatusPill
                tone={(t?.left_cm ?? -1) < 0 || (t?.right_cm ?? -1) < 0 ? "warn" : "ok"}
                label={
                  (t?.left_cm ?? -1) < 0 || (t?.right_cm ?? -1) < 0 ? "SENSOR INVALID" : "SENSORS OK"
                }
              />
            </div>
          </div>
        </Panel>

        <Panel title="Drive output" subtitle="Live actuator output">
          <div className="flex items-center justify-around">
            <Gauge
              value={((t?.left_pwm ?? 0) / 255) * 100}
              label="Left"
              color="#38BDF8"
              sub={`${t?.left_pwm ?? 0}/255`}
            />
            <Gauge
              value={((t?.right_pwm ?? 0) / 255) * 100}
              label="Right"
              color="#19C7A1"
              sub={`${t?.right_pwm ?? 0}/255`}
            />
          </div>
          <div className="mt-2 grid grid-cols-2 gap-2 text-center text-[11px] text-t-secondary">
            <div className="panel-elevated py-1">
              left dir{" "}
              <b className="text-t-primary">
                {(t?.left_dir ?? 0) > 0 ? "FWD" : (t?.left_dir ?? 0) < 0 ? "REV" : "STOP"}
              </b>
            </div>
            <div className="panel-elevated py-1">
              right dir{" "}
              <b className="text-t-primary">
                {(t?.right_dir ?? 0) > 0 ? "FWD" : (t?.right_dir ?? 0) < 0 ? "REV" : "STOP"}
              </b>
            </div>
          </div>
          <div className="mt-3 border-t border-t-border pt-3">
            <div className="flex items-center justify-between">
              <span className="text-[11px] uppercase tracking-wider text-t-secondary">
                Conveyor
              </span>
              <StatusPill
                tone={
                  t?.conveyor === "FAULT"
                    ? "crit"
                    : t?.conveyor === "RUNNING" || t?.conveyor === "STARTING"
                      ? "ok"
                      : "idle"
                }
                label={t?.conveyor ?? "—"}
              />
            </div>
            <div className="mt-2 grid grid-cols-2 gap-2">
              <button
                type="button"
                className="btn-secondary text-xs disabled:opacity-40"
                disabled={latched}
                onClick={() => void runConveyor(true)}
              >
                Conveyor ON
              </button>
              <button
                type="button"
                className="btn-secondary text-xs disabled:opacity-40"
                disabled={latched}
                onClick={() => void runConveyor(false)}
              >
                Conveyor OFF
              </button>
            </div>
          </div>
        </Panel>

        <div className="space-y-4">
          <div className="grid grid-cols-2 gap-3">
            <Metric
              label="GPS"
              value={t ? (t.gps_fix ? "FIX" : "NO FIX") : "—"}
              quality={quality}
              tone={t && !t.gps_fix ? "warn" : "ok"}
              sub={t ? `${t.satellites} satellites` : "—"}
            />
            <Metric
              label="Fix age"
              value={t ? formatDuration(t.gps_age_ms) : "—"}
              quality={quality}
              sub="Time since last fix"
            />
          </div>
          <Panel title="Position" subtitle={t ? formatLatLon(t.latitude, t.longitude, t.gps_fix) : "—"}>
            <div className="flex items-center gap-2 text-xs text-t-secondary">
              <Satellite className="h-4 w-4 text-secondary" aria-hidden />
              {t?.gps_fix
                ? "Position streams from the NMEA GPS on UART2."
                : "No fix yet — coordinates report 0.000000 until satellites lock."}
            </div>
            <div className="mt-2 grid grid-cols-2 gap-2 font-mono text-xs">
              <div className="panel-elevated p-2">
                <div className="text-t-secondary">latitude</div>
                <div className="text-t-primary">{t?.latitude.toFixed(6) ?? "—"}</div>
              </div>
              <div className="panel-elevated p-2">
                <div className="text-t-secondary">longitude</div>
                <div className="text-t-primary">{t?.longitude.toFixed(6) ?? "—"}</div>
              </div>
            </div>
          </Panel>
          <Panel title="Controller">
            <div className="space-y-1 text-xs text-t-secondary">
              <div className="flex justify-between">
                <span>Protocol</span>
                <span className="font-mono text-t-primary">
                  v{t?.protocol_version ?? "—"} · {t?.device_id ?? "—"}
                </span>
              </div>
              <div className="flex justify-between">
                <span>Uptime</span>
                <span className="font-mono text-t-primary">
                  {t ? formatDuration(t.uptime_ms) : "—"}
                </span>
              </div>
              <div className="flex justify-between">
                <span>Acked frames</span>
                <span className="font-mono text-t-primary">{lastAck}</span>
              </div>
              <div className="flex justify-between">
                <span>Mode</span>
                <span className="font-mono text-t-primary">{demo ? "SIMULATION" : conn.status}</span>
              </div>
            </div>
          </Panel>
        </div>
      </div>

      <div className="flex items-center gap-2 text-[11px] text-t-secondary">
        <GaugeIcon className="h-3.5 w-3.5" aria-hidden />
        Hold-to-run was replaced by duration-bounded commands (0–60000 ms) —
        firmware auto-stops when the duration elapses or the bridge times out.
      </div>
    </div>
  );
}
