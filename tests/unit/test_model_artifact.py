"""Unit tests: model artifact integrity (no hardware, stdlib only).

Run:  python -m unittest discover -s tests/unit -v
"""
import io
import json
import os
import re
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TFLITE = os.path.join(ROOT, "ai", "models", "waste_classifier.tflite")
HEADER = os.path.join(ROOT, "firmware", "models", "waste_model_data.h")
CONFIG = os.path.join(ROOT, "firmware", "include", "config.h")
REPORT = os.path.join(ROOT, "ai", "evaluation", "model_report.json")

EXPECTED_CLASSES = [
    "plastic_bottle",
    "plastic_wrapper",
    "leaf_or_organic_debris",
    "other_visible_waste",
    "background_or_no_waste",
]


def read(path):
    with io.open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


class ModelArtifactTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open(TFLITE, "rb") as f:
            cls.tflite = f.read()
        cls.header = read(HEADER)
        cls.config = read(CONFIG)
        cls.report = json.loads(read(REPORT))

    def test_tflite_exists_and_is_flatbuffer(self):
        self.assertGreater(len(self.tflite), 1000)
        # TFLite flatbuffers carry the "TFL3" identifier at offset 4.
        self.assertEqual(self.tflite[4:8], b"TFL3",
                         "waste_classifier.tflite is not a TFLite flatbuffer")

    def test_header_bytes_match_tflite(self):
        hdr_bytes = bytes(int(b, 16) for b in
                          re.findall(r"0x([0-9a-fA-F]{2})", self.header))
        self.assertEqual(len(hdr_bytes), len(self.tflite),
                         "header byte count != tflite size")
        self.assertEqual(hdr_bytes, self.tflite,
                         "firmware/models/waste_model_data.h is out of sync "
                         "with ai/models/waste_classifier.tflite "
                         "(re-run ai/models/convert_int8.py)")

    def test_header_length_constant(self):
        m = re.search(r"waste_model_data_len\s*=\s*(\d+)", self.header)
        self.assertIsNotNone(m, "waste_model_data_len missing from header")
        self.assertEqual(int(m.group(1)), len(self.tflite))

    def test_report_matches_artifact(self):
        self.assertEqual(self.report["tflite_bytes"], len(self.tflite))
        t8 = self.report.get("tflite_int8")
        self.assertIsNotNone(t8, "model_report.json missing tflite_int8 block")
        self.assertEqual(t8["bytes"], len(self.tflite))
        self.assertEqual(t8["input"]["shape"], [1, 96, 96, 3])
        self.assertEqual(t8["output"]["shape"], [1, 5])
        acc = t8["sample_accuracy"]
        self.assertEqual(acc["correct"], 184)
        self.assertEqual(acc["total"], 300)

    def test_class_names_consistent(self):
        # config.h CLASS_NAMES order must match report/training order
        m = re.search(r"CLASS_NAMES\[NUM_CLASSES\]\s*=\s*\{(.*?)\};",
                      self.config, re.S)
        self.assertIsNotNone(m, "CLASS_NAMES not found in config.h")
        cfg_classes = re.findall(r'"([a-z_]+)"', m.group(1))
        self.assertEqual(cfg_classes, EXPECTED_CLASSES)
        self.assertEqual(self.report["classes"], EXPECTED_CLASSES)

    def test_accuracy_claims_present_and_honest(self):
        # float-model held-out accuracy recorded, and int8 sample <= 1.0
        self.assertGreater(self.report["test_accuracy"], 0.0)
        self.assertLessEqual(self.report["test_accuracy"], 1.0)
        acc = self.report["tflite_int8"]["sample_accuracy"]
        self.assertAlmostEqual(acc["acc"], acc["correct"] / acc["total"], places=6)

    def test_detection_threshold_documented(self):
        m = re.search(r"DETECT_CONF_THRESHOLD\s+([0-9.]+)f", self.config)
        self.assertIsNotNone(m)
        self.assertEqual(float(m.group(1)), 0.55)


if __name__ == "__main__":
    unittest.main(verbosity=2)
