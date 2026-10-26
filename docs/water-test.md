# Water test instructions

## 0. Preconditions

- Firmware flashed (`pio run -t upload --upload-port COM3` from `firmware/`).
- Serial monitor at 115200 shows:
  `[selftest] camera+ai ready; state=IDLE`, heartbeat `[hb] ... ai=1 cam=1`
  with **no** `FAULT_STOP`.
- Battery charged; props unguarded — **restrain the hull for bench runs**.
- Known-good readings on the dashboard (`http://192.168.4.1/status.json`).

## 1. Bench tests (props removed or hull clamped)

1. Power on → join Wi-Fi **RiverBot-AP** (password `cleanriver`) →
   open `http://192.168.4.1/`.
2. Confirm `status.json` fields: `state` transitions SELF_TEST →
   `MANUAL/IDLE`; `ai_class`/`ai_conf` update as you point the camera at
   objects (`cls=…` counters on serial increment only above the 0.55
   confirmation gate).
3. **Motors:** `GET /cmd/fwd`, `/cmd/rev`, `/cmd/left`, `/cmd/right`,
   `/cmd/stopm` — verify direction sign convention (swap ESC leads if
   mirrored). 8 s command timeout must cut power if you stop sending.
4. **Conveyor:** `GET /cmd/conveyor` — runs nominal 4 s sweep
   (`COLLECT_DURATION_MS`), hard-stops at 8 s.
5. **Streaming:** `GET /stream` (MJPEG). Expect visible frame rate; AI
   cycles (~680 ms) continue underneath.
6. **Auto loop:** `GET /cmd/start` → `state=SEARCH`; verify left/region
   steering from the JSON (`region`, `obs_l/obs_r`); `/cmd/stop` returns
   to `MANUAL/IDLE`.

## 2. Still-water trial (pool / calm edge)

1. Place a clear plastic bottle 2–3 m ahead.
2. `/cmd/start`. Expected: SEARCH (slow sweep) → detection → APPROACH
   (class-locked for 3 frames) → conveyor sweep on arrival → RESUME search.
3. If it never locks: the scene confidence is under `DETECT_CONF_THRESHOLD`
   (0.55). Record confidences from serial (`[ai] cycle … conf=… cls=…`)
   and log 30–60 s of JSON status — use these to retune the threshold
   (see `docs/model-card.md`).
4. Obstacle check: hold a board ~40 cm in front of US1 → boat must stop/turn
   (`OBSTACLE_STOP_CM 40`). US1 reports `-1` if not fitted.
5. GPS: outdoors with sky view, `gps_fix` should flip true within a minute
   (`sats>0`); route is logged (128 points max).

## 3. River trial checklist

- [ ] Hull sealed, battery strap checked, buoyancy OK at loaded weight.
- [ ] Recovery: kill via dashboard `/cmd/stopm` + `/cmd/stop`, then power.
- [ ] Note water surface glare angle vs sun for future dataset captures.
- [ ] Photograph every approach (success/failure) for the next training set —
      current model is **61.3%** on synthetic data; real footage is the
      fastest path to a better one.
- [ ] Basket: `basket_fill` is `SENSOR_FAULT` until the second ultrasonic
      is fitted — empty manually on a schedule.

## 4. Failure playbook

| Symptom | First look |
|---|---|
| `FAULT_STOP` at boot | serial self-test line; camera ribbon / power sag |
| Wi-Fi drops from PC | Ralink client roams to another AP within ~15–60 s — use phone or `netsh wlan connect name="RiverBot-AP"` then reload immediately |
| Motors silent | CAM↔main link down (`[hb] up=0` → `LINK_LOST` stops motors), main board not flashed/powered, or L298N power (ENA/ENB **jumpers removed**, Vin/GND common) |
| Garbage classifications | save frames; retrain with real footage |
| Brown-out resets | BEC current rating / battery sag under prop load |
