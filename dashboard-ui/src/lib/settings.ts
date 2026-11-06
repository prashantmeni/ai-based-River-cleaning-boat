import { loadJson, saveJson, removeKey } from "./storage";

export const STORAGE_KEYS = {
  settings: "riverclean.settings.v1",
  csvLog: "riverclean.csvlog.v1",
} as const;

export interface AppSettings {
  /**
   * Connection endpoint: cloud MQTT broker (…/mqtt) or direct WebSocket to
   * the boat's own gateway (ws://192.168.4.1:81 — no internet required).
   */
  wsUrl: string;
  /** Manual-control throttle used by hold-to-run buttons (0..255). */
  manualPwm: number;
  /** Command default duration, ms (0 = latched until stop). */
  commandDurationMs: number;
  /** CSV telemetry logging while connected. */
  csvLogEnabled: boolean;
  /** Show the satellite map on Overview / Navigation. */
  mapEnabled: boolean;
}

export const DEFAULT_SETTINGS: AppSettings = {
  wsUrl: "wss://broker.hivemq.com:8884/mqtt",
  manualPwm: 150,
  commandDurationMs: 600,
  csvLogEnabled: false,
  mapEnabled: true,
};

export function loadSettings(): AppSettings {
  const stored = loadJson<Partial<AppSettings>>(STORAGE_KEYS.settings, {});
  return { ...DEFAULT_SETTINGS, ...stored };
}

export function saveSettings(s: AppSettings): void {
  saveJson(STORAGE_KEYS.settings, s);
}

export function resetSettings(): void {
  removeKey(STORAGE_KEYS.settings);
}

export interface CsvLogState {
  enabled: boolean;
  startedAt: number;
  rows: string[];
  source: "live" | "demo";
}

export function loadCsvLog(): CsvLogState {
  return loadJson<CsvLogState>(STORAGE_KEYS.csvLog, {
    enabled: false,
    startedAt: 0,
    rows: [],
    source: "live",
  });
}

export function saveCsvLog(state: CsvLogState): void {
  saveJson(STORAGE_KEYS.csvLog, state);
}

export function clearCsvLog(): void {
  removeKey(STORAGE_KEYS.csvLog);
}
