import type { Telemetry } from "../services/communication";
import { formatClockTime } from "./format";

const CSV_HEADER = [
  "timestamp_iso",
  "wall_time",
  "state",
  "comms_ok",
  "estop",
  "autonomous",
  "detection",
  "confidence",
  "class_id",
  "center_x",
  "left_cm",
  "right_cm",
  "gps_fix",
  "latitude",
  "longitude",
  "satellites",
  "gps_age_ms",
  "collection_count",
  "conveyor",
  "fault_flags",
  "uptime_ms",
  "left_pwm",
  "right_pwm",
  "left_dir",
  "right_dir",
  "rx_bytes",
  "rx_errors",
  "perr",
  "drops",
  "dbg",
  "bottle_count",
  "bag_count",
  "debris_count",
  "tds_ppm",
  "tds_mv",
  "source",
].join(",");

function escapeCell(v: string | number | boolean): string {
  const s = String(v);
  return /[",\n]/.test(s) ? `"${s.replace(/"/g, '""')}"` : s;
}

export function telemetryToCsvRow(t: Telemetry, at: number, source: string): string {
  const d = new Date(at);
  const cells: (string | number | boolean)[] = [
    d.toISOString(),
    formatClockTime(at),
    t.state,
    t.comms_ok,
    t.estop,
    t.autonomous,
    t.detection,
    t.confidence,
    t.class_id,
    t.center_x,
    t.left_cm,
    t.right_cm,
    t.gps_fix,
    t.latitude,
    t.longitude,
    t.satellites,
    t.gps_age_ms,
    t.collection_count,
    t.conveyor,
    t.fault_flags,
    t.uptime_ms,
    t.left_pwm,
    t.right_pwm,
    t.left_dir,
    t.right_dir,
    t.rx_bytes,
    t.rx_errors,
    t.perr,
    t.drops,
    t.dbg,
    t.bottle_count ?? 0,
    t.bag_count ?? 0,
    t.debris_count ?? 0,
    t.tds_ppm ?? -1,
    t.tds_mv ?? 0,
    source,
  ];
  return cells.map(escapeCell).join(",");
}

export function buildCsv(rows: string[]): string {
  return [CSV_HEADER, ...rows].join("\n") + "\n";
}

export function downloadCsv(rows: string[], filename: string): void {
  const blob = new Blob([buildCsv(rows)], { type: "text/csv;charset=utf-8" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}
