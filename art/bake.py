#!/usr/bin/env python3
"""Bake all cat frames into a C++ data file (flash resident, const).
Format: frames are RLE of 4-bit role ids, row-major: byte = (role << 4) | run, run 1..15.
Frame table order: ((set * 3 + body) * NF + animBase[anim] + idx).  Sets: tabby, tux, calico, plain, points, hamster, fluffy."""
import sys, os
import numpy as np
import catgen as C
import hamgen
import fluffgen

SETS = ['tabby', 'tux', 'calico', 'plain', 'points', 'hamster', 'fluffy']
BODIES = ['skinny', 'normal', 'chonk']
ANIMS = [('sit', 3), ('walk', 4), ('sleep', 2), ('eat', 2), ('scratch', 5), ('back', 8)]
NF = sum(n for _, n in ANIMS)

def H(h): return ((h >> 16) & 255, (h >> 8) & 255, h & 255)
def rgb565(c): return ((c[0] & 0xF8) << 8) | ((c[1] & 0xFC) << 3) | (c[2] >> 3)

# role order: T, OUT, FUR, FURD, FURL, MARK, MARKD, WHT, WHTD, EAR, EYE, PUP, SHN, NOSE, MOUTH, BLUSH
def P(out, fur, furd, furl, mark, markd, wht, whtd, ear, eye, pup, nose=0xF07A90, mouth=0x6A3040, blush=0xFF9AA8):
    return [(0, 0, 0), H(out), H(fur), H(furd), H(furl), H(mark), H(markd), H(wht), H(whtd), H(ear), H(eye), H(pup), (255, 255, 255), H(nose), H(mouth), H(blush)]

# breed -> (set, palette)       order matches breeds.h / Species: Tabby, Siamese, Tuxedo, Void, Calico, Sphynx, Skogkatt, Kitten, Rainbow, Hamster
BREEDS = [
    ('tabby', P(0x5A2A18, 0xF0A040, 0xC8702A, 0xFFD08A, 0xC86A24, 0xA0501A, 0xFFF2DE, 0xE8C8A8, 0xF7A8A8, 0xA8E060, 0x2A1A2E)),
    ('points', P(0x4A3430, 0xFBEBD2, 0xDCC4A4, 0xFFFAEE, 0x6A4A40, 0x4A3228, 0xFFF4E0, 0xE4D0B4, 0xC8A090, 0x9CD0F4, 0x1A2030)),
    ('tux', P(0x14141C, 0x3A3A4A, 0x24242E, 0x5A5A70, 0x2A2A36, 0x1E1E28, 0xF4F4F8, 0xC8C8D8, 0xE8909C, 0xC8E870, 0x1A2A1A)),
    ('plain', P(0x0A0810, 0x2A2632, 0x18141E, 0x46404E, 0x2A2632, 0x18141E, 0x46404E, 0x2A2632, 0x7A5A6A, 0xFFD040, 0x050308, 0x7A5A6A, 0x3A2A3A, 0x4A3A4A)),
    ('calico', P(0x4A3228, 0xFFF4E4, 0xE0CCB4, 0xFFFFFF, 0xEE9A48, 0xC87430, 0xFFFFFF, 0xE8DCD0, 0xF7A8B4, 0x80C8F0, 0x2A1A2E)),
    ('plain', P(0x6A3A3A, 0xEBB8A4, 0xCE8E80, 0xF8D4C4, 0xCE8E80, 0xB07268, 0xF8D4C4, 0xDCA698, 0xF0889A, 0xFFE8B0, 0x2A1A2E, 0xE0707E, 0x7A4048, 0xF890A0)),
    # 6 Skogkatt: the full-floof set
    ('fluffy', P(0x3A2C20, 0x9A8062, 0x76603E, 0xBCA484, 0x5E4A32, 0x42341F, 0xE8E0D0, 0xC4B8A0, 0xE8A8A0, 0xF0D860, 0x2A1A18)),
    ('plain', P(0x5A4A44, 0xD8CCC0, 0xB4A698, 0xF2EAE0, 0xB4A698, 0x968778, 0xF2EAE0, 0xD4C8BC, 0xF0B0B0, 0xA8D8F4, 0x2A1A2E)),
    # 8 Rainbow Cat: pale base; the game recolours its fur in drifting rainbow bands as it draws (see catart.cpp)
    ('plain', P(0x6A4AA0, 0xF4ECFF, 0xD8C8F4, 0xFFFFFF, 0xF4ECFF, 0xD8C8F4, 0xFFFFFF, 0xE8E0FF, 0xFFB8E0, 0x70E0FF, 0x203060)),
    # 9 Hamster
    ('hamster', P(0x5A3418, 0xF2B660, 0xD08C3C, 0xFFDA98, 0xC8803A, 0xA0602A, 0xFFF6E8, 0xEED8C0, 0xFFA8B8, 0x1A1018, 0x000000, 0xF07A90, 0x6A3040, 0xFF8CA8)),
]

def rle(role):
    flat = role.reshape(-1); out = bytearray(); i = 0; n = len(flat)
    while i < n:
        v = int(flat[i]); r = 1
        while i + r < n and flat[i + r] == v and r < 15: r += 1
        out.append((v << 4) | r); i += r
    return bytes(out)

def main(outdir):
    data = bytearray(); table = []
    for si, pat in enumerate(SETS):
        for bi, body in enumerate(BODIES):
            fr = hamgen.build(body) if pat == 'hamster' else fluffgen.build(body) if pat == 'fluffy' else C.build(body, pat)
            for name, n in ANIMS:
                # Eye geometry for this animation (from its frames with open eyes): ex2 = twice the x of the face's
                # mirror line, ec2 = twice the x of the right eye's centre, ew = eye width. When the game scales a
                # sprite it samples each eye symmetrically about its own centre (and the rest about the face's line),
                # so at any size both eyes come out alike.
                def eyes(role):
                    xs = sorted(set(np.nonzero(np.isin(role, (10, 11)))[1].tolist()))
                    if not xs: return None
                    a2 = xs[0] + xs[-1] + 1            # twice the middle
                    right = [x for x in xs if 2 * x + 1 > a2]
                    if right and not any(2 * x + 1 == a2 or 2 * x + 2 == a2 or 2 * x == a2 for x in xs):   # nothing on the middle: two eyes
                        return (a2, right[0] + right[-1] + 1, right[-1] - right[0] + 1)
                    return (a2, a2, xs[-1] - xs[0] + 1)   # one eye (side on)
                base = None
                for k in range(n):
                    role, _ = fr[name][k]
                    e = eyes(role)
                    if e: base = (role.shape[1], e); break
                for k in range(n):
                    role, (ax, ay) = fr[name][k]
                    h, w = role.shape
                    e = eyes(role) or (base[1] if base and base[0] == w else (0, 0, 0))   # blinks reuse the open eyes
                    table.append((len(data), w, h, ax, ay) + tuple(min(v, 255) for v in e))
                    data += rle(role)
    assert len(table) == len(SETS) * len(BODIES) * NF
    L = []
    L.append('// GENERATED by art/bake.py - do not edit.\n#include "catsprites.h"\nnamespace cat {')
    L.append(f'const uint16_t CAT_FRAMES_N = {len(table)};')
    L.append('const SpriteFrame CAT_FRAMES[] = {')
    for off, w, h, ax, ay, e, c, ew in table: L.append(f'  {{{off}u,{w},{h},{ax},{ay},{e},{c},{ew}}},')
    L.append('};')
    L.append(f'const uint16_t CAT_PALS[{len(BREEDS)}][16] = {{')
    for _, pal in BREEDS: L.append('  {' + ','.join(f'0x{rgb565(c):04X}' for c in pal) + '},')
    L.append('};')
    L.append(f'const uint8_t CAT_BREED_SET[{len(BREEDS)}] = {{' + ','.join(str(SETS.index(s)) for s, _ in BREEDS) + '};')
    L.append(f'const uint8_t CAT_DATA[{len(data)}] = {{')
    for i in range(0, len(data), 24): L.append('  ' + ','.join(str(b) for b in data[i:i + 24]) + ',')
    L.append('};\n}  // namespace cat')
    open(os.path.join(outdir, 'catsprites_data.cpp'), 'w').write('\n'.join(L) + '\n')
    print(f'frames={len(table)} data={len(data)} bytes')

if __name__ == '__main__':
    main(sys.argv[1])
