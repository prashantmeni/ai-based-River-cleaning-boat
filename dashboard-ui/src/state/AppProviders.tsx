import type { ReactNode } from "react";
import { SettingsProvider } from "./SettingsContext";
import { DemoProvider } from "./DemoContext";
import { TelemetryProvider } from "./TelemetryContext";

/** Provider order matters: Demo needs Settings; Telemetry needs Demo. */
export function AppProviders({ children }: { children: ReactNode }) {
  return (
    <SettingsProvider>
      <DemoProvider>
        <TelemetryProvider>{children}</TelemetryProvider>
      </DemoProvider>
    </SettingsProvider>
  );
}
