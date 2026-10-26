#!/usr/bin/env python3
"""Continuous MJPEG stream test (PROMPTS\\test.txt steps 4-5).

Waits for the 192.168.4.x DHCP lease, then reads /stream continuously,
reconnecting whenever the device or the link drops the socket, until
--seconds elapse. Every event is printed AND appended to --log.

Camera PASS = at least one JPEG frame marker (SOI FF D8) delivered to
this PC's client. Internet/API is NOT tested here (the orchestrator
reports that separately, per test.txt step 9).

Usage:
    python tests/integration/test_stream.py [--seconds 60] [--log FILE]
"""
import argparse
import os
import subprocess
import sys
import time
import urllib.request

SOI = b"\xff\xd8"
EOI = b"\xff\xd9"


class Tee:
    """Log to stdout and a local file at once."""

    def __init__(self, path):
        self.t0 = time.time()
        self.path = path
        self.f = open(path, "a", encoding="utf-8", errors="replace")

    def log(self, msg):
        line = "[%6.1fs] %s" % (time.time() - self.t0, msg)
        print(line, flush=True)
        self.f.write(line + "\n")
        self.f.flush()


def wait_dhcp(tee, seconds=15):
    """test.txt step 4: wait until Windows holds a 192.168.4.x address."""
    for i in range(seconds):
        try:
            out = subprocess.run(
                ["powershell", "-NoProfile", "-Command",
                 "(Get-NetIPAddress -AddressFamily IPv4 -InterfaceAlias 'Wi-Fi' "
                 "-ErrorAction SilentlyContinue | Where-Object {"
                 "$_.IPAddress -like '192.168.4.*'}).IPAddress"],
                capture_output=True, text=True, timeout=8).stdout.strip()
            if out:
                tee.log("dhcp lease %s after %ds" % (out.splitlines()[0], i + 1))
                return out.splitlines()[0]
        except Exception as e:
            tee.log("dhcp poll error: %s" % type(e).__name__)
        time.sleep(1)
    return None


def stream_once(tee, base, deadline):
    """One /stream connection; returns stats dict or None on connect fail."""
    url = base.rstrip("/") + "/stream"
    t_attempt = time.time()
    try:
        r = urllib.request.urlopen(url, timeout=10)
    except Exception as e:
        tee.log("connect failed: %s %s" % (type(e).__name__, str(e)[:80]))
        return None

    ctype = r.headers.get("Content-Type", "")
    tee.log("headers +%.1fs url=%s ct=%r"
            % (time.time() - t_attempt, r.geturl(), ctype))
    got_headers = "multipart" in ctype
    total = soi = eoi = 0
    try:
        while time.time() < deadline:
            try:
                c = r.read(4096)
            except Exception as e:
                tee.log("read err total=%d: %s %s"
                        % (total, type(e).__name__, str(e)[:80]))
                break
            if not c:
                tee.log("eof +%.1fs total=%d" % (time.time() - t_attempt, total))
                break
            total += len(c)
            s, e = c.count(SOI), c.count(EOI)
            soi += s
            eoi += e
            tee.log("chunk +%.2fs len=%5d total=%7d soi+=%d eoi+=%d"
                    % (time.time() - t_attempt, len(c), total, s, e))
    finally:
        try:
            r.close()
        except Exception:
            pass
    return {"headers": got_headers, "bytes": total, "soi": soi, "eoi": eoi}


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default="http://192.168.4.1")
    ap.add_argument("--seconds", type=float, default=60)
    ap.add_argument("--log", default=None)
    args = ap.parse_args()

    if not args.log:
        logdir = os.path.join(here, "..", "..", "logs")
        os.makedirs(logdir, exist_ok=True)
        args.log = os.path.join(
            logdir, "stream_test_%s.log" % time.strftime("%Y%m%d_%H%M%S"))
    tee = Tee(args.log)
    tee.log("continuous stream test -> %s for %.0fs (log %s)"
            % (args.base, args.seconds, args.log))

    if not wait_dhcp(tee):
        tee.log("CAMERA STREAM: FAIL (no 192.168.4.x DHCP lease)")
        return 1

    deadline = time.time() + args.seconds
    attempts = hdr_ok = total_b = soi = eoi = 0
    while time.time() < deadline:
        attempts += 1
        tee.log("---- attempt %d ----" % attempts)
        try:
            st = stream_once(tee, args.base, deadline)
        except Exception as e:
            tee.log("attempt error: %s %s" % (type(e).__name__, str(e)[:80]))
            st = None
        if st:
            if st["headers"]:
                hdr_ok += 1
            total_b += st["bytes"]
            soi += st["soi"]
            eoi += st["eoi"]
        if time.time() < deadline:
            time.sleep(1)

    tee.log("===== SUMMARY =====")
    tee.log("attempts=%d multipart_headers=%d body_bytes=%d soi=%d eoi=%d"
            % (attempts, hdr_ok, total_b, soi, eoi))
    if soi >= 1:
        tee.log("CAMERA STREAM: PASS (%d jpeg SOI marker(s), %d body bytes)"
                % (soi, total_b))
        return 0
    if hdr_ok and total_b == 0:
        tee.log("CAMERA STREAM: FAIL (multipart headers reached this PC on "
                "%d attempt(s) but 0 body bytes delivered - check serial "
                "for [stream] send diagnostics)" % hdr_ok)
        return 1
    tee.log("CAMERA STREAM: FAIL (no multipart headers, no data)")
    return 1


if __name__ == "__main__":
    sys.exit(main())
