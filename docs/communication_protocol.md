# UART Inter-Controller Protocol v1 (p2.txt Phase 3)

Physical: UART 115200 8N1, no flow control, common GND.
CAM `TX=GPIO14 → ESP32 RX=GPIO26`; CAM `RX=GPIO13 ← ESP32 TX=GPIO27`.
Framing: **newline-delimited JSON (NDJSON)** — one object per line, UTF-8,
`\n` terminated (`\r\n` tolerated). Max line **255 chars** + `\n` = 256-byte
buffer; oversized lines are discarded byte-wise to the next `\n` and counted
(`oversize`). **No images or raw frames ever cross the UART.**

Implementation: `shared_protocol/rb_protocol/` (PlatformIO library used by
both targets). Host reference tests: `tests/protocol/test_protocol.py`
(parsing rules + header/config consistency; no host C++ compiler available —
C builders/parsers are validated on-target per `docs/testing_plan.md`).

## Versioning & tolerance

- `"v":1` required; other versions → reject (counted), link stays up.
- Parsers must **ignore unknown keys** (forward-compatible additive fields).
- Unknown `"type"` → reject + count; never disconnect on a single bad line.
- Breaking changes bump `v`.

## Direction A: ESP32-CAM → ESP32 main

### `hb` — heartbeat, 1 Hz
```json
{"v":1,"type":"hb","ms":123456,"cam":1,"ai":1,"temp":59.4,"th":1,"seq":12}
```
Validation: `cam,ai ∈{0,1}`, `temp ∈[-40,120]`, `th ∈{0,1,2}`, `seq` uint32,
`ms` uint32 (CAM uptime ms).

### `det` — classification result, one per AI cycle (≤ ~1/1.5 s)
```json
{"v":1,"type":"det","cls":0,"name":"plastic_bottle","conf":0.85,"dir":-1,"det":true,"seq":42}
```
- `cls ∈ [0,4]` index into the 5-class list (classification model — **no
  bounding boxes exist**; `dir` is the region steering hint −1/0/1).
- `name` must equal the canonical class name for `cls` (mismatch → reject).
- `conf ∈ [0,1]`; `det = (conf ≥ 0.55)` — receiver re-validates independently.
- Sent only when inference succeeded; failures surface via `hb`/`fault`.

### `fault` — camera/AI failure report (event)
```json
{"v":1,"type":"fault","cam":0,"ai":1,"msg":"camera_init_failed","seq":43}
```
`msg` ≤ 31 chars, ASCII `[a-z0-9_]`.

## Direction B: ESP32 main → ESP32-CAM

### `hb` — heartbeat, 1 Hz
```json
{"v":1,"type":"hb","ms":54321,"state":"SEARCH","estop":0,"seq":7}
```
`state` ≤ 15 chars (boat state name), `estop ∈ {0,1}`.

### `state` — dashboard snapshot, 2 Hz
```json
{"v":1,"type":"state","st":"SEARCH","basket":"FILLING","fill":42.5,
 "obs_l":55.0,"obs_r":-1.0,"lat":13.08,"lon":77.55,"fix":1,"sats":7,
 "conv":0,"att":3,"cfm":0,"fault":"","seq":8}
```
Validation: `st,basket` ≤ 15 chars, `fill ∈ [0,100]`, `obs_l/obs_r ≥ -1`
(`-1` = invalid/absent), `lat ∈ [-90,90]`, `lon ∈ [-180,180]`, `fix∈{0,1}`,
`sats ∈ [0,31]`, `conv ∈ [0,3]` (conveyor enum), `att,cfm ≥ 0`,
`fault` ≤ 23 chars (`""` = none).

### `cmd` — dashboard command forwarded by CAM, event
```json
{"v":1,"type":"cmd","action":"auto_on","seq":9}
```
Actions (whitelist): `auto_on, auto_off, fwd, rev, left, right, stopm,
conveyor, reset, clear` — 1:1 with existing `/cmd/*` endpoints.

## Sequence, duplicates, staleness

- `seq` = per-sender uint32 counter, increments per sent message.
- Receiver drops a message whose `seq` **equals the last accepted seq of the
  same type** (stuck/duplicate guard). Seq resets are tolerated (receiver
  reboots) — time is the authority for freshness:
- **Det staleness**: `det` older than **4000 ms** at receive time is never
  used for navigation (`RB_DET_STALE_MS`).
- **Link timeouts**: ESP32 side stops actuators if no valid CAM message for
  **3000 ms** (`RB_LINK_TIMEOUT_MS`); CAM shows link-lost if no valid main
  message for **10000 ms** (`RB_CAM_LINK_TIMEOUT_MS`).

## Receive-side error counters (logged, not fatal)

`bad_json, bad_version, unknown_type, bad_field, oversize, dup_seq`.
