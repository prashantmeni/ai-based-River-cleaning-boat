#!/usr/bin/env python3
"""
Serial-to-HTTP telemetry bridge for the AI-Based River Cleaning Boat.

Reads JSON telemetry lines from the main ESP32 serial port and exposes
them over an HTTP server for the browser dashboard. Also accepts manual
commands and writes them to the serial port.
"""

import os
import argparse
import json
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import serial

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
            "left_pwm": 0,
            "right_pwm": 0,
            "last_update": 0,
        }
        self._port_name = ""
        self._baud = 115200
        self.ser = None  # shared handle owned by the reader thread

STATE = BoatState()


def serial_reader_loop(port, baud, stop_event):
    while not stop_event.is_set():
        try:
            ser = serial.Serial(port, baud, timeout=1.0)
            STATE.ser = ser
            while not stop_event.is_set():
                line = ser.readline()
                if line:
                    try:
                        text = line.decode('utf-8', errors='ignore').strip()
                        if text.startswith('{') and text.endswith('}'):
                            data = json.loads(text)
                            with STATE.lock:
                                STATE.telemetry.update(data)
                                STATE.telemetry["last_update"] = time.time()
                    except json.JSONDecodeError:
                        pass
            STATE.ser = None
            ser.close()
        except serial.SerialException:
            time.sleep(1.0)


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
                    (time.time() - STATE.telemetry["last_update"] < 3.0)
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
            # Send command line to ESP32 main controller via the reader's
            # already-open handle (Windows allows only one open per port).
            cmd_line = f"CMD:{cmd},{value},{duration}\n"
            ser = STATE.ser
            if ser is None or not ser.is_open:
                self._send(503, json.dumps({"error": "serial port not open"}))
                return
            try:
                ser.write(cmd_line.encode())
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
