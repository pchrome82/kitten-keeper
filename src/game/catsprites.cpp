#include "catsprites.h"
#include <math.h>

namespace cat {

SpriteEyePts gSpriteEyes;

int spriteAnimFrames(int anim) { static const uint8_t n[SPRITE_ANIMS] = {3, 4, 2, 2, 5, 8}; return n[anim >= SPRITE_ANIMS ? 0 : anim]; }

const SpriteFrame& spriteFrame(int set, int body, int anim, int idx) {
  static const uint8_t base[SPRITE_ANIMS] = {0, 3, 7, 9, 11, 16};
  int n = spriteAnimFrames(anim);
  if (idx < 0) idx = 0;
  idx %= n;
  return CAT_FRAMES[(set * SPRITE_BODIES + body) * SPRITE_NF + base[anim >= SPRITE_ANIMS ? 0 : anim] + idx];
}

// soft HSV -> RGB565 for the Rainbow Cat (h wraps, s and v 0..1)
static Color hsv565(float h, float s, float v) {
  h -= floorf(h); h *= 6.0f;
  const int i = (int)h; const float f = h - i;
  const float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
  float r, g, b;
  switch (i) { case 0: r = v; g = t; b = p; break; case 1: r = q; g = v; b = p; break; case 2: r = p; g = v; b = t; break;
               case 3: r = p; g = q; b = v; break; case 4: r = t; g = p; b = v; break; default: r = v; g = p; b = q; }
  return rgb((int)(r * 255), (int)(g * 255), (int)(b * 255));
}

static Color rainbowRole(int r, int sx, int sy, float t, const uint16_t* pal) {
  const float hue = sy * 0.022f + sx * 0.010f - t * 0.25f;
  switch (r) {
    case 2: case 5: return hsv565(hue, 0.30f, 1.00f);          // fur
    case 3: case 6: return hsv565(hue + 0.04f, 0.42f, 0.90f);  // shaded fur
    case 4: return hsv565(hue, 0.14f, 1.00f);                  // highlight
    case 7: return hsv565(hue + 0.5f, 0.12f, 1.00f);           // pale chest
    case 8: return hsv565(hue + 0.5f, 0.22f, 1.00f);
    case 1: return hsv565(hue + 0.1f, 0.40f, 0.70f);           // outline: a deeper tint of the same rainbow
    default: return pal[r];                                    // eyes, nose, mouth keep their colours
  }
}

void drawSprite(Canvas& cv, const SpriteFrame& f, const uint16_t* pal, int ox, int oy, bool flip, float scale, int* b, float rainbowT, Color flat) {
  // decode the RLE stream into a row buffer one source row at a time
  uint8_t row[160];
  const uint8_t* p = CAT_DATA + f.off;
  int run = 0, val = 0;
  const int dw = (int)(f.w * scale + 0.5f), dh = (int)(f.h * scale + 0.5f);
  int left = 1 << 20, top = 1 << 20, right = -(1 << 20), bottom = -(1 << 20);
  // destination columns -> source columns, worked out once per sprite (not once per pixel)
  uint8_t colMap[320];
  const int dwc = dw < 320 ? dw : 320;
  if (f.ex2 && scale != 1.0f) {
    // Plain nearest sampling can drop the outline or the iris from one eye only, so the eyes don't match. Instead:
    // sample mirror-symmetrically about the face's centre line, and inside each eye symmetrically about that eye's
    // own centre, so at any size both eyes keep / lose the same columns and stay alike.
    const int e2 = flip ? 2 * f.w - f.ex2 : f.ex2;                       // (all in display = possibly flipped x)
    const float a = e2 * 0.5f;
    const float ax0 = (float)(flip ? (f.w - 1 - f.ax) : f.ax);
    float D = (a - ax0) * scale + (int)(ax0 * scale + 0.5f);              // the line on the destination (from xBase)...
    D = (e2 & 1) ? floorf(D) + 0.5f : floorf(D + 0.5f);                   // ...snapped so dest pixels mirror about it
    const float c = a + fabsf((flip ? 2 * f.w - f.ec2 : f.ec2) * 0.5f - a);   // the right-hand eye's centre (the other mirrors it)
    const bool two = f.ec2 != f.ex2;
    const float er = f.ew * 0.5f * scale;                                  // the eye's half-width on the destination
    float E = D + (c - a) * scale;                                         // its centre on the destination, snapped
    E = (f.ew & 1) ? floorf(E) + 0.5f : floorf(E + 0.5f);
    // its innermost pixel centre; keep at least a pixel of face between the eyes (they never merge into one bar)
    auto inner = [&](float e) { float p = floorf(e - er - 0.5f) + 0.5f; while (p < e - er - 0.01f) p += 1.0f; return p; };
    if (two && c - a - f.ew * 0.5f >= 1.0f) while (inner(E) - D < 1.0f) E += 1.0f;
    const float u1 = two ? E - er : D, s1 = two ? c - f.ew * 0.5f : a;     // inner edge of the eye (dest, source)
    for (int dx = 0; dx < dwc; dx++) {
      const float u = dx + 0.5f;
      const float m = u >= D ? 1.0f : -1.0f, q = fabsf(u - D);             // fold onto the right half...
      float s;
      if (q >= u1 - D) s = (c - a) + (q - (E - D)) / scale;                // the eye and beyond: steps of 1/scale from its centre
      else s = (u1 - D > 0 ? q * (s1 - a) / (u1 - D) : 0.0f);              // between the eyes: stretched to fit
      s = a + m * s;                                                       // ...and back
      int sx = (int)floorf(s);
      if (sx < 0) sx = 0; else if (sx >= f.w) sx = f.w - 1;
      colMap[dx] = (uint8_t)(flip ? (f.w - 1 - sx) : sx);
    }
  } else {
    for (int dx = 0; dx < dwc; dx++) {
      int sx = (int)((dx + 0.5f) / scale);
      if (sx >= f.w) sx = f.w - 1;
      colMap[dx] = (uint8_t)(flip ? (f.w - 1 - sx) : sx);
    }
  }
  const int xBase = ox - (int)((flip ? (f.w - 1 - f.ax) : f.ax) * scale + 0.5f);
  // destination rows map back to source rows; walk the source rows in order, draw those that are needed
  int nextDy = 0;
  if (b) gSpriteEyes.n = 0;
  for (int sy = 0; sy < f.h; sy++) {
    for (int x = 0; x < f.w; x++) {
      if (run == 0) { uint8_t c = *p++; val = c >> 4; run = c & 15; }
      row[x] = (uint8_t)val; run--;
    }
    // every destination row whose source row is sy
    while (nextDy < dh) {
      int src = (int)((nextDy + 0.5f) / scale);
      if (src > sy) break;
      if (src == sy) {
        int y = oy - (int)(f.ay * scale + 0.5f) + nextDy;
        for (int dx = 0; dx < dwc; dx++) {
          uint8_t r = row[colMap[dx]];
          if (!r) continue;
          int x = xBase + dx;
          cv.set(x, y, flat ? flat : (rainbowT >= 0 ? rainbowRole(r, colMap[dx], sy, rainbowT, pal) : pal[r]));
          if (x < left) left = x; if (x > right) right = x;
          if (y < top) top = y; if (y > bottom) bottom = y;
          if (b && (r == 10 || r == 11) && gSpriteEyes.n < 640 && (unsigned)x < (unsigned)cv.cw && (unsigned)y < (unsigned)cv.ch) {
            const int k = gSpriteEyes.n++; gSpriteEyes.x[k] = (int16_t)x; gSpriteEyes.y[k] = (int16_t)y; gSpriteEyes.role[k] = r;
          }
        }
      }
      nextDy++;
    }
  }
  if (b) { b[0] = left; b[1] = top; b[2] = right; b[3] = bottom; }
}

}  // namespace cat
