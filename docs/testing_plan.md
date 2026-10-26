# Testing Plan & Results — P2 Dual-Controller Refactor

Status keys (p2 honesty rules):

- **[V]** physically validated on hardware (with evidence: serial log / URL)
- **[T]** automated test executed offline — result recorded below
- **[C]** compile/build verified only
- **[U]** untested — hardware/spec blocked

## 1. Automated tests (executed)

| Suite | Command | Result | Date |
|---|---|---|---|
| Protocol (framing, validation, seq/staleness, config sync, limits) | `python -m unittest discover -s tests\protocol` | **40/40 PASS, exit 0** | 2026-10-24 |
| Model artifacts / unit | `python -m unittest discover -s tests\unit` | **7/7 PASS, exit 0** | 2026-10-24 |
| Integration: stream, endpoints, device | `python -m unittest discover -s tests\integration` | **[V] PASS in P1 (exit 0) — must be re-run after P2 flash** | P1 date |

Coverage of the p2 Phase-8 test list, offline-mocked (no serial hardware):
serialization/parsing round-trips, valid/invalid/incomplete/oversize
messages, unknown waste classes, confidence validation, stale detections,
duplicate sequence numbers, class-name + threshold config sync across
`config.h`/`config_main.h`, `RB_TEXT_MAX` vs longest state name.

**Not host-testable** (no C++ compiler on this machine): state-machine fault
paths, link-loss timeouts, conveyor interlock logic — these are compile-checked
[ C ] and covered by HIL scenarios below on hardware.

## 2. Hardware-in-the-loop scenarios (p2 Phase 8)

| # | Scenario | Steps (summary) | Status |
|---|---|---|---|
| 1 | AI inference with no internet | `wifi_test.ps1 connect riverbot`-style AP join (`RiverBot-AP`), confirm dashboard + detections offline, no DNS/cloud traffic | **[V] P1** (stream+AI on phone via AP); re-run on P2 binary |
| 2 | Main-controller motors/sensors independent | Flash `esp32main` alone; bench: sonar hand-waves → `[hb] obs=…`, manual cmds, no CAM attached → `LINK_LOST` latch stops motors | **PARTIAL [V] 2026-10-24** — flashed to COM5 (hash verified); boot + full init sequence + `[selftest] main controller ready; awaiting CAM link` + stable 1 Hz `[hb]` (heap 339856) observed; `sonar=1` correctly latched after 3 s with no sensors fitted (`obs=-1/-1`); `link=0`, `rx=0`, no actuator ever commanded. Pending: real sonar/`obs` reads, motor legs (L298N not fitted), `LINK_LOST` motor-stop (needs link input) |
| 3 | UART between boards | Both flashed; watch CAM `[hb]` + main `[hb] up=…`; unplug/replug link TX | **PARTIAL [V] 2026-10-24** — both flashed (COM5 main, COM3 CAM). **UART peripherals proven**: main TX/RX via physical 27→26 loopback (bad_field counter climbed 1 Hz) + CAM via internal UART1 loopback self-test `[lb] UART1 loopback PASS`. **Full link logic proven over USB mock** (`tools/link_mock.py`, LINK_USB_MIRROR): main `rx=0→19`, `link=1` on inject → `link=0` after silence (link-loss timeout **[V]**), `det` flag flipped 0→1; CAM `state=NO_LINK→SEARCH`, `auto=1` autoflag. **[U]**: the physical wire path (CAM GPIO14→26, GPIO27→13) still passes zero bits both ways — user rewiring/photo pending |
| 4 | Obstacle during classification | In APPROACH, block US1/US2 → turn-away/stop persists across `det` msgs; `[hb] state=` transitions | **[C]** logic compiled — **[U]** |
| 5 | Camera or AI failure | CAM self-test fault (`camera_init_failed` / model load fail) → `cam=0/ai=0` → main latches `CAM_AI`/`FAULT`, motors stopped | **[C]** — **[U]**; P1 had equivalent single-board fault stops **[V]** |
| 6 | UART communication loss | Pull link while `auto_on` → main stops within 3 s (`LINK_LOST`), auto-resumes after link returns | **[V] link-loss detection 2026-10-24** (mock: `link=0` ~3 s after injection stops; `fault="LINK_LOST"` present in state msgs). Motor-stop leg **[U]** (auto/L298N pending) |
| 7 | Conveyor with safe motor states | `conveyor` cmd only while state machine permits; `COLLECT` stops drive; `COLLECT_DURATION_MS` → `CONVEYOR_TIMEOUT`; motors must be stopped or holding | **[C]** — **[U]** |
| 8 | Controlled water testing + recovery | Hull restrained first; props-off bench run, then shallow-water trial, fault recovery via `clear` | **[U]** — do **after** scenarios 2–7 pass |

Mocking policy: protocol-level messages are mocked by
`tests/protocol/*.py` fixtures; serial-level mocking is
`tools/link_mock.py` — injects `hb_cam`/`det` into the main controller and
`hb_main`/`state` into the CAM over each board's USB console
(`LINK_USB_MIRROR=1` bench flag), exercising the full framer → parser →
seq-gate → application path without UART wires (p2 "mocked serial"
allowance). Evidence: `logs/main_mock_evidence4.txt`,
`logs/cam_mock_evidence.txt`.

Bench diagnostics (both boards, `LINK_*` flags = 1; set 0 for field
builds): main `[stx]` state-TX trace + TX mirrors + UART1 raw sniff;
CAM TX mirror + boot-time UART1 internal loopback self-test.
Protocol line limit raised 192→256 / 191→255 on 2026-10-24 — the
200-char `state` message (with `fault="LINK_LOST"`) exceeded the old
`RB_MAX_MSG_LEN` and every TX silently failed; tests updated (40/40).

## 3. Previously validated behavior retained from P1 **[V]**

Levels 5/6/9 PASS + live phone video via `RiverBot-AP`, thermal guard, fast
conv kernel (≈680 ms/cycle), stream fix, heat fix — CAM vision core unchanged
by the refactor (audit report, `docs/test-report.md`). Integration suite must
be re-run post-flash to re-confirm them on the P2 binary.

## 4. Blockers to executing scenarios

1. Physical UART wires between boards (CAM GPIO14→main GPIO26, GPIO27→13)
   pass zero bits — proven-peripheral software is ready; user rewiring/photo
   pending → scenario 3 full, 4, 6(motor leg), 7.
2. Water access for 8; L298N/motors/sonar/GPS not yet fitted (scenarios
   2 remainder, 4, 7).
3. Unresolved specs: battery cell count (battery ADC disabled), buzzer and
   e-stop button absent from parts list (stubs/disabled — honest gaps).
4. `keeper2` holds COM3 when plugged — stop before flashing.
