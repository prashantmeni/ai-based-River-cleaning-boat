#!/usr/bin/env python3
"""Extract unlabeled (trash-free) frames from the floating-rubbish dataset
as background_or_no_waste class. Real water imagery, no synthetic data.
"""
import os
import io
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "ai", "dataset_tools"))
from remotezip import RemoteZip
from PIL import Image

ROOT = r"C:\Users\Harish\river-cleaning-boat"
RAW = os.path.join(ROOT, "ai", "data", "raw", "background_or_no_waste")
os.makedirs(RAW, exist_ok=True)

URL = ("https://huggingface.co/datasets/gonzz2026/"
       "A-dataset-of-images-of-floating-rubbish-on-the-surface-of-the-water/"
       "resolve/main/"
       "A-dataset-of-images-of-floating-rubbish-on-the-surface-of-the-water.zip")

TARGET = 600


def main():
    z = RemoteZip(URL)
    names = z.namelist()
    added = 0
    for sp in ["datasets/train", "datasets/val", "datasets/test"]:
        imgs = sorted(n for n in names
                      if n.startswith(sp + "/images/") and n.lower().endswith(".jpg"))
        print(sp, "images", len(imgs), flush=True)
        tag = sp.split("/")[-1]
        for i, imn in enumerate(imgs):
            lb = imn.replace("/images/", "/labels/").rsplit(".", 1)[0] + ".txt"
            try:
                raw = z.read(imn)
                lbl = z.read(lb).decode() if lb in names else ""
            except Exception:
                continue
            if not lbl.strip():
                try:
                    img = Image.open(io.BytesIO(raw)).convert("RGB").resize((96, 96))
                except Exception:
                    continue
                p = os.path.join(RAW, "bg_%s_%05d.png" % (tag, i))
                if not os.path.exists(p):
                    img.save(p)
                    added += 1
            if added >= TARGET:
                break
        if added >= TARGET:
            break
    z.close()
    total = len([f for f in os.listdir(RAW) if f.lower().endswith(".png")])
    print("background added", added, "total", total, flush=True)


if __name__ == "__main__":
    main()
