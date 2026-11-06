import { useTelemetry } from "./TelemetryContext";
import {
  decodeFaults,
  riskLevel,
  type RiskLevel,
} from "../lib/faults";
import type {
  ConnectionSnapshot,
  StoreSnapshot,
  Telemetry,
} from "../services/communication";

export interface BoatView {
  snapshot: StoreSnapshot;
  t: Telemetry | null;
  conn: ConnectionSnapshot;
  faults: ReturnType<typeof decodeFaults>;
  risk: RiskLevel;
  demo: boolean;
}

/** Single derivation used by TopBar, banner and pages. */
export function useBoat(): BoatView {
  const { snapshot } = useTelemetry();
  const t = snapshot.latest;
  const conn = snapshot.connection;
  const demo = conn.source === "demo";
  const faults = decodeFaults(t?.fault_flags ?? 0);
  const risk = riskLevel({
    state: t?.state ?? "UNKNOWN",
    faultFlags: t?.fault_flags ?? 0,
    estop: t?.estop ?? false,
    commsOk: t?.comms_ok ?? false,
    connectionStale: conn.status === "stale" || conn.status === "offline",
  });
  return { snapshot, t, conn, faults, risk, demo };
}
