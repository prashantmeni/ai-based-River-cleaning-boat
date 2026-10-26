#!/usr/bin/env python3
"""Build the 5-class floating-waste training set from acquired sources.

Sources:
  A. gonzz2026 floating-rubbish YOLO dataset (remote ZIP, CC-BY-4.0)
     -> plastic_bottle (PET_Bottle), plastic_wrapper (Plastic_ETC),
        other_visible_waste (Glass/Metal/Net/Rope/Styrofoam/Plastic_Buoy*)
  B. bryandts organic/recyclable waste ZIP (local, 22.5k train imgs)
     -> leaf_or_organic_debris (class O only)
  C. cordmaur earth_surface_water (remote ZIP) -> background_or_no_waste
  D. SARA trash-in-river images (already in raw/other_visible_waste)

For A, samples come from bbox crops (what the boat sees in a region) plus
full frames labeled by dominant class.
"""
import os
import io
import time
import zipfile
import urllib.request
from PIL import Image
from remotezip import RemoteZip


def read_retry(z, name, tries=5):
    """Read from RemoteZip with backoff (HF rate-limits anonymous range reads)."""
    last = None
    for k in range(tries):
        try:
            return z.read(name)
        except Exception as e:
            last = e
            time.sleep(1.5 * (k + 1))
    raise last

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW = os.path.join(ROOT, "data", "raw")
IMG = 96
SEED = 42

CLASSES = [
    "plastic_bottle",
    "plastic_wrapper",
    "leaf_or_organic_debris",
    "other_visible_waste",
    "background_or_no_waste",
]
C2I = {c: i for i, c in enumerate(CLASSES)}

GONZZ = ("https://huggingface.co/datasets/gonzz2026/"
         "A-dataset-of-images-of-floating-rubbish-on-the-surface-of-the-water/"
         "resolve/main/"
         "A-dataset-of-images-of-floating-rubbish-on-the-surface-of-the-water.zip")
WATER = ("https://huggingface.co/datasets/cordmaur/earth_surface_water/"
         "resolve/main/earth_surface_water.zip")
BRYANDTS = os.path.join(ROOT, "data", "DATASET.zip")

# YOLO class id -> our class name
GONZZ_MAP = {
    0: "other_visible_waste",   # Glass
    1: "other_visible_waste",   # Styrofoam_Piece
    2: "other_visible_waste",   # Plastic_Buoy_China
    3: "other_visible_waste",   # Metal
    4: "other_visible_waste",   # Net
    5: "other_visible_waste",   # Rope
    6: "other_visible_waste",   # Plastic_Buoy
    7: "plastic_bottle",        # PET_Bottle
    8: "other_visible_waste",   # Styrofoam_Box
    9: "plastic_wrapper",       # Plastic_ETC (misc flexible plastic)
    10: "other_visible_waste",  # Styrofoam_Buoy
}


def ensure():
    for c in CLASSES:
        os.makedirs(os.path.join(RAW, c), exist_ok=True)


def counters():
    return {c: len([f for f in os.listdir(os.path.join(RAW, c))
                    if f.lower().endswith((".png", ".jpg", ".jpeg"))])
            for c in CLASSES}


def save(img, cls, tag):
    d = os.path.join(RAW, cls)
    path = os.path.join(d, f"{tag}.png")
    if os.path.exists(path):
        return False
    img.convert("RGB").resize((IMG, IMG)).save(path)
    return True


def build_gonzz(max_imgs=1900):
    """Stream images + labels from remote ZIP; emit bbox crops + full frames."""
    print("[A] floating-rubbish dataset (remote range reads)", flush=True)
    added = 0
    failed = 0
    state_path = os.path.join(ROOT, "data", "gonzz_done.txt")
    done = set()
    if os.path.exists(state_path):
        with open(state_path) as f:
            done = {int(x) for x in f.read().split() if x.isdigit()}
        print(f"[A] resume: {len(done)} indices already done", flush=True)
    else:
        # seed: earlier runs processed indices 0-121 (max g_full/g_crop idx = 121)
        done = set(range(122))
        with open(state_path, "w") as f:
            f.write("\n".join(str(x) for x in sorted(done)))
        print(f"[A] seeded {len(done)} previously-done indices", flush=True)
    state = open(state_path, "a")
    with RemoteZip(GONZZ) as z:
        names = z.namelist()
        img_names = sorted(n for n in names
                           if n.startswith("datasets/train/images/")
                           and n.lower().endswith(".jpg"))[:max_imgs]
        for i, imn in enumerate(img_names):
            if i in done:
                continue
            lb = imn.replace("/images/", "/labels/").rsplit(".", 1)[0] + ".txt"
            if lb not in names:
                state.write(f"{i}\n")
                state.flush()
                continue
            try:
                raw = read_retry(z, imn)
                img = Image.open(io.BytesIO(raw)).convert("RGB")
                w, h = img.size
                rows = [l.split() for l in read_retry(z, lb).decode().splitlines() if l.strip()]
            except Exception as e:
                failed += 1
                if failed <= 3 or failed % 50 == 0:
                    print(f"   read-fail #{failed} {imn}: {type(e).__name__}")
                continue
            # full frame: dominant (largest-area) object
            objs = []
            for r in rows:
                try:
                    cid = int(float(r[0]))
                    bw, bh = float(r[3]) * w, float(r[4]) * h
                    objs.append((bw * bh, cid, [float(x) for x in r[1:5]]))
                except Exception:
                    continue
            if objs:
                objs.sort(reverse=True)
                cls = GONZZ_MAP.get(objs[0][1])
                if cls and save(img, cls, f"g_full_{i:05d}"):
                    added += 1
                # bbox crops for every object (aspect-preserved pad to square)
                for j, (_, cid, box) in enumerate(objs):
                    cls2 = GONZZ_MAP.get(cid)
                    if not cls2:
                        continue
                    cx, cy, bw, bh = box
                    x0, y0 = int((cx - bw / 2) * w), int((cy - bh / 2) * h)
                    x1, y1 = int((cx + bw / 2) * w), int((cy + bh / 2) * h)
                    x0, y0 = max(0, x0), max(0, y0)
                    x1, y1 = min(w, x1), min(h, y1)
                    if x1 - x0 < 8 or y1 - y0 < 8:
                        continue
                    # pad crop to square
                    cw, ch = x1 - x0, y1 - y0
                    side = max(cw, ch)
                    canvas = Image.new("RGB", (side, side), (128, 128, 128))
                    canvas.paste(img.crop((x0, y0, x1, y1)), ((side - cw) // 2, (side - ch) // 2))
                    if save(canvas, cls2, f"g_crop_{i:05d}_{j}"):
                        added += 1
            state.write(f"{i}\n")
            state.flush()
            done.add(i)
            if (i + 1) % 100 == 0:
                print(f"   {i+1}/{len(img_names)} (+{added}, fail={failed})", flush=True)
    state.close()
    print(f"[A] added {added} failed_reads {failed}", flush=True)
    return added


def build_bryandts(max_per_class=500):
    """Class O (organic) -> leaf_or_organic_debris from the local ZIP."""
    print("[B] organic waste (local DATASET.zip)")
    if not os.path.exists(BRYANDTS):
        print("[B] missing", BRYANDTS)
        return 0
    added = 0
    with zipfile.ZipFile(BRYANDTS) as z:
        names = [n for n in z.namelist()
                 if "/O/" in n and n.lower().endswith((".jpg", ".jpeg", ".png"))]
        # prefer train split
        train = [n for n in names if "/TRAIN/" in n] or names
        step = max(1, len(train) // max_per_class)
        for i in range(0, len(train), step):
            n = train[i]
            try:
                img = Image.open(io.BytesIO(z.read(n))).convert("RGB")
            except Exception:
                continue
            if save(img, "leaf_or_organic_debris", f"b_org_{i:05d}"):
                added += 1
            if added >= max_per_class:
                break
    print(f"[B] added {added}")
    return added


def build_water(max_imgs=300):
    """Clean water surface -> background_or_no_waste."""
    print("[C] water background (remote range reads)")
    added = 0
    try:
        with RemoteZip(WATER) as z:
            names = [n for n in z.namelist()
                     if n.lower().endswith((".jpg", ".jpeg", ".png"))][:max_imgs]
            for i, n in enumerate(names):
                try:
                    img = Image.open(io.BytesIO(read_retry(z, n))).convert("RGB")
                except Exception as e:
                    print(f"   water-read-fail {n}: {type(e).__name__}")
                    continue
                if save(img, "background_or_no_waste", f"w_bg_{i:05d}"):
                    added += 1
                if (i + 1) % 50 == 0:
                    print(f"   {i+1}/{len(names)}")
    except Exception as e:
        print("[C] fail:", e)
    print(f"[C] added {added}")
    return added


def report(label="final"):
    c = counters()
    print(f"[{label}] counts:", c)
    print(f"[{label}] total:", sum(c.values()))


def main():
    ensure()
    report("start")
    build_gonzz()
    build_bryandts()
    build_water()
    report("final")


if __name__ == "__main__":
    main()
