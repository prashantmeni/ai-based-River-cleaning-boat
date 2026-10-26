# Hardware & test report — completion levels

Evidence captured on the development PC (Windows 10, COM3, AI-Thinker
ESP32-CAM) during the final integration pass. Dates are session-relative;
re-run any command to reproduce.

Status legend: **PASS** (evidence on real hardware/workstation) ·
**BLOCKED** (hardware absent) · **NOT TESTED** (needs user/environment).

---

**LEVEL 1: Source code created — PASS**
- Evidence: `firmware/src/{main,ai_model,camera,control,state_machine,motors,gps,ultrasonics,conveyor,basket,dashboard}.cpp` + `include/config.h`; `ai/` pipeline; `tests/`.
- Command: `Get-ChildItem -Recurse firmware\src` / `python -m unittest discover -s tests/unit`.

**LEVEL 2: Desktop tests passed — PASS**
- Command: `python -m unittest discover -s tests/unit -v`
- Observed: `Ran 7 tests in 0.045s  OK` (tflite/header byte-sync, TFL3 magic, report/class/threshold consistency).
- Remaining: none for artifact tests; more unit tests possible for pure logic (region math) later.

**LEVEL 3: Firmware compiled — PASS**
- Command: `cd firmware; pio run`
- Observed: `Successfully created esp32 image. [SUCCESS] Took 37.68 seconds`.
- Flags: `-O2 -DTF_LITE_USE_CTIME -DBOARD_HAS_PSRAM ...` (see `platformio.ini`).

**LEVEL 4: ESP32-CAM successfully flashed — PASS**
- Command: `pio run -t upload --upload-port COM3` (cwd = `firmware/`)
- Observed: `[SUCCESS] Took 34.78 seconds`, `Hard resetting via RTS pin...`, then fresh boot log on serial.

**LEVEL 5: Camera verified — PASS (init + frames + live video on phone)**
- Evidence (serial): `[cam] init OK (527ms)`, `[selftest] camera+ai ready; state=IDLE`.
- Frames proven indirectly: every `[ai] cycle 680ms conf=...` line is produced from a freshly captured QVGA frame.
- JPEG encode proven: `[stream] frame captured len=1933-3601 enc=103-210ms`
  (software `frame2jpg` on the RGB565 QVGA frame).
- **Live video verified on a phone joined to RiverBot-AP (`http://192.168.4.1/`)
  — user-confirmed visual: frames streaming on the dashboard (2026-10-24).**
- Stream send verified in serial: `[stream] frame sent` (6/8 in the watch log);
  failures log bounded diagnostics `send failed/timeout (w=0+0/... in 10016ms)`
  = dead peer, write() times out per contract and the stream closes cleanly.
- **Root cause found & fixed (2026-10-23):** `wait_tx_space()` used
  `WiFiClient::availableForWrite()`, which this framework never overrides
  (`Print.h` base returns 0) — the guard always timed out after 3 s and the
  frame body was NEVER written. Headers (unguarded) always arrived, so every
  client saw dashboard+headers but zero video. Fix: removed the guard;
  `WiFiClient::write()` is internally bounded (select 1 s × 10, MSG_DONTWAIT).
- Laptop caveat (post-fix, 2026-10-24): device wrote frames to the laptop
  connection (`frame sent`) but the Ralink received 0 B and its association
  flapped (`wifi=0` 26/94 heartbeats) with TCP connect stalls — the Ralink↔AP
  link is genuinely unstable; phone is the reliable video client.

**LEVEL 6: AI inference verified on hardware — PASS**
- Evidence (serial): `[ai] model ready, in_type=9 out_type=1 (fast conv)`; repeated `[ai] cycle 679-681ms conf=0.31-0.44 cls=0..3`; heartbeats `ai=1`.
- Measured: invoke 657 ms (CONV_2D 594 ms via profiler CSV), quant 16 ms; target ≤1.5 s met.
- Command: `python tests/integration/test_device.py` → `SERIAL: PASS (exit 0)`.
- Remaining: accuracy on real water (61.3% synthetic sample — `docs/model-card.md`).

**LEVEL 7: Sensors verified — BLOCKED**
- Missing hardware: both ultrasonics, GPS module (`obs_l/obs_r = -1`, `gps_fix=false`, `sats=0` are the graceful placeholders).
- Reproducible procedure once fitted: `docs/water-test.md` §1.5 (obstacle at 40 cm → stop/turn) and §1.6 (GPS fix outdoors).

**LEVEL 8: Motor drivers verified — BLOCKED**
- Missing hardware: PCA9685 (`[motors] PCA9685 NOT found` every boot, handled) + ESCs/props/conveyor.
- Reproducible procedure: `docs/water-test.md` §1.3–1.5 (`/cmd/fwd|rev|left|right|stopm`, `/cmd/conveyor`, 8 s timeout).

**LEVEL 9: Dashboard verified — PASS**
- Command: join `RiverBot-AP`, then `Invoke-WebRequest http://192.168.4.1/status.json` etc.
- Observed: `GET /` → 200, 3167 B HTML incl. **Thermal card** (`thermal_card=True`);
  `GET /status.json` → valid JSON incl. `"cpu_temp":67.2,"thermal":0`;
  `GET /stream` → multipart MJPEG frames; `GET /cmd/start` → `state=SEARCH`;
  `GET /cmd/stop` → `state=MANUAL/IDLE`.
- Automated: `python tests/integration/test_device.py --http` (serial + status + index + stream).
  **2026-10-24 full re-run PASS on final firmware**: SERIAL PASS, HTTP PASS
  (`thermal_card=True`, stream headers OK/WARN on this adapter, Wi-Fi restored
  + verified in `finally`). The HTTP phase now uses `tools\wifi_test.ps1`
  (policy-safe join/restore) with index retries for the Ralink's transient drops.
- Environment quirks (documented, not product faults): PC Wi-Fi roams away after
  ~15–60 s; after rapid connect cycles the profile binding breaks — repair via
  export/delete/re-add (`docs/troubleshooting.md`). `interface=` on `netsh wlan
  connect` must not be used.

**LEVEL 10: Integrated bench tests passed — PARTIAL (software side PASS)**
- PASS (observed): boot → SELF_TEST → auto `SEARCH` via `/cmd/start` → JSON state transitions → `/cmd/stop` → `MANUAL/IDLE`; AI cycles continue inside auto mode; 1 Hz heartbeats with stable heap (~71 KB) across ≥18 s; no FAULT.
- NOT TESTED: physical drive/collect legs of the bench checklist (needs Level 7/8 hardware): `docs/water-test.md` §1.

**LEVEL 11: Controlled water trials — NOT TESTED**
- Deliberately unclaimed until the user runs them. Procedure + pass criteria:
  `docs/water-test.md` §2 (still water) and §3 (river trial).

---

## Summary

| Level | Status |
|---|---|
| 1 Source created | PASS |
| 2 Desktop tests | PASS |
| 3 Compiled | PASS |
| 4 Flashed | PASS |
| 5 Camera | PASS — init + frames + **live video on phone** (user-verified) |
| 6 AI on hardware | PASS |
| 7 Sensors | BLOCKED — no sensor hardware |
| 8 Motors | BLOCKED — no PCA9685/ESC hardware |
| 9 Dashboard | PASS |
| 10 Integrated bench | PARTIAL — software PASS, physical legs blocked |
| 11 Water trials | NOT TESTED — user procedure ready |

---

## P2 Refactor (dual-controller) — status 2026-10-24

| Item | Status |
|---|---|
| Code audit (`docs/audit-report.md`) | DONE — [V]/[C]/[U]/[D] per file |
| UART protocol lib + spec | DONE — `shared_protocol/rb_protocol`, `docs/communication_protocol.md` |
| Protocol tests | PASS — 40/40 exit 0 (`tests/protocol`) |
| Model-artifact tests | PASS — 7/7 exit 0 (`tests/unit`) |
| `esp32cam` vision build (refactored) | SUCCESS — RAM 18.8%, Flash 33.3% (`firmware/logs/cam_build2.log`) |
| `esp32main` boat build (new) | SUCCESS — RAM 8.5%, Flash 22.6% (`firmware/esp32_main/logs/main_build.log`) |
| Model/pipeline preserved | YES — no model changes; markers `[ai] model ready`, `>=2 [ai] cycle … conf=` kept |
| HIL scenarios 1–8 | PARTIAL — plan + status keys in `docs/testing_plan.md`; mocked-serial legs **PASS** (below) |
| `esp32main` on hardware | **PASS [V] 2026-10-24** — COM5, hash verified; boot + init + 1 Hz `[hb]` + `sonar=1` at 3 s (`logs/main_boot_evidence.txt`); state TX fixed (see below), `[stx] ok n=200/201` |
| `esp32cam` on hardware | **PASS [V] 2026-10-24** — COM3 flashed; `test_device.py` SERIAL PASS earlier; after instrumentation: `[lb] UART1 loopback PASS` (UART1 TX→RX proven), AI/camera/AP all up, `state=NO_LINK` |
| State TX bug (root-caused + fixed) | `state` msgs are 200 chars with `fault="LINK_LOST"` > old `RB_MAX_MSG_LEN=191` → every `rb_build_state` returned -1 (`[stx] BUILD_FAIL`). Fix: limits 192/191 → 256/255 (`protocol.h`, tests, doc); protocol suite **40/40 PASS** re-run |
| Mocked serial E2E (`tools/link_mock.py`, USB mirror) | **PASS [V] 2026-10-24** — main: `rx=0→19`, `link=1` on inject → `link=0` after silence (link-loss timeout), `det` flag 0→1 (`logs/main_mock_evidence4.txt`); CAM: `state=NO_LINK→SEARCH`, `auto=1` (`logs/cam_mock_evidence.txt`) |
| Physical UART wire path | **FAIL [V] 2026-10-24** — zero bits both directions (CAM14→26, 27→13) despite both peripherals proven; rewiring/photo pending — last open hardware item for link |

Levels 1–11 above refer to the P1 single-controller build. After both P2
binaries are flashed, re-run `tests/integration/test_device.py` (± `--http`)
and `docs/testing_plan.md` scenarios 1–8 to re-validate on the new
architecture.
