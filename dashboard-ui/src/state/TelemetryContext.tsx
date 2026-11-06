import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useRef,
  useState,
  useSyncExternalStore,
  type ReactNode,
} from "react";
import {
  DIAG_CMD,
  sendCommand,
  store,
  type CommandRequest,
  type CommandResult,
  type StoreSnapshot,
} from "../services/communication";
import { downloadCsv, telemetryToCsvRow } from "../lib/csv";
import { saveCsvLog, loadCsvLog } from "../lib/settings";
import { useDemo } from "./DemoContext";
import { demoEngine } from "./demoEngine";

const CSV_CAP = 20_000;
const CSV_SAVE_MS = 5000;

export interface CsvApi {
  enabled: boolean;
  rowCount: number;
  setEnabled: (v: boolean) => void;
  clear: () => void;
  exportFile: () => void;
}

interface TelemetryApi {
  snapshot: StoreSnapshot;
  send: (req: CommandRequest) => Promise<CommandResult>;
  estopLatch: () => Promise<CommandResult>;
  estopClear: () => Promise<CommandResult>;
  csv: CsvApi;
}

const TelemetryContext = createContext<TelemetryApi | null>(null);

export function TelemetryProvider({ children }: { children: ReactNode }) {
  const snapshot = useSyncExternalStore(store.subscribe, store.getSnapshot);
  const { active: demoActive } = useDemo();

  const initialLog = useRef(loadCsvLog());
  const rowsRef = useRef<string[]>(initialLog.current.rows.slice(0, CSV_CAP));
  const startedAtRef = useRef<number>(initialLog.current.startedAt || Date.now());
  const saveTimerRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const [csvEnabled, setCsvEnabled] = useState(initialLog.current.enabled);
  const [rowCount, setRowCount] = useState(rowsRef.current.length);

  const persistCsv = useCallback(() => {
    saveCsvLog({
      enabled: csvEnabled,
      startedAt: startedAtRef.current,
      rows: rowsRef.current,
      source: demoActive ? "demo" : "live",
    });
  }, [csvEnabled, demoActive]);

  // Append each new sample while logging; persist buffer periodically.
  useEffect(() => {
    if (!csvEnabled) return;
    const { latest, connection } = snapshot;
    if (!latest) return;
    // version changes only on store mutations; latest is fresh enough here
    rowsRef.current.push(
      telemetryToCsvRow(latest, Date.now(), connection.source),
    );
    if (rowsRef.current.length > CSV_CAP) {
      rowsRef.current = rowsRef.current.slice(-CSV_CAP);
    }
    setRowCount(rowsRef.current.length);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [snapshot.version]);

  useEffect(() => {
    if (!csvEnabled) return;
    startedAtRef.current = startedAtRef.current || Date.now();
    saveTimerRef.current = setInterval(persistCsv, CSV_SAVE_MS);
    return () => {
      if (saveTimerRef.current) clearInterval(saveTimerRef.current);
      persistCsv();
    };
  }, [csvEnabled, persistCsv]);

  const setCsv = useCallback(
    (v: boolean) => {
      setCsvEnabled(v);
      if (v) {
        startedAtRef.current = Date.now();
        rowsRef.current = [];
        setRowCount(0);
      } else {
        persistCsv();
      }
    },
    [persistCsv],
  );

  const clearCsv = useCallback(() => {
    rowsRef.current = [];
    setRowCount(0);
    startedAtRef.current = Date.now();
    saveCsvLog({
      enabled: csvEnabled,
      startedAt: startedAtRef.current,
      rows: [],
      source: demoActive ? "demo" : "live",
    });
  }, [csvEnabled, demoActive]);

  const exportCsv = useCallback(() => {
    const stamp = new Date().toISOString().replace(/[:.]/g, "-");
    downloadCsv(
      rowsRef.current,
      `riverclean-telemetry-${demoActive ? "demo" : "live"}-${stamp}.csv`,
    );
  }, [demoActive]);

  const send = useCallback(
    async (req: CommandRequest): Promise<CommandResult> => {
      if (demoActive) {
        const r = demoEngine.handleCommand(
          req.cmd,
          req.value ?? 0,
          req.durationMs ?? 0,
        );
        if (r.ok) {
          return {
            ok: true,
            ack: { type: "ack", id: "demo", status: "ok", cmd: req.cmd },
          };
        }
        return { ok: false, reason: "rejected", detail: r.reason };
      }
      return sendCommand(req);
    },
    [demoActive],
  );

  const value = useMemo<TelemetryApi>(
    () => ({
      snapshot,
      send,
      estopLatch: () => send({ cmd: DIAG_CMD.STOP, value: 1 }),
      estopClear: () => send({ cmd: DIAG_CMD.STOP, value: 2 }),
      csv: {
        enabled: csvEnabled,
        rowCount,
        setEnabled: setCsv,
        clear: clearCsv,
        exportFile: exportCsv,
      },
    }),
    [snapshot, send, csvEnabled, rowCount, setCsv, clearCsv, exportCsv],
  );

  return (
    <TelemetryContext.Provider value={value}>
      {children}
    </TelemetryContext.Provider>
  );
}

export function useTelemetry(): TelemetryApi {
  const ctx = useContext(TelemetryContext);
  if (!ctx) throw new Error("useTelemetry must be used within TelemetryProvider");
  return ctx;
}
