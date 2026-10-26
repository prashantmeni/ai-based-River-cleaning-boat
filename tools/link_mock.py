#!/usr/bin/env python3
"""Mocked link E2E tests (p2 "mocked serial" allowance).

Injects protocol messages into each controller over its own USB console
(LINK_USB_MIRROR=1) to validate the full RX path without physical wires:

  main : hb_cam + det  -> expect rx= climb, link=0->1, det flag, link loss
  cam  : hb_main + state -> expect state=NO_LINK -> injected state name

Usage:
  python tools/link_mock.py main [port]   (default COM5)
  python tools/link_mock.py cam  [port]   (default COM3)
"""
import sys
import time

import serial

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

MODE = sys.argv[1] if len(sys.argv) > 1 else "main"
PORT = sys.argv[2] if len(sys.argv) > 2 else ("COM5" if MODE == "main" else "COM3")
INJECT_S = 10
SILENT_S = 9

state_line = (
    '{"v":1,"type":"state","st":"SEARCH","basket":"EMPTY","fill":12.5,'
    '"obs_l":-1.0,"obs_r":-1.0,"lat":0.000000,"lon":0.000000,"fix":0,'
    '"sats":0,"conv":0,"att":0,"cfm":0,"fault":"","seq":%d}\n'
)

s = serial.Serial(PORT, 115200, timeout=0.2)
s.setDTR(False)
s.setRTS(False)

t_start = time.time()
# settle: let the auto-reset/boot finish before injecting (first lines
# otherwise land in the ROM boot window and are lost)
settle_end = t_start + 3.5
while time.time() < settle_end:
    s.read(4096)
t_start = time.time()
t_end = t_start + INJECT_S + SILENT_S
next_tx = t_start
tick = 0
seq_hb = 0
seq_det = 1000
buf = b""
counts = {"hb": 0, "det": 0, "state": 0}
write_ret = []
last_hb = ""
samples = []

while time.time() < t_end:
    now = time.time()
    if now < t_start + INJECT_S and now >= next_tx:
        next_tx = now + 0.5
        if MODE == "main":
            line = (
                '{"v":1,"type":"hb","ms":%d,"cam":1,"ai":1,'
                '"temp":40.0,"th":1,"seq":%d}\n' % (tick * 1000, seq_hb)
            )
            write_ret.append(s.write(line.encode()))
            seq_hb += 1
            counts["hb"] += 1
            if counts["hb"] % 6 == 0:
                det = (
                    '{"v":1,"type":"det","cls":0,"name":"plastic_bottle",'
                    '"conf":0.870,"dir":1,"det":true,"seq":%d}\n' % seq_det
                )
                write_ret.append(s.write(det.encode()))
                seq_det += 1
                counts["det"] += 1
        else:
            hb = (
                '{"v":1,"type":"hb","ms":%d,"state":"SELF_TEST",'
                '"estop":0,"seq":%d}\n' % (tick * 1000, seq_hb)
            )
            write_ret.append(s.write(hb.encode()))
            seq_hb += 1
            counts["hb"] += 1
            write_ret.append(s.write((state_line % (seq_hb)).encode()))
            seq_hb += 1
            counts["state"] += 1
        tick += 1
    buf += s.read(4096)
    for l in buf.decode("utf-8", "replace").splitlines():
        if l.startswith("[hb]"):
            last_hb = l
    if last_hb and (not samples or time.time() - samples[-1][0] >= 2.0):
        samples.append((time.time(), last_hb))
s.close()

lines = [x for x in buf.decode("utf-8", "replace").splitlines() if x.strip()]
print("== injected: %s  port=%s ==" % (counts, PORT))
print("== write bytes total: %s ==" % write_ret)
print("== console lines=%d ==" % len(lines))
for t, l in samples:
    print("SAMPLE t=%.1f %s" % (t, l[:160]))

hb_lines = [l for l in lines if l.startswith("[hb]")]
interesting = [
    l for l in lines
    if "[link]" in l or "[det]" in l or l.startswith("[stx]")
    or "lost" in l or "up" in l and "[link]" in l
]
foreign = [
    l for l in lines
    if not l.startswith(("[boot]", "[link]", "[init]", "[motors]", "[conv]",
                         "[buzzer]", "[selftest]", "[stx]", "[hb]", "[ai]",
                         "[cam]", "[wifi]", "[lb]", "ets ", "rst:", "load:",
                         "entry", "configsip", "clk_drv", "mode:DIO",
                         "[vision]", "[dash]")) and '"v":' not in l
]

if MODE == "main":
    up = [l for l in hb_lines if " link=1" in l]
    rx_vals = [int(l.split("rx=")[1].split()[0]) for l in hb_lines if "rx=" in l]
    det_flags = sorted({l.split("det=")[1].split()[0] for l in hb_lines if "det=" in l})
    print("RESULT link=1 seen: %d hb lines" % len(up))
    if rx_vals:
        print("RESULT rx= %d -> %d" % (rx_vals[0], rx_vals[-1]))
    print("RESULT det flags: %s" % det_flags)
    for l in up[:3]:
        print("UP  :", l[:175])
else:
    states = []
    for l in hb_lines:
        if "state=" in l:
            states.append(l.split("state=")[1].split()[0])
    seen = []
    for st in states:
        if not seen or seen[-1] != st:
            seen.append(st)
    print("RESULT state transitions: %s" % seen)

for l in interesting[:10]:
    print("INT :", l[:170])
for l in foreign[:6]:
    print("OTH :", repr(l[:120]))
print("RESULT hb lines=%d first=%r last=%r" % (
    len(hb_lines), hb_lines[0][:90] if hb_lines else "",
    hb_lines[-1][:90] if hb_lines else ""))
