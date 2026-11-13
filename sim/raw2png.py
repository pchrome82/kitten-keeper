#!/usr/bin/env python3
"""Convert raw 184x224 RGB565 frames (little-endian) to PNG. Usage: raw2png.py out.png scale in1.raw [in2.raw ...]  (frames tiled horizontally)"""
import sys, struct
from PIL import Image
import os
W, H = int(os.environ.get('RW', 184)), int(os.environ.get('RH', 224))
out, scale, ins = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
cols = min(len(ins), int(__import__('os').environ.get('COLS', len(ins))))
rows = (len(ins) + cols - 1) // cols
sheet = Image.new("RGB", (cols * (W + 4) - 4, rows * (H + 4) - 4), (40, 40, 40))
for i, fn in enumerate(ins):
    data = open(fn, "rb").read()
    px = struct.unpack("<%dH" % (W * H), data[: W * H * 2])
    img = Image.new("RGB", (W, H))
    img.putdata([(((p >> 11) & 31) * 255 // 31, ((p >> 5) & 63) * 255 // 63, (p & 31) * 255 // 31) for p in px])
    sheet.paste(img, ((i % cols) * (W + 4), (i // cols) * (H + 4)))
sheet = sheet.resize((sheet.width * scale, sheet.height * scale), Image.NEAREST)
sheet.save(out)
print("wrote", out, sheet.size)
