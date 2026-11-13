#!/usr/bin/env python3
"""Skogkatt 'full floof' sprite set: the tabby cat from catgen with soft fur tufts on the outline,
a long fluffy head / bib ruff and lynx ear tufts.  Paws and feet stay neat (no fur hanging off them)."""
import math
import numpy as np
from PIL import Image, ImageDraw
import catgen as C
from catgen import M_BG, M_FUR, M_MARK, M_WHT, M_FAR, M_EAR

FURS = [M_FUR, M_MARK, M_WHT, M_FAR]

def _h(x, y, s):
    v = (x * 73856093) ^ (y * 19349663) ^ (s * 83492791); v ^= v >> 13
    v = (v * 0x5bd1e995) & 0xffffffff
    return (v ^ (v >> 15)) & 0xffff

class Fluffy(C.Cat):
    pose = ''
    def sit(self, *a, **k): self.pose = 'sit'; return super().sit(*a, **k)
    def side(self, *a, **k): self.pose = 'side'; return super().side(*a, **k)
    def sleep(self, *a, **k): self.pose = 'sleep'; return super().sleep(*a, **k)
    def back(self, *a, **k): self.pose = 'back'; return super().back(*a, **k)
    def scratch(self, *a, **k): self.pose = 'scratch'; return super().scratch(*a, **k)

    def finish(self):
        a = np.array(self.L).astype(np.int16); H, W = a.shape
        fur = np.isin(a, FURS)
        ys, xs = np.nonzero(fur)
        if not len(ys): return super().finish()
        top, bot = ys.min(), ys.max()
        ear = np.nonzero(a == M_EAR)
        headY0 = ear[0].min() if len(ear[0]) else top
        headY1 = headY0 + int(22 * self.S)
        out = a.copy()
        tuftImg = Image.fromarray(np.zeros_like(a, dtype=np.uint8)); td = ImageDraw.Draw(tuftImg)
        seeds = []
        for y in range(1, H - 1):
            for x in range(1, W - 1):
                if not fur[y, x]: continue
                dx = dy = 0
                for ox, oy in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1), (1, -1), (-1, 1)):
                    if a[y + oy, x + ox] == M_BG: dx += ox; dy += oy
                if dx == 0 and dy == 0: continue
                if y >= bot - 6 * self.S: continue                                    # paws and feet stay neat
                if self.pose == 'scratch' and x > self.cx + 5 * self.S and y > headY1 - 2 * self.S: continue   # paws on the post
                if self.pose == 'sit' and y > top + (bot - top) * 0.62 and abs(x - self.cx) < 8 * self.S: continue  # front legs
                seeds.append((x, y, dx, dy))
        for (x, y, dx, dy) in seeds:
            if (x * 7 + y * 13) % 3: continue                       # tufts spaced along the outline
            inHead = headY0 + 4 * self.S < y < headY1 + 10 * self.S
            p, L, wb = (85, 4.0 + _h(x, y, 9) % 3, 1.9) if a[y, x] == M_WHT or inHead else (75, 3.0 + _h(x, y, 9) % 2, 1.6)
            if _h(x, y, 7) % 100 >= p: continue
            mat = int(a[y, x]) if a[y, x] != M_FAR else M_FUR
            n = math.hypot(dx, dy); ux, uy = dx / n, dy / n
            sweep = 0.45 + 0.25 * ((_h(x, y, 3) % 3) - 1)           # fur lies down a little
            tx, ty = x + ux * L, y + uy * L + sweep * L * 0.6
            px, py = -uy, ux
            td.polygon([(x + px * wb, y + py * wb), (x - px * wb, y - py * wb), (tx, ty)], fill=mat)
        t = np.array(tuftImg)
        put = (t != 0) & (out == M_BG)
        out[put] = t[put]
        if len(ear[0]):                                              # lynx tufts on the ear tips
            for side in (0, 1):
                cols = ear[1][(ear[1] < W // 2) if side == 0 else (ear[1] >= W // 2)]
                if not len(cols): continue
                cx = int(cols.mean())
                col = out[:, max(0, cx - 3):cx + 4]
                tips = np.nonzero(np.isin(col, FURS))
                if not len(tips[0]): continue
                ty = tips[0].min(); tx = max(0, cx - 3) + tips[1][tips[0].argmin()]
                for k in range(1, 7):
                    yy = ty - k; xx = tx + (k // 2) * (1 if side else -1)
                    for w in ((0,) if k > 2 else (0, -1 if side else 1)):
                        if 0 <= yy < H and 0 <= xx + w < W: out[yy, xx + w] = M_MARK
        self.L = Image.fromarray(out.astype(np.uint8))
        return super().finish()

def build(body):
    orig = C.Cat; C.Cat = Fluffy
    try: return C.build(body, 'tabby')
    finally: C.Cat = orig
