#!/usr/bin/env python3
"""Host-side tests for the serial protocol design (simulated, not on hardware).

These tests mirror the framing/checksum/sequence logic in
shared/protocol/protocol.h. They run on the host with Python only — no ESP32
toolchain or hardware required. Marked: HOST-SIDE / SIMULATED.
"""

import struct
import sys

START, END, VERSION = 0x7E, 0x7F, 0x01
TYPE_DET, TYPE_NODET, TYPE_HEALTH, TYPE_MAIN = 0x01, 0x02, 0x03, 0x10
MAX_PAYLOAD = 64


def checksum(data):
    return sum(data) & 0xFF


def serialize(msg_type, seq, payload):
    frame = bytearray()
    frame.append(START)
    frame.append(VERSION)
    frame.append(msg_type)
    frame.append(seq & 0xFF)
    frame.append((seq >> 8) & 0xFF)
    frame.append(len(payload) & 0xFF)
    frame.append((len(payload) >> 8) & 0xFF)
    frame.extend(payload)
    chk = sum(frame[1:]) & 0xFF
    frame.append(chk)
    frame.append(END)
    return bytes(frame)


class Rx:
    def __init__(self):
        self.buf = bytearray()
        self.start_seen = False
        self.expected = 0

    def push(self, b):
        if not self.start_seen:
            if b == START:
                self.start_seen = True
                self.buf = bytearray([b])
            return "INCOMPLETE"
        self.buf.append(b)
        if len(self.buf) < 8:
            return "INCOMPLETE"
        if self.buf[1] != VERSION:
            self.start_seen = False
            return "BAD_VERSION"
        plen = self.buf[5] | (self.buf[6] << 8)
        if plen > MAX_PAYLOAD:
            self.start_seen = False
            return "BAD_LENGTH"
        self.expected = 7 + plen + 2
        if len(self.buf) < self.expected:
            return "INCOMPLETE"
        if self.buf[self.expected - 1] != END:
            self.start_seen = False
            return "BAD_END"
        ftype = self.buf[2]
        if ftype not in (TYPE_DET, TYPE_NODET, TYPE_HEALTH, TYPE_MAIN,
                         0x20, 0x30):
            self.start_seen = False
            return "UNKNOWN_TYPE"
        payload = bytes(self.buf[7:7 + plen])
        chk = sum(self.buf[1:self.expected - 2]) & 0xFF
        if chk != self.buf[self.expected - 2]:
            self.start_seen = False
            return "BAD_CHECKSUM"
        self.start_seen = False
        return ("OK", ftype, payload)


def test_valid_detection_frame():
    payload = struct.pack("<BBhhhhhiBB", 1, 85, 100, 100, 200, 200, 0, 1234, 0, 0)
    frame = serialize(TYPE_DET, 3, payload)
    rx = Rx()
    result = None
    for b in frame:
        r = rx.push(b)
        if r != "INCOMPLETE":
            result = r
    assert result[0] == "OK", result
    assert result[1] == TYPE_DET
    assert result[2] == payload
    print("PASS test_valid_detection_frame")


def test_no_detection_frame():
    payload = struct.pack("<BI BB", 1, 5678, 0, 0)  # reason, ts, cam, inf
    frame = serialize(TYPE_NODET, 0, payload)
    rx = Rx()
    result = None
    for b in frame:
        r = rx.push(b)
        if r != "INCOMPLETE":
            result = r
    assert result[0] == "OK", result
    print("PASS test_no_detection_frame")


def test_corrupted_checksum_rejected():
    payload = b"\x01" * 5
    frame = bytearray(serialize(TYPE_DET, 1, payload))
    frame[-2] ^= 0xFF  # corrupt checksum
    rx = Rx()
    result = None
    for b in frame:
        r = rx.push(b)
        if r != "INCOMPLETE":
            result = r
    assert result == "BAD_CHECKSUM", result
    print("PASS test_corrupted_checksum_rejected")


def test_bad_length_rejected():
    payload = b"\x00" * 70  # > MAX_PAYLOAD
    frame = serialize(TYPE_DET, 1, payload)
    rx = Rx()
    result = None
    for b in frame:
        r = rx.push(b)
        if r != "INCOMPLETE":
            result = r
    assert result == "BAD_LENGTH", result
    print("PASS test_bad_length_rejected")


def test_unknown_type_rejected():
    payload = b"\x00"
    frame = serialize(0x99, 1, payload)
    rx = Rx()
    result = None
    for b in frame:
        r = rx.push(b)
        if r != "INCOMPLETE":
            result = r
    assert result == "UNKNOWN_TYPE", result
    print("PASS test_unknown_type_rejected")


def test_bad_start_byte_recovery():
    rx = Rx()
    # garbage bytes then a valid frame
    payload = b"\x02"
    frame = serialize(TYPE_NODET, 0, payload)
    stream = b"\x00\xff\x11" + bytes(frame)
    results = []
    for b in stream:
        r = rx.push(b)
        if r != "INCOMPLETE":
            results.append(r)
    assert results[0][0] == "OK", results
    print("PASS test_bad_start_byte_recovery")


def test_partial_frame_then_completion():
    payload = b"\x05\x05"
    frame = serialize(TYPE_HEALTH, 7, payload)
    rx = Rx()
    for b in frame[:5]:
        assert rx.push(b) == "INCOMPLETE"
    result = None
    for b in frame[5:]:
        r = rx.push(b)
        if r != "INCOMPLETE":
            result = r
    assert result[0] == "OK", result
    print("PASS test_partial_frame_then_completion")


if __name__ == "__main__":
    test_valid_detection_frame()
    test_no_detection_frame()
    test_corrupted_checksum_rejected()
    test_bad_length_rejected()
    test_unknown_type_rejected()
    test_bad_start_byte_recovery()
    test_partial_frame_then_completion()
    print("ALL PROTOCOL TESTS PASSED (host-side simulated)")
