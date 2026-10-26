"""Host tests for the dual-controller UART protocol (p2.txt Phase 3/8).

Reference implementation of the parsing/framing rules in
shared_protocol/rb_protocol/protocol.cpp (no host C++ compiler available on
this machine; the C implementation is validated on-target per
docs/testing_plan.md).  These tests pin the spec: header constants,
config.h <-> protocol.h consistency, validation accept/reject tables.
"""
import json
import os
import re
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PROTO_H = os.path.join(ROOT, "shared_protocol", "rb_protocol", "protocol.h")
PROTO_CPP = os.path.join(ROOT, "shared_protocol", "rb_protocol", "protocol.cpp")
CAM_CONFIG = os.path.join(ROOT, "firmware", "include", "config.h")
MAIN_CONFIG = os.path.join(ROOT, "firmware", "esp32_main", "include", "config_main.h")

RB_MAX_LINE = 256
RB_MAX_MSG_LEN = 255
RB_CLASS_COUNT = 5
SPEC_CLASS_NAMES = [
    "plastic_bottle",
    "plastic_wrapper",
    "leaf_or_organic_debris",
    "other_visible_waste",
    "background_or_no_waste",
]
SPEC_ACTIONS = ["auto_on", "auto_off", "fwd", "rev", "left", "right",
                "stopm", "conveyor", "reset", "clear"]
SPEC_TIMEOUTS = {"RB_LINK_TIMEOUT_MS": 3000, "RB_CAM_LINK_TIMEOUT_MS": 10000,
                 "RB_DET_STALE_MS": 4000, "RB_PROTO_V": 1}


def read(path):
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


# ---------------------------------------------------------------- framer
class Framer:
    """Mirrors rb_framer_feed()."""

    def __init__(self):
        self.buf = ""
        self.len = 0
        self.dropping = False

    def feed(self, c):
        if c == "\r":
            return 0
        if c == "\n":
            if self.dropping:
                self.dropping = False
                self.len = 0
                self.buf = ""
                return 0
            if self.len == 0:
                return 0
            line = self.buf
            self.buf = ""
            self.len = 0
            return 1, line
        if self.dropping:
            return 0
        if self.len >= RB_MAX_MSG_LEN:
            self.dropping = True
            return -1
        self.buf += c
        self.len += 1
        return 0


def feed_lines(text):
    """Feed a chunk; return (completed_lines, oversize_count)."""
    f = Framer()
    out, over = [], 0
    for c in text:
        r = f.feed(c)
        if r == -1:
            over += 1
        elif isinstance(r, tuple):
            out.append(r[1])
    return out, over


# ---------------------------------------------------------------- parser
def is_num(v):
    return isinstance(v, (int, float)) and not isinstance(v, bool)


def is_int(v):
    return isinstance(v, int) and not isinstance(v, bool)


def parse_msg(line, expect):
    """expect: 'hb_cam'|'det'|'fault'|'hb_main'|'state'|'cmd'.
    Returns (status, dict) with status in
    ok|bad_json|bad_version|bad_type|bad_field|dup  (mirrors rb_status_t)."""
    try:
        doc = json.loads(line)
    except Exception:
        return "bad_json", None
    if not isinstance(doc, dict):
        return "bad_json", None
    if not is_int(doc.get("v")) or doc["v"] != 1:
        return "bad_version", None
    typ = {"hb_cam": "hb", "hb_main": "hb", "det": "det", "fault": "fault",
           "state": "state", "cmd": "cmd"}[expect]
    if doc.get("type") != typ:
        return "bad_type", None
    if not is_int(doc.get("seq")):
        return "bad_field", None

    def rng(key, lo, hi, conv=int):
        v = doc.get(key)
        if conv is int and not is_int(v):
            return None
        if conv is float and not is_num(v):
            return None
        if v is None or not (lo <= v <= hi):
            return None
        return v

    if expect in ("hb_cam", "hb_main"):
        keys = ["cam", "ai", "th"] if expect == "hb_cam" else ["estop"]
        for k in keys:
            if k in doc and rng(k, 0, 2 if k == "th" else 1) is None:
                return "bad_field", None
        if expect == "hb_cam":
            if rng("temp", -40, 120, float) is None:
                return "bad_field", None
            if rng("ms", 0, 2**32 - 1) is None or rng("cam", 0, 1) is None \
                    or rng("ai", 0, 1) is None:
                return "bad_field", None
        else:
            st = doc.get("state")
            if not isinstance(st, str) or len(st) >= 24:
                return "bad_field", None
            if rng("ms", 0, 2**32 - 1) is None or rng("estop", 0, 1) is None:
                return "bad_field", None
        return "ok", doc
    if expect == "det":
        cls = rng("cls", 0, RB_CLASS_COUNT - 1)
        conf = rng("conf", 0.0, 1.0, float)
        dirv = rng("dir", -1, 1)
        if cls is None or conf is None or dirv is None:
            return "bad_field", None
        if not isinstance(doc.get("det"), bool):
            return "bad_field", None
        name = doc.get("name")
        if not isinstance(name, str) or name != SPEC_CLASS_NAMES[cls]:
            return "bad_field", None
        return "ok", doc
    if expect == "fault":
        if rng("cam", 0, 1) is None or rng("ai", 0, 1) is None:
            return "bad_field", None
        msg = doc.get("msg")
        if not isinstance(msg, str) or len(msg) >= 24:
            return "bad_field", None
        if not re.fullmatch(r"[a-z0-9_]*", msg):
            return "bad_field", None
        return "ok", doc
    if expect == "state":
        for k in ("st", "basket", "fault"):
            v = doc.get(k)
            lim = 24
            if not isinstance(v, str) or len(v) >= lim:
                return "bad_field", None
        checks = [("fill", 0, 100, float), ("obs_l", -1, 400, float),
                  ("obs_r", -1, 400, float), ("lat", -90, 90, float),
                  ("lon", -180, 180, float), ("fix", 0, 1, int),
                  ("sats", 0, 31, int), ("conv", 0, 3, int),
                  ("att", 0, 1000000, int), ("cfm", 0, 1000000, int)]
        for k, lo, hi, conv in checks:
            if rng(k, lo, hi, conv) is None:
                return "bad_field", None
        return "ok", doc
    if expect == "cmd":
        a = doc.get("action")
        if not isinstance(a, str) or a not in SPEC_ACTIONS:
            return "bad_field", None
        return "ok", doc
    raise AssertionError(expect)


class SeqGate:
    def __init__(self):
        self.last = {}

    def accept(self, kind, seq):
        if self.last.get(kind) == seq:
            return False
        self.last[kind] = seq
        return True


# ---------------------------------------------------------------- fixtures
def det_msg(cls=0, conf=0.85, dirv=-1, det=True, seq=42, v=1):
    return {"v": v, "type": "det", "cls": cls, "name": SPEC_CLASS_NAMES[cls],
            "conf": conf, "dir": dirv, "det": det, "seq": seq}


def state_msg(**kw):
    base = {"v": 1, "type": "state", "st": "SEARCH", "basket": "FILLING",
            "fill": 42.5, "obs_l": 55.0, "obs_r": -1.0, "lat": 13.08,
            "lon": 77.55, "fix": 1, "sats": 7, "conv": 0, "att": 3,
            "cfm": 0, "fault": "", "seq": 8}
    base.update(kw)
    return base


class TestHeaderConsistency(unittest.TestCase):
    def test_header_constants(self):
        h = read(PROTO_H)
        for name, val in SPEC_TIMEOUTS.items():
            m = re.search(r"#define\s+%s\s+(\d+)" % name, h)
            self.assertTrue(m, "missing %s in protocol.h" % name)
            self.assertEqual(int(m.group(1)), val, name)

    def test_header_class_names_match_spec(self):
        h = read(PROTO_H)
        m = re.search(r"RB_CLASS_COUNT\s+(\d+)", h)
        self.assertEqual(int(m.group(1)), RB_CLASS_COUNT)
        c = read(PROTO_CPP)
        block = c.split("RB_CLASS_NAMES[RB_CLASS_COUNT] = {")[1].split("};")[0]
        names = re.findall(r'"([a-z_]+)"', block)
        self.assertEqual(names, SPEC_CLASS_NAMES)

    def test_cam_config_consistent_with_protocol(self):
        cfg = read(CAM_CONFIG)
        m = re.search(r"CLASS_NAMES\[NUM_CLASSES\]\s*=\s*\{(.*?)\};", cfg, re.S)
        names = re.findall(r'"([a-z_]+)"', m.group(1))
        self.assertEqual(names, SPEC_CLASS_NAMES, "config.h classes drifted")
        t = re.search(r"DETECT_CONF_THRESHOLD\s+([0-9.]+)f", cfg)
        self.assertEqual(float(t.group(1)), 0.55)

    def test_main_config_threshold_matches_cam(self):
        cam = re.search(r"DETECT_CONF_THRESHOLD\s+([0-9.]+)f", read(CAM_CONFIG))
        main = re.search(r"DETECT_CONF_THRESHOLD\s+([0-9.]+)f", read(MAIN_CONFIG))
        self.assertTrue(main, "config_main.h missing DETECT_CONF_THRESHOLD")
        self.assertEqual(float(main.group(1)), float(cam.group(1)),
                         "threshold drift between CAM and main configs")

    def test_text_max_fits_longest_state_name(self):
        h = read(PROTO_H)
        m = re.search(r"#define\s+RB_TEXT_MAX\s+(\d+)", h)
        self.assertTrue(m, "missing RB_TEXT_MAX")
        longest = max(len("VERIFY_COLLECTION"),
                      len("RETURN_OR_STOP"),
                      len("SELF_TEST"),
                      len("FAULT_STOP"))
        self.assertGreaterEqual(int(m.group(1)), longest + 1,
                                "RB_TEXT_MAX truncates a state name")

    def test_cpp_and_header_agree_on_limits(self):
        h = read(PROTO_H)
        self.assertTrue(re.search(r"#define\s+RB_MAX_LINE\s+256", h))
        self.assertTrue(re.search(r"#define\s+RB_MAX_MSG_LEN\s+255", h))
        cpp = read(PROTO_CPP)
        for a in SPEC_ACTIONS:
            self.assertIn('"%s"' % a, cpp)


class TestFraming(unittest.TestCase):
    def test_valid_line_roundtrip(self):
        line = json.dumps(det_msg())
        lines, over = feed_lines(line + "\n")
        self.assertEqual(lines, [line])
        self.assertEqual(over, 0)

    def test_crlf_tolerated(self):
        line = json.dumps(det_msg())
        lines, over = feed_lines(line + "\r\n")
        self.assertEqual(lines, [line])

    def test_empty_line_skipped(self):
        lines, over = feed_lines("\n\n")
        self.assertEqual(lines, [])

    def test_oversize_discarded_once(self):
        big = "x" * 300
        lines, over = feed_lines(big + "\n")
        self.assertEqual(lines, [])
        self.assertEqual(over, 1)

    def test_exact_max_len_ok(self):
        line = "a" * RB_MAX_MSG_LEN
        lines, over = feed_lines(line + "\n")
        self.assertEqual(lines, [line])

    def test_byte_at_max_plus_one_overflows(self):
        f = Framer()
        for _ in range(RB_MAX_MSG_LEN):
            self.assertEqual(f.feed("a"), 0)
        self.assertEqual(f.feed("a"), -1)
        self.assertEqual(f.feed("\n"), 0)
        self.assertEqual(f.feed("b"), 0)
        self.assertEqual(f.feed("\n"), (1, "b"))


class TestDetValidation(unittest.TestCase):
    def p(self, obj):
        return parse_msg(json.dumps(obj), "det")

    def test_valid_det(self):
        st, doc = self.p(det_msg())
        self.assertEqual(st, "ok")

    def test_valid_every_class(self):
        for c in range(RB_CLASS_COUNT):
            st, _ = self.p(det_msg(cls=c))
            self.assertEqual(st, "ok", c)

    def test_bad_json(self):
        self.assertEqual(parse_msg("{nope", "det")[0], "bad_json")
        self.assertEqual(parse_msg("", "det")[0], "bad_json")

    def test_wrong_version(self):
        self.assertEqual(self.p(det_msg(v=2))[0], "bad_version")

    def test_unknown_type(self):
        d = det_msg()
        d["type"] = "telemetry"
        self.assertEqual(parse_msg(json.dumps(d), "det")[0], "bad_type")

    def test_missing_type(self):
        d = det_msg()
        del d["type"]
        self.assertEqual(parse_msg(json.dumps(d), "det")[0], "bad_type")

    def test_cls_out_of_range(self):
        self.assertEqual(self.p(det_msg(cls=4))[0], "ok")
        d = det_msg()
        d["cls"] = 5
        d["name"] = "x"
        self.assertEqual(self.p(d)[0], "bad_field")
        d["cls"] = -1
        self.assertEqual(self.p(d)[0], "bad_field")

    def test_conf_out_of_range(self):
        d = det_msg(conf=1.5)
        self.assertEqual(self.p(d)[0], "bad_field")
        d = det_msg(conf=-0.1)
        self.assertEqual(self.p(d)[0], "bad_field")
        self.assertEqual(self.p(det_msg(conf=0.0))[0], "ok")
        self.assertEqual(self.p(det_msg(conf=1.0))[0], "ok")

    def test_conf_not_number(self):
        d = det_msg()
        d["conf"] = "0.9"
        self.assertEqual(self.p(d)[0], "bad_field")
        d["conf"] = True
        self.assertEqual(self.p(d)[0], "bad_field")

    def test_dir_out_of_range(self):
        self.assertEqual(self.p(det_msg(dirv=2))[0], "bad_field")
        self.assertEqual(self.p(det_msg(dirv=-2))[0], "bad_field")

    def test_det_must_be_bool(self):
        d = det_msg()
        d["det"] = 1
        self.assertEqual(self.p(d)[0], "bad_field")

    def test_name_class_mismatch(self):
        d = det_msg(cls=1)
        d["name"] = "plastic_bottle"
        self.assertEqual(self.p(d)[0], "bad_field")

    def test_name_missing(self):
        d = det_msg()
        del d["name"]
        self.assertEqual(self.p(d)[0], "bad_field")

    def test_extra_keys_tolerated(self):
        d = det_msg()
        d["extra_future_key"] = 123
        self.assertEqual(self.p(d)[0], "ok")

    def test_seq_required_int(self):
        d = det_msg()
        d["seq"] = "42"
        self.assertEqual(self.p(d)[0], "bad_field")
        d["seq"] = 42.5
        self.assertEqual(self.p(d)[0], "bad_field")


class TestStateValidation(unittest.TestCase):
    def p(self, obj):
        return parse_msg(json.dumps(obj), "state")

    def test_valid(self):
        self.assertEqual(self.p(state_msg())[0], "ok")

    def test_fill_bounds(self):
        self.assertEqual(self.p(state_msg(fill=101))[0], "bad_field")
        self.assertEqual(self.p(state_msg(fill=-0.5))[0], "bad_field")

    def test_obs_invalid_sentinel_ok(self):
        self.assertEqual(self.p(state_msg(obs_l=-1))[0], "ok")
        self.assertEqual(self.p(state_msg(obs_l=-1.5))[0], "bad_field")

    def test_lat_lon_bounds(self):
        self.assertEqual(self.p(state_msg(lat=91))[0], "bad_field")
        self.assertEqual(self.p(state_msg(lon=181))[0], "bad_field")

    def test_sats_conv_bounds(self):
        self.assertEqual(self.p(state_msg(sats=32))[0], "bad_field")
        self.assertEqual(self.p(state_msg(conv=4))[0], "bad_field")

    def test_stale_fields_empty_ok(self):
        self.assertEqual(self.p(state_msg(fault=""))[0], "ok")


class TestCmdAndFault(unittest.TestCase):
    def test_all_actions_allowed(self):
        for a in SPEC_ACTIONS:
            d = {"v": 1, "type": "cmd", "action": a, "seq": 1}
            self.assertEqual(parse_msg(json.dumps(d), "cmd")[0], "ok", a)

    def test_bad_action(self):
        d = {"v": 1, "type": "cmd", "action": "launch", "seq": 1}
        self.assertEqual(parse_msg(json.dumps(d), "cmd")[0], "bad_field")

    def test_fault_charset(self):
        d = {"v": 1, "type": "fault", "cam": 0, "ai": 1,
             "msg": "camera_init_failed", "seq": 1}
        self.assertEqual(parse_msg(json.dumps(d), "fault")[0], "ok")
        d["msg"] = "CAMERA FAILED"
        self.assertEqual(parse_msg(json.dumps(d), "fault")[0], "bad_field")
        d["msg"] = "x" * 24
        self.assertEqual(parse_msg(json.dumps(d), "fault")[0], "bad_field")


class TestSeqGate(unittest.TestCase):
    def test_dup_rejected_once(self):
        g = SeqGate()
        self.assertTrue(g.accept("det", 42))
        self.assertFalse(g.accept("det", 42))
        self.assertTrue(g.accept("det", 43))
        self.assertTrue(g.accept("state", 42), "different type independent")

    def test_reboot_seq_reset_tolerated(self):
        g = SeqGate()
        self.assertTrue(g.accept("hb", 900))
        self.assertTrue(g.accept("hb", 1), "receiver reboot: lower seq ok")


class TestStalenessRule(unittest.TestCase):
    def test_4s_window(self):
        stale_after = SPEC_TIMEOUTS["RB_DET_STALE_MS"]
        recv_times = [0, 1000, 3999, 4001]
        fresh = [t - 0 <= stale_after for t in recv_times]
        self.assertEqual(fresh, [True, True, True, False])

    def test_link_timeout_ordering(self):
        self.assertLess(SPEC_TIMEOUTS["RB_LINK_TIMEOUT_MS"],
                        SPEC_TIMEOUTS["RB_CAM_LINK_TIMEOUT_MS"])
        self.assertGreater(SPEC_TIMEOUTS["RB_DET_STALE_MS"],
                           SPEC_TIMEOUTS["RB_LINK_TIMEOUT_MS"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
