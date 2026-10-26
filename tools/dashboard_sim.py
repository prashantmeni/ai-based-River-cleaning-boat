#!/usr/bin/env python3
"""Desktop simulator for the River Cleaning Boat dashboard.

Serves the *exact* index page embedded in firmware/src/dashboard.cpp plus a
simulated /status.json, /cmd/* and MJPEG /stream so the UI can run on a PC
with no ESP32-CAM attached.

Usage:
    python tools/dashboard_sim.py [--host 127.0.0.1] [--port 8080]
    open http://127.0.0.1:8080/
"""
import argparse
import io
import json
import math
import random
import re
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from PIL import Image, ImageDraw

REPO = __import__("pathlib").Path(__file__).resolve().parents[1]
FW_CPP = REPO / "firmware" / "src" / "dashboard.cpp"
FW_CONFIG = REPO / "firmware" / "include" / "config.h"

STATE_NAMES = {
    0: "BOOT", 1: "SELF_TEST", 2: "IDLE", 3: "SEARCH", 4: "DETECT",
    5: "ALIGN", 6: "APPROACH", 7: "COLLECT", 8: "VERIFY_COLLECTION",
    9: "RETURN_OR_STOP", 10: "FAULT_STOP",
}
ST_SELF_TEST, ST_IDLE, ST_SEARCH, ST_DETECT, ST_ALIGN, ST_APPROACH, \
    ST_COLLECT, ST_VERIFY, ST_RETURN = 1, 2, 3, 4, 5, 6, 7, 8, 9
# state -> (duration s, next); COLLECT/RETURN resolved dynamically
AUTO_FLOW = {
    ST_SELF_TEST: (1.5, ST_IDLE),
    ST_IDLE: (1.0, ST_SEARCH),
    ST_SEARCH: (None, ST_DETECT),   # random 3-6 s then a detection event
    ST_DETECT: (1.0, ST_ALIGN),
    ST_ALIGN: (1.2, ST_APPROACH),
    ST_APPROACH: (3.0, ST_COLLECT),
    ST_COLLECT: (4.0, ST_VERIFY),   # conveyor RUN during COLLECT
    ST_VERIFY: (1.5, ST_SEARCH),
    ST_RETURN: (2.5, ST_IDLE),
}
CLASS_NAMES_FALLBACK = [
    "plastic_bottle", "plastic_wrapper", "leaf_or_organic_debris",
    "other_visible_waste", "background_or_no_waste",
]


def load_class_names():
    try:
        m = re.search(r"CLASS_NAMES\[NUM_CLASSES\]\s*=\s*\{(.*?)\};",
                      FW_CONFIG.read_text(encoding="utf-8"), re.S)
        names = re.findall(r'"([^"]+)"', m.group(1))
        if len(names) >= 5:
            return names
    except Exception:
        pass
    return CLASS_NAMES_FALLBACK


def load_index_html():
    txt = FW_CPP.read_text(encoding="utf-8", errors="replace")
    m = re.search(r'R"HTML\((.*)\)HTML"', txt, re.S)
    if not m:
        raise RuntimeError(f"INDEX_HTML not found in {FW_CPP}")
    html = m.group(1)
    arr = json.dumps(load_class_names(), separators=(",", ":"))
    return html.replace("%CLASSN%", arr)


class Sim:
    """Mirrors firmware semantics (main.cpp snapshot + state machine)."""

    def __init__(self, class_names):
        self.cls = class_names
        self.lock = threading.Lock()
        self.t0 = time.time()
        self.auto = False
        self.sm = ST_IDLE
        self.sm_entered = time.time()
        self.sm_dur = 1.0
        # AI
        self.ai_class = -1
        self.ai_conf = 0.0
        self.region = "-"
        self.pending_class = None
        # counts / basket
        self.counts = [0, 0, 0, 0]      # bottle, wrapper, organic, other
        self.attempts = 0
        self.confirmed = 0
        self.fill = 0                   # percent
        self.basket = "EMPTY"
        # misc telemetry
        self.obs_l = -1
        self.obs_r = -1
        self.conveyor = "OFF"
        self.conveyor_off_at = 0.0
        self.gps_fix_at = time.time() + 8.0
        self.lat, self.lon = 17.4126, 78.4456   # seed drift point (FakeRiver)

    # ---- state machine -------------------------------------------------
    def start_auto(self):
        self.auto = True
        self._enter(ST_SELF_TEST)

    def stop_auto(self):
        self.auto = False
        self.sm = ST_IDLE

    def _enter(self, s):
        self.sm = s
        self.sm_entered = time.time()
        d = AUTO_FLOW.get(s)
        if s == ST_SEARCH:
            self.sm_dur = random.uniform(3.0, 6.0)
        else:
            self.sm_dur = d[0] if d else 1.0
        if s == ST_COLLECT:
            self.attempts += 1
            self.conveyor = "RUN"
            self.conveyor_off_at = time.time() + 4.0
        if s == ST_DETECT:
            # a locked detection: conf >= 0.55 so ai_class/region light up
            weights = [0.35, 0.30, 0.25, 0.10]
            self.pending_class = random.choices([0, 1, 2, 3], weights)[0]
            self.ai_class = self.pending_class
            self.ai_conf = round(random.uniform(0.58, 0.79), 2)
            self.region = random.choice(["left", "center", "right", "center"])
        if s == ST_VERIFY:
            if random.random() < 0.72 and self.pending_class is not None:
                self.confirmed += 1
                if self.pending_class < 3:
                    self.counts[self.pending_class] += 1
                else:
                    self.counts[3] += 1
                self.fill = min(100, self.fill + random.randint(10, 16))
            self.pending_class = None

    def _tick_state(self, now):
        if not self.auto:
            return
        if self.sm == ST_IDLE and self.basket == "FULL":
            return          # wait for reset before resuming the cycle
        if now - self.sm_entered < self.sm_dur:
            return
        nxt = AUTO_FLOW.get(self.sm)
        if self.sm == ST_SEARCH:
            self._enter(ST_DETECT)
        elif self.sm == ST_RETURN:
            self._enter(ST_IDLE)
        elif nxt:
            if self.sm == ST_VERIFY and self.fill >= 100:
                self.basket = "FULL"
                self._enter(ST_RETURN)
            else:
                self._enter(nxt[1])
        if self.fill >= 90:
            self.basket = "NEAR_FULL"
        elif self.fill >= 55:
            self.basket = "FILLING"
        elif 0 < self.fill < 55:
            self.basket = "FILLING" if self.basket == "EMPTY" else self.basket
        elif self.fill == 0 and self.basket != "FULL":
            self.basket = "EMPTY"

    # ---- telemetry -----------------------------------------------------
    def _tick_ai(self, now):
        if self.sm in (ST_DETECT, ST_ALIGN, ST_APPROACH, ST_COLLECT) and self.auto:
            if self.sm == ST_COLLECT:
                self.ai_conf = max(0.0, self.ai_conf - 0.05)
                self.region = "-" if self.ai_conf < 0.55 else self.region
                self.ai_class = -1 if self.ai_conf < 0.55 else self.ai_class
            return  # keep locked values
        # wander below threshold like the real model (0.28-0.52)
        self.ai_conf = round(random.uniform(0.28, 0.52), 2)
        self.ai_class = -1
        self.region = "-"

    def _tick_misc(self, now):
        if self.conveyor == "RUN" and now >= self.conveyor_off_at:
            self.conveyor = "OFF"
        # obstacles: absent (-1) except occasional proximity blips
        if random.random() < 0.015:
            self.obs_l, self.obs_r = random.randint(18, 70), random.randint(18, 70)
            self._obs_off = time.time() + 3.0
        if hasattr(self, "_obs_off") and now >= self._obs_off:
            self.obs_l = self.obs_r = -1
        # slow GPS wander once fix acquired
        if now >= self.gps_fix_at:
            self.lat += random.uniform(-3e-6, 3e-6)
            self.lon += random.uniform(-3e-6, 3e-6)

    @staticmethod
    def cpu_temp(now):
        # sine so the thermal card can legitimately show normal -> throttled
        return 61.5 + 9.5 * math.sin(now / 90.0)

    @staticmethod
    def thermal_level(t):
        return 2 if t >= 80.0 else 1 if t >= 70.0 else 0

    def tick(self):
        now = time.time()
        with self.lock:
            self._tick_ai(now)
            self._tick_state(now)
            self._tick_misc(now)

    def status(self):
        now = time.time()
        with self.lock:
            t = self.cpu_temp(now)
            has_fix = now >= self.gps_fix_at
            # same field order/values as dashboard.cpp handle_status
            return {
                "state": (STATE_NAMES[self.sm] if self.auto else "MANUAL/IDLE"),
                "ai_class": self.ai_class,
                "ai_conf": self.ai_conf,
                "region": self.region,
                "c_bottle": self.counts[0],
                "c_wrap": self.counts[1],
                "c_org": self.counts[2],
                "c_other": self.counts[3],
                "attempts": self.attempts,
                "confirmed": self.confirmed,
                "basket_state": self.basket,
                "basket_fill": self.fill,
                "obs_l": self.obs_l,
                "obs_r": self.obs_r,
                "lat": round(self.lat, 6),
                "lon": round(self.lon, 6),
                "gps_fix": has_fix,
                "sats": (9 if has_fix else 0),
                "conveyor": self.conveyor,
                "fault": "FAULT" if self.sm == 10 else "",
                "battery": -1,
                "wifi_on": True,
                "cpu_temp": round(t, 1),
                "thermal": self.thermal_level(t),
            }

    # ---- image ---------------------------------------------------------
    def frame_jpeg(self):
        w, h = 320, 240
        img = Image.new("RGB", (w, h))
        d = ImageDraw.Draw(img)
        grad = int(60 + 30 * math.sin(time.time() / 5.0))
        for y in range(h):
            k = y / h
            d.line([(0, y), (w, y)], fill=(int(20 + 40 * k), int(90 + 60 * k),
                                           int(120 + 70 * k + grad * 0.2)))
        for i in range(4):   # waves
            yy = 40 + i * 55
            d.line([(0, yy), (w, yy + random.randint(-4, 4))],
                   fill=(150, 200, 230), width=1)
        debris = [(70, 90, (240, 240, 250)), (190, 150, (250, 200, 60)),
                  (130, 190, (90, 160, 80)), (250, 70, (230, 230, 220))]
        now = time.time()
        for i, (x, y, col) in enumerate(debris):
            dx = x + int(12 * math.sin(now / 3 + i))
            d.ellipse([dx - 9, y - 6, dx + 9, y + 6], fill=col)
        state = STATE_NAMES[self.sm] if self.auto else "MANUAL/IDLE"
        t = self.cpu_temp(now)
        d.rectangle([0, 0, w, 34], fill=(0, 0, 0))
        d.text((6, 4), "SIMULATED FEED - no camera attached", fill=(255, 200, 80))
        d.text((6, 18), f"state={state}  temp={t:.0f}C  conf={self.ai_conf:.2f}",
               fill=(220, 240, 255))
        if self.thermal_level(t) > 0:
            d.text((w - 120, h - 18),
                   "THROTTLED" if self.thermal_level(t) == 1 else "AI PAUSED",
                   fill=(255, 120, 80))
        buf = io.BytesIO()
        img.save(buf, "JPEG", quality=70)
        return buf.getvalue()


SIM = None
INDEX_HTML = ""


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        pass

    def _send(self, code, ctype, body):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        p = self.path
        if p == "/":
            self._send(200, "text/html", INDEX_HTML.encode())
        elif p == "/status.json":
            body = json.dumps(SIM.status(), separators=(",", ":")).encode()
            self._send(200, "application/json", body)
        elif p.startswith("/cmd/"):
            op = p[5:]
            if op == "start":
                SIM.start_auto()
            elif op == "stop":
                SIM.stop_auto()
            elif op == "conveyor":
                with SIM.lock:
                    SIM.conveyor = "RUN"
                    SIM.conveyor_off_at = time.time() + 4.0
            elif op == "reset":
                with SIM.lock:
                    SIM.counts = [0, 0, 0, 0]
                    SIM.attempts = SIM.confirmed = SIM.fill = 0
                    SIM.basket = "EMPTY"
            elif op in ("fwd", "rev", "left", "right", "stopm"):
                with SIM.lock:
                    SIM.auto = False
            self._send(200, "text/plain", b"ok")
        elif p == "/stream":
            self.send_response(200)
            self.send_header("Content-Type",
                             "multipart/x-mixed-replace; boundary=frame")
            self.end_headers()
            try:
                while True:
                    jpg = SIM.frame_jpeg()
                    self.wfile.write(
                        b"--frame\r\nContent-Type: image/jpeg\r\n"
                        b"Content-Length: " + str(len(jpg)).encode() +
                        b"\r\n\r\n" + jpg + b"\r\n")
                    self.wfile.flush()
                    time.sleep(0.1)
            except (BrokenPipeError, ConnectionResetError):
                return
        else:
            self._send(404, "text/plain", b"not found")


def main():
    global SIM, INDEX_HTML
    ap = argparse.ArgumentParser(description="River Bot dashboard simulator")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8080)
    args = ap.parse_args()

    INDEX_HTML = load_index_html()
    SIM = Sim(load_class_names())

    def loop():
        while True:
            SIM.tick()
            time.sleep(0.2)

    threading.Thread(target=loop, daemon=True).start()
    srv = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"River Bot dashboard SIMULATOR (no ESP32) -> "
          f"http://{args.host}:{args.port}/")
    print("Same UI as the device; data is simulated. Ctrl+C to stop.")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
