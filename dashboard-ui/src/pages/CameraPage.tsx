import { Crosshair, Radio } from "lucide-react";
import { Panel } from "../components/ui/Panel";
import { StatusPill } from "../components/ui/StatusPill";
import { Metric } from "../components/ui/Metric";
import { useBoat } from "../state/useBoat";
import { classLabel } from "../services/communication";
import { formatClockTime } from "../lib/format";

export function CameraPage() {
  const { t, conn, demo, snapshot } = useBoat();

  const quality = demo ? "demo" : conn.status === "online" ? "live" : "stale";
  const events = snapshot.detections;

  return (
    <div className="space-y-4">
      <header className="flex flex-wrap items-center justify-between gap-3">
        <div>
          <h1 className="text-lg font-bold">AI Camera</h1>
          <p className="text-xs text-t-secondary">
            On-device waste detection, AI output and link health — no live video stream
          </p>
        </div>
        <div className="flex items-center gap-2">
          <StatusPill
            tone={t?.detection ? "ok" : "idle"}
            label={t?.detection ? "TARGET LOCK" : "SCANNING"}
            pulse={t?.detection}
          />
          <StatusPill
            tone={t?.comms_ok ? (demo ? "demo" : "ok") : "crit"}
            label={
              demo
                ? "SIMULATION"
                : t
                  ? t.comms_ok
                    ? "CAM LINK OK"
                    : "CAM LINK LOST"
                  : "NO LINK DATA"
            }
          />
        </div>
      </header>

      <div className="grid gap-4 lg:grid-cols-3">
        <Panel
          title="Camera status"
          subtitle="Detection-only vision — MJPEG stream removed by design"
          className="lg:col-span-2"
          bodyClassName="p-2"
        >
          <div className="relative flex min-h-[220px] w-full items-center justify-center overflow-hidden rounded-lg border border-t-border bg-black/60 p-6 text-center">
            <div className="max-w-md space-y-3">
              <Crosshair className="mx-auto h-8 w-8 text-accent" aria-hidden />
              <p className="text-sm font-medium text-t-primary">
                Detection-only camera
              </p>
              <p className="text-xs text-t-secondary">
                The ESP32-CAM captures 160×120 grayscale frames, segments waste
                blobs against the water background and classifies them on-device
                (plastic bottle / plastic bag / general debris). Name, accuracy
                and position reach this dashboard over ESP-NOW → main
                controller → MQTT. No video is streamed.
              </p>
            </div>
            {t?.detection && (
              <div
                className="pointer-events-none absolute bottom-3 left-3 rounded border border-accent/60 bg-bg/85 px-2 py-1 text-xs"
                title="Latest AI lock"
              >
                <span className="text-accent">{classLabel(t.class_id)}</span>{" "}
                <span className="font-mono">{t.confidence}%</span>
              </div>
            )}
          </div>
        </Panel>

        <div className="space-y-4">
          <Panel title="Link health" subtitle="ESP-NOW radio to camera module">
            <div className="grid grid-cols-2 gap-3">
              <Metric
                label="Comms"
                value={t ? (t.comms_ok ? "OK" : "LOST") : "—"}
                quality={quality}
                tone={t && !t.comms_ok ? "crit" : "ok"}
                sub="Frames < 2 s"
              />
              <Metric
                label="RX frames"
                value={t?.rx_bytes ?? "—"}
                quality={quality}
                sub="Cumulative bytes"
              />
            </div>
            <div className="mt-3 flex items-start gap-2 rounded-lg border border-t-border bg-elevated p-2 text-[11px] text-t-secondary">
              <Radio className="mt-0.5 h-3.5 w-3.5 shrink-0 text-secondary" aria-hidden />
              <span>
                Link runs on Wi-Fi channel 1 with sleep disabled. Diagnostics
                counters live on the Diagnostics page.
              </span>
            </div>
          </Panel>
        </div>
      </div>

      <Panel
        title="AI detection"
        subtitle="Live inference output from the ESP32-CAM"
        actions={
          <StatusPill
            tone={csvPillTone(t?.detection)}
            label={t?.detection ? "LOCK" : "SCANNING"}
          />
        }
      >
        <div className="grid gap-4 lg:grid-cols-3">
          <div className="lg:col-span-2">
            {t?.detection ? (
              <div className="space-y-3">
                <div className="flex flex-wrap items-end justify-between gap-3">
                  <div>
                    <div className="text-2xl font-bold text-accent">
                      {classLabel(t.class_id)}
                    </div>
                    <div className="text-xs text-t-secondary">
                      class_id {t.class_id} · centre x = {t.center_x} / 96
                    </div>
                  </div>
                  <div className="text-right">
                    <div className="font-mono text-3xl font-bold">
                      {t.confidence}
                      <span className="text-lg text-t-secondary">%</span>
                    </div>
                    <div className="text-xs text-t-secondary">confidence</div>
                  </div>
                </div>
                <div className="h-2.5 overflow-hidden rounded-full bg-elevated">
                  <div
                    className="h-full rounded-full bg-accent transition-all"
                    style={{ width: `${Math.max(0, Math.min(100, t.confidence))}%` }}
                  />
                </div>
              </div>
            ) : (
              <div className="flex h-24 flex-col items-center justify-center gap-1 text-center">
                <span className="text-sm font-medium text-t-primary">
                  No active detection
                </span>
                <p className="max-w-md text-xs text-t-secondary">
                  {demo
                    ? "Simulation is between targets — advance the mission through SEARCH to generate detections."
                    : "The camera reports no-object frames while idle or searching. Detection activates when debris is found."}
                </p>
              </div>
            )}
          </div>
          <div className="grid grid-cols-2 gap-3">
            <Metric
              label="Detections"
              value={events.length}
              quality={quality}
              sub="This session"
            />
            <Metric
              label="Center X"
              value={t?.detection ? t.center_x : "—"}
              quality={quality}
              sub="Pixels 0–96"
            />
          </div>
        </div>

        <div className="mt-4 border-t border-t-border pt-3">
          <div className="mb-2 flex items-center gap-1.5 text-[11px] uppercase tracking-wider text-t-secondary">
            <Crosshair className="h-3.5 w-3.5" aria-hidden /> Recent events
          </div>
          {events.length === 0 ? (
            <p className="text-xs text-t-secondary">No detection events yet.</p>
          ) : (
            <ul className="space-y-1">
              {events.slice(0, 5).map((d) => (
                <li
                  key={d.at}
                  className="flex items-center justify-between rounded bg-elevated/60 px-2 py-1.5 text-xs"
                >
                  <span className="font-mono text-t-secondary">
                    {formatClockTime(d.at)}
                  </span>
                  <span className="text-t-primary">{d.label}</span>
                  <span className="font-mono text-t-secondary">
                    {d.confidence}% · x={d.centerX}
                  </span>
                  <StatusPill
                    tone={d.source === "demo" ? "demo" : "ok"}
                    label={d.source === "demo" ? "SIM" : "LIVE"}
                  />
                </li>
              ))}
            </ul>
          )}
        </div>
      </Panel>
    </div>
  );
}

function csvPillTone(detection: boolean | undefined) {
  return detection ? "ok" : "idle";
}
