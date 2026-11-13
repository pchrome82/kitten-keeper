#!/usr/bin/env python3
"""Parametric pixel-cat generator.  Produces palette-index frames (role ids 0..15) for:
   sit (front, 2f), walk (side, 4f), sleep (loaf, 2f), eat (side, 2f)  x  skinny / normal / chonk.
Role ids are mapped to RGB by a per-breed palette at bake time (see bake.py)."""
import math
import numpy as np
from PIL import Image, ImageDraw

W, HH = 128, 140
# ---- roles (palette indices)
R_T, R_OUT, R_FUR, R_FURD, R_FURL, R_MARK, R_MARKD, R_WHT, R_WHTD, R_EAR, R_EYE, R_PUP, R_SHN, R_NOSE, R_MOUTH, R_BLUSH = range(16)
# ---- materials in the label image (mapped to roles + shading in finish())
M_BG, M_FUR, M_MARK, M_WHT, M_EAR, M_EYE, M_PUP, M_SHN, M_NOSE, M_MOUTH, M_BLUSH, M_FAR, M_WHISK, M_TOE = range(14)

BODIES = {'skinny': dict(S=1.1, fat=-0.3), 'normal': dict(S=1.3, fat=0.2), 'chonk': dict(S=1.7, fat=1.0)}

class Cat:
    def __init__(self, body, pattern):
        b = BODIES[body]
        self.S, self.fat, self.pattern, self.body = b['S'], b['fat'], pattern, body
        self.fp = max(self.fat, 0.0); self.fn = max(-self.fat, 0.0)
        self.L = Image.new('L', (W, HH), M_BG)
        self.d = ImageDraw.Draw(self.L)
        self.g = HH - 8      # ground line
        self.cx = W // 2

    # ---------- helpers
    def mask(self, fn):
        m = Image.new('L', (W, HH), 0); fn(ImageDraw.Draw(m)); return np.array(m) > 0
    def paint(self, fn, mat, over=(M_FUR, M_FAR)):
        """Paint `mat` only over pixels currently in `over` (clips markings to the silhouette)."""
        m = self.mask(fn); a = np.array(self.L)
        sel = m & np.isin(a, over); a[sel] = mat; self.L = Image.fromarray(a); self.d = ImageDraw.Draw(self.L)
    def line(self, pts, w, mat):
        self.d.line(pts, fill=mat, width=max(1, int(round(w))), joint='curve')
    def ell(self, cx, cy, rx, ry, mat):
        self.d.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=mat)
    def tri(self, pts, mat): self.d.polygon(pts, fill=mat)

    # ---------- ears (shared)
    def ears(self, hx, hy, hrx, hry, tilt=0.0, near_only=False):
        S = self.S
        for s in (-1, 1):
            ex = hx + s * hrx * 0.58
            # base sits inside the head ellipse, only the tip pokes out above it
            self.tri([(ex - s * 4.2 * S, hy - hry * 0.62), (ex + s * 1.2 * S + tilt, hy - hry - 4.6 * S), (ex + s * 4.4 * S, hy - hry * 0.46)], M_FUR)
            self.tri([(ex - s * 2.0 * S, hy - hry * 0.66), (ex + s * 1.2 * S + tilt, hy - hry - 1.2 * S), (ex + s * 3.0 * S, hy - hry * 0.54)], M_EAR)

    # =================================================== FRONT SIT
    def sit(self, blink=False, bob=0):
        S, fp, fn, g, cx = self.S, self.fp, self.fn, self.g, self.cx
        brx = (9 + 10 * fp - 5 * fn) * S; bry = (12 + 2 * fp + 6 * fn) * S
        bcy = g - bry
        hy = bcy - bry + 3 * S + (2 * S if fn else 0) + bob
        hrx = (12.5 + 2.5 * fp - 2 * fn) * S; hry = (10.5 + 0.5 * fp - 1.2 * fn) * S
        # tail
        pts = []
        for i in range(18):
            t = i / 17; a = -0.2 + t * 2.4
            pts.append((cx + (brx + 3 * S) * math.cos(a) * (1 - 0.25 * t) + 2 * S, g - 6 * S - (bry + 6 * S) * math.sin(a) * 0.9 * (1 - t) - t * 2 * S))
        pts = pts[::-1]
        self.line(pts, (4 if fp < 0.6 else 5) * S * 0.9, M_FUR)
        self.d.ellipse([pts[0][0] - 2 * S, pts[0][1] - 2 * S, pts[0][0] + 2 * S, pts[0][1] + 2 * S], fill=M_FUR)
        # body + chest + legs
        self.ell(cx, bcy - S, brx, bry - S, M_FUR)                      # body ends just above the floor so the paws show
        if fp > 0.6: self.ell(cx, g - 6.5 * S, brx - 2 * S, 4.5 * S, M_FUR)
        lx = (4 + brx * 0.25 / S) * S
        for s in (-1, 1):
            x = cx + s * lx
            self.d.rectangle([x - 2.5 * S, bcy - 2 * S, x + 2.5 * S, g - 2 * S], fill=M_FUR)
            self.ell(x + s * 0.6 * S, g - 1.8 * S, 4.2 * S, 2.5 * S, M_FUR)   # chunky front paw, a little wider than the leg
        self.ears(cx, hy, hrx, hry)
        self.ell(cx, hy, hrx, hry, M_FUR)
        if fp > 0.6:
            for s in (-1, 1): self.ell(cx + s * (hrx - 3 * S), hy + hry * 0.5, 4 * S, 4 * S, M_FUR)
        # patterns
        p = self.pattern
        if p in ('tux', 'calico', 'tabby'):
            self.paint(lambda d: d.ellipse([cx - brx * .55, bcy - bry + 6 * S, cx + brx * .55, g - S], fill=1), M_WHT)
        if p == 'tabby':
            for dx in (-3, 0, 3):
                self.paint(lambda d, dx=dx: d.line([(cx + dx * S, hy - hry + S), (cx + dx * .8 * S, hy - hry + 5 * S)], fill=1, width=max(1, int(S))), M_MARK)
            for s in (-1, 1):
                for k in range(2):
                    y = hy + (1 + k * 3) * S
                    self.paint(lambda d, s=s, y=y: d.line([(cx + s * hrx, y), (cx + s * (hrx - 4 * S), y + S)], fill=1, width=max(1, int(S))), M_MARK)
                for k in range(3):
                    y = bcy - 4 * S + k * 5 * S
                    self.paint(lambda d, s=s, y=y: d.line([(cx + s * brx, y), (cx + s * (brx - 4 * S), y + S)], fill=1, width=max(1, int(2 * S))), M_MARK)
            for k in range(3):
                px, py = pts[3 + k * 4]
                self.paint(lambda d, px=px, py=py: d.ellipse([px - 2 * S, py - 2 * S, px + 2 * S, py + 2 * S], fill=1), M_MARK)
        if p == 'tux':
            self.paint(lambda d: d.polygon([(cx - 5 * S, hy + hry), (cx, hy + S), (cx + 5 * S, hy + hry)], fill=1), M_WHT)
            self.paint(lambda d: d.ellipse([cx - 4 * S, hy + 2 * S, cx + 4 * S, hy + hry - S], fill=1), M_WHT)
            for s in (-1, 1): self.paint(lambda d, s=s: d.ellipse([cx + s * lx - 3.6 * S, g - 4.4 * S, cx + s * lx + 4.8 * S, g + 0.6 * S], fill=1), M_WHT)
        if p == 'calico':
            self.paint(lambda d: d.ellipse([cx - hrx, hy - hry, cx - S, hy - 2 * S], fill=1), M_MARK)
            self.paint(lambda d: d.ellipse([cx + brx * .2, bcy - 4 * S, cx + brx + S, bcy + 9 * S], fill=1), M_MARK)
            self.paint(lambda d: d.ellipse([cx + 3 * S, hy - hry - 2 * S, cx + hrx + 3 * S, hy - 2 * S], fill=1), M_FAR)   # dark patch
            self.paint(lambda d: d.ellipse([cx - 4 * S, hy + S, cx + 4 * S, hy + hry], fill=1), M_WHT)
        if p == 'points':
            self.paint(lambda d: d.rectangle([cx - hrx - 2, hy - hry - 9 * S, cx + hrx + 2, hy - hry + 3 * S], fill=1), M_MARK)
            self.paint(lambda d: d.ellipse([cx - 6 * S, hy, cx + 6 * S, hy + hry], fill=1), M_MARK)
            for sgn in (-1, 1): self.paint(lambda d, sgn=sgn: d.ellipse([cx + sgn * lx - 3.6 * S, g - 5 * S, cx + sgn * lx + 3.6 * S, g + 1], fill=1), M_MARK)
            self.paint(lambda d: d.ellipse([pts[0][0] - 4 * S, pts[0][1] - 4 * S, pts[0][0] + 4 * S, pts[0][1] + 4 * S], fill=1), M_MARK)
        for sg in (-1, 1):   # toe lines
            tx = cx + sg * lx + sg * 0.6 * S
            for dx in (-1.3, 1.3): self.d.line([(tx + dx * S, g - 2.4 * S), (tx + dx * S, g - 0.4 * S)], fill=M_TOE, width=1)
        self.face_front(cx, hy, hrx, hry, blink)
        return self

    def eye(self, ex, ey, ew, eh):
        """rounded iris, centred 2px slit pupil, dark upper lid, white glint"""
        d = self.d
        x0 = int(round(ex)) - ew; x1 = x0 + 2 * ew - 1
        y0 = int(round(ey)) - eh; y1 = y0 + 2 * eh - 1
        d.rectangle([x0 + 1, y0, x1 - 1, y1], fill=M_EYE)
        d.rectangle([x0, y0 + 1, x1, y1 - 1], fill=M_EYE)
        d.line([(x0 + 1, y0 - 1), (x1 - 1, y0 - 1)], fill=M_PUP)           # upper lid
        cxp = x0 + ew - 1
        d.rectangle([cxp, y0 + 1, cxp + 1, y1 - 1], fill=M_PUP)           # slit pupil
        d.point([(x0 + 1, y0 + 1)], fill=M_SHN)                           # glint
        if eh >= 4: d.point([(x1 - 1, y1 - 1)], fill=M_SHN)

    def face_front(self, hx, hy, hrx, hry, blink):
        S = self.S; d = self.d
        ey = hy + S
        ew = max(3, round(2.3 * S)); eh = max(3, round(2.8 * S))
        for s in (-1, 1):
            ex = hx + s * 5.5 * S
            if blink:   # the closed eye covers exactly the open eye's columns
                x0 = int(round(ex)) - ew
                d.line([(x0, ey + S), (x0 + 2 * ew - 1, ey + S)], fill=M_PUP, width=max(1, int(S))); continue
            self.eye(ex, ey, ew, eh)
            d.rectangle([ex + s * 4 * S, ey + 4 * S, ex + s * 4 * S + max(1, int(S)), ey + 4 * S], fill=M_BLUSH)
        nx, ny = hx, ey + 3 * S
        d.rectangle([nx - S, ny, nx + S, ny + max(0, int(S) - 1)], fill=M_NOSE)
        d.point([(nx, ny + 1 + int(S))], fill=M_NOSE)
        my = ey + 5 * S
        d.line([(nx - 2 * S, my), (nx - S, my + S), (nx, my), (nx + S, my + S), (nx + 2 * S, my)], fill=M_MOUTH, width=1)
        for s in (-1, 1):
            for k in (0, 2):
                d.line([(hx + s * (hrx - 2 * S), ey + (3 + k) * S), (hx + s * (hrx + 4 * S), ey + (2 + k * 1.5) * S)], fill=M_WHISK)

    # =================================================== SIDE (walk / eat), facing right: the "bean trot"
    # A round little bean of a cat: body and head blend together, four tiny feet shuffle underneath on short
    # legs (so they never come loose from the body), the tail held straight up. Simple profile face, one big eye.
    def side(self, phase=0.0, eat=0, mouth=False, bob=0):
        S, fp, fn, g, cx = self.S, self.fp, self.fn, self.g, self.cx
        rx = (14 + 6.5 * fp - 1.5 * fn) * S; ry = (10 + 4.5 * fp - 2.0 * fn) * S
        by = g - 2.6 * S - ry + (bob * 0.6 * S if not eat else 0)
        bx0 = cx - 2 * S
        sw = math.sin(phase) * 1.6 * S
        # tail held straight up, a soft bend at the tip
        a_, b_, c_, e_ = (bx0 - rx * 0.75, by - ry * 0.4), (bx0 - rx * 0.95, by - ry - 4 * S), (bx0 - rx * 0.9 + sw * .5, by - ry - 11 * S), (bx0 - rx * 0.55 + sw, by - ry - 14 * S)
        pts = []
        for t in [i / 12 for i in range(13)]:
            pts.append(((1 - t) ** 3 * a_[0] + 3 * (1 - t) ** 2 * t * b_[0] + 3 * (1 - t) * t * t * c_[0] + t ** 3 * e_[0],
                        (1 - t) ** 3 * a_[1] + 3 * (1 - t) ** 2 * t * b_[1] + 3 * (1 - t) * t * t * c_[1] + t ** 3 * e_[1]))
        self.line(pts, (3.8 + 0.4 * fp) * S, M_FUR)
        # feet on short stubby legs that start inside the body: shuffling, but always attached
        legw = (3.6 + 0.8 * fp) * S
        def foot(x0, ph, mat):
            dx = 0 if eat else math.sin(ph) * 2.0 * S
            up = 0 if eat else max(0.0, math.cos(ph)) * 1.4 * S
            paw = (x0 + dx, g - 1.7 * S - up)
            self.line([(x0 + dx * 0.35, by + ry * 0.55), paw], legw, mat)
            self.ell(paw[0], paw[1], 2.8 * S, 1.9 * S, mat)
        fxl, bxl = bx0 + rx * 0.42, bx0 - rx * 0.5
        foot(fxl + 3 * S, phase + math.pi, M_FAR); foot(bxl + 3 * S, phase, M_FAR)
        self.ell(bx0, by, rx, ry, M_FUR)
        # head: blended into the front of the bean; dips down to the bowl when eating
        hrx = (10.5 + 2.5 * fp - 1.5 * fn) * S; hry = (9.5 + 1.5 * fp - 1 * fn) * S
        hx = bx0 + rx * 0.62 + (3 * S if eat else 0)
        hy = by - ry * 0.45 + (bob * 0.3 * S if not eat else 0)
        if eat: hy = min(by + ry * 0.25, g - hry - 1)
        for s_ in (-1, 1):   # two little upright ears (the far one darker)
            ex = hx + s_ * hrx * 0.45 - 0.5 * S
            self.tri([(ex - 3.6 * S, hy - hry * 0.6), (ex + s_ * 0.6 * S, hy - hry - 4.4 * S), (ex + 3.6 * S, hy - hry * 0.55)], M_FUR if s_ > 0 else M_FAR)
            self.tri([(ex - 1.8 * S, hy - hry * 0.66), (ex + s_ * 0.6 * S, hy - hry - 1.4 * S), (ex + 1.8 * S, hy - hry * 0.6)], M_EAR)
        if eat: self.line([(bx0 + rx * 0.4, by - ry * 0.2), (hx - 2 * S, hy)], 8 * S, M_FUR)   # neck reaching down
        self.ell(hx, hy, hrx, hry, M_FUR)
        foot(fxl, phase, M_FUR); foot(bxl, phase + math.pi, M_FUR)
        # patterns
        p = self.pattern
        if p in ('tux', 'calico', 'tabby'):
            self.paint(lambda d: d.ellipse([bx0 - rx * .2, by + ry * 0.05, bx0 + rx + S, by + ry + 2 * S], fill=1), M_WHT)
        if p == 'tabby':
            for k in range(4):
                x = bx0 - rx * 0.7 + k * rx * 0.32
                self.paint(lambda d, x=x: d.line([(x, by - ry), (x + 1.4 * S, by - ry * 0.3)], fill=1, width=max(1, int(1.6 * S))), M_MARK)
            for k in range(3):
                a2, b2 = pts[4 + k * 3]
                self.paint(lambda d, a2=a2, b2=b2: d.ellipse([a2 - 2 * S, b2 - 2 * S, a2 + 2 * S, b2 + 2 * S], fill=1), M_MARK)
            for dx in (-3, 0, 3):
                self.paint(lambda d, dx=dx: d.line([(hx + dx * S - 1 * S, hy - hry + S), (hx + dx * .8 * S - 1 * S, hy - hry + 4.5 * S)], fill=1, width=max(1, int(S))), M_MARK)
        if p == 'tux':
            self.paint(lambda d: d.ellipse([hx + 2 * S, hy + 1 * S, hx + hrx + 1, hy + hry], fill=1), M_WHT)               # white muzzle
            for lx_ in (fxl, bxl):                                                                                      # white socks
                self.paint(lambda d, lx_=lx_: d.rectangle([lx_ - 4 * S, g - 4.5 * S, lx_ + 7 * S, g + 2], fill=1), M_WHT)
        if p == 'calico':
            self.paint(lambda d: d.ellipse([bx0 - rx * .6, by - ry - S, bx0 + rx * .2, by + ry * .1], fill=1), M_MARK)
            self.paint(lambda d: d.ellipse([bx0 - rx - S, by - ry * .2, bx0 - rx * .35, by + ry * .7], fill=1), M_FAR)
            self.paint(lambda d: d.ellipse([hx - hrx, hy - hry - S, hx + 1 * S, hy + 0 * S], fill=1), M_MARK)
        if p == 'points':   # dark ears, muzzle, feet and tail tip (shaped to the head, so it never spills onto the back)
            for s_ in (-1, 1):
                ex = hx + s_ * hrx * 0.45 - 0.5 * S
                self.paint(lambda d, ex=ex, s_=s_: d.polygon([(ex - 3.6 * S, hy - hry * 0.6), (ex + s_ * 0.6 * S, hy - hry - 4.4 * S), (ex + 3.6 * S, hy - hry * 0.55)], fill=1), M_MARK, over=(M_FUR, M_FAR, M_EAR))
            self.paint(lambda d: d.ellipse([hx + 2 * S, hy + 0.5 * S, hx + hrx + 2, hy + hry - 1 * S], fill=1), M_MARK)
            for lx_ in (fxl, bxl): self.paint(lambda d, lx_=lx_: d.rectangle([lx_ - 4 * S, g - 4.5 * S, lx_ + 7 * S, g + 2], fill=1), M_MARK, over=(M_FUR, M_FAR, M_WHT))
            self.paint(lambda d: d.ellipse([pts[-1][0] - 4 * S, pts[-1][1] - 4 * S, pts[-1][0] + 4 * S, pts[-1][1] + 4 * S], fill=1), M_MARK)
        # face: one big round eye (happy closed ^ while eating), button nose, little mouth
        d = self.d
        ex, ey = hx + 4.2 * S, hy - 0.2 * S
        if eat:
            w2 = max(3, round(2.2 * S))
            d.line([(ex - w2, ey + S), (ex, ey - S), (ex + w2, ey + S)], fill=M_PUP, width=max(1, int(S * 0.8)))
        else:
            self.eye(ex, ey, max(3, round(2.2 * S)), max(3, round(2.9 * S)))
        nx, ny = hx + hrx - 1.0 * S, hy + 2.6 * S
        d.rectangle([nx - S, ny - S * .5, nx + S * .4, ny + S * .4], fill=M_NOSE)
        if mouth:
            d.rectangle([nx - 3 * S, ny + 1.4 * S, nx - 0.5 * S, ny + 3.2 * S], fill=M_MOUTH)   # nom
        else:
            d.line([(nx - 2.6 * S, ny + 2.0 * S), (nx - 1.3 * S, ny + 2.8 * S), (nx, ny + 2.0 * S)], fill=M_MOUTH, width=1)
        d.rectangle([hx + 1.0 * S, hy + 3.6 * S, hx + 1.0 * S + max(1, int(1.5 * S)), hy + 3.6 * S], fill=M_BLUSH)
        # whiskers: rooted on the cheek just behind the nose, sweeping BACK across the face in a little fan
        # (pointing forward they looked like they were coming out of their mouth)
        rx0 = hx + hrx * 0.5
        for k, dy in ((0, -1.4), (1, 0.4), (2, 2.2)):
            d.line([(rx0, ny + (1.2 + k * 0.7) * S), (rx0 - 7.5 * S, ny + (1.2 + k * 0.7 + dy) * S)], fill=M_WHISK)
        return self

    # =================================================== SCRATCH (side view, rearing up against a post on the right)
    def scratch(self, arm=0, bob=0, tired=False, tailph=0.0):
        S, fp, fn, g, cx = self.S, self.fp, self.fn, self.g, self.cx
        d = self.d
        brx = (7.5 + 4 * fp - 1.0 * fn) * S; bry = (13 + 3.5 * fp - 1.5 * fn) * S
        by = g - 3 * S - bry + bob
        hrx = (11 + 2.5 * fp - 1.5 * fn) * S; hry = (10 + 1.5 * fp - 1 * fn) * S
        hx = cx + 5.5 * S; hy = by - bry * 0.82 + (2 * S if tired else 0)
        shx, shy = cx + 4.5 * S, by - bry * 0.2
        # tail: swishes behind (curls up; droops when tired)
        sw = math.sin(tailph) * 3 * S
        if tired: tl = [(cx - brx + S, by + bry * .6), (cx - brx - 5 * S, by + bry * .7), (cx - brx - 9 * S, by + bry * .8 + sw * .3), (cx - brx - 12 * S, by + bry * .95)]
        else: tl = [(cx - brx + S, by + bry * .5), (cx - brx - 6 * S, by + bry * .5), (cx - brx - 10 * S + sw * .4, by - bry * .1), (cx - brx - 6 * S + sw, by - bry * .8)]
        pts = []
        for t in [i / 12 for i in range(13)]:
            a, b, c, e = tl
            pts.append(((1 - t) ** 3 * a[0] + 3 * (1 - t) ** 2 * t * b[0] + 3 * (1 - t) * t * t * c[0] + t ** 3 * e[0],
                        (1 - t) ** 3 * a[1] + 3 * (1 - t) ** 2 * t * b[1] + 3 * (1 - t) * t * t * c[1] + t ** 3 * e[1]))
        self.line(pts, (3.6 + 0.4 * fp) * S, M_FUR)
        # arm positions: (paw x, paw y) for up / mid / down; near arm (in front) and far arm (behind)
        up = (cx + 19 * S, shy - 8 * S); mid = (cx + 19 * S, shy - 1 * S); dn = (cx + 17 * S, shy + 7 * S)
        if tired: near = (cx + 9 * S, shy + 13 * S); far = (cx + 12 * S, shy + 14 * S)
        else:
            near = {0: up, 1: mid, 2: dn, 3: mid}[arm]; far = {0: dn, 1: mid, 2: up, 3: mid}[arm]
        def armf(paw, mat):
            self.line([(shx, shy), (paw[0] - 2 * S, paw[1])], 4.2 * S, mat)
            self.ell(paw[0], paw[1], 3.6 * S, 2.8 * S, mat)
        armf(far, M_FAR)
        # body + belly, haunch and feet
        self.ell(cx, by, brx, bry, M_FUR)
        self.ell(cx - 2 * S, by + bry * 0.55, brx * 0.95, bry * 0.55, M_FUR)
        for fx in (cx - 3 * S, cx + 5 * S):
            self.ell(fx + S, g - 1.8 * S, 4.6 * S, 2.5 * S, M_FUR)
        armf(near, M_FUR)
        # ears: tilt slightly, flick with the arm
        for s_, off in ((1, 0), (-1, 0)):
            ex = hx + s_ * hrx * 0.5 - 1.0 * S
            fl = (1.2 * S if (arm == 0 and s_ == 1) else 0) - (1.2 * S if (arm == 2 and s_ == -1) else 0)
            self.tri([(ex - s_ * 4.2 * S, hy - hry * 0.62), (ex + s_ * 1.0 * S + fl, hy - hry - 4.6 * S), (ex + s_ * 4.4 * S, hy - hry * 0.5)], M_FUR)
            self.tri([(ex - s_ * 2.0 * S, hy - hry * 0.66), (ex + s_ * 1.0 * S + fl, hy - hry - 1.2 * S), (ex + s_ * 3.0 * S, hy - hry * 0.56)], M_EAR)
        self.ell(hx, hy, hrx, hry, M_FUR)
        if fp > 0.6: self.ell(hx - 2 * S, hy + hry * 0.5, hrx * 0.7, hry * 0.6, M_FUR)
        # ---- patterns ----
        p = self.pattern
        if p in ('tux', 'calico', 'tabby'):
            self.paint(lambda dd: dd.ellipse([cx + 0 * S, by - bry * 0.2, cx + brx + 1, by + bry + 2 * S], fill=1), M_WHT)     # belly faces the post
        if p == 'tabby':
            for k in range(4):
                y = by - bry * 0.8 + k * bry * 0.38
                self.paint(lambda dd, y=y: dd.line([(cx - brx, y), (cx - brx * 0.2, y + 1.5 * S)], fill=1, width=max(1, int(1.6 * S))), M_MARK)
            for k in range(3):
                a_, b_ = pts[3 + k * 3]
                self.paint(lambda dd, a_=a_, b_=b_: dd.ellipse([a_ - 2 * S, b_ - 2 * S, a_ + 2 * S, b_ + 2 * S], fill=1), M_MARK)
            for dx in (-3, 0, 3):
                self.paint(lambda dd, dx=dx: dd.line([(hx + dx * S - 2 * S, hy - hry + S), (hx + dx * .8 * S - 2 * S, hy - hry + 5 * S)], fill=1, width=max(1, int(S))), M_MARK)
        if p == 'tux':
            self.paint(lambda dd: dd.ellipse([hx + 1 * S, hy + 1 * S, hx + hrx + 1, hy + hry], fill=1), M_WHT)
        if p == 'calico':
            self.paint(lambda dd: dd.ellipse([cx - brx, by - bry, cx + brx * .2, by - bry * .1], fill=1), M_MARK)
            self.paint(lambda dd: dd.ellipse([hx - hrx * .9, hy - hry, hx + 2 * S, hy - 2 * S], fill=1), M_MARK)
        if p == 'points':
            self.paint(lambda dd: dd.rectangle([hx - hrx - 2, hy - hry - 9 * S, hx + hrx + 2, hy - hry + 3 * S], fill=1), M_MARK)
            self.paint(lambda dd: dd.ellipse([hx + 1 * S, hy - 1 * S, hx + hrx + 2, hy + hry], fill=1), M_MARK)
            self.paint(lambda dd: dd.ellipse([pts[-1][0] - 4 * S, pts[-1][1] - 4 * S, pts[-1][0] + 4 * S, pts[-1][1] + 4 * S], fill=1), M_MARK)
            for paw in (near, far):
                self.paint(lambda dd, paw=paw: dd.ellipse([paw[0] - 4 * S, paw[1] - 3 * S, paw[0] + 4 * S, paw[1] + 3 * S], fill=1), M_MARK, over=(M_FUR, M_FAR, M_WHT))
        d = self.d   # paint() swaps the draw surface
        # toe beans / claws on the raking paws
        for paw in (near, far):
            for k in (-1, 0, 1):
                d.point([(paw[0] + 2.2 * S, paw[1] + k * S * 1.1)], fill=M_TOE)
        # ---- face (3/4 profile looking right at the post) ----
        ew = max(2, round(1.9 * S)); eh = max(3, round(2.6 * S))
        ey = hy - 0.5 * S
        # both eyes the same size and shape, on whole pixels, so they always match
        eyeY = int(round(ey))
        w2 = max(2, round(ew * 0.95)); tw = max(2, round(ew * 0.9)); th = max(3, round(eh))
        lw = max(1, int(S * 0.8))
        for k, ex in enumerate((int(round(hx + 2.2 * S)), int(round(hx + 8.6 * S)))):
            if tired:   # wide worried eyes (a little further in, clear of the panting mouth and the head's edge)
                exT = ex - int(round(1.0 * S)); eyT = eyeY - int(round(0.5 * S))
                self.eye(exT, eyT, tw, th)
                by = eyT - th - 1 - max(1, int(round(S)))
                d.line([(exT - tw, by - 1), (exT + tw - 1, by + 1)], fill=M_PUP)   # worried brow, above the lid
            else:   # blissful closed eyes:  ^ ^
                h2 = max(1, round(S))
                d.line([(ex - w2, eyeY + h2), (ex, eyeY - h2), (ex + w2, eyeY + h2)], fill=M_PUP, width=lw)
        nx, ny = hx + hrx - 1.5 * S, hy + 2.2 * S
        d.rectangle([nx - S, ny - S * .5, nx + S * .6, ny + S * .4], fill=M_NOSE)
        my = ny + 2.6 * S
        if tired:
            d.rectangle([nx - 3 * S, my - S, nx, my + 1.5 * S], fill=M_MOUTH)        # panting mouth
            d.rectangle([nx - 2 * S, my + 0.5 * S, nx - 0.5 * S, my + 2.5 * S], fill=M_NOSE)   # tongue
            d.point([(hx + hrx * 0.2, hy - hry - 2 * S)], fill=M_SHN)               # sweat drop
            d.point([(hx + hrx * 0.2, hy - hry - 1 * S)], fill=M_SHN)
        else:
            d.line([(nx - 3 * S, my), (nx - 1.5 * S, my + S * .9), (nx, my)], fill=M_MOUTH, width=1)   # little smile
        d.rectangle([hx + 4 * S, hy + 3.4 * S, hx + 4 * S + max(1, int(1.5 * S)), hy + 3.4 * S + 1], fill=M_BLUSH)   # rosy cheek
        # whiskers: rooted on the cheek just behind the nose, sweeping BACK across the face in a little fan
        # (pointing forward they looked like they were coming out of their mouth)
        rx0 = hx + hrx * 0.5
        for k, dy in ((0, -1.4), (1, 0.4), (2, 2.2)):
            d.line([(rx0, ny + (1.2 + k * 0.7) * S), (rx0 - 7.5 * S, ny + (1.2 + k * 0.7 + dy) * S)], fill=M_WHISK)
        return self

    # =================================================== BACK (sitting facing away: looking out of the window / sulking)
    # frame 0 still, 1 ear twitch, 2 tail-tip flick, 3 glancing back over their shoulder (one eye peeks round),
    # 4..7 "bird tracker": tail raised beside them in a slow swish while their head tilts left and right.
    def _bez4(self, p, n=13):
        a, b, c, e = p; out = []
        for i in range(n):
            t = i / (n - 1)
            out.append(((1-t)**3*a[0] + 3*(1-t)**2*t*b[0] + 3*(1-t)*t*t*c[0] + t**3*e[0], (1-t)**3*a[1] + 3*(1-t)**2*t*b[1] + 3*(1-t)*t*t*c[1] + t**3*e[1]))
        return out

    def ears_back(self, hx, hy, hrx, hry, twitch=0.0, tilt=0.0):
        S = self.S
        for s in (-1, 1):
            ex = hx + s * hrx * 0.55
            tw = (twitch if s > 0 else 0) + tilt
            # the furry backs of the ears, with just a sliver of pink at the inner edge
            self.tri([(ex - s * 4.4 * S, hy - hry * 0.55), (ex + s * 1.0 * S + tw, hy - hry - 5.0 * S), (ex + s * 4.6 * S, hy - hry * 0.35)], M_FUR)
            self.tri([(ex - s * 3.6 * S, hy - hry * 0.62), (ex + s * 0.6 * S + tw, hy - hry - 3.6 * S), (ex - s * 2.0 * S, hy - hry * 0.66)], M_EAR)
        return [(hx + s * hrx * 0.55, s) for s in (-1, 1)]

    def back(self, frame=0):
        S, fp, fn, g, cx = self.S, self.fp, self.fn, self.g, self.cx
        tracker = frame >= 4
        ph = (frame - 4) * math.pi / 2 if tracker else (math.pi / 2 if frame == 2 else 0.0)
        twitch = 2.5 * S if frame == 1 else 0.0
        tilt = math.sin(ph) * 1.6 * S if tracker else 0.0
        glance = 1 if frame == 3 else 0
        brx = (11 + 9 * fp - 3 * fn) * S; bry = (13 + 2 * fp + 2 * fn) * S
        by = g - bry
        hrx = (12 + 2.5 * fp - 2 * fn) * S; hry = (10.5 + 0.5 * fp - 1.2 * fn) * S
        hx = cx + tilt + glance * 2.5 * S; hy = by - bry + 2 * S
        if tracker:   # raised beside them in a big slow swish
            sw = math.sin(ph) * 5 * S
            tp = [(cx + brx * 0.4, g - 3 * S), (cx + brx + 6 * S, g - 6 * S), (cx + brx + 9 * S + sw * 0.5, by - bry * 0.2), (cx + brx + 4 * S + sw, by - bry * 0.9)]
        else:         # curled on the floor round their side, the tip flicking
            fl = (1.6 * S if frame == 2 else -0.6 * S)
            tp = [(cx + brx * 0.3, g - 2 * S), (cx + brx * 0.9, g - 1.5 * S), (cx + brx + 4 * S, g - 3 * S + fl * 0.3), (cx + brx + 7 * S, g - 6 * S - fl)]
        pts = self._bez4(tp)
        self.ell(cx, by, brx, bry, M_FUR)                                                           # the pear-shaped back
        self.ell(cx, g - 6 * S, brx + 2 * S, 6.5 * S, M_FUR)                                        # haunches
        for s in (-1, 1): self.ell(cx + s * (brx - 1 * S), g - 1.8 * S, 4 * S, 2.3 * S, M_FUR)       # back feet peeking out
        ears = self.ears_back(hx, hy, hrx, hry, twitch, tilt * 0.4)
        self.ell(hx, hy, hrx, hry, M_FUR)
        self.line(pts, (3.8 + 0.4 * fp) * S, M_FUR)
        self.ell(pts[-1][0], pts[-1][1], 1.9 * S, 1.9 * S, M_FUR)
        p = self.pattern
        if p == 'tabby':
            for k in range(4):   # stripes curving round them back
                y = by - bry * 0.55 + k * bry * 0.38
                self.paint(lambda d, y=y: d.arc([cx - brx, y - 3 * S, cx + brx, y + 5 * S], 200, 340, fill=1, width=max(1, int(1.6 * S))), M_MARK)
            for dx in (-3, 0, 3):
                self.paint(lambda d, dx=dx: d.line([(hx + dx * S, hy - hry + S), (hx + dx * 0.8 * S, hy + hry * 0.4)], fill=1, width=max(1, int(S))), M_MARK)
            for k in range(3):
                q = pts[3 + k * 3]
                self.paint(lambda d, q=q: d.ellipse([q[0] - 2 * S, q[1] - 2 * S, q[0] + 2 * S, q[1] + 2 * S], fill=1), M_MARK)
        if p == 'tux':   # all black from behind, just the white feet
            for s in (-1, 1): self.paint(lambda d, s=s: d.ellipse([cx + s * (brx - S) - 4.5 * S, g - 4.5 * S, cx + s * (brx - S) + 4.5 * S, g + 1], fill=1), M_WHT)
        if p == 'calico':
            self.paint(lambda d: d.ellipse([cx - brx - S, by - bry * 0.7, cx + brx * 0.1, by + bry * 0.3], fill=1), M_MARK)
            self.paint(lambda d: d.ellipse([cx + brx * 0.2, by, cx + brx + 3 * S, g], fill=1), M_FAR)
            self.paint(lambda d: d.ellipse([hx - hrx, hy - hry - 2 * S, hx - S, hy + 2 * S], fill=1), M_MARK)
        if p == 'points':   # dark ears, tail and feet
            for ex, s in ears:
                self.paint(lambda d, ex=ex, s=s: d.polygon([(ex - s * 4.4 * S, hy - hry * 0.55), (ex + s * 1.0 * S, hy - hry - 5.0 * S), (ex + s * 4.6 * S, hy - hry * 0.35)], fill=1), M_MARK, over=(M_FUR, M_FAR, M_EAR))
            self.paint(lambda d: d.line(pts[5:], fill=1, width=int(5 * S)), M_MARK)
            for s in (-1, 1): self.paint(lambda d, s=s: d.ellipse([cx + s * (brx - S) - 4.5 * S, g - 4.5 * S, cx + s * (brx - S) + 4.5 * S, g + 1], fill=1), M_MARK)
        d = self.d
        if glance:   # one eye, a nose tip and whiskers peek round the side of their head
            ex = hx + hrx - 3.5 * S; ey = hy + 0.5 * S
            self.eye(ex, ey, max(2, round(1.6 * S)), max(3, round(2.6 * S)))
            nx = hx + hrx + 0.2 * S; ny = hy + 3 * S
            d.rectangle([nx - S * .6, ny - S * .5, nx + S * .6, ny + S * .4], fill=M_NOSE)
            d.rectangle([ex - 2 * S, hy + 3.6 * S, ex - 2 * S + max(1, int(S)), hy + 3.6 * S], fill=M_BLUSH)
            for k in (0, 2): d.line([(hx + hrx - 2 * S, ny + (k - 1) * S), (hx + hrx + 6 * S, ny + (k * 1.6 - 2) * S)], fill=M_WHISK)
            for k in (0, 2): d.line([(hx - hrx + 1 * S, hy + (3 + k) * S), (hx - hrx - 3 * S, hy + (2 + k * 1.4) * S)], fill=M_WHISK)
        else:        # whisker tips poke out either side of their cheeks
            for s in (-1, 1):
                for k in (0, 2): d.line([(hx + s * (hrx - 1 * S), hy + (3 + k) * S), (hx + s * (hrx + 3 * S), hy + (2 + k * 1.4) * S)], fill=M_WHISK)
        return self

    # =================================================== SLEEP (loaf)
    def sleep(self, breathe=0):
        S, fp, fn, g, cx = self.S, self.fp, self.fn, self.g, self.cx
        rx = (17 + 6 * fp - 2 * fn) * S; ry = (8.5 + 4 * fp - 2 * fn) * S + breathe
        by = g - ry + 1
        # tail curled round the front
        self.line([(cx - rx + 2 * S, by + ry * .2), (cx - rx * .3, g - S * 1.5), (cx + rx * .6, g - 1.2 * S)], (3.4 + 0.8 * fp) * S, M_FUR)
        self.ell(cx, by, rx, ry, M_FUR)
        if fp > 0.6: self.ell(cx, by + ry * .35, rx * 0.95, ry * .8, M_FUR)
        hrx = (11.5 + 2.5 * fp - 1.5 * fn) * S; hry = (9.5 + 1.5 * fp - 1 * fn) * S
        hx, hy = cx + rx * 0.55, by + ry * 0.12
        self.ears(hx, hy, hrx, hry, near_only=True)
        self.ell(hx, hy, hrx, hry, M_FUR)
        self.ell(hx + 3 * S, g - 2.5 * S, 5 * S, 2.5 * S, M_FUR)      # paws tucked under chin
        p = self.pattern
        if p in ('tux', 'calico', 'tabby'):
            self.paint(lambda d: d.ellipse([hx - 3 * S, hy + 2 * S, hx + hrx, hy + hry + S], fill=1), M_WHT)
            self.paint(lambda d: d.ellipse([hx - 2 * S, g - 5 * S, hx + 8 * S, g + 1], fill=1), M_WHT)
        if p == 'tabby':
            for k in range(5):
                x = cx - rx * 0.75 + k * rx * 0.33
                self.paint(lambda d, x=x: d.line([(x, by - ry), (x + S, by - ry * .1)], fill=1, width=max(1, int(1.6 * S))), M_MARK)
            for dx in (-3, 0, 3):
                self.paint(lambda d, dx=dx: d.line([(hx + dx * S, hy - hry + S), (hx + dx * .8 * S, hy - hry + 5 * S)], fill=1, width=max(1, int(S))), M_MARK)
        if p == 'calico':
            self.paint(lambda d: d.ellipse([cx - rx * .3, by - ry, cx + rx * .4, by + ry * .1], fill=1), M_MARK)
            self.paint(lambda d: d.ellipse([cx - rx, by - ry * .4, cx - rx * .4, by + ry], fill=1), M_FAR)
        if p == 'points':
            self.paint(lambda d: d.rectangle([hx - hrx - 2, hy - hry - 9 * S, hx + hrx + 2, hy - hry + 3 * S], fill=1), M_MARK)
            self.paint(lambda d: d.ellipse([hx - 4 * S, hy + S, hx + hrx, hy + hry], fill=1), M_MARK)
        d = self.d
        for s in (-1, 1):   # closed eyes ^ ^ and a tiny smile
            ex = int(round(hx + s * 5 * S)); r = int(round(2.5 * S))
            d.arc([ex - r, int(round(hy - 0.5 * S)), ex + r, int(round(hy + 4 * S))], 200, 340, fill=M_PUP, width=1)
        d.rectangle([hx - S * .5, hy + 3 * S, hx + S * .5, hy + 3 * S + max(0, int(S) - 1)], fill=M_NOSE)
        d.point([(hx - S, hy + 5 * S), (hx, hy + 5.8 * S), (hx + S, hy + 5 * S)], fill=M_MOUTH)
        d.rectangle([hx - 8 * S, hy + 3 * S, hx - 8 * S + int(S), hy + 3 * S], fill=M_BLUSH)
        return self

    # =================================================== finish -> role image
    def finish(self):
        a = np.array(self.L).astype(int)
        cat = a != M_BG
        def sh(dx, dy):
            s = np.zeros_like(cat)
            s[max(dy, 0):HH + min(dy, 0), max(dx, 0):W + min(dx, 0)] = cat[max(-dy, 0):HH + min(-dy, 0), max(-dx, 0):W + min(-dx, 0)]
            return s
        rim_d2 = cat & ~sh(-2, -2); rim_d1 = cat & ~sh(-1, -1); rim_l = cat & ~sh(2, 2)
        yy, xx = np.mgrid[0:HH, 0:W]
        dith = ((xx + yy) % 2 == 0)
        role = np.zeros((HH, W), np.uint8)
        base = {M_FUR: (R_FUR, R_FURD, R_FURL), M_MARK: (R_MARK, R_MARKD, R_MARK), M_WHT: (R_WHT, R_WHTD, R_WHT), M_FAR: (R_FURD, R_FURD, R_FURD)}
        for mat, (b, dk, lt) in base.items():
            m = a == mat
            role[m] = b
            role[m & rim_d2 & ~rim_d1 & dith] = dk
            role[m & rim_d1] = dk
            role[m & rim_l & ~rim_d1] = lt
        flat = {M_EAR: R_EAR, M_EYE: R_EYE, M_PUP: R_PUP, M_SHN: R_SHN, M_NOSE: R_NOSE, M_MOUTH: R_MOUTH, M_BLUSH: R_BLUSH, M_WHISK: R_WHT, M_TOE: (R_WHTD if self.pattern in ('tux', 'calico') else R_FURD)}
        for mat, r in flat.items(): role[a == mat] = r
        # under-chin / head-over-body contact shadow: darken fur just below the head (front + side poses)
        out = np.zeros_like(cat)
        body = cat & (a != M_WHISK)          # whiskers get no outline (it made them look thick and dark)
        def shb(dx, dy):
            s2 = np.zeros_like(cat)
            s2[max(dy, 0):HH + min(dy, 0), max(dx, 0):W + min(dx, 0)] = body[max(-dy, 0):HH + min(-dy, 0), max(-dx, 0):W + min(-dx, 0)]
            return s2
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)): out |= shb(dx, dy)
        out &= ~cat
        role[out] = R_OUT
        return role

def crop(role):
    ys, xs = np.nonzero(role)
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    return role[y0:y1, x0:x1].copy(), (W // 2 - x0, HH - 8 - y0)   # anchor = ground contact, horizontal centre

def build(body, pattern='tabby'):
    fr = {}
    fr['sit'] = [crop(Cat(body, pattern).sit(False, 0).finish()), crop(Cat(body, pattern).sit(True, 0).finish()), crop(Cat(body, pattern).sit(False, 1 if body != 'chonk' else 1).finish())]
    fr['walk'] = [crop(Cat(body, pattern).side(k * math.pi / 2, 0, False, (1 if k % 2 else 0)).finish()) for k in range(4)]
    fr['sleep'] = [crop(Cat(body, pattern).sleep(0).finish()), crop(Cat(body, pattern).sleep(1).finish())]
    fr['eat'] = [crop(Cat(body, pattern).side(0, 1, False, 0).finish()), crop(Cat(body, pattern).side(0, 1, True, 1).finish())]
    fr['back'] = [crop(Cat(body, pattern).back(k).finish()) for k in range(8)]
    fr['scratch'] = [crop(Cat(body, pattern).scratch(a, b, False, a * 1.3).finish()) for a, b in ((0, 0), (1, 1), (2, 0), (3, 1))] + [crop(Cat(body, pattern).scratch(0, 0, True, 0).finish())]
    return fr

# ------------------------------------------------------------------ preview
def H(h): return ((h >> 16) & 255, (h >> 8) & 255, h & 255)
PAL = {  # role -> rgb, by pattern/breed look
    'tabby': [None, H(0x5A2A18), H(0xF0A040), H(0xC8702A), H(0xFFD08A), H(0xC86A24), H(0xA0501A), H(0xFFF2DE), H(0xE8C8A8), H(0xF7A8A8), H(0xFFFFFF), H(0x2A1A2E), H(0xFFFFFF), H(0xF07A90), H(0x6A3040), H(0xFF9AA8)],
    'tux': [None, H(0x14141C), H(0x3A3A4A), H(0x24242E), H(0x5A5A70), H(0x2A2A36), H(0x1E1E28), H(0xF4F4F8), H(0xC8C8D8), H(0xE8909C), H(0xC8E870), H(0x1A2A1A), H(0xFFFFFF), H(0xF07A90), H(0x6A3040), H(0xFF9AA8)],
    'calico': [None, H(0x4A3228), H(0xFFF4E4), H(0xE0CCB4), H(0xFFFFFF), H(0xEE9A48), H(0xC87430), H(0xFFFFFF), H(0xE8DCD0), H(0xF7A8B4), H(0xFFFFFF), H(0x2A1A2E), H(0xFFFFFF), H(0xF07A90), H(0x6A3040), H(0xFF9AA8)],
}
def render(role, pal):
    a = np.zeros(role.shape + (4,), np.uint8)
    for i, c in enumerate(pal):
        if c is None: continue
        m = role == i; a[m, :3] = c; a[m, 3] = 255
    return Image.fromarray(a, 'RGBA')

if __name__ == '__main__':
    import sys
    from PIL import ImageFont
    out = sys.argv[1]; k = 4
    rows = []
    for body, pat in (('skinny', 'tabby'), ('normal', 'tux'), ('chonk', 'calico')):
        fr = build(body, pat)
        order = [('sit', 0), ('sit', 1), ('walk', 0), ('walk', 1), ('walk', 2), ('walk', 3), ('eat', 0), ('eat', 1), ('sleep', 0), ('sleep', 1)]
        rows.append((body, [(n, i, fr[n][i]) for n, i in order]))
    cw, chh = 128 * 3, 84 * 3
    sheet = Image.new('RGB', (10 * cw // 3 + 20, 3 * chh + 20), (36, 30, 52))
    f = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 13)
    d = ImageDraw.Draw(sheet)
    sc = 2
    sheet = Image.new('RGB', (10 * 128 + 20, 3 * 190 + 20), (36, 30, 52)); d = ImageDraw.Draw(sheet)
    for r, (body, items) in enumerate(rows):
        y0 = 10 + r * 190
        d.rectangle([0, y0 + 150, sheet.width, y0 + 190], fill=(52, 44, 72))
        d.text((6, y0 + 2), body, fill=(255, 226, 160), font=f)
        pat = ('tabby', 'tux', 'calico')[r]
        for c, (n, i, (role, anc)) in enumerate(items):
            im = render(role, PAL[pat]).resize((role.shape[1] * sc, role.shape[0] * sc), Image.NEAREST)
            x0 = 10 + c * 128 + 64 - anc[0] * sc; yb = y0 + 168
            sheet.paste(im, (int(x0), int(yb - anc[1] * sc)), im)
            d.text((10 + c * 128 + 4, y0 + 172), f'{n}{i}', fill=(190, 180, 215), font=f)
    sheet.save(out + '/catgen.png'); print('ok', sheet.size)
