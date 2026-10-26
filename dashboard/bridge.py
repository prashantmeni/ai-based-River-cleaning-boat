#!/usr/bin/env python3
"""
Serial-to-HTTP telemetry bridge for the AI-Based River Cleaning Boat.

Reads framed protocol bytes from the main ESP32 serial port, decodes them,
and exposes telemetry over a small HTTP server for the browser dashboard.
Also accepts dashboard manual commands and forwards them as DIAG_COMMAND
frames to the ESP32.

Usage:
    pip install pyserial
    python bridge.py --port COM3 --baud 115200 --http-port 8080

Then open http://localhost:8080 in a browser.

No cloud, no paid API. Works entirely on localhost.
"""

import os
import argparse
import json
import threading
import time
import struct
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import serial

START = 0x7E
END = 0x7F
VERSION = 0x01

TYPE_NAMES = {
    0x01: "AI_DETECTION",
    0x02: "AI_NO_DETECTION",
    0x03: "AI_HEALTH",
    0x10: "MAIN_STATUS",
    0x20: "DIAG_COMMAND",
    0x30: "FAULT_REPORT",
}

NAV_STATE_NAMES = {
    0: "BOOT", 1: "SELF_TEST", 2: "IDLE", 3: "SEARCH",
    4: "TARGET_DETECTED", 5: "ALIGN", 6: "APPROACH", 7: "COLLECT",
    8: "VERIFY_COLLECT", 9: "TARGET_LOST", 10: "OBSTACLE_AVOID",
    11: "FAULT", 12: "EMERGENCY_STOP",
}

CONVEYOR_STATE_NAMES = {
    0: "OFF", 1: "STARTING", 2: "RUNNING", 3: "STOPPING", 4: "FAULT",
}


def checksum_ok(data):
    return (sum(data) & 0xFF) == 0


class BoatState:
    def __init__(self):
        self.lock = threading.Lock()
        self.telemetry = {
            "state": "UNKNOWN",
            "comms_ok": False,
            "detection": False,
            "confidence": 0,
            "class_id": 0,
            "center_x": 0,
            "left_cm": None,
            "right_cm": None,
            "gps_fix": False,
            "latitude": None,
            "longitude": None,
            "collection_count": 0,
            "conveyor": "OFF",
            "fault_flags": 0,
            "uptime_ms": 0,
            "left_pwm": 0, "right_pwm": 0,
            "last_update": 0,
        }
        self.last_frame_time = 0
        self.error_count = 0


STATE = BoatState()


def parse_main_status(payload):
    if len(payload) < 14:
        return None
    (nav_state, ml_pwm, mr_pwm, ml_dir, mr_dir, conv_state,
     comm_health, fault_flags) = struct.unpack_from("<BBBBBBBB", payload, 0)
    collection_count, uptime_ms = struct.unpack_from("<HI", payload, 8)
    return {
        "state": NAV_STATE_NAMES.get(nav_state, f"UNKNOWN({nav_state})"),
        "comms_ok": comm_health == 0,
        "left_pwm": ml_pwm, "right_pwm": mr_pwm,
        "left_dir": ml_dir, "right_dir": mr_dir,
        "conveyor": CONVEYOR_STATE_NAMES.get(conv_state, f"UNKNOWN({conv_state})"),
        "fault_flags": fault_flags,
        "collection_count": collection_count,
        "uptime_ms": uptime_ms,
    }


def parse_ai_detection(payload):
    if len(payload) < 18:
        return None
    (class_id, confidence) = struct.unpack_from("<BB", payload, 0)
    bx, by, bw, bh, cx = struct.unpack_from("<hhhhh", payload, 2)
    timestamp_ms = struct.unpack_from("<I", payload, 12)[0]
    cam_health, inf_status = struct.unpack_from("<BB", payload, 16)
    return {
        "class_id": class_id, "confidence": confidence,
        "bbox": [bx, by, bw, bh], "center_x": cx,
        "timestamp_ms": timestamp_ms,
        "camera_health": cam_health, "inference_status": inf_status,
    }


def process_frame(frame_type, payload):
    if frame_type == 0x10:  # MAIN_STATUS
        ts = parse_main_status(payload)
        if ts:
            with STATE.lock:
                STATE.telemetry.update(ts)
                STATE.telemetry["last_update"] = time.time()
    elif frame_type == 0x01:  # AI_DETECTION
        det = parse_ai_detection(payload)
        if det:
            with STATE.lock:
                STATE.telemetry["detection"] = True
                STATE.telemetry["confidence"] = det["confidence"]
                STATE.telemetry["class_id"] = det["class_id"]
                STATE.telemetry["center_x"] = det["center_x"]
                STATE.telemetry["last_update"] = time.time()
    elif frame_type == 0x02:  # AI_NO_DETECTION
        with STATE.lock:
            STATE.telemetry["detection"] = False
            STATE.telemetry["confidence"] = 0
            STATE.telemetry["last_update"] = time.time()


class FrameReader:
    """Byte-by-byte framed protocol reader mirroring the C implementation."""
    def __init__(self, callback):
        self.cb = callback
        self.buf = bytearray()
        self.start_seen = False
        self.expected_len = 0

    def push(self, b):
        if not self.start_seen:
            if b == START:
                self.start_seen = True
                self.buf = bytearray([b])
            return
        self.buf.append(b)
        if len(self.buf) < 7:
            return
        if self.buf[1] != VERSION:
            self.start_seen = False
            return
        plen = self.buf[5] | (self.buf[6] << 8)
        if plen > 64:
            self.start_seen = False
            return
        self.expected_len = 7 + plen + 2  # overhead + payload + chk + end
        if len(self.buf) < self.expected_len:
            return
        if self.buf[self.expected_len - 1] != END:
            self.start_seen = False
            return
        ftype = self.buf[2]
        if ftype not in TYPE_NAMES:
            self.start_seen = False
            return
        payload = bytes(self.buf[7:7 + plen])
        chk = sum(self.buf[1:self.expected_len - 2]) & 0xFF
        if chk != self.buf[self.expected_len - 2]:
            self.start_seen = False
            return
        self.cb(ftype, payload)
        self.start_seen = False


def serial_reader_loop(port, baud, stop_event):
    ser = serial.Serial(port, baud, timeout=0.1)
    reader = FrameReader(process_frame)
    while not stop_event.is_set():
        try:
            data = ser.read(256)
            if data:
                for b in data:
                    reader.push(b)
        except serial.SerialException:
            time.sleep(0.5)
    ser.close()


def build_diag_frame(cmd, value, duration_ms):
    payload = struct.pack("<BBH", cmd, value, duration_ms)
    frame = bytearray()
    frame.append(START)
    frame.append(VERSION)
    frame.append(0x20)  # DIAG_COMMAND
    frame.append(0)  # sequence low (ignored)
    frame.append(0)
    frame.append(len(payload) & 0xFF)
    frame.append((len(payload) >> 8) & 0xFF)
    frame.extend(payload)
    chk = sum(frame[1:]) & 0xFF
    frame.append(chk)
    frame.append(END)
    return bytes(frame)


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        return  # quiet

    def _send(self, code, body, ctype="application/json"):
        b = body.encode() if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(b)))
        self.end_headers()
        self.wfile.write(b)

    def do_GET(self):
        if self.path == "/" or self.path == "/index.html":
            html_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "index.html")
            try:
                with open(html_path, "r", encoding="utf-8") as f:
                    self._send(200, f.read(), "text/html")
            except FileNotFoundError:
                self._send(404, "dashboard not found")
        elif self.path == "/api/telemetry":
            with STATE.lock:
                tele = dict(STATE.telemetry)
                tele["comms_alive"] = (
                    STATE.telemetry["comms_ok"]
                    and (time.time() - STATE.telemetry["last_update"] < 2.0)
                    if STATE.telemetry["last_update"] else False
                )
            self._send(200, json.dumps(tele))
        else:
            self._send(404, "not found")

    def do_POST(self):
        if self.path == "/api/command":
            length = int(self.headers.get("Content-Length", 0))
            raw = self.rfile.read(length)
            try:
                data = json.loads(raw.decode())
            except Exception:
                self._send(400, json.dumps({"error": "invalid json"}))
                return
            cmd = int(data.get("cmd", 0))
            value = int(data.get("value", 100))
            duration = int(data.get("duration_ms", 1000))
            if cmd not in (1, 2, 3, 4, 5, 6, 7, 8):
                self._send(400, json.dumps({"error": "unknown cmd"}))
                return
            frame = build_diag_frame(cmd, value, duration)
            try:
                ser = serial.Serial(STATE._port_name, STATE._baud, timeout=0.2)
                ser.write(frame)
                ser.close()
                self._send(200, json.dumps({"ok": True}))
            except Exception as e:
                self._send(500, json.dumps({"error": str(e)}))
        else:
            self._send(404, "not found")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True, help="Serial port of main ESP32")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--http-port", type=int, default=8080)
    args = ap.parse_args()

    STATE._port_name = args.port
    STATE._baud = args.baud

    stop_event = threading.Event()
    t = threading.Thread(target=serial_reader_loop,
                         args=(args.port, args.baud, stop_event), daemon=True)
    t.start()

    server = ThreadingHTTPServer(("0.0.0.0", args.http_port), Handler)
    print(f"Dashboard bridge on http://0.0.0.0:{args.http_port}")
    print(f"Serial: {args.port} @ {args.baud}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    stop_event.set()
    server.shutdown()


if __name__ == "__main__":
    main()
