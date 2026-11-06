import { beforeEach, describe, expect, it, vi } from "vitest";
import { render, screen, fireEvent } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import { SettingsProvider } from "../state/SettingsContext";
import { DemoProvider } from "../state/DemoContext";
import { TelemetryProvider } from "../state/TelemetryContext";
import { ToastProvider } from "../components/ui/Toast";
import { DrivePage } from "./DrivePage";
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
              <DrivePage />
            </MemoryRouter>
          </ToastProvider>
        </TelemetryProvider>
      </DemoProvider>
    </SettingsProvider>,
  );
}

describe("DrivePage", () => {
  beforeEach(() => {
    vi.restoreAllMocks();
    vi.spyOn(connectionManager, "connect").mockImplementation(() => {});
    vi.spyOn(connectionManager, "disconnect").mockImplementation(() => {});
    vi.spyOn(connectionManager, "updateUrl").mockImplementation(() => {});
    store.setWsStatus("open", "ws://test:81", Date.now());
  });

  it("shows live state and allows manual drive when safe", () => {
    store.pushTelemetry(fake({ state: "IDLE" }), "live", Date.now());
    renderPage();
    expect(screen.getAllByText("IDLE").length).toBeGreaterThan(0);
    const forward = screen.getByRole("button", { name: /Forward/ });
    expect(forward).toBeEnabled();
    expect(screen.getByRole("button", { name: /Stop motors/ })).toBeEnabled();
  });

  it("locks manual drive while autonomy is engaged", () => {
    store.pushTelemetry(fake({ state: "SEARCH", autonomous: true }), "live", Date.now());
    renderPage();
    expect(screen.getByRole("button", { name: /Disengage autonomy/ })).toBeInTheDocument();
    expect(screen.getByRole("button", { name: /Forward/ })).toBeDisabled();
    expect(screen.getByRole("button", { name: /Turn left/ })).toBeDisabled();
    expect(screen.getByRole("button", { name: /Stop motors/ })).toBeEnabled();
  });

  it("renders the clear action when E-STOP is latched", () => {
    store.pushTelemetry(
      fake({ state: "EMERGENCY_STOP", estop: true }),
      "live",
      Date.now(),
    );
    renderPage();
    expect(screen.getAllByText(/E-STOP LATCHED/).length).toBeGreaterThan(0);
    expect(screen.getByRole("button", { name: /Clear E-STOP/ })).toBeEnabled();
    expect(screen.queryByRole("button", { name: /ENGAGE E-STOP/ })).toBeNull();
  });

  it("reports honest failure when commands cannot be acknowledged", async () => {
    store.pushTelemetry(fake({ state: "IDLE" }), "live", Date.now());
    renderPage();
    const estop = screen.getByRole("button", { name: /ENGAGE E-STOP/ });
    fireEvent.click(estop);
    fireEvent.click(estop); // second click confirms
    expect(
      await screen.findByText(/E-STOP not acknowledged/),
    ).toBeInTheDocument();
  });

  it("shows sensor invalid state as a warning, never as distance", () => {
    store.pushTelemetry(fake({ left_cm: -1, right_cm: 90 }), "live", Date.now());
    renderPage();
    expect(screen.getAllByText("Invalid").length).toBeGreaterThan(0);
    expect(screen.getByText("SENSOR INVALID")).toBeInTheDocument();
  });
});
