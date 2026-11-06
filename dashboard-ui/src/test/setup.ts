import "@testing-library/jest-dom/vitest";
import { afterEach, beforeEach } from "vitest";
import { cleanup } from "@testing-library/react";

// jsdom lacks ResizeObserver (recharts) — provide a minimal stub.
if (typeof globalThis.ResizeObserver === "undefined") {
  class RO {
    observe() {}
    unobserve() {}
    disconnect() {}
  }
  globalThis.ResizeObserver = RO as unknown as typeof ResizeObserver;
}

// Deterministic settings for tests: map off (leaflet needs a real layout).
beforeEach(() => {
  window.localStorage.clear();
  window.localStorage.setItem(
    "riverclean.settings.v1",
    JSON.stringify({ mapEnabled: false }),
  );
});

afterEach(() => {
  cleanup();
});
