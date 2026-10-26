#!/usr/bin/env python3
"""Train a compact CNN for floating-waste classification.

Output: ai/models/waste_classifier.tflite (int8-friendly, ESP32-CAM sized)
Input : 96x96x3 RGB float32 in [0,1]

Classes (5):
  0 plastic_bottle
  1 plastic_wrapper
  2 leaf_or_organic_debris
  3 other_visible_waste
  4 background_or_no_waste

Honesty note: accuracy reported here is only valid for the dataset actually
used. If the raw data is synthetic/partial, the model card must say so.
"""
import os
import sys
import json
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW = os.path.join(ROOT, "data", "raw")
OUT_DIR = os.path.join(ROOT, "models")
os.makedirs(OUT_DIR, exist_ok=True)

IMG = 96
CLASSES = [
    "plastic_bottle",
    "plastic_wrapper",
    "leaf_or_organic_debris",
    "other_visible_waste",
    "background_or_no_waste",
]
SEED = 42


def load_images(max_per_class=None):
    """Load images from data/raw/<class>. If max_per_class is None, balance
    all classes to the size of the smallest class (undersample)."""
    from PIL import Image

    # first pass: count available
    avail = {}
    for c in CLASSES:
        d = os.path.join(RAW, c)
        if not os.path.isdir(d):
            avail[c] = 0
            continue
        avail[c] = len([f for f in os.listdir(d)
                        if f.lower().endswith((".png", ".jpg", ".jpeg", ".bmp"))])
    if max_per_class is None:
        nonzero = [v for v in avail.values() if v > 0]
        max_per_class = min(nonzero) if nonzero else 0
    print(f"[balance] cap per class = {max_per_class}")

    X, y = [], []
    per_class = {}
    rng = np.random.default_rng(SEED)
    for idx, c in enumerate(CLASSES):
        d = os.path.join(RAW, c)
        if not os.path.isdir(d) or avail[c] == 0:
            print(f"[missing] {c}")
            per_class[c] = 0
            continue
        files = [f for f in os.listdir(d)
                 if f.lower().endswith((".png", ".jpg", ".jpeg", ".bmp"))]
        rng.shuffle(files)
        files = files[:max_per_class]
        n = 0
        for f in files:
            try:
                img = Image.open(os.path.join(d, f)).convert("RGB")
                img = img.resize((IMG, IMG))
                X.append(np.asarray(img, dtype=np.float32) / 255.0)
                y.append(idx)
                n += 1
            except Exception:
                continue
        per_class[c] = n
        print(f"[load] {c}: {n}")
    return np.array(X, dtype=np.float32), np.array(y, dtype=np.int32), per_class


def synthetic_dataset(n_per_class=200):
    """Fallback synthetic set for pipeline validation ONLY.

    Clearly labeled synthetic: proves training+conversion+eval loop works.
    NOT representative of real river waste imagery.
    """
    rng = np.random.default_rng(SEED)
    X, y = [], []
    for idx in range(len(CLASSES)):
        for _ in range(n_per_class):
            img = rng.random((IMG, IMG, 3), dtype=np.float32) * 0.3
            # class-correlated structure so the net has something to learn
            if idx == 0:  # bottle: vertical bright ellipse
                yy, xx = np.mgrid[0:IMG, 0:IMG]
                mask = (((xx - IMG // 2) / 18) ** 2 + ((yy - IMG // 2) / 34) ** 2) < 1
                img[mask] = rng.uniform(0.5, 1.0, size=(mask.sum(), 3))
            elif idx == 1:  # wrapper: horizontal streaks
                rows = rng.choice(IMG, 14, replace=False)
                img[rows, :, :] = rng.uniform(0.6, 1.0, size=(len(rows), IMG, 3))
            elif idx == 2:  # leaf: organic blobs, greenish
                for _k in range(5):
                    cy, cx = rng.integers(10, IMG - 10, 2)
                    r = rng.integers(6, 16)
                    yy, xx = np.mgrid[0:IMG, 0:IMG]
                    mask = ((yy - cy) ** 2 + (xx - cx) ** 2) < r * r
                    img[mask] = [rng.uniform(0.2, 0.5), rng.uniform(0.4, 0.9), rng.uniform(0.1, 0.3)]
            elif idx == 3:  # other waste: random bright blobs
                for _k in range(4):
                    cy, cx = rng.integers(10, IMG - 10, 2)
                    r = rng.integers(8, 20)
                    yy, xx = np.mgrid[0:IMG, 0:IMG]
                    mask = ((yy - cy) ** 2 + (xx - cx) ** 2) < r * r
                    img[mask] = rng.uniform(0.5, 1.0, size=(mask.sum(), 3))
            else:  # background: water-like blue/grey texture
                img[:, :, 0] = rng.uniform(0.05, 0.3, (IMG, IMG))
                img[:, :, 1] = rng.uniform(0.1, 0.4, (IMG, IMG))
                img[:, :, 2] = rng.uniform(0.2, 0.6, (IMG, IMG))
            X.append(img)
            y.append(idx)
    return np.array(X, dtype=np.float32), np.array(y, dtype=np.int32)


def build_model():
    import tensorflow as tf
    model = tf.keras.Sequential([
        tf.keras.layers.Input(shape=(IMG, IMG, 3)),
        tf.keras.layers.Conv2D(8, 3, padding="same", activation="relu"),
        tf.keras.layers.MaxPooling2D(),
        tf.keras.layers.Conv2D(16, 3, padding="same", activation="relu"),
        tf.keras.layers.MaxPooling2D(),
        tf.keras.layers.Conv2D(16, 3, padding="same", activation="relu"),
        tf.keras.layers.MaxPooling2D(),
        tf.keras.layers.GlobalAveragePooling2D(),
        tf.keras.layers.Dropout(0.2),
        tf.keras.layers.Dense(32, activation="relu"),
        tf.keras.layers.Dense(len(CLASSES), activation="softmax"),
    ])
    model.compile(
        optimizer=tf.keras.optimizers.Adam(1e-3),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    return model


def train():
    X, y, per_class = load_images()
    source = ("mixed: real river-trash/gonzz+bryandts waste images; "
              "background class = procedural water textures (synthetic bootstrap)")
    if len(X) == 0:
        print("[warn] no real images; using SYNTHETIC data for pipeline validation")
        X, y = synthetic_dataset()
        per_class = {c: 200 for c in CLASSES}
        source = "SYNTHETIC (pipeline validation only, not real accuracy)"

    rng = np.random.default_rng(SEED)
    idx = rng.permutation(len(X))
    X, y = X[idx], y[idx]

    split = int(0.8 * len(X))
    Xtr, ytr = X[:split], y[:split]
    Xte, yte = X[split:], y[split:]

    model = build_model()
    model.summary()
    hist = model.fit(
        Xtr, ytr,
        validation_data=(Xte, yte),
        epochs=15,
        batch_size=16,
        verbose=2,
    )

    import tensorflow as tf
    loss, acc = model.evaluate(Xte, yte, verbose=0)
    print(f"[eval] test loss={loss:.4f} acc={acc:.4f}")

    # confusion matrix + per-class precision/recall
    probs = model.predict(Xte, verbose=0)
    preds = probs.argmax(1)
    n = len(CLASSES)
    cm = np.zeros((n, n), dtype=int)
    for t, p in zip(yte, preds):
        cm[t, p] += 1
    prec, rec = [], []
    for i in range(n):
        tp = cm[i, i]
        fp = cm[:, i].sum() - tp
        fn = cm[i, :].sum() - tp
        prec.append(tp / (tp + fp) if (tp + fp) else 0.0)
        rec.append(tp / (tp + fn) if (tp + fn) else 0.0)

    # save keras + tflite
    keras_path = os.path.join(OUT_DIR, "waste_classifier.keras")
    model.save(keras_path)

    # Pure float32 conversion: TFLite Micro rejects hybrid models (int8
    # weights + float32 activations) in CONV_2D. No weight quantization.
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = []
    tflite_model = converter.convert()
    tfl_path = os.path.join(OUT_DIR, "waste_classifier.tflite")
    with open(tfl_path, "wb") as f:
        f.write(tflite_model)
    print(f"[tflite] {tfl_path} size={len(tflite_model)} bytes")

    # header for firmware embedding
    hdr = os.path.join(ROOT, "..", "firmware", "models", "waste_model_data.h")
    hdr = os.path.normpath(hdr)
    with open(hdr, "w") as f:
        f.write("#pragma once\n#include <stdint.h>\n")
        f.write(f"// Auto-generated: {len(tflite_model)} bytes\n")
        f.write("// source: {}\n".format(source))
        f.write(f"const unsigned char waste_model_data[] = {{\n")
        for i in range(0, len(tflite_model), 16):
            chunk = tflite_model[i:i + 16]
            f.write("  " + ", ".join(f"0x{b:02x}" for b in chunk) + ",\n")
        f.write("};\n")
        f.write(f"const unsigned int waste_model_data_len = {len(tflite_model)};\n")
    print(f"[hdr] {hdr}")

    report = {
        "source": source,
        "classes": CLASSES,
        "input_shape": [IMG, IMG, 3],
        "per_class_counts": per_class,
        "test_accuracy": float(acc),
        "test_loss": float(loss),
        "precision": {CLASSES[i]: float(prec[i]) for i in range(n)},
        "recall": {CLASSES[i]: float(rec[i]) for i in range(n)},
        "confusion_matrix": cm.tolist(),
        "tflite_bytes": len(tflite_model),
        "epochs": 15,
    }
    rep_path = os.path.join(ROOT, "evaluation", "model_report.json")
    with open(rep_path, "w") as f:
        json.dump(report, f, indent=2)
    print(f"[report] {rep_path}")
    return report


if __name__ == "__main__":
    train()
