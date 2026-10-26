#!/usr/bin/env python3
"""Host-side simulated test of the navigation state machine transitions.

Mirrors the logic in firmware/esp32_main_controller/include/navigation.h
and docs/STATE_MACHINE.md. Runs on host with Python only — SIMULATED.
"""


class Nav:
    BOOT, SELF_TEST, IDLE, SEARCH, TARGET_DETECTED = range(5)
    ALIGN, APPROACH, COLLECT, VERIFY_COLLECT, TARGET_LOST = range(5, 10)
    OBSTACLE_AVOID, FAULT, EMERGENCY_STOP = range(10, 13)

    NAMES = {
        BOOT: "BOOT", SELF_TEST: "SELF_TEST", IDLE: "IDLE",
        SEARCH: "SEARCH", TARGET_DETECTED: "TARGET_DETECTED",
        ALIGN: "ALIGN", APPROACH: "APPROACH", COLLECT: "COLLECT",
        VERIFY_COLLECT: "VERIFY_COLLECT", TARGET_LOST: "TARGET_LOST",
        OBSTACLE_AVOID: "OBSTACLE_AVOID", FAULT: "FAULT",
        EMERGENCY_STOP: "EMERGENCY_STOP",
    }

    def __init__(self):
        self.state = self.BOOT
        self.transitions = []
        self._log("BOOT")

    def _log(self, name):
        self.transitions.append(name)

    def transition(self, nxt):
        self.state = nxt
        self._log(self.NAMES[nxt])

    def feed(self, detection, fresh, confident, consecutive, obstacle,
             both_blocked, comms_ok, autonomous):
        """Simulate one navigation tick."""
        if not autonomous:
            self.state = self.IDLE
            return
        if not comms_ok:
            self.state = self.FAULT
            return
        if self.state == self.BOOT:
            self.transition(self.SELF_TEST)
            return
        if self.state == self.SELF_TEST:
            self.transition(self.IDLE)
            return
        if self.state == self.IDLE:
            self.transition(self.SEARCH)
        elif self.state == self.SEARCH:
            if not detection:
                return  # keep searching
            if consecutive >= 2:
                self.transition(self.TARGET_DETECTED)
        elif self.state == self.TARGET_DETECTED:
            if not detection or not fresh:
                self.transition(self.TARGET_LOST)
            elif not confident:
                self.transition(self.SEARCH)
            else:
                # centered -> approach, else align
                self.transition(self.APPROACH)
        elif self.state == self.APPROACH:
            if not detection or not fresh:
                self.transition(self.TARGET_LOST)
            elif obstacle and both_blocked:
                self.transition(self.OBSTACLE_AVOID)
            else:
                self.transition(self.COLLECT)
        elif self.state == self.COLLECT:
            self.transition(self.VERIFY_COLLECT)
        elif self.state == self.VERIFY_COLLECT:
            self.transition(self.SEARCH)
        elif self.state == self.TARGET_LOST:
            if detection:
                self.transition(self.SEARCH)
        elif self.state == self.OBSTACLE_AVOID:
            if not (obstacle and both_blocked):
                self.transition(self.SEARCH)


def run_transition_test():
    n = Nav()
    assert n.state == Nav.BOOT
    n.feed(False, True, True, 0, False, False, True, True)
    assert n.state == Nav.SELF_TEST
    n.feed(False, True, True, 0, False, False, True, True)
    assert n.state == Nav.IDLE
    n.feed(False, True, True, 0, False, False, True, True)
    assert n.state == Nav.SEARCH, n.NAMES[n.state]
    # SEARCH -> TARGET_DETECTED (needs 2 consecutive)
    n.feed(True, True, True, 1, False, False, True, True)
    assert n.state == Nav.SEARCH
    n.feed(True, True, True, 2, False, False, True, True)
    assert n.state == Nav.TARGET_DETECTED
    # TARGET_DETECTED -> APPROACH (centered)
    n.feed(True, True, True, 2, False, False, True, True)
    assert n.state == Nav.APPROACH
    # APPROACH -> COLLECT
    n.feed(True, True, True, 2, False, False, True, True)
    assert n.state == Nav.COLLECT
    # COLLECT -> VERIFY_COLLECT -> SEARCH
    n.feed(True, True, True, 2, False, False, True, True)
    assert n.state == Nav.VERIFY_COLLECT
    n.feed(True, True, True, 2, False, False, True, True)
    assert n.state == Nav.SEARCH
    print("PASS run_transition_test")


def test_target_lost():
    n = Nav()
    n.state = Nav.SEARCH
    n.feed(True, True, True, 2, False, False, True, True)
    assert n.state == Nav.TARGET_DETECTED
    n.feed(False, True, True, 0, False, False, True, True)
    assert n.state == Nav.TARGET_LOST
    print("PASS test_target_lost")


def test_comms_lost_safe():
    n = Nav()
    n.state = Nav.SEARCH
    n.feed(True, True, True, 2, False, False, False, True)
    assert n.state == Nav.FAULT, n.NAMES[n.state]
    print("PASS test_comms_lost_safe")


def test_obstacle_both_blocked():
    n = Nav()
    n.state = Nav.APPROACH
    n.feed(True, True, True, 2, True, True, True, True)
    assert n.state == Nav.OBSTACLE_AVOID
    # clear obstacle -> back to SEARCH
    n.feed(True, True, True, 2, False, False, True, True)
    assert n.state == Nav.SEARCH
    print("PASS test_obstacle_both_blocked")


def test_stale_detection_rejected():
    n = Nav()
    n.state = Nav.TARGET_DETECTED
    n.feed(True, False, True, 2, False, False, True, True)
    assert n.state == Nav.TARGET_LOST
    print("PASS test_stale_detection_rejected")


if __name__ == "__main__":
    run_transition_test()
    test_target_lost()
    test_comms_lost_safe()
    test_obstacle_both_blocked()
    test_stale_detection_rejected()
    print("ALL NAVIGATION TESTS PASSED (host-side simulated)")
