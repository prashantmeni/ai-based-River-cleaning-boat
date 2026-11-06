import type { WsStatus } from "./websocketClient";
import type {
  DetectionEvent,
  HelloMessage,
  Telemetry,
} from "./types";
import { classLabel } from "./types";

export type SourceMode = "live" | "demo";

export type ConnStatus = "connecting" | "online" | "stale" | "offline" | "demo";

export interface ConnectionSnapshot {
  status: ConnStatus;
  wsStatus: WsStatus;
  url: string;
  lastTelemetryAt: number | null;
  lastMessageAt: number | null;
  hello: HelloMessage | null;
  parseErrors: number;
  lastParseError: string | null;
  telemetryCount: number;
  source: SourceMode;
}

export interface StoreSnapshot {
  version: number;
  connection: ConnectionSnapshot;
  latest: Telemetry | null;
  /** Ring buffer of recent samples for charts. */
  series: { at: number; t: Telemetry }[];
  detections: DetectionEvent[];
}

const STALE_AFTER_MS = 3000;
const MAX_SERIES = 300;
const MAX_DETECTIONS = 50;

export type StoreListener = () => void;

/**
 * Framework-agnostic application store. The live WebSocket pipeline and the
 * demo generator both push through here; React reads immutable snapshots via
 * useSyncExternalStore.
 */
export class TelemetryStore {
  private snapshot: StoreSnapshot;
  private listeners = new Set<StoreListener>();

  constructor() {
    this.snapshot = {
      version: 0,
      connection: {
        status: "offline",
        wsStatus: "idle",
        url: "",
        lastTelemetryAt: null,
        lastMessageAt: null,
        hello: null,
        parseErrors: 0,
        lastParseError: null,
        telemetryCount: 0,
        source: "live",
      },
      latest: null,
      series: [],
      detections: [],
    };
  }

  subscribe = (fn: StoreListener): (() => void) => {
    this.listeners.add(fn);
    return () => {
      this.listeners.delete(fn);
    };
  };

  getSnapshot = (): StoreSnapshot => this.snapshot;

  private commit(mutate: (s: StoreSnapshot) => void): void {
    const next: StoreSnapshot = {
      ...this.snapshot,
      connection: { ...this.snapshot.connection },
      series: this.snapshot.series,
      detections: this.snapshot.detections,
    };
    mutate(next);
    next.version = this.snapshot.version + 1;
    this.snapshot = next;
    this.listeners.forEach((fn) => fn());
  }

  pushTelemetry(t: Telemetry, source: SourceMode, now: number): void {
    const prevDetections = this.snapshot.detections;
    let detections = prevDetections;
    if (t.detection) {
      const last = prevDetections[0];
      const dup =
        last &&
        last.classId === t.class_id &&
        Math.abs(last.confidence - t.confidence) < 6 &&
        now - last.at < 800;
      if (!dup) {
        const ev: DetectionEvent = {
          at: now,
          classId: t.class_id,
          label: classLabel(t.class_id),
          confidence: t.confidence,
          centerX: t.center_x,
          source,
        };
        detections = [ev, ...prevDetections].slice(0, MAX_DETECTIONS);
      }
    }
    const series = [...this.snapshot.series, { at: now, t }].slice(
      -MAX_SERIES,
    );
    this.commit((s) => {
      s.latest = t;
      s.series = series;
      s.detections = detections;
      s.connection.lastTelemetryAt = now;
      s.connection.telemetryCount += 1;
      s.connection.source = source;
      if (source === "demo") s.connection.status = "demo";
      else this.refreshStatus(s.connection, now);
    });
  }

  setWsStatus(status: WsStatus, url: string, now: number): void {
    this.commit((s) => {
      s.connection.wsStatus = status;
      s.connection.url = url;
      if (s.connection.source === "live") this.refreshStatus(s.connection, now);
    });
  }

  setHello(hello: HelloMessage): void {
    this.commit((s) => {
      s.connection.hello = hello;
    });
  }

  noteParseError(error: string): void {
    this.commit((s) => {
      s.connection.parseErrors += 1;
      s.connection.lastParseError = error;
    });
  }

  noteMessage(now: number): void {
    this.commit((s) => {
      s.connection.lastMessageAt = now;
      if (s.connection.source === "live") this.refreshStatus(s.connection, now);
    });
  }

  setSource(source: SourceMode, now: number): void {
    this.commit((s) => {
      s.connection.source = source;
      if (source === "demo") s.connection.status = "demo";
      else this.refreshStatus(s.connection, now);
    });
  }

  /** Re-evaluate stale/online (called by a 1 s heartbeat). */
  tick(now: number): void {
    if (this.snapshot.connection.source === "live") {
      this.commit((s) => this.refreshStatus(s.connection, now));
    }
  }

  clearSeries(): void {
    this.commit((s) => {
      s.series = [];
      s.detections = [];
    });
  }

  private refreshStatus(c: ConnectionSnapshot, now: number): void {
    if (c.wsStatus === "connecting") c.status = "connecting";
    else if (c.wsStatus !== "open") c.status = "offline";
    else if (c.lastTelemetryAt === null) c.status = "stale";
    else if (now - c.lastTelemetryAt > STALE_AFTER_MS) c.status = "stale";
    else c.status = "online";
  }
}

export const store = new TelemetryStore();
