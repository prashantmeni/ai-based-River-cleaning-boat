import { useState } from "react";
import { Outlet } from "react-router-dom";
import { AlertOctagon } from "lucide-react";
import { Sidebar } from "./Sidebar";
import { TopBar } from "./TopBar";
import { useBoat } from "../../state/useBoat";
import { useTelemetry } from "../../state/TelemetryContext";
import { ConfirmButton } from "../ui/Toast";

export function Layout() {
  const [menuOpen, setMenuOpen] = useState(false);
  const { t, conn, demo } = useBoat();
  const { estopClear } = useTelemetry();

  return (
    <div className="flex min-h-screen bg-bg">
      <Sidebar open={menuOpen} onClose={() => setMenuOpen(false)} />
      <div className="flex min-w-0 flex-1 flex-col">
        <TopBar onMenu={() => setMenuOpen(true)} />

        {t?.estop && (
          <div
            role="alert"
            className="flex flex-wrap items-center gap-3 border-b border-critical/40 bg-critical/15 px-4 py-2 text-sm text-critical"
          >
            <AlertOctagon className="h-4 w-4 shrink-0" />
            <span className="font-semibold">
              Emergency stop is LATCHED — motors and conveyor are disabled.
            </span>
            <ConfirmButton
              label="Clear E-STOP"
              confirmLabel="Confirm clear"
              onConfirm={() => void estopClear()}
              className="btn-danger text-xs"
            />
          </div>
        )}

        {demo && (
          <div className="border-b border-purple-400/30 bg-purple-400/10 px-4 py-1.5 text-xs font-medium text-purple-300">
            SIMULATION ACTIVE — synthetic telemetry only; no hardware is being
            controlled.
          </div>
        )}

        <main className="min-w-0 flex-1 p-4 lg:p-6">
          <Outlet />
        </main>

        <footer className="border-t border-t-border px-4 py-2 text-[10px] text-t-secondary">
          RiverClean AI · safety interlocks run on the ESP32, not in this
          browser · link {conn.status}
        </footer>
      </div>
    </div>
  );
}
