/** Shared formatting helpers — unit-safe, no fabricated precision. */

export function clamp(v: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, v));
}

/** "1h 02m 03s" — firmware uptime_ms / wall durations. */
export function formatDuration(ms: number): string {
  if (!Number.isFinite(ms) || ms < 0) return "—";
  const totalSec = Math.floor(ms / 1000);
  const h = Math.floor(totalSec / 3600);
  const m = Math.floor((totalSec % 3600) / 60);
  const s = totalSec % 60;
  if (h > 0) return `${h}h ${String(m).padStart(2, "0")}m ${String(s).padStart(2, "0")}s`;
  if (m > 0) return `${m}m ${String(s).padStart(2, "0")}s`;
  return `${s}s`;
}

/** ms until a timestamp, updating label: "3s ago". */
export function formatAge(ts: number | null, now: number): string {
  if (ts === null) return "never";
  const d = Math.max(0, now - ts);
  if (d < 1000) return "just now";
  if (d < 60_000) return `${Math.floor(d / 1000)}s ago`;
  return `${Math.floor(d / 60_000)}m ago`;
}

export function formatPercent(v: number, digits = 0): string {
  if (!Number.isFinite(v)) return "—";
  return `${v.toFixed(digits)}%`;
}

/** -1 = invalid ultrasonic reading (firmware), never render as distance. */
export function formatDistanceCm(cm: number): string {
  if (!Number.isFinite(cm) || cm < 0) return "Invalid";
  return `${Math.round(cm)} cm`;
}

export function formatLatLon(lat: number, lon: number, fix: boolean): string {
  if (!fix || (lat === 0 && lon === 0)) return "No fix";
  return `${lat.toFixed(6)}, ${lon.toFixed(6)}`;
}

/** Motor PWM 0..255 → 0..100 % for gauges. */
export function pwmPercent(pwm: number): number {
  return clamp((pwm / 255) * 100, 0, 100);
}

export function formatBytes(n: number): string {
  if (!Number.isFinite(n) || n < 0) return "—";
  if (n < 1024) return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / (1024 * 1024)).toFixed(2)} MB`;
}

export function formatClockTime(ts: number): string {
  const d = new Date(ts);
  const p = (x: number) => String(x).padStart(2, "0");
  return `${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`;
}
