interface GaugeProps {
  value: number;
  label: string;
  color?: string;
  size?: number;
  unit?: string;
  sub?: string;
}

const TAU = Math.PI * 2;

/** Radial gauge, 270° sweep, SVG only (no canvas). value: 0..100. */
export function Gauge({
  value,
  label,
  color = "#19C7A1",
  size = 120,
  unit = "%",
  sub,
}: GaugeProps) {
  const clamped = Math.max(0, Math.min(100, Number.isFinite(value) ? value : 0));
  const stroke = 9;
  const r = size / 2 - stroke - 2;
  const cx = size / 2;
  const cy = size / 2;
  const start = -0.75 * TAU; // top-left of 270° arc
  const sweep = 0.75 * TAU;

  const arc = (from: number, to: number): string => {
    const x1 = cx + r * Math.cos(from);
    const y1 = cy + r * Math.sin(from);
    const x2 = cx + r * Math.cos(to);
    const y2 = cy + r * Math.sin(to);
    const large = to - from > Math.PI ? 1 : 0;
    return `M ${x1.toFixed(2)} ${y1.toFixed(2)} A ${r} ${r} 0 ${large} 1 ${x2.toFixed(2)} ${y2.toFixed(2)}`;
  };

  const end = start + sweep * (clamped / 100);

  return (
    <div
      className="flex flex-col items-center"
      role="img"
      aria-label={`${label}: ${clamped.toFixed(0)}${unit}`}
    >
      <svg width={size} height={size} viewBox={`0 0 ${size} ${size}`}>
        <path
          d={arc(start, start + sweep)}
          fill="none"
          stroke="#293548"
          strokeWidth={stroke}
          strokeLinecap="round"
        />
        {clamped > 0 && (
          <path
            d={arc(start, Math.max(end, start + 0.001))}
            fill="none"
            stroke={color}
            strokeWidth={stroke}
            strokeLinecap="round"
          />
        )}
        <text
          x={cx}
          y={cy - 2}
          textAnchor="middle"
          className="fill-t-primary font-mono text-xl font-semibold"
        >
          {clamped.toFixed(0)}
          <tspan className="fill-t-secondary text-xs">{unit}</tspan>
        </text>
        <text
          x={cx}
          y={cy + 16}
          textAnchor="middle"
          className="fill-t-secondary text-[9px] uppercase tracking-wider"
        >
          {label}
        </text>
      </svg>
      {sub && <span className="text-[11px] text-t-secondary">{sub}</span>}
    </div>
  );
}
