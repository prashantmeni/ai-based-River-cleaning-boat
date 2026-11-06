import {
  AlertTriangle,
  Bug,
  Cable,
  Download,
  Eraser,
  RefreshCw,
  Terminal,
} from "lucide-react";
import { Panel } from "../components/ui/Panel";
import { Metric } from "../components/ui/Metric";
import { StatusPill } from "../components/ui/StatusPill";
import { useBoat } from "../state/useBoat";
import { useTelemetry } from "../state/TelemetryContext";
import { useSettings } from "../state/SettingsContext";
import { useDemo } from "../state/DemoContext";
import { useToast } from "../components/ui/Toast";
import { connectionManager, FAULT } from "../services/communication";
import { formatBytes, formatDuration } from "../lib/format";

const ERR_LEGEND: { code: number | string; label: string; where: string }[] = [
  { code: 1, label: "Incomplete frame (normal during reassembly)", where: "not counted" },
  { code: 2, label: "BAD_START — first byte was not 0x7E", where: "rx_errors" },
  { code: 3, label: "BAD_END — end marker missing (see frame length bug log)", where: "rx_errors" },
  { code: 4, label: "BAD_VERSION — protocol version mismatch", where: "rx_errors" },
  { code: 5, label: "BAD_LENGTH — length field invalid", where: "rx_errors" },
  { code: 6, label: "BAD_CHECKSUM — XOR checksum failed (corrupt RF link)", where: "rx_errors" },
  { code: 7, label: "UNKNOWN_TYPE — unrecognised message id", where: "rx_errors" },
  { code: 8, label: "BUFFER_OVERFLOW — frame exceeded buffer, parser reset", where: "rx_errors" },
  { code: "0xA1", label: "AI_DETECTION payload parse failed", where: "rx_errors" },
  { code: "0xA2", label: "AI_NO_DETECTION payload too short", where: "rx_errors" },
  { code: "0xA3", label: "AI_HEALTH payload too short", where: "rx_errors" },
  { code: "0xA4", label: "DIAG_COMMAND payload too short", where: "rx_errors" },
];

const FAULT_ROWS: { bit: number; label: string; meaning: string }[] = [
  { bit: FAULT.COMMS_LOST, label: "COMMS_LOST", meaning: "No ESP-NOW frame from CAM for 2 s" },
  { bit: FAULT.OBSTACLE, label: "OBSTACLE", meaning: "Obstacle < 30 cm latched avoidance stop" },
  { bit: FAULT.SENSOR_BAD, label: "SENSOR_BAD", meaning: "Ultrasonic reported invalid (−1 cm)" },
  { bit: FAULT.GPS_NO_FIX, label: "GPS_NO_FIX", meaning: "Navigating without satellite fix" },
  { bit: FAULT.WATCHDOG, label: "WATCHDOG", meaning: "Task watchdog reset was recorded" },
];

function hex(v: number): string {
  return `0x${Math.max(0, v).toString(16).toUpperCase().padStart(2, "0")}`;
}

export function DiagnosticsPage() {
  const { t, conn, demo, faults } = useBoat();
  const { csv } = useTelemetry();
  const { settings } = useSettings();
  const { active: demoActive } = useDemo();
  const { show } = useToast();

  const quality = demo ? "demo" : conn.status === "online" ? "live" : "stale";
  const hello = conn.hello;
  const clientParseFails = conn.parseErrors;

  const reconnect = () => {
    if (demoActive) {
      show("Exit demo mode first", "warn", "Demo owns the connection while active.");
      return;
    }
    connectionManager.disconnect();
    connectionManager.connect(settings.wsUrl);
    show("Reconnecting…", "info", settings.wsUrl);
  };

  return (
    <div className="space-y-4">
      <header className="flex flex-wrap items-center justify-between gap-3">
        <div>
          <h1 className="text-lg font-bold">Diagnostics</h1>
          <p className="text-xs text-t-secondary">
            Link counters, error codes, faults and raw telemetry
          </p>
        </div>
        <div className="flex items-center gap-2">
          <StatusPill
            tone={conn.status === "online" ? "ok" : conn.status === "demo" ? "demo" : conn.status === "stale" ? "warn" : "crit"}
            label={conn.status.toUpperCase()}
          />
          <button type="button" className="btn-secondary text-xs" onClick={reconnect}>
            <RefreshCw className="h-3.5 w-3.5" /> Reconnect
          </button>
        </div>
      </header>

      <div className="grid grid-cols-2 gap-3 sm:grid-cols-3 xl:grid-cols-6">
        <Metric label="RX bytes" value={t ? formatBytes(t.rx_bytes) : "—"} quality={quality} sub="ESP-NOW payload total" />
        <Metric
          label="RX errors"
          value={t?.rx_errors ?? "—"}
          quality={quality}
          tone={(t?.rx_errors ?? 0) > 0 ? "warn" : "ok"}
          sub="Cumulative frame/payload errors"
        />
        <Metric
          label="Last error"
          value={t ? (t.perr === 0 ? "none" : hex(t.perr)) : "—"}
          quality={quality}
          tone={t && t.perr !== 0 ? "crit" : "ok"}
          sub="perr field (0 = OK)"
        />
        <Metric
          label="Drops"
          value={t?.drops ?? "—"}
          quality={quality}
          tone={(t?.drops ?? 0) > 0 ? "warn" : "ok"}
          sub="Ring-buffer overwrites"
        />
        <Metric
          label="Client parse fails"
          value={clientParseFails}
          quality={quality}
          tone={clientParseFails > 0 ? "warn" : "ok"}
          sub="Dashboard JSON rejections"
        />
        <Metric
          label="Uptime"
          value={t ? formatDuration(t.uptime_ms) : "—"}
          quality={quality}
          sub="Since controller reset"
        />
      </div>

      <div className="grid gap-4 lg:grid-cols-3">
        <Panel title="Frame error legend" subtitle="perr / rx_errors decoding">
          <div className="max-h-72 space-y-1.5 overflow-y-auto pr-1">
            {ERR_LEGEND.map((e) => {
              const active = t && String(e.code) === String(t.perr);
              return (
                <div
                  key={String(e.code)}
                  className={`flex items-start gap-2 rounded px-2 py-1.5 text-xs ${
                    active ? "bg-critical/15 ring-1 ring-critical/40" : "bg-elevated/60"
                  }`}
                >
                  <span className="w-12 shrink-0 font-mono font-semibold text-secondary">
                    {e.code}
                  </span>
                  <span className="text-t-secondary">
                    {e.label}
                    <span className="ml-1 opacity-70">· {e.where}</span>
                  </span>
                  {active && (
                    <span className="ml-auto shrink-0 text-[10px] font-semibold text-critical">
                      CURRENT
                    </span>
                  )}
                </div>
              );
            })}
          </div>
        </Panel>

        <Panel
          title="Fault bits"
          subtitle="safety.h flags — active set highlighted"
          actions={
            <StatusPill
              tone={faults.length > 0 ? "crit" : "ok"}
              label={faults.length > 0 ? `${faults.length} ACTIVE` : "CLEAR"}
            />
          }
        >
          <div className="space-y-1.5">
            {FAULT_ROWS.map((f) => {
              const active = (t?.fault_flags ?? 0) & f.bit;
              return (
                <div
                  key={f.bit}
                  className={`flex items-center justify-between rounded px-2 py-1.5 text-xs ${
                    active ? "bg-critical/15 ring-1 ring-critical/40" : "bg-elevated/60"
                  }`}
                >
                  <div>
                    <span className={`font-semibold ${active ? "text-critical" : "text-t-primary"}`}>
                      {f.label}
                    </span>
                    <span className="ml-2 text-t-secondary">{f.meaning}</span>
                  </div>
                  <span className="font-mono text-t-secondary">0x{f.bit.toString(16)}</span>
                </div>
              );
            })}
            {t && faults.length === 0 && (
              <p className="pt-1 text-center text-[11px] text-accent">
                No faults latched — {hex(t.fault_flags)}
              </p>
            )}
          </div>
          <div className="mt-3 rounded border border-t-border bg-elevated p-2 text-[11px] leading-relaxed text-t-secondary">
            Decoded live:{" "}
            {faults.length > 0
              ? faults.map((f) => f.label).join(" · ")
              : "none (0x00)"}
          </div>
        </Panel>

        <div className="space-y-4">
          <Panel title="Gateway" subtitle="WebSocket hello from firmware">
            {hello ? (
              <div className="space-y-1 font-mono text-xs text-t-secondary">
                <div className="flex justify-between">
                  <span>device_id</span>
                  <span className="text-t-primary">{hello.device_id}</span>
                </div>
                <div className="flex justify-between">
                  <span>protocol_version</span>
                  <span className="text-t-primary">{hello.protocol_version}</span>
                </div>
                <div className="flex justify-between">
                  <span>ws_port</span>
                  <span className="text-t-primary">{hello.ws_port}</span>
                </div>
                <div className="flex justify-between">
                  <span>espnow_channel</span>
                  <span className="text-t-primary">{hello.espnow_channel}</span>
                </div>
              </div>
            ) : (
              <p className="text-xs text-t-secondary">
                No hello received yet — connect to the boat AP
                (<span className="font-mono">RiverClean-Boat</span>) and open{" "}
                <span className="font-mono">{settings.wsUrl}</span>.
              </p>
            )}
            <div className="mt-2 flex items-center gap-2 text-[11px] text-t-secondary">
              <Cable className="h-3.5 w-3.5" aria-hidden />
              endpoint: <span className="font-mono">{conn.url || settings.wsUrl}</span>
            </div>
          </Panel>

          <Panel title="Last raw frame (CAM debug hex)">
            <code className="block break-all rounded bg-black/40 p-2 font-mono text-[11px] text-accent">
              {t?.dbg ? t.dbg : "—"}
            </code>
            <p className="mt-1.5 text-[10px] text-t-secondary">
              First 20 bytes of the most recent ESP-NOW datagram, as captured
              by the receiver.
            </p>
          </Panel>
        </div>
      </div>

      <Panel
        title="CSV telemetry log"
        subtitle={
          csv.enabled
            ? `Recording — ${csv.rowCount} rows (capped 20000)`
            : "Off — enable to record every sample"
        }
        actions={
          <>
            <button
              type="button"
              className={csv.enabled ? "btn-warning text-xs" : "btn-primary text-xs"}
              onClick={() => csv.setEnabled(!csv.enabled)}
            >
              {csv.enabled ? (
                <>
                  <AlertTriangle className="h-3.5 w-3.5" /> Stop logging
                </>
              ) : (
                <>
                  <Terminal className="h-3.5 w-3.5" /> Start logging
                </>
              )}
            </button>
            <button
              type="button"
              className="btn-secondary text-xs"
              disabled={csv.rowCount === 0}
              onClick={csv.exportFile}
            >
              <Download className="h-3.5 w-3.5" /> Export CSV
            </button>
            <button
              type="button"
              className="btn-secondary text-xs"
              disabled={csv.rowCount === 0}
              onClick={() => {
                csv.clear();
                show("CSV buffer cleared", "info");
              }}
            >
              <Eraser className="h-3.5 w-3.5" /> Clear
            </button>
          </>
        }
      >
        <p className="text-xs leading-relaxed text-t-secondary">
          Rows persist to browser storage every 5 s (survives refresh) and
          export as <span className="font-mono">riverclean-telemetry-*.csv</span>{" "}
          with a header row covering every telemetry field. Toggle from System
          Settings as well.
        </p>
      </Panel>

      <Panel
        title="Raw telemetry JSON"
        subtitle="Exactly what the controller published"
        actions={
          <StatusPill
            tone={demo ? "demo" : "info"}
            label={demo ? "SIMULATED FRAME" : "LIVE FRAME"}
          />
        }
      >
        {t ? (
          <pre className="max-h-72 overflow-auto rounded-lg bg-black/40 p-3 font-mono text-[11px] leading-relaxed text-accent">
            {JSON.stringify(t, null, 2)}
          </pre>
        ) : (
          <div className="flex h-24 items-center justify-center text-xs text-t-secondary">
            <Bug className="mr-2 h-4 w-4" aria-hidden />
            No frame received yet — check connection status above.
          </div>
        )}
      </Panel>
    </div>
  );
}
