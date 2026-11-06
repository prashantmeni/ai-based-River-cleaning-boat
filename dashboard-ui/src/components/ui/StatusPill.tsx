export type PillTone = "ok" | "warn" | "crit" | "info" | "idle" | "demo";

const TONES: Record<PillTone, string> = {
  ok: "bg-accent/15 text-accent border-accent/30",
  warn: "bg-warning/15 text-warning border-warning/30",
  crit: "bg-critical/15 text-critical border-critical/40",
  info: "bg-secondary/15 text-secondary border-secondary/30",
  idle: "bg-elevated text-t-secondary border-t-border",
  demo: "bg-purple-400/15 text-purple-300 border-purple-400/30",
};

const DOTS: Record<PillTone, string> = {
  ok: "bg-accent",
  warn: "bg-warning",
  crit: "bg-critical",
  info: "bg-secondary",
  idle: "bg-t-secondary",
  demo: "bg-purple-300",
};

interface StatusPillProps {
  tone: PillTone;
  label: string;
  title?: string;
  pulse?: boolean;
}

export function StatusPill({ tone, label, title, pulse }: StatusPillProps) {
  return (
    <span
      title={title}
      className={`pill border ${TONES[tone]} ${pulse ? "animate-pulse" : ""}`}
    >
      <span className={`h-1.5 w-1.5 rounded-full ${DOTS[tone]}`} />
      {label}
    </span>
  );
}
