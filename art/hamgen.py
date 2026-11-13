"""The secret Hamster: a tiny golden ball with big ol' cheek pouches. Same frame set as the cats
(sit 3, walk 4, sleep 2, eat 2, scratch 5, back 8), same role palette, so it bakes and draws like any breed."""
import math
from catgen import Cat, M_FUR, M_MARK, M_WHT, M_EAR, M_EYE, M_PUP, M_SHN, M_NOSE, M_MOUTH, M_BLUSH, M_FAR, M_WHISK, M_TOE

class Hamster(Cat):
    def __init__(self, body, pattern='hamster'):
        super().__init__(body, pattern)
        self.S *= 0.78                       # hamsters are tiny
        self.r = {'skinny': 0.88, 'normal': 1.0, 'chonk': 1.22}[body]   # roundness

    def ear(self, x, y, r):
        self.ell(x, y, r, r, M_FUR); self.ell(x, y + 0.3 * r, r * 0.55, r * 0.55, M_EAR)

    def cheeks(self, hx, hy, hrx, hry, puff=1.0, sides=(-1, 1)):
        S = self.S
        for s in sides:
            self.ell(hx + s * hrx * 0.80, hy + hry * 0.5, 6.1 * S * puff, 4.9 * S * puff, M_FUR)   # (a little smaller than before: less owl)

    def bead(self, ex, ey):
        S = self.S; d = self.d
        r = max(2, round(2.5 * S))   # big shiny eyes
        d.ellipse([ex - r, ey - r, ex + r, ey + r], fill=M_EYE)
        d.rectangle([ex - r + 1, ey - r + 1, ex - r + 1 + max(0, int(S * 0.6)), ey - r + 1 + max(0, int(S * 0.6))], fill=M_SHN)

    # ---------------- front sit
    def sit(self, blink=False, bob=0):
        S, g, cx, r = self.S, self.g, self.cx, self.r
        brx, bry = 13 * S * r, 11 * S * (0.92 + 0.08 * r)
        bcy = g - bry
        hrx, hry = 11 * S * (0.95 + 0.05 * r), 9 * S
        hy = bcy - bry * 0.55 + bob
        self.ell(cx, bcy, brx, bry, M_FUR)                                   # the ball
        for s in (-1, 1): self.ear(cx + s * hrx * 0.62, hy - hry * 0.78, 3.4 * S)
        self.ell(cx, hy, hrx, hry, M_FUR)
        self.cheeks(cx, hy, hrx, hry, 1.0 + 0.1 * (r - 1))
        # cream muzzle + belly
        self.paint(lambda d: d.ellipse([cx - brx * 0.62, bcy - bry * 0.35, cx + brx * 0.62, g - S], fill=1), M_WHT)
        self.paint(lambda d: d.ellipse([cx - hrx * 0.55, hy + hry * 0.05, cx + hrx * 0.55, hy + hry * 1.05], fill=1), M_WHT)
        for s in (-1, 1):
            self.paint(lambda d, s=s: d.ellipse([cx + s * hrx * 0.78 - 3.6 * S, hy + hry * 0.45 - 2.0 * S, cx + s * hrx * 0.78 + 3.6 * S, hy + hry * 0.45 + 4.0 * S], fill=1), M_WHT)
        # a stripe of darker gold down the forehead
        self.paint(lambda d: d.ellipse([cx - 2.2 * S, hy - hry, cx + 2.2 * S, hy - hry * 0.2], fill=1), M_MARK)
        # tiny pink paws held together at the chest, and feet
        d = self.d
        for s in (-1, 1):
            d.ellipse([cx + s * 2.6 * S - 1.7 * S, bcy - 1.5 * S, cx + s * 2.6 * S + 1.7 * S, bcy + 1.5 * S], fill=M_EAR)
            d.ellipse([cx + s * brx * 0.55 - 2.6 * S, g - 2.2 * S, cx + s * brx * 0.55 + 2.6 * S, g], fill=M_EAR)
        ey = hy - 0.5 * S
        for s in (-1, 1):
            ex = cx + s * 4.9 * S
            if blink:
                x0 = int(round(ex - 1.8 * S)); hw = int(round(3.6 * S))
                d.line([(x0, int(round(ey))), (x0 + hw, int(round(ey)))], fill=M_PUP, width=max(1, int(S)))
            else: self.bead(ex, ey)
            d.ellipse([cx + s * hrx * 0.8 - 2 * S, hy + hry * 0.5 - 1 * S, cx + s * hrx * 0.8 + 2 * S, hy + hry * 0.5 + 1 * S], fill=M_BLUSH)
        d.rectangle([cx - S * 0.8, hy + 2.2 * S, cx + S * 0.8, hy + 2.2 * S + max(0, int(S) - 1)], fill=M_NOSE)
        my = hy + 3.6 * S
        d.line([(cx - 1.8 * S, my), (cx - 0.9 * S, my + 0.9 * S), (cx, my), (cx + 0.9 * S, my + 0.9 * S), (cx + 1.8 * S, my)], fill=M_MOUTH, width=1)
        d.rectangle([cx - 0.7 * S, my + 1.2 * S, cx + 0.7 * S, my + 2.4 * S], fill=M_WHISK)    # two little front teeth
        return self

    # ---------------- side (walk / eat), facing right
    def side(self, phase=0.0, eat=0, mouth=False, bob=0):
        S, g, cx, r = self.S, self.g, self.cx, self.r
        brx, bry = 15 * S * r, 9.5 * S * (0.92 + 0.08 * r)
        by = g - 2.2 * S - bry + bob * 0.6
        hx = cx + brx * 0.55 + (2 * S if eat else 0); hy = by - bry * 0.25 + (5 * S if eat else 0)
        hrx, hry = 9.5 * S, 8 * S
        d = self.d
        # four tiny feet shuffling underneath
        for k, (fx, off) in enumerate(((-0.55, 0), (-0.25, math.pi), (0.25, math.pi), (0.55, 0))):
            lift = max(0.0, math.sin(phase + off)) * 1.6 * S if not eat else 0
            step = math.cos(phase + off) * 1.4 * S if not eat else 0
            x = cx + fx * brx + step
            d.ellipse([x - 2.2 * S, g - 2.6 * S - lift, x + 2.2 * S, g - lift], fill=M_EAR)
        self.ell(cx - brx + 1.5 * S, by + bry * 0.25, 2.2 * S, 1.8 * S, M_FUR)                # nub tail
        self.ell(cx, by, brx, bry, M_FUR)
        self.ear(hx - 1.5 * S, hy - hry * 0.85, 3.2 * S)
        self.ell(hx, hy, hrx, hry, M_FUR)
        puff = 1.15 if (eat and mouth) else 1.0
        self.ell(hx + 0.5 * S, hy + hry * 0.6, 7.2 * S * puff, 6.0 * S * puff, M_FUR)          # the cheek pouch
        self.paint(lambda d: d.ellipse([cx - brx * 0.7, by + bry * 0.15, cx + brx * 0.75, g], fill=1), M_WHT)
        self.paint(lambda d: d.ellipse([hx - 2 * S, hy + hry * 0.1, hx + hrx + 1 * S, hy + hry * 1.25], fill=1), M_WHT)
        self.paint(lambda d: d.ellipse([cx - brx * 0.45, by - bry, cx + brx * 0.25, by - bry * 0.35], fill=1), M_MARK)   # darker saddle
        d = self.d
        ex, ey = hx + 3.2 * S, hy - 1.2 * S
        self.bead(ex, ey)
        d.ellipse([hx + 1 * S, hy + hry * 0.5 - S, hx + 4.5 * S, hy + hry * 0.5 + S], fill=M_BLUSH)
        nx = hx + hrx - 0.4 * S; ny = hy + 1.6 * S
        d.rectangle([nx - S * 0.7, ny - S * 0.5, nx + S * 0.7, ny + S * 0.5], fill=M_NOSE)
        if eat:   # nibbling: paws up to the mouth, a seed
            d.ellipse([hx + hrx - 4 * S, hy + hry * 0.8 - 1.5 * S, hx + hrx, hy + hry * 0.8 + 1.5 * S], fill=M_EAR)
            if mouth: d.rectangle([nx - 0.2 * S, ny + 1.4 * S, nx + 0.6 * S, ny + 2.6 * S], fill=M_WHISK)
        for k in (0, 2): d.line([(nx - S, ny + (k - 1) * 0.8 * S), (nx + 3.6 * S, ny + (k * 1.2 - 1.6) * S)], fill=M_WHISK)
        return self

    # ---------------- sleep: a curled-up ball
    def sleep(self, breathe=0):
        S, g, cx, r = self.S, self.g, self.cx, self.r
        rx, ry = 15 * S * r, 9.5 * S * (0.92 + 0.08 * r) + breathe * 0.8
        by = g - ry
        self.ell(cx, by, rx, ry, M_FUR)
        hx, hy = cx + rx * 0.42, by + ry * 0.2
        self.ear(hx - 4 * S, hy - 7.5 * S, 3 * S)
        self.cheeks(hx, hy, 7 * S, 7 * S, 0.95, sides=(1,))
        self.paint(lambda d: d.ellipse([hx - 5 * S, hy - 1 * S, hx + 9 * S, g], fill=1), M_WHT)
        self.paint(lambda d: d.ellipse([cx - rx * 0.6, by - ry, cx + rx * 0.2, by - ry * 0.3], fill=1), M_MARK)
        d = self.d
        for s in (-1, 1):
            ex = hx + s * 3.4 * S
            d.arc([ex - 1.8 * S, hy - 1.5 * S, ex + 1.8 * S, hy + 1.5 * S], 200, 340, fill=M_PUP, width=1)
        d.ellipse([hx + 4.5 * S, hy + 2 * S, hx + 7.5 * S, hy + 3.6 * S], fill=M_BLUSH)
        d.rectangle([hx - S * 0.5, hy + 2.2 * S, hx + S * 0.5, hy + 2.2 * S + max(0, int(S) - 1)], fill=M_NOSE)
        d.ellipse([hx - 1 * S, g - 2.4 * S, hx + 3 * S, g], fill=M_EAR)                       # paws tucked in
        return self

    # ---------------- back: watching out of the window
    def back(self, frame=0):
        S, g, cx, r = self.S, self.g, self.cx, self.r
        tracker = frame >= 4
        ph = (frame - 4) * math.pi / 2 if tracker else 0.0
        tilt = math.sin(ph) * 1.5 * S if tracker else 0.0
        glance = frame == 3
        twitch = 1.4 * S if frame == 1 else 0.0
        brx, bry = 13 * S * r, 11 * S * (0.92 + 0.08 * r)
        bcy = g - bry
        hrx, hry = 11 * S, 9 * S
        hx, hy = cx + tilt + (2 * S if glance else 0), bcy - bry * 0.55
        self.ell(cx, bcy, brx, bry, M_FUR)
        wig = 1.2 * S if frame == 2 else 0
        self.ell(cx + wig, g - 2.8 * S, 2.4 * S, 2 * S, M_FUR)                               # nub tail
        for s in (-1, 1):
            self.ear(hx + s * hrx * 0.62, hy - hry * 0.78 - (twitch if s > 0 else 0), 3.4 * S)
        self.ell(hx, hy, hrx, hry, M_FUR)
        self.cheeks(hx, hy, hrx, hry, 0.9)                                                    # cheeks bulge out either side
        self.paint(lambda d: d.ellipse([hx - 2.6 * S, hy - hry, hx + 2.6 * S, bcy + bry * 0.4], fill=1), M_MARK)   # dark stripe down the back
        d = self.d
        for s in (-1, 1): d.ellipse([cx + s * brx * 0.55 - 2.6 * S, g - 2 * S, cx + s * brx * 0.55 + 2.6 * S, g], fill=M_EAR)
        if glance:
            self.bead(hx + hrx * 0.78, hy - 0.5 * S)
            d.rectangle([hx + hrx + 0.6 * S, hy + 1.6 * S, hx + hrx + 1.6 * S, hy + 2.4 * S], fill=M_NOSE)
        if frame == 2: d.point([(cx + wig + 3 * S, g - 4 * S)], fill=M_FURL if False else M_WHISK)
        return self

    # ---------------- scratch: up on its back feet, paws scrabbling at the post (right)
    def scratch(self, arm=0, bob=0, tired=False, tailph=0.0):
        S, g, cx, r = self.S, self.g, self.cx, self.r
        brx, bry = 10.5 * S * r, 13.5 * S
        bcy = g - bry - 0.5 * S
        hx, hy = cx + 2.5 * S, bcy - bry * 0.82 + (2 * S if tired else 0) - bob * 0.5
        hrx, hry = 9.5 * S, 8 * S
        d = self.d
        for s in (-1, 1): d.ellipse([cx + s * 4 * S - 2.8 * S, g - 2.4 * S, cx + s * 4 * S + 2.8 * S, g], fill=M_EAR)
        self.ell(cx - brx + 1.5 * S, g - 3 * S, 2.2 * S, 1.8 * S, M_FUR)
        self.ell(cx, bcy, brx, bry, M_FUR)
        self.ear(hx - hrx * 0.55, hy - hry * 0.8, 3.1 * S); self.ear(hx + hrx * 0.45, hy - hry * 0.85, 3.1 * S)
        self.ell(hx, hy, hrx, hry, M_FUR)
        self.cheeks(hx, hy, hrx, hry, 0.95)
        self.paint(lambda d: d.ellipse([cx - brx * 0.6, bcy - bry * 0.5, cx + brx * 0.7, g - S], fill=1), M_WHT)
        self.paint(lambda d: d.ellipse([hx - hrx * 0.5, hy, hx + hrx * 0.6, hy + hry * 1.05], fill=1), M_WHT)
        d = self.d
        ys = [-2, 2, -1, 3] if not tired else [4, 4, 4, 4]
        for k, s in enumerate((-1, 1)):
            py = bcy - bry * 0.35 + ys[(arm + k) % 4] * S
            d.ellipse([cx + brx * 0.55 + k * 2 * S - 2 * S, py - 1.7 * S, cx + brx * 0.55 + k * 2 * S + 2 * S, py + 1.7 * S], fill=M_EAR)
        for s in (-1, 1):
            ex = hx + s * 4.6 * S + 1 * S
            if tired:   # closed eyes, whole pixels so both come out the same
                x0 = int(round(ex - 1.6 * S)); hw = int(round(3.2 * S))
                d.line([(x0, int(round(hy))), (x0 + hw, int(round(hy)))], fill=M_PUP, width=max(1, int(S)))
            else: self.bead(ex, hy - 0.5 * S)
        d.rectangle([hx + 1 * S - 0.7 * S, hy + 2.2 * S, hx + 1 * S + 0.7 * S, hy + 2.8 * S], fill=M_NOSE)
        return self

def build(body):
    import catgen as C
    def H(*a): return C.crop(Hamster(body).__getattribute__(a[0])(*a[1:]).finish())
    fr = {}
    fr['sit'] = [H('sit', False, 0), H('sit', True, 0), H('sit', False, 1)]
    fr['walk'] = [H('side', k * math.pi / 2, 0, False, (1 if k % 2 else 0)) for k in range(4)]
    fr['sleep'] = [H('sleep', 0), H('sleep', 1)]
    fr['eat'] = [H('side', 0, 1, False, 0), H('side', 0, 1, True, 1)]
    fr['back'] = [H('back', k) for k in range(8)]
    fr['scratch'] = [H('scratch', a, b, False, 0) for a, b in ((0, 0), (1, 1), (2, 0), (3, 1))] + [H('scratch', 0, 0, True, 0)]
    return fr
