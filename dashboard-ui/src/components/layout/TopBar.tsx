import { Menu, ShieldAlert, PlayCircle, MonitorSmartphone } from "lucide-react";
import { StatusPill, type PillTone } from "../ui/StatusPill";
import { ConfirmButton } from "../ui/Toast";
import { useBoat } from "../../state/useBoat";
import { STATE_PILL } from "../../lib/faults";
import { useTelemetry } from "../../state/TelemetryContext";
import { useDemo } from "../../state/DemoContext";
import { formatDuration } from "../../lib/format";

const CONN_PILL: Record<string, { tone: PillTone; label: string }> = {
  online: { tone: "ok", label: "LINK ONLINE" },
  stale: { tone: "warn", label: "STALE DATA" },
  offline: { tone: "crit", label: "OFFLINE" },
  connecting: { tone: "info", label: "CONNECTING" },
  demo: { tone: "demo", label: "SIMULATION" },
};

export function TopBar({ onMenu }: { onMenu: () => void }) {
  const { t, conn, faults, demo } = useBoat();
  const { estopClear } = useTelemetry();
  const { active: demoActive, startDemo, stopDemo } = useDemo();

  const connPill = CONN_PILL[conn.status] ?? CONN_PILL.offline;
  const stateLabel = t?.state ?? "NO DATA";
  const stateTone: PillTone = t ? STATE_PILL[t.state] : "idle";

  return (
    <header className="sticky top-0 z-20 flex h-14 items-center gap-3 border-b border-t-border bg-sidebar/95 px-4 backdrop-blur">
      <button
        type="button"
        className="rounded p-1.5 text-t-secondary hover:text-t-primary lg:hidden"
        onClick={onMenu}
        aria-label="Open menu"
      >
        <Menu className="h-5 w-5" />
      </button>

      <div className="flex min-w-0 flex-wrap items-center gap-2">
        <StatusPill
          tone={connPill.tone}
          label={connPill.label}
          title={
            demo
              ? "Demo mode — simulated telemetry only"
              : `WebSocket ${conn.url || "not configured"}`
          }
          pulse={conn.status === "connecting"}
        />
        <StatusPill tone={stateTone} label={stateLabel} title="Navigation state" />
        {faults.slice(0, 2).map((f) => (
          <StatusPill key={f.bit} tone={f.severity === "critical" ? "crit" : "warn"} label={f.label} />
        ))}
        {faults.length > 2 && (
          <StatusPill tone="warn" label={`+${faults.length - 2} more`} />
        )}
      </div>

      <div className="ml-auto flex items-center gap-3">
        {t && (
          <span
            className="hidden font-mono text-xs text-t-secondary sm:inline"
            title="Controller uptime"
          >
            up {formatDuration(t.uptime_ms)}
          </span>
        )}
        <button
          type="button"
          onClick={() => (demoActive ? stopDemo() : startDemo())}
          className={`btn-secondary hidden text-xs sm:inline-flex ${demoActive ? "border-purple-400/50 text-purple-300" : ""}`}
          title="Toggle demo mode (disconnects live link while active)"
        >
          {demoActive ? (
            <>
              <MonitorSmartphone className="h-3.5 w-3.5" /> Exit demo
            </>
          ) : (
            <>
              <PlayCircle className="h-3.5 w-3.5" /> Demo mode
            </>
          )}
        </button>
        {t?.estop ? (
          <ConfirmButton
            label="E-STOP LATCHED — Clear"
            confirmLabel="Press again to clear"
            onConfirm={() => {
              void estopClear();
            }}
            className="btn-danger animate-pulse text-xs"
          />
        ) : (
          <StatusPill tone="crit" label="E-STOP ready" title="Emergency stop available in Control" />
        )}
        <span className="hidden text-t-secondary md:inline" title="Safety supervisor">
          <ShieldAlert className="h-4 w-4" />
        </span>
      </div>
    </header>
  );
}
