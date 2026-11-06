import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useRef,
  useState,
  type ReactNode,
} from "react";

export type ToastTone = "ok" | "warn" | "crit" | "info";

interface ToastItem {
  id: number;
  message: string;
  tone: ToastTone;
  detail?: string;
}

interface ToastApi {
  show: (message: string, tone?: ToastTone, detail?: string) => void;
}

const ToastContext = createContext<ToastApi | null>(null);

const TONE_CLS: Record<ToastTone, string> = {
  ok: "border-accent/50 text-accent",
  warn: "border-warning/50 text-warning",
  crit: "border-critical/60 text-critical",
  info: "border-secondary/50 text-secondary",
};

export function ToastProvider({ children }: { children: ReactNode }) {
  const [items, setItems] = useState<ToastItem[]>([]);
  const nextId = useRef(1);

  const show = useCallback(
    (message: string, tone: ToastTone = "info", detail?: string) => {
      const id = nextId.current++;
      setItems((prev) => [...prev.slice(-3), { id, message, tone, detail }]);
      setTimeout(() => {
        setItems((prev) => prev.filter((t) => t.id !== id));
      }, 4200);
    },
    [],
  );

  const value = useMemo(() => ({ show }), [show]);

  return (
    <ToastContext.Provider value={value}>
      {children}
      <div
        className="pointer-events-none fixed bottom-4 right-4 z-50 flex w-80 flex-col gap-2"
        role="status"
        aria-live="polite"
      >
        {items.map((t) => (
          <div
            key={t.id}
            className={`pointer-events-auto rounded-lg border bg-elevated px-3 py-2 text-sm shadow-panel ${TONE_CLS[t.tone]}`}
          >
            <div className="font-medium">{t.message}</div>
            {t.detail && (
              <div className="mt-0.5 text-xs font-normal text-t-secondary">
                {t.detail}
              </div>
            )}
          </div>
        ))}
      </div>
    </ToastContext.Provider>
  );
}

export function useToast(): ToastApi {
  const ctx = useContext(ToastContext);
  if (!ctx) throw new Error("useToast must be used within ToastProvider");
  return ctx;
}

/** Confirm-on-second-click button for destructive actions (E-STOP clear…). */
export function ConfirmButton({
  label,
  confirmLabel,
  onConfirm,
  className = "btn-danger",
}: {
  label: string;
  confirmLabel: string;
  onConfirm: () => void;
  className?: string;
}) {
  const [armed, setArmed] = useState(false);
  const timer = useRef<ReturnType<typeof setTimeout> | null>(null);
  useEffect(() => () => void (timer.current && clearTimeout(timer.current)), []);

  return (
    <button
      type="button"
      className={className}
      onClick={() => {
        if (armed) {
          setArmed(false);
          if (timer.current) clearTimeout(timer.current);
          onConfirm();
        } else {
          setArmed(true);
          if (timer.current) clearTimeout(timer.current);
          timer.current = setTimeout(() => setArmed(false), 3000);
        }
      }}
    >
      {armed ? confirmLabel : label}
    </button>
  );
}
