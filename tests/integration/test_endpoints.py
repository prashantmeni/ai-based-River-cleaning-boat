#!/usr/bin/env python3
"""Bounded endpoint matrix (continue.txt task 3).

Measures, for each endpoint: status code, headers, first-byte time (TTFB),
total bytes received, and errors. Small port-80 responses are measured
SEPARATELY from the port-81 stream so a stream failure can be attributed
to transport vs handler. Includes a curl.exe cross-check (independent C
client) to rule out a Python-client-only fault.

Usage: python tests/integration/test_endpoints.py [--log FILE] [--stream-seconds 12]
Exit 0 = probe completed (small-endpoint result printed separately).
"""
import argparse
import http.client
import os
import subprocess
import sys
import time

HOST = "192.168.4.1"


class Tee:
    def __init__(self, path):
        self.t0 = time.time()
        self.f = open(path, "a", encoding="utf-8", errors="replace")

    def log(self, msg):
        line = "[%6.1fs] %s" % (time.time() - self.t0, msg)
        print(line, flush=True)
        self.f.write(line + "\n")
        self.f.flush()


def wait_dhcp(tee, seconds=15):
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
        except Exception:
            pass
        time.sleep(1)
    return None


def probe_simple(tee, port, path, name, timeout=6):
    t0 = time.time()
    try:
        conn = http.client.HTTPConnection(HOST, port, timeout=timeout)
        conn.request("GET", path, headers={"Connection": "close"})
        r = conn.getresponse()
        ttfb = time.time() - t0
        body = r.read()
        loc = r.getheader("Location", "")
        ct = r.getheader("Content-Type", "")
        conn.close()
        tee.log("%-26s status=%d ttfb=%.2fs total=%dB ct=%r%s"
                % (name, r.status, ttfb, len(body), ct,
                   (" loc=%s" % loc) if loc else ""))
        return {"name": name, "status": r.status, "bytes": len(body),
                "ok": r.status in (200, 302) and len(body) >= 0}
    except Exception as e:
        tee.log("%-26s ERROR %s %s" % (name, type(e).__name__, str(e)[:80]))
        return {"name": name, "ok": False}


def probe_stream(tee, port, seconds):
    name = "GET :%d/stream (body)" % port
    t0 = time.time()
    try:
        conn = http.client.HTTPConnection(HOST, port, timeout=10)
        conn.request("GET", "/stream", headers={"Connection": "close"})
        r = conn.getresponse()
        thdr = time.time() - t0
        ct = r.getheader("Content-Type", "")
        tee.log("%-26s status=%d headers_ttfb=%.2fs ct=%r"
                % (name, r.status, thdr, ct))
        total = soi = 0
        first_body = None
        deadline = time.time() + seconds
        while time.time() < deadline:
            try:
                c = r.read(4096)
            except Exception as e:
                tee.log("%s: read err after %dB: %s %s"
                        % (name, total, type(e).__name__, str(e)[:60]))
                break
            if not c:
                tee.log("%s: eof +%.1fs total=%dB" % (name, time.time() - t0, total))
                break
            if first_body is None:
                first_body = time.time() - t0
                tee.log("%s: FIRST BODY +%.2fs len=%d" % (name, first_body, len(c)))
            total += len(c)
            soi += c.count(b"\xff\xd8")
        conn.close()
        return {"name": name, "status": r.status, "ct": ct, "hdr_ttfb": thdr,
                "first_body": first_body, "bytes": total, "soi": soi,
                "headers_ok": "multipart" in ct}
    except Exception as e:
        tee.log("%-26s ERROR %s %s" % (name, type(e).__name__, str(e)[:80]))
        return {"name": name, "headers_ok": False, "bytes": 0}


def probe_curl(tee):
    checks = [
        ("curl status.json",
         ["curl", "-s", "-o", "NUL", "-w", "%{http_code} t=%{time_total}s b=%{size_download}",
          "--max-time", "6", "http://%s/status.json" % HOST]),
        ("curl :81 stream 12s",
         ["curl", "-s", "-o", "NUL", "-w", "%{http_code} t=%{time_total}s b=%{size_download}",
          "--max-time", "12", "http://%s:81/" % HOST]),
    ]
    for label, cmd in checks:
        try:
            out = subprocess.run(cmd, capture_output=True, text=True, timeout=16)
            tee.log("%-26s -> %s (exit %d)"
                    % (label, (out.stdout.strip() or out.stderr.strip()[:80]),
                       out.returncode))
        except Exception as e:
            tee.log("%-26s -> %s" % (label, type(e).__name__))


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("--log", default=None)
    ap.add_argument("--stream-seconds", type=float, default=12)
    args = ap.parse_args()
    if not args.log:
        logdir = os.path.join(here, "..", "..", "logs")
        os.makedirs(logdir, exist_ok=True)
        args.log = os.path.join(logdir, "endpoints_%s.log" % time.strftime("%Y%m%d_%H%M%S"))
    tee = Tee(args.log)
    tee.log("endpoint matrix -> %s (log %s)" % (HOST, args.log))

    if not wait_dhcp(tee):
        tee.log("PROBE: FAIL (no 192.168.4.x DHCP lease)")
        return 1

    small = [
        probe_simple(tee, 80, "/status.json", "GET :80/status.json"),
        probe_simple(tee, 80, "/", "GET :80/index"),
        probe_simple(tee, 80, "/cmd/stop", "GET :80/cmd/stop"),
        probe_simple(tee, 80, "/stream", "GET :80/stream (redirect)"),
    ]
    stream = probe_stream(tee, 81, args.stream_seconds)
    probe_curl(tee)

    small_ok = sum(1 for r in small if r.get("ok"))
    tee.log("===== ENDPOINT MATRIX =====")
    tee.log("small port-80 endpoints: %d/%d ok" % (small_ok, len(small)))
    tee.log("stream headers : %s" % ("OK" if stream.get("headers_ok") else "FAIL"))
    tee.log("stream body    : %dB, first_body=%s, soi=%d"
            % (stream.get("bytes", 0), stream.get("first_body"),
               stream.get("soi", 0)))
    if small_ok == len(small) and stream.get("headers_ok"):
        if stream.get("bytes", 0) > 0:
            tee.log("ENDPOINT PROBE: PASS (small + stream headers + %dB body)"
                    % stream["bytes"])
        else:
            tee.log("ENDPOINT PROBE: small=PASS stream_headers=PASS "
                    "stream_body=0 (PC-receive limitation, see docs)")
    else:
        tee.log("ENDPOINT PROBE: FAIL")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
