# Build & Flash — Dual-Controller River Cleaning Boat

Both targets use PlatformIO (espressif32@6.7.0, Arduino framework) — the P1
workflow was preserved (p2 rule: no toolchain migration).

## Targets

| Env | Project dir | Board | Firmware |
|---|---|---|---|
| `esp32cam` | `firmware/` | AI-Thinker ESP32-CAM | Vision: camera + TFLM AI + Wi-Fi dashboard + UART link (transmitter) |
| `esp32main` | `firmware/esp32_main/` | `esp32dev` (standard ESP32) | Boat: state machine, motors, conveyor, sonar, GPS, buzzer stub, UART link (receiver) |

Shared library: `shared_protocol/rb_protocol` (referenced by both via
`lib_extra_dirs`).

## Build commands

PowerShell (repo root); re-establish PATH first:

```powershell
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";"
          + [System.Environment]::GetEnvironmentVariable("Path","User")
```

```powershell
# Target 1 — ESP32-CAM
cd firmware
pio run -e esp32cam

# Target 2 — standard ESP32
cd firmware\esp32_main
pio run -e esp32main
```

Long builds (first CAM build ≈ 15 min: full TFLM rebuild) must run detached
with log polling — inline shells die at the tool timeout:

```powershell
New-Item -ItemType Directory -Force -Path logs   # firmware/logs must exist!
Start-Process -FilePath cmd -ArgumentList '/c',
  'pio run -e esp32cam > logs\cam_build.log 2> logs\cam_build.err' `
  -WorkingDirectory (Get-Location)
# poll: Get-Content logs\cam_build.log -Tail 8
```

CAM logs: `firmware/logs/cam_build*.log`; main logs:
`firmware/esp32_main/logs/main_build.log`.

## Flash commands

```powershell
# CAM (board on CH340; stop keeper2 first — it holds COM3!)
cd firmware
pio run -e esp32cam -t upload --upload-port COM3

# Main (when the ESP32 dev board is connected)
cd firmware\esp32_main
pio run -e esp32main -t upload --upload-port <COMx>
```

- **One flash per change, build first** — never flash unverified binaries.
- If the CAM sits in `boot:0x3 DOWNLOAD_BOOT`: pulse RTS/DTR (the capture
  scripts already do this), or unplug/replug USB. `tools/` serial scripts
  reset with `setDTR(False); setRTS(True); 0.15 s; setRTS(False)`.
- `keeper2.py` auto-holds COM3 in run mode — kill it before uploading
  (`Get-CimInstance Win32_Process -Filter "name='python.exe'"` → match the
  keeper command line → `Stop-Process -Id <pid>`; check
  `logs/keeper2_*.log`).

## Serial monitoring

```powershell
pio device monitor -p COM3 -b 115200
```

Required markers that must survive (integration tests parse them):
`[ai] model ready`, `[selftest] camera+ai ready`, `>=2 [ai] cycle … conf=`,
`[hb] … ai=1 cam=1` (CAM side). Main side: `[selftest] main controller
ready; awaiting CAM link`, `[hb] up=… state=…`.

## Wi-Fi / network policy (unchanged from P1)

- Home SSID `realme 9 5G` only via `tools/wifi_test.ps1 connect` with restore
  in `finally`; never embed credentials in the repo.
- `RiverBot-AP` (pw `cleanriver`) is the offline test network — scenario 1
  requires **no internet**, AP mode only.

## Compilation results (recorded honestly)

| Target | Result | Date | RAM | Flash | Notes |
|---|---|---|---|---|---|
| `esp32cam` (P1, single-controller) | SUCCESS | before P2 | 18.8% | 33.3% | baseline |
| `esp32cam` (P2, vision-only + link) | SUCCESS | 2026-10-24 | 18.8% (61688 B) | 33.3% (1047245 B) | full rebuild 882 s + incremental 45.8 s after `RB_TEXT_MAX` change |
| `esp32main` (P2, first build) | SUCCESS | 2026-10-24 | 8.5% (27816 B) | 22.6% (295949 B) | compile-verified only — no ESP32 dev board attached yet |

Flash ≠ validated: neither P2 binary has been on hardware yet
(`docs/testing_plan.md` declares status per test).
