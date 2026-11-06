import { RiverMqttClient } from "./mqttClient";
import { WebSocketClient, type WsStatus } from "./websocketClient";
import { parseTelemetry } from "./telemetryParser";
import { store } from "./store";
import type { AckMessage, HelloMessage } from "./types";

export type AckListener = (ack: AckMessage) => void;
export type HelloListener = (hello: HelloMessage) => void;

const HEARTBEAT_MS = 1000;
const DEFAULT_MQTT_URL = "wss://broker.hivemq.com:8884/mqtt";

/** Both transports expose the same surface (status/url/send/connect). */
interface TransportClient {
  getStatus(): WsStatus;
  getUrl(): string;
  setUrl(url: string): void;
  connect(): void;
  disconnect(): void;
  send(text: string): boolean;
}

export type TransportKind = "mqtt" | "ws";

/**
 * Pick the transport for a connection URL:
 * - broker.hivemq.com or any URL containing "/mqtt" -> cloud MQTT over WSS
 * - any other ws:// or wss:// endpoint (e.g. ws://192.168.4.1:81) -> direct
 *   WebSocket to the ESP32's own gateway — no internet, laptop/phone joins
 *   the boat's Wi-Fi (or the same router) instead of a broker.
 * - anything else / empty -> default public broker.
 */
export function transportForUrl(url: string): TransportKind {
  const u = url.trim().toLowerCase();
  if (u.includes("broker.hivemq.com") || u.includes("/mqtt")) return "mqtt";
  if (u.startsWith("ws://") || u.startsWith("wss://")) return "ws";
  return "mqtt";
}

/**
 * Live-mode controller: owns the transport (cloud MQTT or direct WebSocket),
 * feeds the TelemetryStore, and routes ack/hello frames to listeners. One
 * instance per app. Both transports carry the identical JSON schema
 * (telemetry / hello / ack), so everything downstream is transport-agnostic.
 */
export class ConnectionManager {
  private client: TransportClient | null = null;
  private clientKind: TransportKind | null = null;
  private heartbeat: ReturnType<typeof setInterval> | null = null;
  private ackListeners = new Set<AckListener>();
  private helloListeners = new Set<HelloListener>();

  connect(url: string): void {
    const kind = transportForUrl(url);
    // Non-ws input (or empty) falls back to the public broker, as before.
    const endpoint = url.trim().toLowerCase().startsWith("ws") ? url : DEFAULT_MQTT_URL;
    if (this.client) {
      this.client.disconnect();
      // Same transport + same endpoint: keep it simple and recreate anyway —
      // connect() is only called on demand (mount / Reconnect / settings save).
    }
    const handlers = {
      onMessage: this.handleMessage,
      onStatusChange: this.handleStatus,
    };
    this.client =
      kind === "mqtt"
        ? new RiverMqttClient(endpoint, handlers)
        : new WebSocketClient(endpoint, handlers);
    this.clientKind = kind;
    this.client.connect();
    store.setWsStatus("connecting", endpoint, Date.now());
    if (!this.heartbeat) {
      this.heartbeat = setInterval(() => {
        store.tick(Date.now());
      }, HEARTBEAT_MS);
    }
  }

  disconnect(): void {
    this.client?.disconnect();
    this.client = null;
    this.clientKind = null;
    if (this.heartbeat) {
      clearInterval(this.heartbeat);
      this.heartbeat = null;
    }
    store.setWsStatus("idle", store.getSnapshot().connection.url, Date.now());
  }

  updateUrl(url: string): void {
    if (!this.client) return;
    if (transportForUrl(url) !== this.clientKind) {
      // Transport changed (e.g. cloud MQTT -> direct WiFi): recreate.
      this.connect(url);
      return;
    }
    this.client.setUrl(url);
  }

  /** Which transport the current endpoint uses ("mqtt" | "ws"), or null. */
  getTransport(): TransportKind | null {
    return this.clientKind;
  }

  isConnected(): boolean {
    return this.client?.getStatus() === "open";
  }

  send(text: string): boolean {
    return this.client ? this.client.send(text) : false;
  }

  onAck(fn: AckListener): () => void {
    this.ackListeners.add(fn);
    return () => this.ackListeners.delete(fn);
  }

  onHello(fn: HelloListener): () => void {
    this.helloListeners.add(fn);
    return () => this.helloListeners.delete(fn);
  }

  private handleStatus = (status: WsStatus): void => {
    const url = this.client?.getUrl() ?? "";
    store.setWsStatus(status, url, Date.now());
  };

  private handleMessage = (raw: string): void => {
    const now = Date.now();
    store.noteMessage(now);
    const result = parseTelemetry(raw);
    if (!result.ok) {
      store.noteParseError(result.error);
      return;
    }
    const msg = result.message;
    if (msg.type === "telemetry") {
      store.pushTelemetry(msg, "live", now);
    } else if (msg.type === "hello") {
      store.setHello(msg);
      this.helloListeners.forEach((fn) => fn(msg));
    } else {
      this.ackListeners.forEach((fn) => fn(msg));
    }
  };
}

export const connectionManager = new ConnectionManager();
