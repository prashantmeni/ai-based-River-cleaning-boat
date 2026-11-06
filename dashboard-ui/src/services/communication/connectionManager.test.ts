import { describe, expect, it } from "vitest";
import { transportForUrl } from "./connectionManager";

describe("transportForUrl", () => {
  it("routes the default public broker to MQTT", () => {
    expect(transportForUrl("wss://broker.hivemq.com:8884/mqtt")).toBe("mqtt");
  });

  it("routes any URL containing /mqtt to MQTT", () => {
    expect(transportForUrl("wss://mqtt.example.com:8884/mqtt")).toBe("mqtt");
    expect(transportForUrl("WSS://BROKER.EXAMPLE.COM/MQTT")).toBe("mqtt");
  });

  it("routes the boat SoftAP gateway to direct WebSocket", () => {
    expect(transportForUrl("ws://192.168.4.1:81")).toBe("ws");
    expect(transportForUrl("ws://192.168.31.150:81")).toBe("ws");
  });

  it("routes a LAN WebSocket server (no /mqtt path) to WebSocket", () => {
    expect(transportForUrl("wss://boat.local:81")).toBe("ws");
  });

  it("trims and lowercases before deciding", () => {
    expect(transportForUrl("  WS://192.168.4.1:81  ")).toBe("ws");
  });

  it("falls back to MQTT for empty or non-ws input", () => {
    expect(transportForUrl("")).toBe("mqtt");
    expect(transportForUrl("tcp://broker:1883")).toBe("mqtt");
    expect(transportForUrl("nonsense")).toBe("mqtt");
  });
});
