import { useState } from "react";
import {
  HelpCircle,
  Link2,
  MonitorSmartphone,
  RotateCcw,
  Route,
  Save,
  Sliders,
  Square,
  Wifi,
} from "lucide-react";
import { Panel } from "../components/ui/Panel";
import { StatusPill } from "../components/ui/StatusPill";
import { useSettings } from "../state/SettingsContext";
import { useTelemetry } from "../state/TelemetryContext";
import { useBoat } from "../state/useBoat";
import { useDemo } from "../state/DemoContext";
import { DEMO_SCENARIOS } from "../state/demoEngine";
import { useToast } from "../components/ui/Toast";
import { ConfirmButton } from "../components/ui/Toast";
import { connectionManager } from "../services/communication";
import { DEFAULT_SETTINGS } from "../lib/settings";

export function SettingsPage() {
  const { settings, update, reset } = useSettings();
  const { csv } = useTelemetry();
  const { conn } = useBoat();
  const { active: demoActive, scenario, startDemo, stopDemo } = useDemo();
  const { show } = useToast();

  const [wsUrl, setWsUrl] = useState(settings.wsUrl);
  const [dirtyWs, setDirtyWs] = useState(false);

  const saveWs = () => {
    const v = wsUrl.trim();
    if (!/^wss?:\/\/.+/i.test(v)) {
      show("Invalid WebSocket URL", "crit", "Must start with ws:// or wss://");
      return;
    }
    update({ wsUrl: v });
    setDirtyWs(false);
    if (!demoActive) {
      connectionManager.disconnect();
      connectionManager.connect(v);
    }
    show("Endpoint saved", "ok", v);
  };

  return (
    <div className="space-y-4">
      <header className="flex flex-wrap items-center justify-between gap-3">
        <div>
          <h1 className="text-lg font-bold">System Settings</h1>
          <p className="text-xs text-t-secondary">
            Connection, controls and display preferences — stored in this
            browser (localStorage)
          </p>
        </div>
        <StatusPill
          tone={conn.status === "online" ? "ok" : conn.status === "demo" ? "demo" : conn.status === "stale" ? "warn" : "crit"}
          label={`LINK ${conn.status.toUpperCase()}`}
        />
      </header>

      <div className="grid gap-4 lg:grid-cols-2">
        <Panel
          title="Connection"
          subtitle="Cloud MQTT or direct WebSocket to the boat"
          actions={
            <StatusPill
              tone={demoActive ? "demo" : "info"}
              label={demoActive ? "DEMO ACTIVE" : "LIVE"}
            />
          }
        >
          <label className="block text-[11px] font-medium uppercase tracking-wider text-t-secondary">
            Endpoint
          </label>
          <div className="mt-1 flex gap-2">
            <input
              type="text"
              value={wsUrl}
              onChange={(e) => {
                setWsUrl(e.target.value);
                setDirtyWs(true);
              }}
              className="flex-1 rounded-lg border border-t-border bg-elevated px-3 py-2 font-mono text-xs text-t-primary focus:border-secondary focus:outline-none"
              placeholder="ws://192.168.4.1:81"
              spellCheck={false}
            />
            <button type="button" className="btn-primary text-xs" disabled={!dirtyWs} onClick={saveWs}>
              <Save className="h-3.5 w-3.5" /> Save
            </button>
          </div>
          <p className="mt-1 text-[11px] text-t-secondary">
            Current: <span className="font-mono">{conn.url || settings.wsUrl}</span>
          </p>

          <div className="mt-3 rounded-lg border border-t-border bg-elevated p-3 text-[11px] leading-relaxed text-t-secondary">
            <Wifi className="mr-1 inline h-3.5 w-3.5 text-secondary" aria-hidden />
            <b className="text-t-primary">Two ways to connect:</b>
            <ul className="mt-1 list-inside list-disc space-y-1">
              <li>
                <b className="text-t-primary">Direct Wi-Fi (no internet needed):</b>{" "}
                enter{" "}
                <span className="font-mono text-t-primary">ws://192.168.4.1:81</span>{" "}
                and join the <span className="font-mono">RiverClean-Boat</span> Wi-Fi
                (password in firmware <span className="font-mono">boat_config.h</span>).
                The ESP32 serves its own WebSocket gateway. Internet is NOT available
                while connected to the boat.
              </li>
              <li>
                <b className="text-t-primary">Cloud MQTT (internet needed):</b>{" "}
                <span className="font-mono text-t-primary">
                  wss://broker.hivemq.com:8884/mqtt
                </span>{" "}
                — works from anywhere with internet; the boat needs a router link too.
              </li>
            </ul>
            The ESP32 keeps ESP-NOW on channel 1 while hosting the AP. Any endpoint
            containing <span className="font-mono">/mqtt</span> is treated as a
            broker; every other <span className="font-mono">ws://</span> endpoint is
            a direct gateway.
          </div>

          <div className="mt-3 flex gap-2">
            <button
              type="button"
              className="btn-secondary text-xs"
              disabled={demoActive}
              onClick={() => {
                connectionManager.disconnect();
                connectionManager.connect(settings.wsUrl);
                show("Reconnecting…", "info");
              }}
            >
              <Link2 className="h-3.5 w-3.5" /> Reconnect now
            </button>
            <button
              type="button"
              className="btn-secondary text-xs"
              disabled={demoActive}
              onClick={() => {
                connectionManager.disconnect();
                show("Disconnected", "warn", "Press reconnect when ready.");
              }}
            >
              Disconnect
            </button>
          </div>
        </Panel>

        <Panel title="Camera & map" subtitle="Vision is detection-only — no video stream">
          <p className="text-[11px] leading-relaxed text-t-secondary">
            The ESP32-CAM no longer serves an MJPEG stream: it captures
            160×120 grayscale frames, runs the on-device waste detector and
            sends <b className="text-t-primary">name · accuracy · position</b>{" "}
            over ESP-NOW to the main controller, which publishes them to this
            dashboard over MQTT (see AI Camera page).
          </p>
          <label className="mt-3 flex items-center gap-2 text-xs text-t-secondary">
            <input
              type="checkbox"
              checked={settings.mapEnabled}
              onChange={(e) => update({ mapEnabled: e.target.checked })}
              className="accent-accent"
            />
            Show satellite map (Overview)
          </label>
        </Panel>

        <Panel title="Manual controls" subtitle="Defaults for Navigation buttons">
          <label className="block text-[11px] font-medium uppercase tracking-wider text-t-secondary">
            Throttle:{" "}
            <span className="font-mono text-t-primary">{settings.manualPwm}/255</span>
          </label>
          <input
            type="range"
            min={40}
            max={255}
            value={settings.manualPwm}
            onChange={(e) => update({ manualPwm: Number(e.target.value) })}
            className="mt-1 w-full accent-accent"
          />
          <label className="mt-3 block text-[11px] font-medium uppercase tracking-wider text-t-secondary">
            Duration:{" "}
            <span className="font-mono text-t-primary">
              {settings.commandDurationMs === 0 ? "latched" : `${settings.commandDurationMs} ms`}
            </span>
          </label>
          <input
            type="range"
            min={0}
            max={4000}
            step={100}
            value={settings.commandDurationMs}
            onChange={(e) => update({ commandDurationMs: Number(e.target.value) })}
            className="mt-1 w-full accent-accent"
          />
          <div className="mt-2 flex items-center gap-2 text-[11px] text-t-secondary">
            <Sliders className="h-3.5 w-3.5" aria-hidden />
            0 ms = latched until STOP (still auto-expires via safety timeout)
          </div>
        </Panel>

        <Panel title="Data & reset">
          <label className="flex items-center justify-between rounded-lg border border-t-border bg-elevated px-3 py-2 text-xs">
            <span className="text-t-primary">CSV telemetry logging</span>
            <input
              type="checkbox"
              checked={csv.enabled}
              onChange={(e) => csv.setEnabled(e.target.checked)}
              className="accent-accent"
            />
          </label>
          <p className="mt-1 text-[11px] text-t-secondary">
            {csv.enabled
              ? `${csv.rowCount} rows buffered — manage on the Diagnostics page.`
              : "Enable to record every sample with full timestamps."}
          </p>

          <div className="mt-3 flex items-center justify-between rounded-lg border border-warning/30 bg-warning/10 px-3 py-2 text-xs">
            <span>
              <RotateCcw className="mr-1 inline h-3.5 w-3.5" aria-hidden />
              Reset all settings to defaults
            </span>
            <ConfirmButton
              label="Reset"
              confirmLabel="Confirm reset"
              onConfirm={() => {
                reset();
                setWsUrl(DEFAULT_SETTINGS.wsUrl);
                show("Settings reset to defaults", "info");
              }}
              className="btn-warning text-xs"
            />
          </div>

          <div className="mt-3 rounded-lg border border-t-border bg-elevated p-3 text-[11px] leading-relaxed text-t-secondary">
            <Route className="mr-1 inline h-3.5 w-3.5 text-secondary" aria-hidden />
            <MonitorSmartphone className="mr-1 inline h-3.5 w-3.5 text-secondary" aria-hidden />
            Settings never leave this device — nothing is uploaded anywhere.
          </div>
        </Panel>
      </div>

      <div className="grid gap-4 lg:grid-cols-2">
        <Panel
          title="Simulation"
          subtitle="Demo telemetry — replaces live data until stopped"
          actions={
            <StatusPill
              tone={demoActive ? "demo" : "info"}
              label={demoActive ? "SIMULATION ON" : "LIVE DATA"}
            />
          }
        >
          <div className="grid gap-2 sm:grid-cols-3">
            {DEMO_SCENARIOS.map((s) => (
              <button
                key={s.id}
                type="button"
                className={`rounded-lg border px-3 py-2 text-left text-xs transition ${
                  demoActive && scenario === s.id
                    ? "border-accent bg-accent/10 text-t-primary"
                    : "border-t-border bg-elevated text-t-secondary hover:border-secondary"
                }`}
                title={s.blurb}
                onClick={() => {
                  startDemo(s.id);
                  show("Simulation started", "info", s.label);
                }}
              >
                <span className="block font-medium text-t-primary">{s.label}</span>
                <span className="mt-0.5 block text-[11px] leading-snug">
                  {s.blurb}
                </span>
              </button>
            ))}
          </div>
          <div className="mt-3 flex items-center justify-between gap-2">
            <p className="text-[11px] text-t-secondary">
              Demo mode fakes telemetry in the browser only — no commands are
              sent to the boat.
            </p>
            <button
              type="button"
              className="btn-secondary shrink-0 text-xs"
              disabled={!demoActive}
              onClick={() => {
                stopDemo();
                show("Back to live telemetry", "ok");
              }}
            >
              <Square className="h-3.5 w-3.5" /> Stop
            </button>
          </div>
        </Panel>

        <Panel
          title="Getting started"
          subtitle="From unboxing to driving in four steps"
          actions={<HelpCircle className="h-4 w-4 text-secondary" aria-hidden />}
        >
          <ol className="space-y-2 text-xs leading-relaxed text-t-secondary">
            <li>
              <b className="text-t-primary">1. Power both boards.</b> Main
              ESP32 (COM8) and ESP32-CAM (COM4) need their own 5 V supply.
            </li>
            <li>
              <b className="text-t-primary">2. Join the boat Wi-Fi.</b>{" "}
              <span className="font-mono text-t-primary">RiverClean-Boat</span>{" "}
              (password <span className="font-mono">riverclean2026</span>) — no
              internet while connected.
            </li>
            <li>
              <b className="text-t-primary">3. Open this dashboard.</b>{" "}
              Overview shows <b className="text-accent">LINK ONLINE</b> when
              telemetry flows. Camera page shows live video.
            </li>
            <li>
              <b className="text-t-primary">4. Drive.</b> Use the Drive page —
              the red <b className="text-critical">E-STOP</b> is always one press
              away; autonomy engages with ENGAGE AUTONOMY.
            </li>
          </ol>
        </Panel>

        <Panel title="Troubleshooting" subtitle="The five issues you are most likely to hit">
          <ul className="space-y-2 text-xs leading-relaxed text-t-secondary">
            <li>
              <b className="text-t-primary">LINK OFFLINE.</b> Rejoin{" "}
              <span className="font-mono">RiverClean-Boat</span>, check both
              boards are powered, then press Reconnect above.
            </li>
            <li>
              <b className="text-t-primary">No detections.</b> Point the CAM at
              waste against the water background — the on-device detector needs
              visible contrast (blob at least ~0.7% of frame). Check{" "}
              <b className="text-t-primary">CAM LINK OK</b> on the AI Camera
              page first.
            </li>
            <li>
              <b className="text-t-primary">CAM LINK LOST / fault 1.</b> The
              ESP32-CAM is unplugged or off — reconnect it; the main board
              auto-recovers within 2 s.
            </li>
            <li>
              <b className="text-t-primary">Motors do nothing.</b> Clear any
              E-STOP latch on the Drive page, remove the ENA/ENB jumpers on the
              L298N board, and verify the motor supply.
            </li>
            <li>
              <b className="text-t-primary">Still stuck.</b> Full matrix in{" "}
              <span className="font-mono">docs/TROUBLESHOOTING.md</span>.
            </li>
          </ul>
        </Panel>
      </div>

      <Panel title="Endpoint defaults" subtitle="Reference for restoring connectivity">
        <div className="grid gap-2 font-mono text-xs text-t-secondary sm:grid-cols-2 lg:grid-cols-4">
          <div className="panel-elevated p-2">
            <div>Wi-Fi SSID</div>
            <div className="text-t-primary">RiverClean-Boat</div>
          </div>
          <div className="panel-elevated p-2">
            <div>IP</div>
            <div className="text-t-primary">192.168.4.1</div>
          </div>
          <div className="panel-elevated p-2">
            <div>WebSocket</div>
            <div className="text-t-primary">ws://192.168.4.1:81</div>
          </div>
          <div className="panel-elevated p-2">
            <div>ESP-NOW channel</div>
            <div className="text-t-primary">1 (fixed)</div>
          </div>
        </div>
      </Panel>
    </div>
  );
}
