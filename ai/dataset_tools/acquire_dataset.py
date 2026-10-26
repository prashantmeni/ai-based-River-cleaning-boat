#!/usr/bin/env python3
"""Acquire floating-waste images for training.

Priority order:
1. Kaggle 'river-floating-trash-datasets' (needs kaggle.json credentials)
2. HuggingFace datasets (no credentials for public sets)
3. Direct-download fallback URLs

Writes images into ai/data/raw/<class>/.
"""
import os
import sys
import json
import urllib.request
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW = os.path.join(ROOT, "data", "raw")

CLASSES = [
    "plastic_bottle",
    "plastic_wrapper",
    "leaf_or_organic_debris",
    "other_visible_waste",
    "background_or_no_waste",
]


def ensure_dirs():
    for c in CLASSES:
        os.makedirs(os.path.join(RAW, c), exist_ok=True)


def try_kaggle():
    """Download Kaggle river trash dataset if credentials exist."""
    cred = os.path.expanduser("~/.kaggle/kaggle.json")
    if not os.path.exists(cred):
        print("[skip] kaggle.json not found")
        return False
    try:
        import kaggle  # noqa
    except ImportError:
        os.system(f"{sys.executable} -m pip install kaggle --quiet")
        import kaggle  # noqa
    try:
        os.system(
            'kaggle datasets download -d zhiaun/river-floating-trash-datasets '
            f'-p "{os.path.join(ROOT, "data", "kaggle")}" --unzip'
        )
        return True
    except Exception as e:
        print(f"[fail] kaggle: {e}")
        return False


def try_huggingface():
    """Try public HF datasets for waste/trash imagery."""
    try:
        from datasets import load_dataset
    except ImportError:
        os.system(f"{sys.executable} -m pip install datasets --quiet")
        from datasets import load_dataset

    candidates = [
        ("art3m4s/waste-classification", None),
        ("GaryCu/waste-dataset", None),
    ]
    for name, _ in candidates:
        try:
            print(f"[try] HF dataset {name}")
            ds = load_dataset(name, split="train", streaming=True)
            count = 0
            for ex in ds:
                img = ex.get("image")
                if img is None:
                    continue
                label = str(ex.get("label", ex.get("labels", "unknown")))
                d = os.path.join(RAW, label)
                os.makedirs(d, exist_ok=True)
                img.save(os.path.join(d, f"hf_{count:05d}.png"))
                count += 1
                if count >= 400:
                    break
            if count > 0:
                print(f"[ok] {name}: {count} images")
                return True
        except Exception as e:
            print(f"[fail] {name}: {e}")
    return False


def main():
    ensure_dirs()
    got = False
    if try_kaggle():
        got = True
    if not got:
        got = try_huggingface()
    if not got:
        print("[warn] No external dataset acquired.")
        print("Manual option: download FloW dataset via https://www.orca-tech.cn/datasets")
        print("or Kaggle 'river-floating-trash-datasets' into data/raw/<class>/")
        sys.exit(2)
    print("[done] raw images in", RAW)


if __name__ == "__main__":
    main()
