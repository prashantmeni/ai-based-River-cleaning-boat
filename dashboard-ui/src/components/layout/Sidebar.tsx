import { NavLink } from "react-router-dom";
import {
  LayoutDashboard,
  Camera,
  Compass,
  Activity,
  Settings,
  Waves,
  X,
} from "lucide-react";

const ITEMS = [
  { to: "/", label: "Overview", icon: LayoutDashboard, end: true },
  { to: "/camera", label: "AI Camera", icon: Camera },
  { to: "/drive", label: "Drive", icon: Compass },
  { to: "/diagnostics", label: "Diagnostics", icon: Activity },
  { to: "/settings", label: "Settings", icon: Settings },
];

interface SidebarProps {
  open: boolean;
  onClose: () => void;
}

export function Sidebar({ open, onClose }: SidebarProps) {
  return (
    <>
      {open && (
        <div
          className="fixed inset-0 z-30 bg-black/50 lg:hidden"
          onClick={onClose}
          aria-hidden
        />
      )}
      <aside
        className={`fixed inset-y-0 left-0 z-40 flex w-60 flex-col border-r border-t-border bg-sidebar transition-transform lg:static lg:translate-x-0 ${
          open ? "translate-x-0" : "-translate-x-full"
        }`}
        aria-label="Main navigation"
      >
        <div className="flex h-14 items-center justify-between border-b border-t-border px-4">
          <div className="flex items-center gap-2">
            <span className="flex h-8 w-8 items-center justify-center rounded-lg bg-accent/15">
              <Waves className="h-5 w-5 text-accent" aria-hidden />
            </span>
            <div className="leading-tight">
              <div className="text-sm font-bold tracking-wide">RiverClean AI</div>
              <div className="text-[10px] uppercase tracking-widest text-t-secondary">
                Marine Control
              </div>
            </div>
          </div>
          <button
            type="button"
            className="rounded p-1 text-t-secondary hover:text-t-primary lg:hidden"
            onClick={onClose}
            aria-label="Close menu"
          >
            <X className="h-5 w-5" />
          </button>
        </div>
        <nav className="flex-1 space-y-1 overflow-y-auto p-3">
          {ITEMS.map(({ to, label, icon: Icon, end }) => (
            <NavLink
              key={to}
              to={to}
              end={end}
              onClick={onClose}
              className={({ isActive }) =>
                `flex items-center gap-3 rounded-lg px-3 py-2 text-sm font-medium transition-colors ${
                  isActive
                    ? "bg-accent/15 text-accent"
                    : "text-t-secondary hover:bg-elevated hover:text-t-primary"
                }`
              }
            >
              <Icon className="h-4 w-4 shrink-0" aria-hidden />
              {label}
            </NavLink>
          ))}
        </nav>
        <div className="border-t border-t-border px-4 py-3 text-[10px] text-t-secondary">
          ESP32-CAM + ESP32 · ESP-NOW ch1
          <br />
          WebSocket gateway :81
        </div>
      </aside>
    </>
  );
}
