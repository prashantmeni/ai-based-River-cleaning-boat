# RiverClean AI — Dashboard (`dashboard-ui`)

Professional 5-page control dashboard for the river-cleaning boat.
React + TypeScript + Vite + Tailwind CSS 3, connecting to the ESP32 main
controller's **WebSocket gateway** over the boat's own Wi-Fi access point.

> Safety: this app is a **command/telemetry client only**. E-STOP, fault
> latches, mode guards and motor timeouts run in the ESP32 firmware.

## Quick start

```bash
cd dashboard-ui
npm install
npm run dev          # http://localhost:5173
```

**Live hardware:**

1. Power both boards (ESP32-CAM + main ESP32).
2. Join Wi-Fi `RiverClean-Boat` from this machine (password:
   `DASH_AP_PASS` in `firmware/esp32_main_controller/include/boat_config.h`).
   *Your laptop loses internet while connected — that is expected.*
3. Open the dev server (or a production build). The app auto-connects to
   `ws://192.168.4.1:81`; the top bar should read **LINK ONLINE**.

**No hardware:** click **Demo mode** in the top bar and pick a scenario
(Normal mission / Fault & obstacle / CAM link loss). Demo data is
banner-marked `SIMULATION` and never touches the radio.

## Scripts

| Command | What it does |
|---|---|
| `npm run dev` | Vite dev server (port 5173) |
| `npm run build` | `tsc --noEmit` typecheck + production bundle → `dist/` |
| `npm run preview` | Serve the production bundle locally |
| `npm run test` | Vitest suite (51 tests, jsdom) |
| `npm run lint` | ESLint (flat config, `--max-warnings 0`) |

## Pages

| Route | Purpose |
|---|---|
| `/` Overview | Status header, vital metrics, motor gauges, live detection, drive trend, position map, recent detections, collection stats |
| `/camera` | Live MJPEG stream from the ESP32-CAM (`http://rivercam.local:8080/stream`), AI detection panel (target lock, confidence, events), ESP-NOW link health |
| `/drive` | Manual drive (bounded commands), conveyor ON/OFF, state machine map, autonomy engage, ultrasonic bars, GPS, E-STOP latch/clear |
| `/diagnostics` | RX counters, frame-error legend (2–8, 0xA1–0xA4), fault-bit decoder, hello info, CSV logger, raw JSON |
| `/settings` | WS endpoint, stream URL, manual PWM/duration, map + CSV toggles, demo scenarios, getting started, troubleshooting, reset (localStorage) |

Old URLs from the pre-merge 9-page layout (`/detection`, `/navigation`,
`/automation`, `/collection`, `/help`) redirect to the pages above.

## Architecture

```
src/
  services/communication/     ← firmware contract mirrors
    types.ts                  Telemetry/Hello/Ack interfaces + DIAG_CMD, FAULT,
                              CLASS_LABELS (ai_config.h), NAV_STATES
    telemetryParser.ts        strict JSON parsing (rejects, never defaults)
    websocketClient.ts        reconnect with capped backoff
    connectionManager.ts      socket ↔ store pipeline, hello/ack routing
    commandService.ts         "#id CMD:cmd,value,dur" + awaited acks (≤3 s)
    store.ts                  immutable snapshots for useSyncExternalStore
  state/
    SettingsContext.tsx       localStorage-backed settings
    DemoContext.tsx           demo switching (disconnects live link)
    TelemetryContext.tsx      React binding + CSV logging + command routing
    demoEngine.ts             synthetic mission state machine (3 scenarios)
    useBoat.ts                shared risk/fault derivation
  components/  layout/ ui/ map/    shell, kit (Panel, pills, gauges…), Leaflet
  pages/                      the 5 routes
  lib/                        format, faults, csv, storage, settings
```

### Firmware contract (source of truth)

- **Telemetry** — `firmware/esp32_main_controller/src/telemetry.cpp`
  (`type/protocol_version/device_id` envelope + state, comms_ok, estop,
  autonomous, detection…, ultrasonic `left_cm/right_cm` where **-1 = invalid**,
  GPS incl. `satellites/gps_age_ms`, conveyor, fault_flags, motors, and
  diagnostics `rx_bytes/rx_errors/perr/drops/dbg`).
  `perr` is the **last frame error code** (0 = OK, 2–8 parse errors,
  0xA1–0xA4 payload errors) — see the Diagnostics legend.
- **Hello** — `src/ws_gateway.cpp` on connect: `{type:"hello", ws_port:81,
  espnow_channel:1, …}`.
- **Commands** — text `[#<id> ]CMD:<cmd>,<value>,<duration_ms>` (ids ≤23 chars
  `[A-Za-z0-9_-]`, value 0–255, duration 0–60000 ms); ack:
  `{"type":"ack","id":…,"status":"ok|rejected","cmd":N,"reason":…}`.
  Codes: 1 STOP (value 1 = E-STOP latch, 2 = clear), 2 FWD, 3 LEFT, 4 RIGHT,
  5 CONV_ON, 6 CONV_OFF, 7 AUTO_ON, 8 AUTO_OFF.
- **Wi-Fi contract** — the main radio is `WIFI_AP_STA`: ESP-NOW + SoftAP both
  pinned to **channel 1**; AP SSID `RiverClean-Boat`, IP `192.168.4.1`,
  WPA2. No internet while joined; map tiles then fail → honest offline
  notice (coordinates still live).

## Honest-data rules (by design)

- Never invents readings: missing/unavailable → "Unavailable/Invalid/No fix";
  `-1 cm` ultrasonic renders as *Invalid*, battery has no sensor → not shown.
- Live vs demo is always distinguishable (source pill, banner, purple accent).
- Commands only report success after a firmware ack; timeouts/rejections are
  surfaced as toasts.
- `settings`, CSV buffers persist in `localStorage` (device-local, no cloud).

## Testing

```
npm run test     # 51 tests: parser, command framing, store/stale logic,
                 # fault decoder, CSV, demo engine, Drive/Overview pages
npm run lint     # 0 problems
npm run build    # typecheck + bundle green
```

## Legacy dashboard

`dashboard/` (bridge.py + single-page HTML over USB serial) still works and
is useful when the Wi-Fi gateway is unavailable — see the repo README.
