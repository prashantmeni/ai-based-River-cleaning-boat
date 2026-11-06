import { beforeEach, describe, expect, it, vi } from "vitest";
import { render, screen } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import { SettingsProvider } from "../state/SettingsContext";
import { DemoProvider } from "../state/DemoContext";
import { TelemetryProvider } from "../state/TelemetryContext";
import { ToastProvider } from "../components/ui/Toast";
import { OverviewPage } from "./OverviewPage";
import {
  connectionManager,
  store,
  type Telemetry,
} from "../services/communication";

function fake(over: Partial<Telemetry> = {}): Telemetry {
  return {
    type: "telemetry",
    protocol_version: 1,
    device_id: "riverclean_main",
    state: "IDLE",
    comms_ok: true,
    estop: false,
    autonomous: false,
    detection: false,
    confidence: 0,
    class_id: 0,
    center_x: 48,
    left_cm: 140,
    right_cm: 138,
    gps_fix: false,
    latitude: 0,
    longitude: 0,
    satellites: 0,
    gps_age_ms: 0,
    collection_count: 0,
    conveyor: "OFF",
    fault_flags: 0,
    uptime_ms: 10_000,
    left_pwm: 0,
    right_pwm: 0,
    left_dir: 0,
    right_dir: 0,
    rx_bytes: 1000,
    rx_errors: 0,
    perr: 0,
    drops: 0,
    dbg: "",
    ...over,
  };
}

function renderPage() {
  return render(
    <SettingsProvider>
      <DemoProvider>
        <TelemetryProvider>
          <ToastProvider>
            <MemoryRouter>
              <OverviewPage />
            </MemoryRouter>
          </ToastProvider>
        </TelemetryProvider>
      </DemoProvider>
    </SettingsProvider>,
  );
}

describe("OverviewPage", () => {
  beforeEach(() => {
    vi.restoreAllMocks();
    vi.spyOn(connectionManager, "connect").mockImplementation(() => {});
    vi.spyOn(connectionManager, "disconnect").mockImplementation(() => {});
    vi.spyOn(connectionManager, "updateUrl").mockImplementation(() => {});
    // Simulate an open socket: without it everything reads as offline.
    store.setWsStatus("open", "ws://test:81", Date.now());
  });

  it("shows NO DATA before any telemetry arrives", () => {
    renderPage();
    expect(screen.getByText("NO DATA")).toBeInTheDocument();
    // socket open but no frames yet → stale → critical is honest
    expect(screen.getByText("CRITICAL")).toBeInTheDocument();
  });

  it("renders live state, link and mission data", () => {
    store.pushTelemetry(
      fake({ state: "SEARCH", autonomous: true, collection_count: 4, comms_ok: true }),
      "live",
      Date.now(),
    );
    renderPage();
    expect(screen.getAllByText("SEARCH").length).toBeGreaterThan(0);
    expect(screen.getByText("AUTONOMOUS")).toBeInTheDocument();
    expect(screen.getByText("CAM link")).toBeInTheDocument();
    expect(screen.getByText("ALL SYSTEMS OK")).toBeInTheDocument();
  });

  it("surfaces active faults prominently", () => {
    store.pushTelemetry(fake({ fault_flags: 1, comms_ok: false }), "live", Date.now());
    renderPage();
    expect(screen.getByText("CAM link lost")).toBeInTheDocument();
    expect(screen.getByText("CRITICAL")).toBeInTheDocument();
  });

  it("marks GPS as unavailable honestly without a fix", () => {
    store.pushTelemetry(fake({ gps_fix: false, satellites: 0 }), "live", Date.now());
    renderPage();
    expect(screen.getAllByText("NO FIX").length).toBeGreaterThan(0);
    expect(screen.queryByText("0.000000")).not.toBeInTheDocument();
  });
});
