import {
  LineChart,
  Line,
  ResponsiveContainer,
  YAxis,
  Tooltip,
  CartesianGrid,
} from "recharts";

export interface SparkPoint {
  v: number;
}

interface SparklineProps {
  data: SparkPoint[];
  color?: string;
  height?: number;
  min?: number;
  max?: number;
  unit?: string;
  label?: string;
}

export function Sparkline({
  data,
  color = "#38BDF8",
  height = 120,
  min,
  max,
  unit = "",
  label,
}: SparklineProps) {
  if (data.length === 0) {
    return (
      <div
        className="flex items-center justify-center rounded-lg border border-dashed border-t-border text-xs text-t-secondary"
        style={{ height }}
      >
        No samples yet — waiting for telemetry
      </div>
    );
  }
  return (
    <div style={{ height }}>
      <ResponsiveContainer width="100%" height="100%">
        <LineChart data={data} margin={{ top: 6, right: 6, bottom: 2, left: 6 }}>
          <CartesianGrid stroke="#293548" strokeDasharray="3 3" vertical={false} />
          <YAxis
            domain={[min ?? "auto", max ?? "auto"]}
            width={34}
            stroke="#94A3B8"
            fontSize={10}
            tickLine={false}
            axisLine={false}
          />
          <Tooltip
            contentStyle={{
              background: "#1B283B",
              border: "1px solid #293548",
              borderRadius: 8,
              fontSize: 12,
              color: "#F1F5F9",
            }}
            labelFormatter={() => label ?? ""}
            formatter={(v: number | string) => [`${v}${unit}`, label ?? "value"]}
          />
          <Line
            type="monotone"
            dataKey="v"
            stroke={color}
            strokeWidth={2}
            dot={false}
            isAnimationActive={false}
          />
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
}
