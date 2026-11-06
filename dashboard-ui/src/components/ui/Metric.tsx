import type { ReactNode } from "react";

export type MetricQuality = "live" | "stale" | "unavailable" | "demo";

interface MetricProps {
  label: string;
  value: ReactNode;
  unit?: string;
  quality?: MetricQuality;
  sub?: ReactNode;
  tone?: "default" | "ok" | "warn" | "crit";
}

const TONE_CLS: Record<NonNullable<MetricProps["tone"]>, string> = {
  default: "text-t-primary",
  ok: "text-accent",
  warn: "text-warning",
  crit: "text-critical",
};

const QUALITY_HINT: Record<MetricQuality, string> = {
  live: "Live reading",
  stale: "Stale — no recent data",
  unavailable: "Unavailable on this hardware",
  demo: "Simulated value",
};

export function Metric({
  label,
  value,
  unit,
  quality = "live",
  sub,
  tone = "default",
}: MetricProps) {
  const dim = quality === "unavailable" || quality === "stale";
  return (
    <div className="panel-elevated px-3 py-2.5" title={QUALITY_HINT[quality]}>
      <div className="flex items-center justify-between">
        <span className="text-[11px] font-medium uppercase tracking-wider text-t-secondary">
          {label}
        </span>
        <span
          className={`h-1.5 w-1.5 rounded-full ${
            quality === "live"
              ? "bg-accent"
              : quality === "demo"
                ? "bg-purple-300"
                : quality === "stale"
                  ? "bg-warning"
                  : "bg-t-secondary/50"
          }`}
        />
      </div>
      <div
        className={`mt-1 font-mono text-xl font-semibold tabular-nums ${dim ? "text-t-secondary" : TONE_CLS[tone]}`}
      >
        {value}
        {unit && <span className="ml-1 text-xs font-normal text-t-secondary">{unit}</span>}
      </div>
      {sub && <div className="mt-0.5 text-[11px] text-t-secondary">{sub}</div>}
    </div>
  );
}
