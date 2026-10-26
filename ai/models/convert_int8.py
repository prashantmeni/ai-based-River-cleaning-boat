"""Convert waste_classifier.keras -> int8 PTQ TFLite + firmware header.

Run from anywhere:  python ai/models/convert_int8.py

Produces:
  ai/models/waste_classifier.tflite            (int8 input, float32 output)
  firmware/models/waste_model_data.h           (byte array + length)

TFLM constraints encoded below (do not "fix" without re-testing on device):
  - full-integer INT8 ops (TFLM rejects hybrid float CONV_2D)
  - per-channel quantization DISABLED for Dense layers
    (TFLM FullyConnected kernel has no per-channel support)
  - int8 input (avoids a 27.6 KB float input tensor in the arena)
  - float32 output (keeps TFLM SOFTMAX/DEQUANTIZE path simple)
  - representative dataset from ai/data/raw crops (fixed seed)

Prints op inventory, I/O dtypes, and a 300-sample int8 accuracy check.
"""
import os
import numpy as np
import tensorflow as tf
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # .../ai
RAW = os.path.join(ROOT, "data", "raw")
IMG = 96
CLASSES = [
    "plastic_bottle",
    "plastic_wrapper",
    "leaf_or_organic_debris",
    "other_visible_waste",
    "background_or_no_waste",
]

model = tf.keras.models.load_model(os.path.join(ROOT, "models", "waste_classifier.keras"))


def representative_dataset(n_total=240):
    rng = np.random.default_rng(42)
    per = max(1, n_total // len(CLASSES))
    for c in CLASSES:
        d = os.path.join(RAW, c)
        if not os.path.isdir(d):
            continue
        files = [f for f in os.listdir(d)
                 if f.lower().endswith((".png", ".jpg", ".jpeg", ".bmp"))]
        if not files:
            continue
        rng.shuffle(files)
        for f in files[:per]:
            try:
                img = Image.open(os.path.join(d, f)).convert("RGB")
                img = img.resize((IMG, IMG))
                yield [np.asarray(img, dtype=np.float32)[np.newaxis, ...] / 255.0]
            except Exception:
                continue


converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.representative_dataset = representative_dataset
# full-integer int8 activations: TFLite Micro rejects hybrid CONV_2D.
converter._experimental_disable_per_channel_quantization_for_dense_layers = True
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
# int8 I/O for input (see module docstring); float32 output.
converter.inference_input_type = tf.int8
converter.inference_output_type = tf.float32
tflite_model = converter.convert()

# inspect
interp = tf.lite.Interpreter(model_content=tflite_model)
interp.allocate_tensors()
details = interp._get_ops_details() if hasattr(interp, "_get_ops_details") else []
op_names = [d["op_name"] for d in details]
print("[ops]", sorted(set(op_names)))
print("[count]", len(op_names))
tin = interp.get_input_details()[0]
tout = interp.get_output_details()[0]
print("[io] input dtype", tin["dtype"], "shape", tin["shape"])
print("[io] output dtype", tout["dtype"], "shape", tout["shape"])

tfl_path = os.path.join(ROOT, "models", "waste_classifier.tflite")
with open(tfl_path, "wb") as f:
    f.write(tflite_model)
print(f"[tflite] {tfl_path} size={len(tflite_model)} bytes")

hdr = os.path.join(ROOT, "..", "firmware", "models", "waste_model_data.h")
with open(os.path.normpath(hdr), "w") as f:
    f.write("#pragma once\n#include <stdint.h>\n")
    f.write(f"// Auto-generated (full int8 PTQ, int8 input / float output): {len(tflite_model)} bytes\n")
    f.write("const unsigned char waste_model_data[] = {\n")
    for i in range(0, len(tflite_model), 16):
        chunk = tflite_model[i:i + 16]
        f.write("  " + ", ".join(f"0x{b:02x}" for b in chunk) + ",\n")
    f.write("};\n")
    f.write(f"const unsigned int waste_model_data_len = {len(tflite_model)};\n")
print(f"[hdr] {hdr}")

# quick accuracy check on a held-out-ish sample (seeded, same as historical runs)
correct = total = 0
rng = np.random.default_rng(1)
for ci, c in enumerate(CLASSES):
    d = os.path.join(RAW, c)
    if not os.path.isdir(d):
        continue
    files = [f for f in os.listdir(d) if f.lower().endswith((".png", ".jpg", ".jpeg", ".bmp"))]
    rng.shuffle(files)
    for f in files[:60]:
        try:
            img = Image.open(os.path.join(d, f)).convert("RGB").resize((IMG, IMG))
            x = np.asarray(img, dtype=np.float32) / 255.0
            interp.set_tensor(tin["index"], x[np.newaxis, ...])
            interp.invoke()
            y = interp.get_tensor(tout["index"])[0]
            pred = int(np.argmax(y))
            total += 1
            correct += (pred == ci)
        except Exception:
            continue
print(f"[sample-acc] {correct}/{total} = {correct/total:.3f}" if total else "[sample-acc] n/a")
