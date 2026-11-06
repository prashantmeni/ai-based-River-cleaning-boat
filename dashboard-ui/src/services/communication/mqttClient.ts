import mqtt, { MqttClient } from "mqtt";
import { WsStatus } from "./websocketClient";

export interface MqttClientHandlers {
  onMessage: (data: string) => void;
  onStatusChange: (status: WsStatus) => void;
}

/**
  * MQTT WebSocket client wrapper connecting to broker.hivemq.com:8884/mqtt
  * Subscribes to riverclean/boat/telemetry + riverclean/boat/ack and
  * publishes to riverclean/boat/command.
  */
export class RiverMqttClient {
  private client: MqttClient | null = null;
  private url: string;
  private status: WsStatus = "idle";
  private handlers: MqttClientHandlers;

  constructor(url: string = "wss://broker.hivemq.com:8884/mqtt", handlers: MqttClientHandlers) {
    this.url = url;
    this.handlers = handlers;
  }

  getStatus(): WsStatus {
    return this.status;
  }

  getUrl(): string {
    return this.url;
  }

  setUrl(url: string): void {
    if (url === this.url) return;
    this.url = url;
    if (this.client) {
      this.disconnect();
      this.connect();
    }
  }

  connect(): void {
    if (this.client && (this.status === "open" || this.status === "connecting")) {
      return;
    }
    this.setStatus("connecting");
    try {
      this.client = mqtt.connect(this.url, {
        clientId: "RiverCleanDashboard-" + Math.random().toString(16).substring(2, 8),
        clean: true,
        reconnectPeriod: 2000,
      });

      this.client.on("connect", () => {
        this.setStatus("open");
        this.client?.subscribe(
          ["riverclean/boat/telemetry", "riverclean/boat/ack"],
          (err) => {
            if (err) {
              console.error("[MQTT] Subscription error:", err);
            }
          },
        );
      });

      this.client.on("message", (_topic, payload) => {
        const text = payload.toString();
        this.handlers.onMessage(text);
      });

      this.client.on("error", (err) => {
        console.error("[MQTT] Error:", err);
        this.setStatus("error");
      });

      this.client.on("offline", () => {
        this.setStatus("closed");
      });

      this.client.on("close", () => {
        this.setStatus("closed");
      });
    } catch (e) {
      console.error("[MQTT] Connection exception:", e);
      this.setStatus("error");
    }
  }

  disconnect(): void {
    if (this.client) {
      try {
        this.client.end(true);
      } catch {
        // ignore
      }
      this.client = null;
    }
    this.setStatus("idle");
  }

  send(text: string): boolean {
    if (this.client && this.status === "open") {
      this.client.publish("riverclean/boat/command", text);
      return true;
    }
    return false;
  }

  private setStatus(status: WsStatus): void {
    if (this.status === status) return;
    this.status = status;
    this.handlers.onStatusChange(status);
  }
}
