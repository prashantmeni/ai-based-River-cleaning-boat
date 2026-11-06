import { Link } from "react-router-dom";
import {
  Bot,
  Camera as CameraIcon,
  Compass,
  Droplets,
  Radio,
  Recycle,
  Satellite,
  Timer,
  Waves,
} from "lucide-react";
import { Panel } from "../components/ui/Panel";
import { Metric } from "../components/ui/Metric";
import { Gauge } from "../components/ui/Gauge";
import { Sparkline } from "../components/ui/Sparkline";
import { StatusPill } from "../components/ui/StatusPill";
import { VesselMap } from "../components/map/VesselMap";
import { useBoat } from "../state/useBoat";
import { useTelemetry } from "../state/TelemetryContext";
import { useSettings } from "../state/SettingsContext";
import { STATE_DESCRIPTIONS, STATE_PILL } from "../lib/faults";
import {
  formatDistanceCm,
  formatDuration,
  formatLatLon,
  pwmPercent,
} from "../lib/format";
import { classLabel } from "../services/communication";

export function OverviewPage() {
  const { t, conn, faults, risk, demo, snapshot } = useBoat();
  const { csv } = useTelemetry();
  const { settings } = useSettings();

  const quality = demo ? "demo" : conn.status === "online" ? "live" : "stale";
  const trail = snapshot.series
    .filter((s) => s.t.gps_fix && (s.t.latitude !== 0 || s.t.longitude !== 0))
    .map((s) => ({ lat: s.t.latitude, lon: s.t.longitude }));
  const position =
    t && t.gps_fix && (t.latitude !== 0 || t.longitude !== 0)
      ? { lat: t.latitude, lon: t.longitude }
      : null;

  const speedSamples = snapshot.series.map((s) => ({
    v: Math.round((pwmPercent(s.t.left_pwm) + pwmPercent(s.t.right_pwm)) / 2),
  }));
  const recent = snapshot.detections.slice(0, 5);

  return (
    <div className="space-y-4">
      <header className="flex flex-wrap items-center justify-between gap-3">
        <div>
          <h1 className="text-lg font-bold">Overview</h1>
          <p className="text-xs text-t-secondary">
            Real-time mission and vessel status
          </p>
        </div>
        <div className="flex flex-wrap items-center gap-2">
          <StatusPill
            tone={risk === "critical" ? "crit" : risk === "attention" ? "warn" : "ok"}
            label={
              risk === "critical"
                ? "CRITICAL"
                : risk === "attention"
                  ? "ATTENTION"
                  : "ALL SYSTEMS OK"
            }
          />
          <StatusPill
            tone={STATE_PILL[t?.state ?? "UNKNOWN"]}
            label={t?.state ?? "NO DATA"}
          />
          {t?.autonomous && <StatusPill tone="info" label="AUTONOMOUS" />}
        </div>
      </header>

      <div className="grid grid-cols-2 gap-3 sm:grid-cols-3 xl:grid-cols-6">
        <Metric
          label="State"
          value={t?.state ?? "—"}
          quality={quality}
          tone={t?.state === "FAULT" || t?.state === "EMERGENCY_STOP" ? "crit" : "default"}
          sub={t ? STATE_DESCRIPTIONS[t.state] : "No telemetry yet"}
        />
        <Metric
          label="CAM link"
          value={t ? (t.comms_ok ? "OK" : "LOST") : "—"}
          quality={quality}
          tone={t && !t.comms_ok ? "crit" : "ok"}
          sub="ESP-NOW to ESP32-CAM"
        />
        <Metric
          label="Uptime"
          value={t ? formatDuration(t.uptime_ms) : "—"}
          quality={quality}
          sub="Controller uptime"
        />
        <Metric
          label="Collections"
          value={t?.collection_count ?? "—"}
          quality={quality}
          sub="Debris cycles completed"
        />
        <Metric
          label="GPS"
          value={t ? (t.gps_fix ? `${t.satellites} sat` : "NO FIX") : "—"}
          quality={quality}
          tone={t && !t.gps_fix ? "warn" : "ok"}
          sub={t ? `fix age ${formatDuration(t.gps_age_ms)}` : "—"}
        />
        <Metric
          label="Conveyor"
          value={t?.conveyor ?? "—"}
          quality={quality}
          tone={t?.conveyor === "FAULT" ? "crit" : "default"}
          sub="Debris collection system"
        />
      </div>

      {faults.length > 0 && (
        <Panel title="Active faults" subtitle="Firmware safety supervisor">
          <div className="flex flex-wrap gap-2">
            {faults.map((f) => (
              <StatusPill
                key={f.bit}
                tone={f.severity === "critical" ? "crit" : "warn"}
                label={f.label}
                title={`Bit 0x${f.bit.toString(16)}`}
              />
            ))}
          </div>
        </Panel>
      )}

      <div className="grid gap-4 lg:grid-cols-3">
        <Panel title="Motor output" subtitle="Last commanded PWM">
          <div className="flex items-center justify-around">
            <Gauge
              value={pwmPercent(t?.left_pwm ?? 0)}
              label="Left"
              color="#38BDF8"
              sub={
                t
                  ? `${t.left_pwm}/255 · dir ${t.left_dir > 0 ? "fwd" : t.left_dir < 0 ? "rev" : "stop"}`
                  : "—"
              }
            />
            <Gauge
              value={pwmPercent(t?.right_pwm ?? 0)}
              label="Right"
              color="#19C7A1"
              sub={
                t
                  ? `${t.right_pwm}/255 · dir ${t.right_dir > 0 ? "fwd" : t.right_dir < 0 ? "rev" : "stop"}`
                  : "—"
              }
            />
          </div>
        </Panel>

        <Panel
          title="Live detection"
          subtitle="Latest AI lock from ESP32-CAM"
          actions={
            <Link to="/camera" className="text-xs font-medium text-secondary hover:underline">
              Open
            </Link>
          }
        >
          {t?.detection ? (
            <div className="space-y-3">
              <div className="flex items-baseline justify-between">
                <span className="text-xl font-semibold">{classLabel(t.class_id)}</span>
                <span className="font-mono text-2xl font-bold text-accent">
                  {t.confidence}%
                </span>
              </div>
              <div className="h-2 overflow-hidden rounded-full bg-elevated">
                <div
                  className="h-full rounded-full bg-accent transition-all"
                  style={{ width: `${t.confidence}%` }}
                />
              </div>
              <div className="flex justify-between text-xs text-t-secondary">
                <span>Frame centre offset</span>
                <span className="font-mono">x = {t.center_x} / 96</span>
              </div>
            </div>
          ) : (
            <div className="flex h-28 flex-col items-center justify-center gap-1 text-center">
              <CameraIcon className="h-6 w-6 text-t-secondary" aria-hidden />
              <p className="text-sm text-t-primary">No object in frame</p>
              <p className="text-xs text-t-secondary">
                {demo
                  ? "Enable autonomous mode to run a mission"
                  : "CAM feeds detections while linked over ESP-NOW"}
              </p>
            </div>
          )}
        </Panel>

        <Panel title="Drive trend" subtitle="Average motor output, recent samples">
          <Sparkline
            data={speedSamples}
            min={0}
            max={100}
            unit="%"
            label="Motor output"
            color="#38BDF8"
          />
        </Panel>
      </div>

      <div className="grid gap-4 lg:grid-cols-3">
        <Panel
          title="Position"
          subtitle={
            t ? formatLatLon(t.latitude, t.longitude, t.gps_fix) : "No telemetry"
          }
          className="lg:col-span-2"
          actions={
            <StatusPill
              tone={t?.gps_fix ? "ok" : "warn"}
              label={t?.gps_fix ? "GPS FIX" : "NO FIX"}
            />
          }
          bodyClassName={settings.mapEnabled ? "p-2" : ""}
        >
          {settings.mapEnabled ? (
            <VesselMap position={position} trail={trail} />
          ) : (
            <p className="py-6 text-center text-xs text-t-secondary">
              Map disabled in System Settings.
            </p>
          )}
        </Panel>

        <Panel
          title="Recent detections"
          actions={
            <Link to="/camera" className="text-xs font-medium text-secondary hover:underline">
              All
            </Link>
          }
          bodyClassName="p-0"
        >
          {recent.length === 0 ? (
            <p className="px-4 py-6 text-center text-xs text-t-secondary">
              No detections recorded this session.
            </p>
          ) : (
            <ul className="divide-y divide-t-border">
              {recent.map((d) => (
                <li key={d.at} className="flex items-center justify-between px-4 py-2 text-xs">
                  <span className="flex items-center gap-2">
                    <span
                      className={`h-1.5 w-1.5 rounded-full ${d.source === "demo" ? "bg-purple-300" : "bg-accent"}`}
                    />
                    {d.label}
                  </span>
                  <span className="font-mono text-t-secondary">
                    {d.confidence}% · x{d.centerX}
                  </span>
                </li>
              ))}
            </ul>
          )}
        </Panel>
      </div>

      <div className="grid gap-4 sm:grid-cols-2 xl:grid-cols-4">
        <Panel title="Mission">
          <div className="flex items-center gap-2 text-sm">
            <Bot className="h-4 w-4 text-secondary" aria-hidden />
            <span>
              Autonomy{" "}
              <b className={t?.autonomous ? "text-accent" : "text-t-secondary"}>
                {t?.autonomous ? "ENGAGED" : "OFF"}
              </b>
            </span>
          </div>
          <p className="mt-2 text-xs text-t-secondary">
            {t ? STATE_DESCRIPTIONS[t.state] : "Awaiting controller telemetry."}
          </p>
          <Link to="/drive" className="mt-2 inline-block text-xs text-secondary hover:underline">
            Manage mission →
          </Link>
        </Panel>

        <Panel title="Sensors">
          <div className="grid grid-cols-2 gap-2 text-xs">
            <div className="panel-elevated p-2">
              <div className="flex items-center gap-1 text-t-secondary">
                <Waves className="h-3.5 w-3.5" aria-hidden /> Left
              </div>
              <div className="mt-1 font-mono text-sm">{formatDistanceCm(t?.left_cm ?? -1)}</div>
            </div>
            <div className="panel-elevated p-2">
              <div className="flex items-center gap-1 text-t-secondary">
                <Waves className="h-3.5 w-3.5" aria-hidden /> Right
              </div>
              <div className="mt-1 font-mono text-sm">{formatDistanceCm(t?.right_cm ?? -1)}</div>
            </div>
            <div
              className="panel-elevated col-span-2 p-2"
              title={
                t?.tds_mv != null
                  ? `raw sensor output ${t.tds_mv} mV — low mV means the probe is dry/not in water`
                  : undefined
              }
            >
              <div className="flex items-center justify-between">
                <span className="flex items-center gap-1 text-t-secondary">
                  <Droplets className="h-3.5 w-3.5" aria-hidden /> TDS
                </span>
                <span className="text-right">
                  <span className="block font-mono text-sm">
                    {t?.tds_ppm != null && t.tds_ppm >= 0
                      ? `${t.tds_ppm} ppm`
                      : t?.tds_ppm != null
                        ? "invalid"
                        : "—"}
                  </span>
                  {t?.tds_mv != null && (
                    <span className="block text-[10px] text-t-secondary font-mono">
                      {t.tds_mv} mV raw
                    </span>
                  )}
                </span>
              </div>
            </div>
          </div>
        </Panel>

        <Panel title="Data link">
          <div className="space-y-1 text-xs text-t-secondary">
            <div className="flex justify-between">
              <span className="flex items-center gap-1">
                <Radio className="h-3.5 w-3.5" aria-hidden /> RX bytes
              </span>
              <span className="font-mono text-t-primary">{t?.rx_bytes ?? "—"}</span>
            </div>
            <div className="flex justify-between">
              <span>Frame errors</span>
              <span className="font-mono text-t-primary">{t?.rx_errors ?? "—"}</span>
            </div>
            <div className="flex justify-between">
              <span>Drops</span>
              <span className="font-mono text-t-primary">{t?.drops ?? "—"}</span>
            </div>
            <div className="flex justify-between">
              <span>WebSocket</span>
              <span className="font-mono text-t-primary">{conn.status}</span>
            </div>
          </div>
          <Link to="/diagnostics" className="mt-2 inline-block text-xs text-secondary hover:underline">
            Full diagnostics →
          </Link>
        </Panel>

        <Panel title="Session">
          <div className="space-y-1 text-xs text-t-secondary">
            <div className="flex justify-between">
              <span className="flex items-center gap-1">
                <Timer className="h-3.5 w-3.5" aria-hidden /> Samples
              </span>
              <span className="font-mono text-t-primary">{conn.telemetryCount}</span>
            </div>
            <div className="flex justify-between">
              <span className="flex items-center gap-1">
                <Satellite className="h-3.5 w-3.5" aria-hidden /> Satellite sats
              </span>
              <span className="font-mono text-t-primary">{t?.satellites ?? "—"}</span>
            </div>
            <div className="flex justify-between">
              <span>CSV logging</span>
              <span className={csv.enabled ? "font-mono text-accent" : "font-mono text-t-primary"}>
                {csv.enabled ? `ON · ${csv.rowCount} rows` : "OFF"}
              </span>
            </div>
            <div className="flex justify-between">
              <span className="flex items-center gap-1">
                <Compass className="h-3.5 w-3.5" aria-hidden /> Source
              </span>
              <span className="font-mono text-t-primary">{conn.source}</span>
            </div>
          </div>
        </Panel>

        <Panel title="Waste separated" subtitle="Counts per waste type (this run)">
          <div className="space-y-2 text-xs">
            {[1, 2, 3].map((cid) => {
              const count =
                cid === 1
                  ? t?.bottle_count
                  : cid === 2
                    ? t?.bag_count
                    : t?.debris_count;
              const isCurrent = t?.detection && t.class_id === cid;
              return (
                <div
                  key={cid}
                  className="flex items-center justify-between panel-elevated px-2 py-1.5"
                >
                  <span className="flex items-center gap-2">
                    <Recycle
                      className={`h-3.5 w-3.5 ${isCurrent ? "text-accent" : "text-t-secondary"}`}
                      aria-hidden
                    />
                    {classLabel(cid)}
                  </span>
                  <span
                    className={`font-mono text-sm ${isCurrent ? "text-accent" : "text-t-primary"}`}
                  >
                    {t ? (count ?? 0) : "—"}
                  </span>
                </div>
              );
            })}
            <p className="pt-1 text-[11px] text-t-secondary">
              Counted by ESP32 main controller from accepted CAM detections.
            </p>
          </div>
        </Panel>
      </div>
    </div>
  );
}
