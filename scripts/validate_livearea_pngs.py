#!/usr/bin/env python3
from pathlib import Path
from PIL import Image

root = Path("sce_sys")
bad = []
for p in root.rglob("*.png"):
    im = Image.open(p)
    if im.mode != "P":
        bad.append(f"{p}: mode={im.mode}, expected indexed palette mode P")
    if p.name == "icon0.png" and im.size != (120, 120):
        bad.append(f"{p}: size={im.size}, expected 120x120")

if bad:
    print("LiveArea PNG validation FAILED:")
    for x in bad:
        print(" -", x)
    raise SystemExit(1)

print("LiveArea PNG validation: OK")
