import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useState,
  type ReactNode,
} from "react";
import { connectionManager } from "../services/communication";
import { demoEngine, type DemoScenario } from "./demoEngine";
import { useSettings } from "./SettingsContext";

interface DemoApi {
  active: boolean;
  scenario: DemoScenario;
  startDemo: (scenario?: DemoScenario) => void;
  stopDemo: () => void;
  setScenario: (s: DemoScenario) => void;
}

const DemoContext = createContext<DemoApi | null>(null);

/**
 * Demo mode EXCLUDES live data: enabling it disconnects the WebSocket and
 * runs the synthetic engine; disabling reconnects to the configured endpoint.
 */
export function DemoProvider({ children }: { children: ReactNode }) {
  const { settings } = useSettings();
  const [active, setActive] = useState(false);
  const [scenario, setScenarioState] = useState<DemoScenario>("mission");

  // Initial live connection (ConnectionManager.connect() replaces any prior
  // client, so StrictMode's double-invocation stays safe).
  useEffect(() => {
    connectionManager.connect(settings.wsUrl);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Follow URL edits while live.
  useEffect(() => {
    if (!active) connectionManager.updateUrl(settings.wsUrl);
  }, [settings.wsUrl, active]);

  useEffect(() => {
    return () => {
      connectionManager.disconnect();
      demoEngine.stop();
    };
  }, []);

  const startDemo = useCallback((s?: DemoScenario) => {
    const chosen = s ?? scenario;
    setScenarioState(chosen);
    connectionManager.disconnect();
    demoEngine.start(chosen);
    setActive(true);
  }, [scenario]);

  const stopDemo = useCallback(() => {
    demoEngine.stop();
    setActive(false);
    connectionManager.connect(settings.wsUrl);
  }, [settings.wsUrl]);

  const value = useMemo(
    () => ({ active, scenario, startDemo, stopDemo, setScenario: setScenarioState }),
    [active, scenario, startDemo, stopDemo],
  );

  return <DemoContext.Provider value={value}>{children}</DemoContext.Provider>;
}

export function useDemo(): DemoApi {
  const ctx = useContext(DemoContext);
  if (!ctx) throw new Error("useDemo must be used within DemoProvider");
  return ctx;
}
