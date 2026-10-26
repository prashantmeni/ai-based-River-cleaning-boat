# Troubleshooting guide

## Build / flash

| Symptom | Cause / fix |
|---|---|
| `NotPlatformIOProjectError` on upload | Run `pio run -t upload` **from `firmware/`** (cwd must contain `platformio.ini`) |
| Upload hangs / `Failed to connect` | Wrong port or chip held in reset — use `--upload-port COM3`; unplug/replug CH340; hold IO0 low only for UART0 bootloader cases |
| `.ps1 cannot be loaded` in PowerShell | Security policy — prefix commands with `$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")` and call `python`/`pio` directly |
| Slow/odd build after flag change | Flag edits force large rebuilds (~15 min cold, ~40 s incremental). `build_unflags = -Os -Og -O0` must stay so `-O2` actually applies |

## Serial monitor

| Symptom | Cause / fix |
|---|---|
| No output on COM3 | Opening the port can hold the ESP32 in reset — pulse DTR/RTS first: `setDTR(False); setRTS(True); sleep(0.15); setRTS(False)` (see `tests/integration/test_device.py`) |
| Garbled text | 115200 baud, 8N1 — `monitor_speed = 115200` |
| Brown-out loops at boot | Weak 5 V supply under Wi-Fi+camera load — use ≥2 A BEC/USB supply |

## Heat (ESP32-CAM)

| Observation | Guidance |
|---|---|
| Board/regulator warm-hot during continuous AI + Wi-Fi | Normal: the AMS1117 regulator and ESP32 die routinely run 70–90 °C with no airflow. The firmware now reports **die temp** every heartbeat (`t=NNC`) and in `status.json.cpu_temp` |
| `th=1` / `th=2` in heartbeats | Thermal guard active: `th=1` AI slowed to 2 s passes, `th=2` AI paused (heartbeats + dashboard continue) until <50 °C. Thresholds are deliberately low (58/70 °C die) because the TSENS reads ~10-15 °C cooler than the AMS1117 regulator you can feel. AI also now runs at ~45% duty (`AI_PERIOD_MS 1500`) instead of back-to-back — don't defeat the guard |
| Too hot to touch (>~65 °C surface), smell, or visible damage | **Power it off and let it cool.** Check: no shorts at 5 V rail, camera ribbon seated, no conductive debris. Bench duty-cycle: run 5–10 min, cool 5 min |
| Bright white LED flashing **during upload** | Normal — bootloader/flash activity. It should stop after `Hard resetting` |
| LED blinking continuously **after boot / reboot loop** | Not normal. Capture serial: `python tests/integration/test_device.py` — a reboot loop shows `rst:0x...` repeating; brown-out (`rst:0x7/0xf`) → stronger supply |

## AI / TFLM (hard-won, don't regress these)

| Symptom | Cause / fix |
|---|---|
| `FullyConnected per-channel quantization not yet supported` | Converter flag required: `converter._experimental_disable_per_channel_quantization_for_dense_layers = True` (`ai/models/convert_int8.py`) |
| `Did not find a parser for ADD` during AllocateTensors | Hand-made `TFLMRegistration` missing `builtin_code = tflite::BuiltinOperator_CONV_2D` |
| `All temp buffers must be freed before calling ResetTempAllocations()` | A kernel's Prepare leaked `AllocateTemp*Tensor` handles — every temp tensor from `micro_context->AllocateTempInputTensor/OutputTensor` needs `DeallocateTempTfLiteTensor` on **all** return paths (see `FastConvPrepare` in `ai_model.cpp`) |
| `Failed to allocate tail memory ... missing: N` | Tensor arena too small. Current model needs >101 KB; `TENSOR_ARENA_SIZE = 107*1024` with `FastConvData` (per-conv persistent state). Bump in 4 KB steps if the model changes |
| Arena lands in PSRAM (slow) | Keep total DRAM request ≤ largest free internal block (`[ai] sizeof-free` line prints it). Model copy is allocated **before** the arena on purpose |
| invoke ≈ 3.4 s | Built without `-O2`, or the custom fast-conv kernel was bypassed (check `[ai] model ready ... (fast conv)`). Expected now: invoke ≈ 657 ms |
| Profiler prints nothing | Per-op ticks need `-DTF_LITE_USE_CTIME`; on-device CSV is gated by `AI_PROFILE 1` in `ai_model.cpp` |

## Wi-Fi dashboard

| Symptom | Cause / fix |
|---|---|
| PC drops RiverBot-AP after 15–60 s | The Ralink RT3290 roams to the home hotspot. **Policy: use `tools\wifi_test.ps1 connect` before any ESP32 test** — it temporarily sets the home profile to manual (no profile deletion), joins RiverBot-AP, and prevents roaming mid-test. Run `wifi_test.ps1 restore` after every test: it returns the home profile to auto-connect, reconnects, and verifies |
| Laptop auto-joins RiverBot-AP during normal work | RiverBot-AP profile is now `connectionmode=manual` — it only joins on explicit request (`wifi_test.ps1 connect`). Keep the home hotspot (`realme 9 5G`) on `auto` for internet |
| `wifi_test.ps1 connect` fails even so | It performs **one** profile repair (export/delete/re-add from `%TEMP%\riverbot.xml`) and one final connect — never a loop. Do **not** repeatedly delete/recreate profiles; if it still fails, power-cycle the ESP32-CAM and retry once |
| Can't join AP | SSID `RiverBot-AP`, password `cleanriver`, channel 6, ip 192.168.4.1 |
| `/stream` headers arrive but **0 body bytes** | **Root cause (found 2026-10-23, FIXED):** `wait_tx_space()` used `WiFiClient::availableForWrite()`, which arduino-esp32 2.0.x never overrides — `Print.h` base returns **0** — so the guard always hit its 3 s budget and the frame body was never written (headers are unguarded, hence they always arrived). Fix in `main.cpp`: guard removed, direct `print`+`write` (write() is internally bounded: select 1 s × 10 + MSG_DONTWAIT, failures log   `send failed/timeout (w=… in …ms)`). **Live video confirmed on a phone** 2026-10-24. If 0 B recurs, read serial for `[stream]` lines before suspecting the radio.
  **Post-fix (2026-10-24) laptop reality check:** the Ralink still gets 0 body bytes and its association flaps (`wifi=1/0` in heartbeats) — with the firmware bug fixed, the remaining laptop failure is genuinely the Ralink↔softAP link (phone streams fine). Use the **phone for live video**; use `tools\dashboard_sim.py` (or small port-80 endpoints, which do reach the laptop) for laptop-side dashboard work |
| Phone "connecting timeout" / joins then drops (`wifi=1/0/1` in heartbeats) | softAP modem sleep (`WiFi.setSleep(true)`) misses client DHCP/check packets and stations flap — keep `WiFi.setSleep(false)` in `dashboard_begin` (heat is handled by AI duty + thermal guard instead). Recover: forget the network on the phone, rejoin |
| Stream test killed my internet | Use the orchestrator, not ad-hoc commands: `tools\run_stream_test.ps1` joins RiverBot-AP, waits for the 192.168.4.x lease, streams, and **always** restores `realme 9 5G` in a `finally` block, verifying internet afterwards |
| `/cmd/*` does nothing visible | Main controller absent or link down (`[hb] up=0`); L298N/motors not fitted — state machine still transitions (`status.json.state`: SEARCH etc.) |
| `basket_state: SENSOR_FAULT` | Expected — basket HC-SR04 not physically fitted yet (pin allocated: TRIG 13 / ECHO 35 on main board) |

## Detection behaviour

| Symptom | Fix |
|---|---|
| `conf` stays 0.3–0.45, no target lock | Confirmation gate is `DETECT_CONF_THRESHOLD 0.55` × `DETECT_PERSIST_FRAMES 3`. Model is 61.3% on synthetic data — capture real footage, retrain, then retune the threshold (`ai/README.md`) |
| Classes flip between runs | Sub-threshold confidences are noise; only persisted ≥0.55 detections count |
| Region always `-` | No confirmed target yet — region is reported from the locked detection |
