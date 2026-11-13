// Painterly one-point-perspective rooms (368x316 band at physical y 52..368).
// Painted in float RGB, then ordered-dithered to RGB565 so gradients stay smooth.
#include "roomstyles.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <initializer_list>

namespace cat {
static bool gSolid = false;   // while set, what is painted covers window glass (the table stands in front of it)
bool gNoTable = false;   // bake the room without its little table (the table is then its own sprite)
namespace {

// ------------------------------------------------------------------ colour + noise
struct F3 { float r, g, b; };
inline F3 f3(float r, float g, float b) { return {r, g, b}; }
inline F3 hx(uint32_t h) { return {(float)((h >> 16) & 255), (float)((h >> 8) & 255), (float)(h & 255)}; }
inline F3 operator+(F3 a, F3 b) { return {a.r + b.r, a.g + b.g, a.b + b.b}; }
inline F3 operator-(F3 a, F3 b) { return {a.r - b.r, a.g - b.g, a.b - b.b}; }
inline F3 operator*(F3 a, float k) { return {a.r * k, a.g * k, a.b * k}; }
inline F3 mulc(F3 a, F3 b) { return {a.r * b.r / 255.f, a.g * b.g / 255.f, a.b * b.b / 255.f}; }
inline float cl01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
inline F3 lerp(F3 a, F3 b, float t) { return a + (b - a) * t; }
inline float sst(float e0, float e1, float x) { float t = cl01((x - e0) / (e1 - e0)); return t * t * (3 - 2 * t); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

inline uint32_t h2(int x, int y, uint32_t s) {
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + s * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
inline float rnd2(int x, int y, uint32_t s) { return (h2(x, y, s) & 0xFFFFFF) / 16777216.0f; }
float vnoise(float x, float y, uint32_t s) {
  int xi = (int)floorf(x), yi = (int)floorf(y);
  float fx = x - xi, fy = y - yi;
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
  float a = rnd2(xi, yi, s), b = rnd2(xi + 1, yi, s), c = rnd2(xi, yi + 1, s), d = rnd2(xi + 1, yi + 1, s);
  return lerpf(lerpf(a, b, fx), lerpf(c, d, fx), fy);
}
float fbm(float x, float y, uint32_t s, int oct = 4) {
  float v = 0, a = 0.5f, n = 0;
  for (int i = 0; i < oct; i++) { v += vnoise(x, y, s + i * 31) * a; n += a; x *= 2.03f; y *= 2.03f; a *= 0.5f; }
  return v / n;
}

// ------------------------------------------------------------------ picture
constexpr int PW = 368, PH = 316, TOPY = 52;
struct Pic {
  F3* p;
  float* win;    // 1 = window glass (not affected by interior lighting)
  Pic() { p = new F3[PW * PH]; win = new float[PW * PH]; memset(win, 0, sizeof(float) * PW * PH); }
  ~Pic() { delete[] p; delete[] win; }
  F3& at(int x, int y) { return p[y * PW + x]; }
  bool in(int x, int y) const { return x >= 0 && y >= 0 && x < PW && y < PH; }
  void blend(int x, int y, F3 c, float a) {
    if (!in(x, y) || a <= 0) return;
    if (a > 1) a = 1;
    F3& d = at(x, y); d = lerp(d, c, a);
    if (gSolid && a > 0.5f) win[y * PW + x] = 0;   // something solid in front of the window: not glass any more
  }
  void scale(int x, int y, float k) { if (in(x, y)) at(x, y) = at(x, y) * k; }
};

// scanline polygon with horizontal coverage + 2 vertical sub-samples
template <class Fn>
void fillPoly(Pic& P, const float* xs, const float* ys, int n, Fn shade, float alpha = 1.0f) {
  float y0 = 1e9f, y1 = -1e9f, x0 = 1e9f, x1 = -1e9f;
  for (int i = 0; i < n; i++) { y0 = fminf(y0, ys[i]); y1 = fmaxf(y1, ys[i]); x0 = fminf(x0, xs[i]); x1 = fmaxf(x1, xs[i]); }
  int iy0 = (int)floorf(y0), iy1 = (int)ceilf(y1);
  if (iy0 < 0) iy0 = 0; if (iy1 > PH) iy1 = PH;
  int ix0 = (int)floorf(x0) - 1, ix1 = (int)ceilf(x1) + 1;
  if (ix0 < 0) ix0 = 0; if (ix1 > PW) ix1 = PW;
  static float acc[PW];
  for (int y = iy0; y < iy1; y++) {
    for (int x = ix0; x < ix1; x++) acc[x] = 0;
    for (int s = 0; s < 2; s++) {
      float sy = y + 0.25f + 0.5f * s;
      float xi[32]; int m = 0;
      for (int i = 0; i < n && m < 32; i++) {
        int j = (i + 1) % n;
        float ya = ys[i], yb = ys[j];
        if ((ya <= sy && yb > sy) || (yb <= sy && ya > sy)) xi[m++] = xs[i] + (sy - ya) / (yb - ya) * (xs[j] - xs[i]);
      }
      for (int a = 1; a < m; a++) for (int b = a; b > 0 && xi[b] < xi[b - 1]; b--) { float t = xi[b]; xi[b] = xi[b - 1]; xi[b - 1] = t; }
      for (int k = 0; k + 1 < m; k += 2) {
        float a = xi[k], b = xi[k + 1];
        for (int x = (int)floorf(a); x < (int)ceilf(b); x++) {
          if (x < ix0 || x >= ix1) continue;
          float cov = fminf(b, x + 1.0f) - fmaxf(a, (float)x);
          if (cov > 0) acc[x] += cov * 0.5f;
        }
      }
    }
    for (int x = ix0; x < ix1; x++) if (acc[x] > 0.001f) P.blend(x, y, shade(x + 0.5f, y + 0.5f), acc[x] * alpha);
  }
}
template <class Fn> void quad(Pic& P, float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy, Fn shade, float alpha = 1) {
  float xs[4] = {ax, bx, cx, dx}, ys[4] = {ay, by, cy, dy};
  fillPoly(P, xs, ys, 4, shade, alpha);
}
void quadC(Pic& P, float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy, F3 c, float alpha = 1) {
  quad(P, ax, ay, bx, by, cx, cy, dx, dy, [&](float, float) { return c; }, alpha);
}

void seg(Pic& P, float x0, float y0, float x1, float y1, float w, F3 c, float a = 1) {
  int bx0 = (int)floorf(fminf(x0, x1) - w - 1), bx1 = (int)ceilf(fmaxf(x0, x1) + w + 1);
  int by0 = (int)floorf(fminf(y0, y1) - w - 1), by1 = (int)ceilf(fmaxf(y0, y1) + w + 1);
  float dx = x1 - x0, dy = y1 - y0, L2 = dx * dx + dy * dy;
  for (int y = by0; y <= by1; y++) for (int x = bx0; x <= bx1; x++) {
    float px = x + 0.5f - x0, py = y + 0.5f - y0;
    float t = L2 > 0 ? cl01((px * dx + py * dy) / L2) : 0;
    float ex = px - t * dx, ey = py - t * dy;
    float d = sqrtf(ex * ex + ey * ey);
    float cov = cl01(w * 0.5f - d + 0.5f);
    if (cov > 0) P.blend(x, y, c, cov * a);
  }
}
// hand-inked wobbly line (cartoon outlines)
void inkLine(Pic& P, float x0, float y0, float x1, float y1, float w, F3 c, uint32_t seed, float amp = 1.2f) {
  float dx = x1 - x0, dy = y1 - y0, L = sqrtf(dx * dx + dy * dy);
  if (L < 0.5f) return;
  int n = (int)(L / 5) + 1;
  float nx = -dy / L, ny = dx / L;
  float px = x0, py = y0;
  for (int i = 1; i <= n; i++) {
    float t = (float)i / n;
    float o = (fbm(t * L * 0.05f, 0.5f, seed, 2) - 0.5f) * 2 * amp * sinf(t * 3.14159f);
    float qx = x0 + dx * t + nx * o, qy = y0 + dy * t + ny * o;
    float ww = w * (0.75f + 0.5f * vnoise(t * L * 0.08f, 3.0f, seed + 7));
    seg(P, px, py, qx, qy, ww, c);
    px = qx; py = qy;
  }
}
void softEllipse(Pic& P, float cx, float cy, float rx, float ry, F3 c, float amax) {
  for (int y = (int)(cy - ry); y <= (int)(cy + ry) + 1; y++) for (int x = (int)(cx - rx); x <= (int)(cx + rx) + 1; x++) {
    float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry, d2 = dx * dx + dy * dy;
    if (d2 >= 1) continue;
    float f = 1 - d2;
    P.blend(x, y, c, amax * f * f);
  }
}
void glow(Pic& P, float cx, float cy, float r, F3 c, float k) {   // additive light
  for (int y = (int)(cy - r); y <= (int)(cy + r); y++) for (int x = (int)(cx - r); x <= (int)(cx + r); x++) {
    if (!P.in(x, y)) continue;
    float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = sqrtf(dx * dx + dy * dy) / r;
    if (d >= 1) continue;
    float f = (1 - d) * (1 - d) * k;
    P.at(x, y) = P.at(x, y) + c * f;
  }
}

// ------------------------------------------------------------------ room geometry
struct Geo { float vx, vy, bx0, by0, bx1, by1, F; };
enum Surf { S_BACK, S_LEFT, S_RIGHT, S_FLOOR, S_CEIL };
struct Hit { int s; float D, D2, u, v; };

Hit classify(const Geo& g, float x, float y) {
  Hit h; h.s = S_BACK; h.D = 1; h.D2 = 1e9f;
  auto consider = [&](int id, float D) {
    if (D <= 0) return;
    if (D < h.D) { h.D2 = h.D; h.D = D; h.s = id; }
    else if (D < h.D2) h.D2 = D;
  };
  if (x < g.vx) consider(S_LEFT, (g.vx - g.bx0) / (g.vx - x));
  if (x > g.vx) consider(S_RIGHT, (g.bx1 - g.vx) / (x - g.vx));
  if (y > g.vy) consider(S_FLOOR, (g.by1 - g.vy) / (y - g.vy));
  if (y < g.vy) consider(S_CEIL, (g.vy - g.by0) / (g.vy - y));
  switch (h.s) {
    case S_BACK: h.u = x - g.vx; h.v = y - g.vy; break;
    case S_LEFT: case S_RIGHT: h.u = (1 - h.D) * g.F; h.v = (y - g.vy) * h.D; break;
    default: h.u = (x - g.vx) * h.D; h.v = (1 - h.D) * g.F; break;   // floor / ceiling: u across, v = depth from the back wall
  }
  return h;
}
inline void proj(const Geo& g, float X, float Y, float Z, float& x, float& y) { x = g.vx + X * g.F / Z; y = g.vy + Y * g.F / Z; }

// base pass: shade every surface, supersampling only where surfaces meet
template <class Fn>
void paintRoom(Pic& P, const Geo& g, Fn shade) {
  for (int y = 0; y < PH; y++) for (int x = 0; x < PW; x++) {
    Hit c = classify(g, x + 0.5f, y + 0.5f);
    Hit a = classify(g, x + 0.1f, y + 0.1f), b = classify(g, x + 0.9f, y + 0.9f);
    F3 col;
    if (a.s == b.s && a.s == c.s) col = shade(c, x + 0.5f, y + 0.5f);
    else {
      col = f3(0, 0, 0);
      for (int s = 0; s < 4; s++) { float sx = x + 0.25f + 0.5f * (s & 1), sy = y + 0.25f + 0.5f * (s >> 1); col = col + shade(classify(g, sx, sy), sx, sy) * 0.25f; }
    }
    float r = c.D2 / c.D - 1;   // ambient occlusion in the corners
    col = col * (1 - 0.32f * expf(-r * 9));
    P.at(x, y) = col;
  }
}

// ------------------------------------------------------------------ sky + light
float dayLight(float h) { return sst(5.5f, 7.5f, h) * (1 - sst(18.0f, 20.0f, h)); }
float duskAmt(float h) { float a = 1 - fabsf(h - 19.0f) / 1.6f, b = 1 - fabsf(h - 6.5f) / 1.2f; return cl01(fmaxf(a, b)); }

F3 skyCol(float v, float hour) {   // v 0 top .. 1 horizon
  float day = dayLight(hour), dusk = duskAmt(hour);
  F3 top = lerp(hx(0x0A0E2A), hx(0x4C8EE0), day), hor = lerp(hx(0x1E2050), hx(0xB8DEF6), day);
  top = lerp(top, hx(0x3A3C88), dusk * 0.7f); hor = lerp(hor, hx(0xFF9A5A), dusk * 0.85f);
  return lerp(top, hor, powf(cl01(v), 1.4f));
}
// sun / moon / clouds / stars in window-normalised coords
F3 skyAt(float u, float v, float hour, uint32_t seed, float asp = 1.5f) {
  F3 c = skyCol(v, hour);
  float day = dayLight(hour), dusk = duskAmt(hour);
  float night = 1 - fmaxf(day, dusk);
  if (night > 0.2f && rnd2((int)(u * 160), (int)(v * 140), seed + 5) > 0.992f) c = lerp(c, hx(0xFFFFF0), night);
  float su = 0.15f + (hour - 6) / 12.0f * 0.7f, sv = 0.75f - sinf(cl01((hour - 6) / 12.0f) * 3.14159f) * 0.55f;
  if (day + dusk > 0.05f) {
    float d = sqrtf((u - su) * (u - su) * asp * asp + (v - sv) * (v - sv));
    c = c + hx(0xFFF2C0) * (0.35f * expf(-d * 9) * (day + dusk));
    if (d < 0.055f) c = lerp(c, lerp(hx(0xFFE070), hx(0xFFFBEA), day), 1);
  } else {
    float mu = 0.72f, mv = 0.22f, d = sqrtf((u - mu) * (u - mu) * asp * asp + (v - mv) * (v - mv));
    if (d < 0.05f) c = hx(0xF0F0E0);
    else c = c + hx(0x9090C0) * (0.25f * expf(-d * 10));
  }
  float cl = fbm(u * 3.2f + hour * 0.05f, v * 7.0f, seed + 9, 5);
  float m = sst(0.55f, 0.68f, cl) * (1 - sst(0.55f, 0.8f, v));
  F3 cloud = lerp(hx(0x2A2C50), hx(0xFFFFFF), day);
  cloud = lerp(cloud, hx(0xFFC0A0), dusk * 0.7f);
  F3 shadowed = cloud * 0.82f;
  c = lerp(c, lerp(cloud, shadowed, sst(0.6f, 0.8f, cl)), m * 0.9f);
  return c;
}

// overall interior light level for the hour, with tint
void gradeRoom(Pic& P, float hour, F3 lampTint, float lampAmt) {
  float day = dayLight(hour), dusk = duskAmt(hour);
  F3 amb = lerp(hx(0x3A4478), hx(0xFFFFFF), day);
  amb = lerp(amb, hx(0xFFB890), dusk * 0.5f);
  float k = 0.38f + 0.62f * day + 0.15f * dusk;
  amb = amb * k;
  amb = lerp(amb, lampTint * 0.52f, lampAmt * (1 - day) * (1 - dusk * 0.5f));
  for (int i = 0; i < PW * PH; i++) {
    float w = P.win[i];
    F3 lit = mulc(P.p[i], amb);
    P.p[i] = lerp(lit, P.p[i], w);
  }
}
void vignette(Pic& P, float k) {
  for (int y = 0; y < PH; y++) for (int x = 0; x < PW; x++) {
    float dx = (x - PW * 0.5f) / (PW * 0.62f), dy = (y - PH * 0.45f) / (PH * 0.75f);
    float v = 1 - k * sst(0.55f, 1.25f, sqrtf(dx * dx + dy * dy));
    P.at(x, y) = P.at(x, y) * v;
  }
}

// sun patch on the floor, cast through the window mask (mullions throw shadows)
template <class Mask>
void sunbeam(Pic& P, const Geo& g, float hour, Mask glass, F3 tint, float strength) {
  float day = dayLight(hour), dusk = duskAmt(hour);
  float amt = day * strength + dusk * strength * 0.6f;
  if (amt <= 0.01f || hour < 6.5f || hour > 19.5f) return;
  float dx = (hour - 12.5f) * -0.08f;                       // sideways drift through the day
  float dyd = 0.55f + 0.75f * sinf(cl01((hour - 6) / 13.0f) * 3.14159f);   // steeper at noon
  float dz = 0.9f;
  F3 col = lerp(tint, hx(0xFFB070), dusk * 0.8f);
  float H = g.by1 - g.vy;
  for (int y = (int)g.by1; y < PH; y++) for (int x = 0; x < PW; x++) {
    Hit h = classify(g, x + 0.5f, y + 0.5f);
    if (h.s != S_FLOOR) continue;
    float t = (1 - h.D) * g.F / dz;
    float Yw = H - t * dyd;
    float Xw = h.u - t * dx;
    float wx = g.vx + Xw, wy = g.vy + Yw;
    // soft penumbra: 5 taps
    float m = 0;
    static const float o[5][2] = {{0, 0}, {2.5f, 0}, {-2.5f, 0}, {0, 2.5f}, {0, -2.5f}};
    for (auto& q : o) m += glass(wx + q[0], wy + q[1]);
    m *= 0.2f;
    if (m <= 0) continue;
    F3& d = P.at(x, y);
    d = d + mulc(d, col) * (m * amt);
  }
}

// mirror the back wall into a glossy floor
void floorReflect(Pic& P, const Geo& g, float k) {
  for (int y = (int)g.by1 + 1; y < PH; y++) for (int x = 0; x < PW; x++) {
    Hit h = classify(g, x + 0.5f, y + 0.5f);
    if (h.s != S_FLOOR) continue;
    int sy = (int)(2 * g.by1 - y);
    if (sy < (int)g.by0 + 1 || x < g.bx0 || x > g.bx1) continue;
    F3 r = P.at(x, sy);
    float f = k * (0.35f + 0.65f * sst(0.45f, 0.95f, h.D));
    F3& d = P.at(x, y);
    d = lerp(d, r, f * 0.5f) + r * (f * 0.15f);
  }
}

// ------------------------------------------------------------------ props
enum TableKind { T_TEAK, T_WONKY, T_HAIRPIN, T_RUSTIC, T_LOG };

// glass of water: cx, base y on the table, size
void cupOfWater(Pic& P, float cx, float by, float w, float h, F3 tint, bool ink) {
  softEllipse(P, cx + 2, by + 0.5f, w * 0.75f, 2.2f, f3(10, 6, 14), 0.5f);
  float x0 = cx - w / 2, x1 = cx + w / 2, xb0 = cx - w * 0.42f, xb1 = cx + w * 0.42f, ty = by - h;
  // glass body: lighten + tint what is behind
  quad(P, x0, ty, x1, ty, xb1, by, xb0, by, [&](float x, float y) {
    F3 d = P.at((int)x, (int)y);
    float u = (x - cx) / (w / 2);
    float edge = sst(0.55f, 1.0f, fabsf(u));
    return lerp(lerp(d, tint, 0.18f) + f3(18, 22, 26), mulc(d, tint) * 0.7f, edge * 0.6f);
  });
  // water
  float wy = ty + h * 0.32f;
  float wx0 = lerpf(x0, xb0, 0.32f) + 0.6f, wx1 = lerpf(x1, xb1, 0.32f) - 0.6f;
  quad(P, wx0, wy, wx1, wy, xb1 - 0.6f, by - 1.2f, xb0 + 0.6f, by - 1.2f, [&](float x, float y) {
    F3 d = P.at((int)x, (int)y);
    float t = (y - wy) / (by - wy);
    return lerp(lerp(d, hx(0x7CC8EE), 0.45f), lerp(d, hx(0x3C8CC8), 0.55f), t);
  });
  softEllipse(P, cx, wy, (wx1 - wx0) / 2, 1.6f, hx(0xD8F4FF), 0.9f);   // meniscus
  seg(P, wx0 + 0.5f, wy + 0.2f, wx1 - 0.5f, wy + 0.2f, 0.8f, hx(0x4A9AD0), 0.7f);
  // rim + highlights
  softEllipse(P, cx, ty, w / 2, 1.6f, hx(0xFFFFFF), 0.55f);
  seg(P, x0 + 2.2f, ty + 2, xb0 + 1.8f, by - 2, 1.1f, hx(0xFFFFFF), 0.75f);
  seg(P, x1 - 2.6f, ty + 3, xb1 - 2.2f, by - 4, 0.8f, hx(0xFFFFFF), 0.35f);
  seg(P, xb0, by - 0.5f, xb1, by - 0.5f, 1.0f, hx(0xFFFFFF), 0.3f);
  if (ink) {
    inkLine(P, x0, ty, xb0, by, 1.2f, hx(0x201420), 71, 0.4f);
    inkLine(P, x1, ty, xb1, by, 1.2f, hx(0x201420), 72, 0.4f);
    inkLine(P, xb0, by, xb1, by, 1.2f, hx(0x201420), 73, 0.4f);
    inkLine(P, x0, ty, x1, ty, 1.0f, hx(0x201420), 74, 0.3f);
  }
}

// woodgrain shader in local coords
F3 wood(F3 base, float u, float v, uint32_t seed, float grainScale = 1.0f) {
  float n = fbm(u * 0.04f * grainScale, v * 0.6f * grainScale, seed, 4);
  float ring = sinf((u * 0.09f + n * 6.0f) * 3.0f) * 0.5f + 0.5f;
  float k = 0.86f + 0.16f * n + 0.08f * ring;
  return base * k;
}

// Table standing on the floor at world X (centre), Z (distance; F = back wall).
void table(Pic& P, const Geo& g, TableKind kind, float X, float Z, F3 woodCol, bool ink, F3 legCol) {
  if (gNoTable) return;
  struct Solid { Solid() { gSolid = true; } ~Solid() { gSolid = false; } } solid;
  const float W2 = kind == T_HAIRPIN ? 40 : 36, Dp = 26, Hgt = kind == T_LOG ? 30 : 34, th = kind == T_RUSTIC || kind == T_LOG ? 5.0f : 3.2f;
  const float Yf = g.by1 - g.vy, Yt = Yf - Hgt;
  float z0 = Z - Dp / 2, z1 = Z + Dp / 2;   // z1 = back edge
  auto pr = [&](float x, float y, float z, float& sx, float& sy) { proj(g, x, y, z, sx, sy); };
  const F3 INK = hx(0x1E1218);
  // floor shadow
  { float sx, sy; pr(X + 6, Yf, Z - 2, sx, sy); float rx = W2 * g.F / Z * 1.1f, ry = Dp * g.F / Z * 0.22f; softEllipse(P, sx, sy, rx, ry + 3, f3(20, 10, 16), 0.45f); }
  // legs
  struct L { float x, z; } legs[4] = {{X - W2 + 4, z1 - 3}, {X + W2 - 4, z1 - 3}, {X - W2 + 4, z0 + 3}, {X + W2 - 4, z0 + 3}};
  for (int i = 0; i < 4; i++) {
    float lx = legs[i].x, lz = legs[i].z;
    float splay = (kind == T_TEAK || kind == T_HAIRPIN) ? (lx < X ? -4.0f : 4.0f) : 0.0f;
    float wTop = kind == T_HAIRPIN ? 1.0f : (kind == T_TEAK ? 2.6f : (kind == T_LOG ? 4.5f : 3.2f));
    float wBot = kind == T_TEAK ? 1.3f : wTop;
    float ax, ay, bx, by_;
    pr(lx, Yt + th, lz, ax, ay);
    float lean = kind == T_WONKY && i == 1 ? 3.0f : 0.0f;
    pr(lx + splay + lean, Yf - (kind == T_WONKY && i == 3 ? 2.0f : 0.0f), lz, bx, by_);
    float s = g.F / lz;
    F3 lc = i < 2 ? legCol * 0.72f : legCol;
    if (kind == T_HAIRPIN) {
      seg(P, ax - 2 * s * 0.5f, ay, bx, by_, 1.3f, lc);
      seg(P, ax + 2 * s * 0.5f, ay, bx, by_, 1.3f, lc);
    } else {
      quad(P, ax - wTop * s * 0.5f, ay, ax + wTop * s * 0.5f, ay, bx + wBot * s * 0.5f, by_, bx - wBot * s * 0.5f, by_,
           [&](float x, float y) { F3 c = wood(lc, y * 3, x, 41); float e = (x - (ax + bx) * 0.5f) / (wTop * s * 0.5f); return c * (1.05f - 0.25f * sst(-0.2f, 1.0f, e)); });
      if (ink) { inkLine(P, ax - wTop * s * 0.5f, ay, bx - wBot * s * 0.5f, by_, 1.3f, INK, 80 + i, 0.6f); inkLine(P, ax + wTop * s * 0.5f, ay, bx + wBot * s * 0.5f, by_, 1.3f, INK, 90 + i, 0.6f); }
    }
  }
  // apron (front face of the top)
  float tlx, tly, trx, try_, blx, bly, brx, bry, flx, fly, frx, fry, glx, gly, grx, gry;
  float wob = kind == T_WONKY ? 1.5f : 0.0f;
  pr(X - W2, Yt + wob, z1, tlx, tly); pr(X + W2, Yt - wob, z1, trx, try_);
  pr(X - W2, Yt + wob, z0, blx, bly); pr(X + W2, Yt - wob, z0, brx, bry);
  pr(X - W2, Yt + th + wob, z0, flx, fly); pr(X + W2, Yt + th - wob, z0, frx, fry);
  pr(X + W2, Yt + th - wob, z1, grx, gry); pr(X - W2, Yt + th + wob, z1, glx, gly);
  if (kind == T_LOG) {   // round slab: draw the top as an ellipse with bark rim
    float cx = (blx + brx + tlx + trx) / 4, cy = (bly + bry + tly + try_) / 4, rx = (brx - blx) / 2, ry = (bly - tly) / 2 + 1;
    for (int k = 0; k < 6; k++) softEllipse(P, cx, cy + 3 + k * 0.5f, rx + 1, ry + 1, hx(0x4A2E18), 1.0f);
    for (int y = (int)(cy - ry); y <= (int)(cy + ry); y++) for (int x = (int)(cx - rx); x <= (int)(cx + rx); x++) {
      float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry, d = sqrtf(dx * dx + dy * dy);
      if (d > 1) continue;
      float ring = sinf(d * 34 + fbm(dx * 3, dy * 3, 5, 3) * 6) * 0.5f + 0.5f;
      F3 c = lerp(woodCol * 1.1f, woodCol * 0.82f, ring * 0.5f);
      if (d > 0.88f) c = hx(0x5A3A20) * (0.8f + 0.3f * rnd2(x, y, 3));
      P.blend(x, y, c, cl01((1 - d) * rx));
    }
    return;
  }
  quad(P, flx, fly - (fly - bly), frx, fry - (fry - bry), frx, fry, flx, fly,
       [&](float x, float y) { return wood(woodCol * 0.78f, x * 2, y * 0.5f, 33); });
  // right side face when visible (table right of the vanishing point)
  if (X - W2 > 0) quad(P, brx, bry, trx, try_, grx, gry, frx, fry, [&](float x, float y) { return wood(woodCol * 0.62f, x, y, 34); });
  if (X + W2 < 0) quad(P, blx, bly, tlx, tly, glx, gly, flx, fly, [&](float x, float y) { return wood(woodCol * 0.62f, x, y, 34); });
  // top
  quad(P, tlx, tly, trx, try_, brx, bry, blx, bly, [&](float x, float y) {
    float u = (x - blx) / (brx - blx + 0.01f), v = (y - tly) / (bly - tly + 0.01f);
    F3 c = wood(woodCol, u * 400, v * 30 + u * 3, kind == T_RUSTIC ? 51 : 52, kind == T_RUSTIC ? 1.6f : 1.0f);
    c = c * (0.92f + 0.14f * v);
    if (kind == T_RUSTIC && fmodf(u * 4, 1.0f) < 0.02f) c = c * 0.6f;   // board seams
    return c;
  });
  seg(P, blx, bly, brx, bry, 1.0f, lerp(woodCol, hx(0xFFFFFF), 0.35f), 0.7f);   // front edge highlight
  if (ink) {
    inkLine(P, tlx, tly, trx, try_, 1.6f, INK, 61); inkLine(P, trx, try_, brx, bry, 1.6f, INK, 62);
    inkLine(P, blx, bly, tlx, tly, 1.6f, INK, 63); inkLine(P, flx, fly, frx, fry, 1.8f, INK, 64);
    inkLine(P, blx, bly, flx, fly, 1.4f, INK, 65); inkLine(P, brx, bry, frx, fry, 1.4f, INK, 66);
  }
}
void cupOnTable(Pic& P, const Geo& g, float X, float Z, float tableH, float dx, F3 tint, bool ink) {
  if (gNoTable) return;
  float sx, sy; proj(g, X + dx, (g.by1 - g.vy) - tableH, Z + 2, sx, sy);
  float s = g.F / Z;
  cupOfWater(P, sx, sy, 7.5f * s, 12.0f * s, tint, ink);
}

// ------------------------------------------------------------------ STYLE 0: Groovy '70s
const Geo G70 = {184, 112, 52, 22, 316, 190, 300};
float g70Glass(float x, float y) {   // big picture window with two slim mullions
  const float x0 = 104, x1 = 264, y0 = 40, y1 = 146, r = 14;
  if (x < x0 || x > x1 || y < y0 || y > y1) return 0;
  float cx = fminf(fmaxf(x, x0 + r), x1 - r), cy = fminf(fmaxf(y, y0 + r), y1 - r);
  if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > r * r) return 0;
  if (fabsf(x - 157) < 2 || fabsf(x - 211) < 2) return 0;
  return 1;
}
void grimyLitter(Pic& P, float X, float Y);
void grimyBowl(Pic& P, float X, float Y);

void style70s(Pic& P, float hour) {
  const Geo& g = G70;
  const F3 stripes[3] = {hx(0xE07028), hx(0xEFAE30), hx(0x8A4A22)};
  auto superg = [&](float u, float v, F3 base) {   // horizontal band that arcs up the side walls
    const float vb = 66, R = 46, u0 = 26, sw = 9;
    float s;
    float cy = vb - R;
    if (u <= u0) s = vb - v;
    else if (v >= cy) s = R - sqrtf((u - u0) * (u - u0) + (v - cy) * (v - cy));
    else s = R - (u - u0);
    if (s < 0 || s >= 3 * sw) return base;
    int k = (int)(s / sw);
    F3 c = stripes[k];
    return c * (0.95f + 0.08f * fbm(u * 0.2f, v * 0.2f, 9, 2));
  };
  paintRoom(P, g, [&](const Hit& h, float x, float y) -> F3 {
    float n = fbm(x * 0.15f, y * 0.15f, 3, 3);
    switch (h.s) {
      case S_BACK: {
        F3 base = hx(0xF2DDB0) * (0.94f + 0.08f * n);
        if (h.v > 66 - 27 && h.v <= 66) base = superg(0, h.v, base);
        if (h.v > 72) base = wood(hx(0x6A3A1C), x * 3, y, 17) ;   // walnut baseboard
        return base;
      }
      case S_LEFT: case S_RIGHT: {
        F3 base = hx(0xEAD2A0) * (0.92f + 0.08f * n);
        base = superg(h.u, h.v, base);
        if (h.v > 72) base = wood(hx(0x6A3A1C), h.u * 3, h.v, 18);
        return base * (h.s == S_LEFT ? 0.9f : 0.84f);
      }
      case S_FLOOR: {   // shag carpet
        float f = rnd2((int)(h.u * 1.4f), (int)(h.v * 1.4f), 77);
        float big = fbm(h.u * 0.03f, h.v * 0.03f, 4, 3);
        F3 base = lerp(hx(0xB86E2E), hx(0xD08A3A), big);
        float fib = rnd2((int)x, (int)y, 78);
        base = base * (0.78f + 0.34f * f) * (0.9f + 0.18f * fib);
        return base;
      }
      default: {   // popcorn ceiling
        float p = rnd2((int)(h.u * 0.8f), (int)(h.v * 0.8f), 79);
        return hx(0xF4ECDA) * (0.9f + 0.1f * p) * 0.92f;
      }
    }
  });
  // window
  for (int y = 30; y < 160; y++) for (int x = 90; x < 280; x++) {
    float m = g70Glass(x + 0.5f, y + 0.5f);
    if (m <= 0) continue;
    float u = (x - 104) / 160.0f, v = (y - 40) / 106.0f;
    F3 c = skyAt(u, v * 0.8f, hour, 11, 160.0f / 106 * 0.8f);
    // rolling hills + round trees
    float h1 = 0.70f + 0.05f * sinf(u * 6 + 1) + 0.03f * sinf(u * 13);
    float h2 = 0.80f + 0.04f * sinf(u * 4 + 3);
    float day = dayLight(hour) + duskAmt(hour) * 0.5f, lk = 0.25f + 0.75f * cl01(day);
    if (v > h1) c = lerp(hx(0x8AA060), hx(0x6E8A48), (v - h1) * 4) * lk;
    if (v > h2) c = lerp(hx(0x6E8A3C), hx(0x4E6A2C), (v - h2) * 4) * lk;
    for (int t = 0; t < 5; t++) {
      float tx = 0.1f + t * 0.21f + 0.04f * sinf(t * 7.0f), ty = h2 - 0.02f, r = 0.05f + 0.015f * (t % 3);
      float dx = (u - tx) * 1.5f, dy = v - (ty - r);
      if (dx * dx + dy * dy < r * r) c = lerp(hx(0x3E6A2A), hx(0x5A8A36), cl01(-dy / r + 0.3f - dx)) * lk;
      if (fabsf(u - tx) < 0.006f && v > ty - r && v < ty + 0.04f) c = hx(0x5A3A22) * lk;
    }
    P.at(x, y) = lerp(P.at(x, y), c, m);
    P.win[y * PW + x] = m;
  }
  // chunky teak frame
  {
    const F3 teak = hx(0x7A4824);
    for (int y = 30; y < 160; y++) for (int x = 92; x < 278; x++) {
      const float x0 = 98, x1 = 270, y0 = 34, y1 = 152, r = 18;
      float fx = x + 0.5f, fy = y + 0.5f;
      if (fx < x0 || fx > x1 || fy < y0 || fy > y1) continue;
      float cx = fminf(fmaxf(fx, x0 + r), x1 - r), cy = fminf(fmaxf(fy, y0 + r), y1 - r);
      float d = sqrtf((fx - cx) * (fx - cx) + (fy - cy) * (fy - cy));
      if (d > r) continue;
      if (g70Glass(fx, fy) > 0) continue;
      F3 c = wood(teak, fx * 3, fy * 0.5f, 21);
      if (fy > 146) c = c * 1.15f;   // sill
      P.blend(x, y, c, cl01(r - d + 0.5f));
    }
    for (float mx : {157.0f, 211.0f}) { seg(P, mx - 1, 40, mx - 1, 146, 1, hx(0xA0663A)); seg(P, mx + 1, 40, mx + 1, 146, 1, hx(0x4A2A14)); }
    seg(P, 100, 152.5f, 268, 152.5f, 1.5f, hx(0x3A200E), 0.8f);
    // inner glass shading
    for (int y = 40; y < 146; y++) for (int x = 104; x < 264; x++) if (g70Glass(x + 0.5f, y + 0.5f) > 0) {
      float e = fminf(fminf(x - 104.0f, 264.0f - x), fminf(y - 40.0f, 146.0f - y));
      if (e < 4) P.at(x, y) = P.at(x, y) * (0.8f + 0.05f * e);
    }
  }
  sunbeam(P, g, hour, g70Glass, hx(0xFFD27A), 0.55f);
  table(P, g, T_TEAK, 62, 214, hx(0xA0602C), false, hx(0x7A4420));
  cupOnTable(P, g, 62, 214, 34, -6, hx(0xE0A040), false);
  grimyLitter(P, 52, 268);   // the cat's litter box & bowls move house with you (same spots in every room)
  grimyBowl(P, 268, 268);
  gradeRoom(P, hour, hx(0xFFB060), 0.5f);
  vignette(P, 0.32f);
}

bool gBake = false;               // shipped-game bake: no window view, no sunbeam, glass pixels become the key colour
uint8_t* gGlassOut = nullptr;     // optional PW*PH mask written by the bake

// Grimy-cartoon floor objects drawn straight into the picture (so they share its lighting): X = centre, Y = ground, band coords.
void grimyLitter(Pic& P, float X, float Y) {
  const F3 INK = hx(0x1A1016);
  softEllipse(P, X + 4, Y + 1, 52, 9, f3(14, 8, 14), 0.55f);
  // tub front: flared, dull green-grey plastic with grime
  quad(P, X - 38, Y - 26, X + 38, Y - 26, X + 44, Y, X - 44, Y, [&](float x, float y) {
    float u = (x - X) / 44.0f, v = (y - (Y - 26)) / 26.0f;
    F3 c = lerp(hx(0x8FA394), hx(0x66786C), v);
    c = c * (1.04f - 0.22f * sst(0.3f, 1.0f, fabsf(u)));
    c = lerp(c, hx(0x5A5A44), sst(0.55f, 0.8f, fbm(x * 0.12f, y * 0.12f, 91, 3)) * 0.5f);
    return c;
  });
  // opening: rim ellipse + sand
  for (int k = 0; k < 7; k++) softEllipse(P, X, Y - 27, 45 - k * 0.1f, 9.0f, hx(0xB6C4B8), 1.0f);
  softEllipse(P, X, Y - 27, 40, 6.6f, hx(0x7E8F82), 1.0f);
  for (int y = (int)(Y - 34); y < (int)(Y - 20); y++) for (int x = (int)(X - 40); x < (int)(X + 40); x++) {
    float dx = (x + 0.5f - X) / 38.0f, dy = (y + 0.5f - (Y - 27)) / 5.4f;
    if (dx * dx + dy * dy > 1) continue;
    float n = rnd2(x, y, 92);
    F3 c = lerp(hx(0xC9AE7C), hx(0xA88C5C), n);
    if (rnd2(x / 3, y / 2, 93) > 0.93f) c = hx(0xE8D8B0);
    P.blend(x, y, c, 1);
  }
  inkLine(P, X - 38, Y - 26, X - 44, Y, 1.8f, INK, 94, 0.5f); inkLine(P, X + 38, Y - 26, X + 44, Y, 1.8f, INK, 95, 0.5f);
  inkLine(P, X - 44, Y, X + 44, Y, 1.8f, INK, 96, 0.6f);
  for (int a = 0; a < 40; a++) { float t = a / 40.0f * 6.2832f; seg(P, X + cosf(t) * 45, Y - 27 + sinf(t) * 9, X + cosf(t + 0.16f) * 45, Y - 27 + sinf(t + 0.16f) * 9, 1.6f, INK); }
  seg(P, X - 30, Y - 16, X + 30, Y - 16, 1.0f, hx(0x4A5A4E), 0.6f);
}
void grimyBowl(Pic& P, float X, float Y) {
  const F3 INK = hx(0x1A1016);
  softEllipse(P, X + 10, Y + 1, 66, 8, f3(14, 8, 14), 0.5f);
  // food bowl: chipped red enamel
  quad(P, X - 28, Y - 20, X + 28, Y - 20, X + 22, Y - 1, X - 22, Y - 1, [&](float x, float y) {
    float u = (x - X) / 28.0f, v = (y - (Y - 20)) / 19.0f;
    F3 c = lerp(hx(0xC8503C), hx(0x8E2E2A), v);
    c = c * (1.06f - 0.28f * sst(0.2f, 1.0f, u * u));
    if (rnd2((int)(x / 3), (int)(y / 3), 97) > 0.96f) c = hx(0xE8E0D0);   // chips showing metal
    return c;
  });
  softEllipse(P, X, Y - 20, 29, 6, hx(0xE0745C), 1.0f);
  softEllipse(P, X, Y - 20, 25, 4.4f, hx(0x4A2A1C), 1.0f);
  for (int i = 0; i < 26; i++) { float a = rnd2(i, 1, 98) * 6.2832f, r = sqrtf(rnd2(i, 2, 98)); softEllipse(P, X + cosf(a) * r * 20, Y - 21 - r * 2 + sinf(a) * 1.2f, 2.4f, 1.6f, lerp(hx(0xA8683A), hx(0xC8884E), rnd2(i, 3, 98)), 1.0f); }
  for (int k = 0; k < 5; k++) { float t = k / 4.0f * 6.2832f; (void)t; }
  inkLine(P, X - 28, Y - 20, X - 22, Y - 1, 1.7f, INK, 99, 0.4f); inkLine(P, X + 28, Y - 20, X + 22, Y - 1, 1.7f, INK, 100, 0.4f); inkLine(P, X - 22, Y - 1, X + 22, Y - 1, 1.7f, INK, 101, 0.4f);
  for (int a = 0; a < 30; a++) { float t = a / 30.0f * 6.2832f; seg(P, X + cosf(t) * 29, Y - 20 + sinf(t) * 6, X + cosf(t + 0.21f) * 29, Y - 20 + sinf(t + 0.21f) * 6, 1.5f, INK); }
  // water dish beside it
  float wx = X + 62;
  quad(P, wx - 17, Y - 9, wx + 17, Y - 9, wx + 13, Y, wx - 13, Y, [&](float, float y) { return lerp(hx(0x8A96A4), hx(0x5A6674), (y - (Y - 9)) / 9.0f); });
  softEllipse(P, wx, Y - 9, 18, 4.2f, hx(0xB8C4D0), 1.0f);
  softEllipse(P, wx, Y - 9, 14.5f, 2.8f, hx(0x6AB0D8), 1.0f);
  softEllipse(P, wx - 4, Y - 9.5f, 4, 0.9f, hx(0xD8F4FF), 0.9f);
  inkLine(P, wx - 17, Y - 9, wx - 13, Y, 1.4f, INK, 102, 0.3f); inkLine(P, wx + 17, Y - 9, wx + 13, Y, 1.4f, INK, 103, 0.3f); inkLine(P, wx - 13, Y, wx + 13, Y, 1.4f, INK, 104, 0.3f);
  for (int a = 0; a < 24; a++) { float t = a / 24.0f * 6.2832f; seg(P, wx + cosf(t) * 18, Y - 9 + sinf(t) * 4.2f, wx + cosf(t + 0.26f) * 18, Y - 9 + sinf(t + 0.26f) * 4.2f, 1.3f, INK); }
}

// ------------------------------------------------------------------ STYLE 1: Grimy Cartoon (90s cartoon background vibe)
const Geo GTOON = {176, 104, 46, 18, 314, 194, 260};
// crooked window as a quad
const float TW[4][2] = {{112, 36}, {252, 42}, {246, 148}, {116, 142}};
float toonGlass(float x, float y) {
  // point in convex quad, then mullions
  for (int i = 0; i < 4; i++) {
    int j = (i + 1) % 4;
    float ex = TW[j][0] - TW[i][0], ey = TW[j][1] - TW[i][1];
    if (ex * (y - TW[i][1]) - ey * (x - TW[i][0]) < 3.0f) return 0;
  }
  float mx = lerpf(181, 182, (y - 40) / 108), my = lerpf(90, 96, (x - 112) / 140);
  if (fabsf(x - mx) < 2.5f || fabsf(y - my) < 2.5f) return 0;
  return 1;
}
void styleToon(Pic& P, float hour) {
  const Geo& g = GTOON;
  paintRoom(P, g, [&](const Hit& h, float x, float y) -> F3 {
    float brush = fbm(x * 0.03f, y * 0.35f, 5, 4);     // dry-brush streaks
    float grime = fbm(x * 0.02f, y * 0.02f, 6, 4);
    switch (h.s) {
      case S_BACK: case S_LEFT: case S_RIGHT: {
        F3 base = h.s == S_BACK ? hx(0xD8A090) : hx(0xC48A7C);
        base = base * (0.84f + 0.24f * brush);
        base = lerp(base, hx(0x7A8A58), sst(0.55f, 0.8f, grime) * 0.55f);   // green-grey stains
        float vv = h.v;
        if (vv > 78) base = hx(0x5E7A6A) * (0.8f + 0.3f * brush);           // chipped baseboard
        if (vv > 78 && rnd2((int)(x / 5), (int)(y / 4), 8) > 0.975f) base = hx(0xE8D8B8);
        // exposed bricks where plaster fell off
        if (h.s == S_BACK) {
          float bx = x - 72, by = y - 120;
          float blob = fbm(x * 0.08f, y * 0.08f, 12, 3) - (bx * bx + by * by) / 900.0f;
          if (blob > 0.15f) {
            int row = (int)(y / 6); float off = (row & 1) * 7;
            bool mortar = fmodf(y, 6.0f) < 1.2f || fmodf(x + off, 14.0f) < 1.2f;
            base = mortar ? hx(0xB0A090) : hx(0xA24A34) * (0.8f + 0.3f * rnd2((int)((x + off) / 14), row, 4));
            if (blob < 0.2f) base = hx(0x3A2A2A);
          }
        }
        // airbrushed corner darkening
        float cdist = h.s == S_BACK ? fminf(fminf(x - g.bx0, g.bx1 - x), y - g.by0) : h.u * 0.3f;
        base = base * (0.62f + 0.38f * sst(0, 46, cdist));
        return base * (h.s == S_BACK ? 1.0f : 0.82f);
      }
      case S_FLOOR: {   // warped floorboards converging to the VP
        float bx = h.u / 22.0f + sinf(h.v * 0.03f) * 0.3f;
        int board = (int)floorf(bx);
        float fr = bx - board;
        F3 base = lerp(hx(0x6A4458), hx(0x80546A), rnd2(board, 1, 3));
        base = base * (0.8f + 0.25f * fbm(h.u * 0.1f, h.v * 0.02f, board + 20, 3));
        if (fr < 0.07f) base = hx(0x2A1424);
        float dust = fbm(x * 0.05f, y * 0.05f, 15, 3);
        base = lerp(base, hx(0x8A8070), sst(0.6f, 0.8f, dust) * 0.3f);
        return base * (0.7f + 0.3f * sst(g.by1, PH, y));
      }
      default: {
        F3 base = hx(0xC8B8A0) * (0.8f + 0.2f * brush);
        return lerp(base, hx(0x6A6048), sst(0.5f, 0.8f, grime) * 0.5f) * 0.75f;
      }
    }
  });
  // window view: lurid sky, blobby clouds, crooked tenement silhouette, wires
  for (int y = 30; y < 154; y++) for (int x = 104; x < 260; x++) {
    float m = toonGlass(x + 0.5f, y + 0.5f);
    if (m <= 0) continue;
    float u = (x - 112) / 140.0f, v = (y - 38) / 108.0f;
    float day = dayLight(hour), dusk = duskAmt(hour);
    F3 top = lerp(hx(0x1A1440), hx(0x5ED0C8), day), hor = lerp(hx(0x3A2050), hx(0xF4A0B8), day);
    hor = lerp(hor, hx(0xFF8030), dusk); top = lerp(top, hx(0x8040A0), dusk * 0.6f);
    F3 c = lerp(top, hor, v);
    float cl = fbm(u * 2.5f, v * 5, 41, 3);
    if (cl > 0.58f && v < 0.6f) c = cl > 0.61f ? lerp(hx(0xF8E8C8), hx(0x504070), 1 - day) : hx(0x30202A);
    // buildings
    float bh = 0.55f + 0.18f * rnd2((int)(u * 6), 0, 44) + (u - 0.5f) * 0.12f;
    float lean = (v - 0.6f) * 0.06f;
    int bi = (int)((u + lean) * 6);
    if (v > bh) {
      c = lerp(hx(0x40303A), hx(0x2A2028), rnd2(bi, 2, 45));
      float wu = fmodf((u + lean) * 36, 1.0f), wv = fmodf(v * 22, 1.0f);
      if (wu > 0.35f && wu < 0.75f && wv > 0.3f && wv < 0.75f && rnd2((int)((u + lean) * 36), (int)(v * 22), 46) > 0.55f)
        c = (1 - day) > 0.4f ? hx(0xF8D860) : hx(0x504858);
    }
    // telephone wires
    for (int w = 0; w < 2; w++) { float wy = 0.36f + w * 0.05f + 0.06f * (u - 0.5f) * (u - 0.5f) * 4; if (fabsf(v - wy) < 0.006f) c = hx(0x1A1018); }
    if (gBake) c = f3(18, 18, 26);
    P.at(x, y) = lerp(P.at(x, y), c, m);
    P.win[y * PW + x] = m;
  }
  // frame: thick, peeling green paint
  {
    const F3 frame = hx(0x4A6A50), INK = hx(0x1A1016);
    auto edge = [&](int i, int j, float o) {
      float ax = TW[i][0], ay = TW[i][1], bx = TW[j][0], by = TW[j][1];
      float nx = -(by - ay), ny = bx - ax, L = sqrtf(nx * nx + ny * ny); nx /= L; ny /= L;
      quadC(P, ax - nx * o, ay - ny * o, bx - nx * o, by - ny * o, bx + nx * 3, by + ny * 3, ax + nx * 3, ay + ny * 3, frame);
    };
    for (int i = 0; i < 4; i++) edge(i, (i + 1) % 4, 7);
    for (int y = 28; y < 160; y++) for (int x = 100; x < 264; x++) {
      F3& d = P.at(x, y);
      if (P.win[y * PW + x] > 0.5f) continue;
      if (fabsf(d.r - frame.r) < 1 && fabsf(d.g - frame.g) < 1) {
        float n = fbm(x * 0.2f, y * 0.2f, 61, 3);
        d = n > 0.72f ? hx(0xC8B890) : frame * (0.8f + 0.4f * fbm(x * 0.05f, y * 0.6f, 62, 3));
      }
    }
    for (int i = 0; i < 4; i++) { int j = (i + 1) % 4; inkLine(P, TW[i][0], TW[i][1], TW[j][0], TW[j][1], 2.0f, INK, 100 + i); }
    inkLine(P, 181, 40, 182, 146, 1.6f, INK, 110); inkLine(P, 114, 90, 249, 96, 1.6f, INK, 111);
    // sill
    quadC(P, 108, 146, 252, 152, 256, 160, 104, 154, hx(0x5A7A5A));
    inkLine(P, 104, 154, 256, 160, 2.2f, INK, 112);
    // cracked pane
    inkLine(P, 200, 50, 214, 66, 0.8f, hx(0xF0F0F0), 113, 1.5f); inkLine(P, 214, 66, 208, 80, 0.8f, hx(0xF0F0F0), 114, 1.5f); inkLine(P, 214, 66, 232, 70, 0.8f, hx(0xF0F0F0), 115, 1.5f);
  }
  if (!gBake) sunbeam(P, g, hour, toonGlass, hx(0xFFE070), 0.45f);
  // room edges inked
  {
    const F3 INK = hx(0x1A1016);
    inkLine(P, g.bx0, g.by0, g.bx0, g.by1, 2.4f, INK, 120); inkLine(P, g.bx1, g.by0, g.bx1, g.by1, 2.4f, INK, 121);
    inkLine(P, g.bx0, g.by1, g.bx1, g.by1, 2.4f, INK, 122); inkLine(P, g.bx0, g.by0, g.bx1, g.by0, 2.0f, INK, 123);
    float lx = 0, ly = g.by1 + (g.bx0) * (g.by1 - g.vy) / (g.vx - g.bx0);
    inkLine(P, g.bx0, g.by1, lx, ly, 2.4f, INK, 124);
    float rx = PW, ry = g.by1 + (PW - g.bx1) * (g.by1 - g.vy) / (g.bx1 - g.vx);
    inkLine(P, g.bx1, g.by1, rx, ry, 2.4f, INK, 125);
    inkLine(P, g.bx0, g.by0, 0, g.by0 - g.bx0 * (g.vy - g.by0) / (g.vx - g.bx0), 2.0f, INK, 126);
    inkLine(P, g.bx1, g.by0, PW, g.by0 - (PW - g.bx1) * (g.vy - g.by0) / (g.bx1 - g.vx), 2.0f, INK, 127);
    // wall cracks
    inkLine(P, 280, 30, 292, 58, 1.0f, INK, 130, 3); inkLine(P, 292, 58, 286, 80, 0.9f, INK, 131, 3); inkLine(P, 292, 58, 304, 70, 0.8f, INK, 132, 2);
    inkLine(P, 20, 90, 30, 120, 1.0f, INK, 133, 3);
    // board lines on the floor, inked
    for (int b = -9; b <= 9; b++) {
      float ub = b * 22.0f;
      float x0 = g.vx + ub, y0 = g.by1;
      float D = (g.by1 - g.vy) / (PH - g.vy), x1 = g.vx + ub / D + sinf(b) * 6;
      if (x0 < g.bx0 - 1 || x0 > g.bx1 + 1) continue;
      inkLine(P, x0, y0, x1, PH, 1.0f, hx(0x24101C), 140 + b, 2.5f);
    }
  }
  // bare bulb on a cord
  {
    float bx = 300, by = 70;
    inkLine(P, bx, 0, bx + 2, by - 8, 1.2f, hx(0x1A1016), 150, 1.5f);
    quadC(P, bx - 3, by - 9, bx + 5, by - 9, bx + 5, by - 4, bx - 3, by - 4, hx(0x6A6A6A));
    softEllipse(P, bx + 1, by, 6, 7, hx(0xFFF4B0), 1.0f);
    softEllipse(P, bx + 2.5f, by - 2, 2, 3, hx(0xFFFFFF), 0.9f);
  }
  table(P, g, T_WONKY, 64, 196, hx(0x9A6A3C), true, hx(0x8A5A30));
  cupOnTable(P, g, 64, 196, 34, -8, hx(0xC8E8E0), true);
  grimyLitter(P, 52, 268);
  grimyBowl(P, 268, 268);
  gradeRoom(P, hour, hx(0xFFE090), 0.7f);
  glow(P, 301, 70, 90, hx(0xFFE0A0), 0.5f * (1 - dayLight(hour) * 0.7f));
  vignette(P, 0.5f);
}

// ------------------------------------------------------------------ STYLE 2: Modern Apartment
const Geo GMOD = {184, 108, 40, 16, 328, 194, 320};
float modGlass(float x, float y) {   // floor-to-ceiling glass, three panes
  if (x < 64 || x > 304 || y < 24 || y > 190) return 0;
  if (fabsf(x - 144) < 1.5f || fabsf(x - 224) < 1.5f || fabsf(y - 162) < 1.0f) return 0;
  return 1;
}
void styleModern(Pic& P, float hour) {
  const Geo& g = GMOD;
  paintRoom(P, g, [&](const Hit& h, float x, float y) -> F3 {
    switch (h.s) {
      case S_BACK: return hx(0xEEEBE6) * (0.97f + 0.03f * fbm(x * 0.1f, y * 0.1f, 2, 2));
      case S_LEFT: {   // board-formed concrete accent wall
        float n = fbm(h.u * 0.08f, h.v * 0.08f, 3, 4), fine = rnd2((int)(h.u * 2), (int)(h.v * 2), 4);
        F3 c = hx(0xB4B2AC) * (0.86f + 0.16f * n + 0.05f * fine);
        float bu = fmodf(h.v + 200, 24.0f); if (bu < 0.8f) c = c * 0.82f;
        float tu = fmodf(h.u, 40.0f) - 20, tv = fmodf(h.v + 200, 24.0f) - 12;
        if (tu * tu + tv * tv < 3) c = c * 0.55f;
        return c * 0.9f;
      }
      case S_RIGHT: return hx(0xE8E4DE) * 0.86f;
      case S_FLOOR: {   // wide light-oak planks
        float bw = 30.0f;
        int board = (int)floorf(h.u / bw + 100);
        float len = 140 + 60 * rnd2(board, 0, 7), off = rnd2(board, 1, 7) * len;
        int seg_ = (int)floorf((h.v + off) / len);
        F3 base = lerp(hx(0xD8B88E), hx(0xC8A276), rnd2(board, seg_, 8));
        base = wood(base, h.v + off, h.u * 2, board * 13 + seg_);
        float fu = h.u / bw + 100 - board, fv = (h.v + off) / len - seg_;
        if (fu < 0.03f || fv < 0.004f) base = base * 0.72f;
        return base;
      }
      default: {   // ceiling with recessed downlights
        F3 c = hx(0xF4F2EE) * 0.94f;
        float lu = fmodf(h.u + 1000, 90.0f) - 45, lv = fmodf(h.v + 30, 70.0f) - 35;
        if (lu * lu / 36 + lv * lv / 9 < 1) c = hx(0xFFFFFF) * 1.05f;
        return c;
      }
    }
  });
  // city view
  for (int y = 20; y < 194; y++) for (int x = 60; x < 308; x++) {
    float m = modGlass(x + 0.5f, y + 0.5f);
    if (m <= 0) continue;
    float u = (x - 64) / 240.0f, v = (y - 24) / 166.0f;
    float day = dayLight(hour), dusk = duskAmt(hour), night = 1 - fmaxf(day, dusk * 0.8f);
    F3 c = skyAt(u, v * 0.7f, hour, 21, 240.0f / 166 * 0.7f);
    // far towers (hazy) then near towers
    for (int layer = 0; layer < 2; layer++) {
      float sc = layer ? 9.0f : 16.0f;
      int bi = (int)floorf(u * sc);
      float top = layer ? 0.42f + 0.22f * rnd2(bi, 3, 22) : 0.5f + 0.15f * rnd2(bi, 5, 23);
      if (bi % 4 == 2 && layer) top -= 0.12f;
      if (v > top) {
        float fu = u * sc - bi;
        F3 b = layer ? lerp(hx(0x6A7A90), hx(0x8496AE), rnd2(bi, 9, 24)) : hx(0xA8B8CC);
        b = b * (0.35f + 0.65f * fmaxf(day, dusk * 0.6f));
        if (fu > 0.86f) b = b * 0.8f;
        float wu = fmodf(fu * 6, 1.0f), wv = fmodf((v - top) * 60, 1.0f);
        bool win_ = wu > 0.25f && wu < 0.8f && wv > 0.3f && wv < 0.8f && fu < 0.86f;
        if (win_) {
          F3 glassDay = lerp(b, hx(0xC8DCF0), 0.3f);
          bool lit = rnd2((int)(u * sc * 6), (int)(v * 60), 25 + layer) > 0.55f;
          b = lerp(glassDay, lit ? hx(0xFFD890) : b * 0.8f, night);
        }
        if (!layer) b = lerp(b, skyCol(0.9f, hour), 0.5f);
        c = b;
      }
    }
    // balcony glass rail
    if (v > 0.84f) c = lerp(c, hx(0x9CC4D4), 0.25f);
    if (fabsf(v - 0.84f) < 0.006f) c = hx(0x3A3A3A);
    P.at(x, y) = lerp(P.at(x, y), c, m);
    P.win[y * PW + x] = m;
  }
  // slim black frames
  {
    const F3 blk = hx(0x22252A);
    for (float mx : {144.0f, 224.0f}) seg(P, mx, 24, mx, 190, 3, blk);
    seg(P, 64, 162, 304, 162, 2, blk);
    seg(P, 62, 22, 306, 22, 4, blk); seg(P, 62, 22, 62, 192, 4, blk); seg(P, 306, 22, 306, 192, 4, blk); seg(P, 62, 192, 306, 192, 3, blk);
  }
  sunbeam(P, g, hour, modGlass, hx(0xFFF0D0), 0.5f);
  floorReflect(P, g, 0.35f);
  table(P, g, T_HAIRPIN, 70, 220, hx(0xD2AA78), false, hx(0x1C1C1E));
  cupOnTable(P, g, 70, 220, 34, 25, hx(0xE8F4FF), false);   // (right of the window, so it reads as a glass)
  grimyLitter(P, 52, 268);   // the cat's litter box & bowls move house with you (same spots in every room)
  grimyBowl(P, 268, 268);
  gradeRoom(P, hour, hx(0xFFE6C0), 0.6f);
  // downlight pools at night
  float nightK = 1 - dayLight(hour);
  if (nightK > 0.05f) for (float lx : {-90.0f, 0.0f, 90.0f}) { float sx, sy; proj(g, lx, g.by1 - g.vy, 250, sx, sy); glow(P, sx, sy, 60, hx(0xFFE0B0), 0.25f * nightK); }
  vignette(P, 0.22f);
}

// ------------------------------------------------------------------ STYLE 3: Seaside Villa
const Geo GVIL = {184, 110, 50, 20, 318, 192, 300};
float vilGlass(float x, float y) {   // arched window
  const float x0 = 122, x1 = 246, yt = 90, y1 = 160, cx = 184, r = 62;
  if (x < x0 || x > x1 || y > y1) return 0;
  if (y < yt) { float dx = x - cx, dy = y - yt; if (dx * dx + dy * dy > r * r) return 0; }
  if (fabsf(x - 184) < 1.8f) return 0;
  if (fabsf(y - 110) < 1.5f) return 0;
  return 1;
}
void styleVilla(Pic& P, float hour) {
  const Geo& g = GVIL;
  paintRoom(P, g, [&](const Hit& h, float x, float y) -> F3 {
    switch (h.s) {
      case S_BACK: case S_LEFT: case S_RIGHT: {
        // stucco relief: shade by the noise gradient
        float s = 0.09f;
        float a = fbm(x * s, y * s, 31, 3), b = fbm((x + 1) * s, y * s, 31, 3), c2 = fbm(x * s, (y + 1) * s, 31, 3);
        float lit = 1 + ((a - b) * 0.55f + (a - c2) * 0.75f) + 0.04f * (fbm(x * 0.02f, y * 0.02f, 32, 2) - 0.5f);
        F3 base = hx(0xF6F0E2) * (0.95f * lit);
        float vv = h.v;
        if (vv > 70 && vv < 82) {   // blue & white tile strip
          float tu = (h.s == S_BACK ? x : h.u * 1.2f);
          float tt = fmodf(tu, 12.0f), tv = fmodf(vv - 70, 12.0f);
          float dd = fabsf(tt - 6) + fabsf(tv - 6);
          base = dd < 3 ? hx(0x2A5AA8) : (dd < 4 ? hx(0xF0F4FA) : (dd > 5.2f ? hx(0x3A6AB8) : hx(0xF0F4FA)));
          if (tt < 0.6f || tv < 0.6f) base = hx(0xC8C8C0);
        }
        if (vv >= 82) base = hx(0xE8E0D0) * 0.9f;
        return base * (h.s == S_BACK ? 1.0f : (h.s == S_LEFT ? 0.9f : 0.85f));
      }
      case S_FLOOR: {   // terracotta tiles
        float ts = 26.0f;
        int tu = (int)floorf(h.u / ts + 100), tv = (int)floorf(h.v / ts);
        float fu = h.u / ts + 100 - tu, fv = h.v / ts - tv;
        F3 base = lerp(hx(0xC0643C), hx(0xD6845A), rnd2(tu, tv, 33));
        base = base * (0.9f + 0.14f * fbm(h.u * 0.1f, h.v * 0.1f, tu * 7 + tv, 3));
        float edge = fminf(fminf(fu, 1 - fu), fminf(fv, 1 - fv));
        if (edge < 0.05f) base = hx(0xD8CCB8);
        else base = base * (0.88f + 0.12f * sst(0.05f, 0.2f, edge));
        return base;
      }
      default: {   // white ceiling with dark timber beams running front to back
        F3 c = hx(0xF4EEE2) * 0.9f;
        float bu = fmodf(h.u + 1000, 60.0f);
        if (bu < 12) c = wood(hx(0x5A3A22), h.v, h.u, 35) * (bu < 2 || bu > 10 ? 0.7f : 1.0f);
        return c;
      }
    }
  });
  // view: sea, island, bougainvillea
  for (int y = 24; y < 164; y++) for (int x = 118; x < 250; x++) {
    float m = vilGlass(x + 0.5f, y + 0.5f);
    if (m <= 0) continue;
    float u = (x - 122) / 124.0f, v = (y - 28) / 132.0f;
    float day = dayLight(hour), dusk = duskAmt(hour), lk = 0.25f + 0.75f * fmaxf(day, dusk * 0.7f);
    F3 c = skyAt(u, v * 0.95f, hour, 31, 124.0f / 132 * 0.95f);
    const float hz = 0.62f;
    if (v > hz) {
      float d = (v - hz) / (1 - hz);
      c = lerp(hx(0x2A9AC0), hx(0x1A6A9A), d) * lk;
      c = lerp(c, skyCol(1, hour), 0.25f * (1 - d));
      if (rnd2((int)(u * 90), (int)(v * 160), 36) > 0.975f) c = c + hx(0xFFFFFF) * (0.22f * lk);   // glints
    }
    float isl = hz - 0.08f * expf(-((u - 0.72f) * (u - 0.72f)) / 0.006f);
    if (v > isl && v <= hz) c = lerp(hx(0x7A8A80), skyCol(0.9f, hour), 0.45f) * lk;
    // bougainvillea from the top-right
    float bd = sqrtf((u - 1.0f) * (u - 1.0f) + (v - 0.05f) * (v - 0.05f) * 0.6f);
    float fl = fbm(u * 14, v * 14, 37, 3);
    if (bd < 0.34f && fl > 0.5f + bd * 0.5f) { float pt = rnd2((int)(u * 90), (int)(v * 90), 38); c = (pt > 0.55f && fl > 0.56f ? lerp(hx(0xD82A7A), hx(0xFF6AB0), pt) : lerp(hx(0x2A5A2A), hx(0x4A8A3A), fl)) * lk; }
    P.at(x, y) = lerp(P.at(x, y), c, m);
    P.win[y * PW + x] = m;
  }
  // blue frame + open shutters
  {
    const F3 blue = hx(0x2A6AB0);
    for (int y = 24; y < 166; y++) for (int x = 114; x < 254; x++) {
      float fx = x + 0.5f, fy = y + 0.5f;
      if (vilGlass(fx, fy) > 0) continue;
      bool inOuter;
      if (fx < 116 || fx > 252 || fy > 164) inOuter = false;
      else if (fy < 90) { float dx = fx - 184, dy = fy - 90; inOuter = dx * dx + dy * dy <= 68 * 68; }
      else inOuter = true;
      if (inOuter) P.at(x, y) = blue * (0.9f + 0.15f * fbm(fx * 0.1f, fy * 0.3f, 38, 2));
    }
    seg(P, 112, 164, 256, 164, 4, hx(0xEDE6D8)); seg(P, 112, 166.5f, 256, 166.5f, 1, hx(0xB8B0A0));   // sill
    for (int s = 0; s < 2; s++) {   // shutters flat against the wall
      float x0 = s ? 256 : 82, x1 = s ? 286 : 112;
      quad(P, x0, 46, x1, 46, x1, 160, x0, 160, [&](float x, float y) {
        F3 c = blue * 0.95f;
        if (fmodf(y, 6.0f) < 1.4f) c = c * 0.72f;
        if (x < x0 + 2 || x > x1 - 2) c = c * 0.8f;
        return c * (0.92f + 0.12f * fbm(x * 0.2f, y * 0.05f, 39 + s, 2));
      });
      softEllipse(P, (x0 + x1) / 2 + (s ? 2 : -2), 160, 18, 3, f3(30, 20, 20), 0.25f);
    }
  }
  sunbeam(P, g, hour, vilGlass, hx(0xFFE2A8), 0.6f);
  table(P, g, T_RUSTIC, 64, 210, hx(0xC4A070), false, hx(0xA88654));
  cupOnTable(P, g, 64, 210, 34, -6, hx(0xA8D8F0), false);
  grimyLitter(P, 52, 268);   // the cat's litter box & bowls move house with you (same spots in every room)
  grimyBowl(P, 268, 268);
  gradeRoom(P, hour, hx(0xFFD0A0), 0.55f);
  vignette(P, 0.25f);
}

// ------------------------------------------------------------------ STYLE 4: Cozy Treehouse
const Geo GTREE = {184, 112, 54, 24, 314, 190, 280};
float treeGlass(float x, float y) {   // big round window, four spokes
  float dx = x - 184, dy = y - 98, d = sqrtf(dx * dx + dy * dy);
  if (d > 60) return 0;
  if (fabsf(dx) < 2 || fabsf(dy) < 2) return 0;
  if (d < 8) return 0;
  return 1;
}
void styleTree(Pic& P, float hour) {
  const Geo& g = GTREE;
  paintRoom(P, g, [&](const Hit& h, float x, float y) -> F3 {
    switch (h.s) {
      case S_BACK: case S_LEFT: case S_RIGHT: {   // horizontal planks
        float vv = (h.s == S_BACK ? y : h.v + 200);
        float pu = h.s == S_BACK ? x : h.u;
        int row = (int)floorf(vv / 14);
        F3 base = lerp(hx(0xC48A4E), hx(0xD8A060), rnd2(row, 4, 51));
        base = wood(base, pu + row * 37, vv * 2, 52 + row, 1.3f);
        float fv = vv / 14 - row;
        base = base * (0.85f + 0.2f * sinf(fv * 3.14159f));
        if (fv < 0.08f) base = hx(0x5A341C);
        float nu = fmodf(pu + row * 23 + 1000, 70.0f);
        if (fabsf(nu - 4) < 1.2f && fabsf(fv - 0.5f) < 0.1f) base = hx(0x4A3A30);   // nail heads
        return base * (h.s == S_BACK ? 1.0f : (h.s == S_LEFT ? 0.86f : 0.8f));
      }
      case S_FLOOR: {   // rough wide planks running toward the window
        float bw = 34;
        int board = (int)floorf(h.u / bw + 100);
        F3 base = lerp(hx(0x8A5A34), hx(0x9E6A3E), rnd2(board, 1, 53));
        base = wood(base, h.v, h.u * 3, board * 3, 1.2f);
        float fu = h.u / bw + 100 - board;
        if (fu < 0.05f) base = hx(0x3A2210);
        if (fabsf(fmodf(h.v + 1000, 120.0f) - 60) < 2 && fabsf(fu - 0.5f) < 0.3f) base = base * 0.85f;
        return base;
      }
      default: {
        F3 base = hx(0x7A4E2C);
        base = wood(base, h.v, h.u, 54);
        float fu = fmodf(h.u + 1000, 36.0f);
        if (fu < 1.5f) base = base * 0.6f;
        return base * 0.85f;
      }
    }
  });
  // foliage view
  for (int y = 34; y < 162; y++) for (int x = 120; x < 248; x++) {
    float m = treeGlass(x + 0.5f, y + 0.5f);
    if (m <= 0) continue;
    float u = (x - 124) / 120.0f, v = (y - 38) / 120.0f;
    float day = dayLight(hour), dusk = duskAmt(hour), lk = 0.2f + 0.8f * fmaxf(day, dusk * 0.7f);
    F3 c = skyAt(u, v * 0.8f, hour, 41, 0.8f);
    float lf = fbm(u * 7, v * 7, 42, 4);
    float rim = sqrtf((u - 0.5f) * (u - 0.5f) + (v - 0.5f) * (v - 0.5f));
    float th = 0.62f - rim * 0.4f;
    if (lf > th) {
      float tone = fbm(u * 18, v * 18, 43, 3);
      c = lerp(hx(0x2E6A2A), hx(0x7ABA48), tone) * lk;
      if (tone > 0.68f) c = hx(0xB8E070) * lk;
    }
    // branch
    float by = 0.62f + (u - 0.2f) * 0.25f + 0.03f * sinf(u * 9);
    if (u > 0.05f && fabsf(v - by) < 0.035f - u * 0.02f) c = lerp(hx(0x5A3A24), hx(0x3A2414), (v - by) * 20 + 0.5f) * lk;
    P.at(x, y) = lerp(P.at(x, y), c, m);
    P.win[y * PW + x] = m;
  }
  // thick round wooden frame + spokes
  for (int y = 28; y < 170; y++) for (int x = 114; x < 256; x++) {
    float dx = x + 0.5f - 184, dy = y + 0.5f - 98, d = sqrtf(dx * dx + dy * dy);
    bool spoke = d < 60 && (fabsf(dx) < 2 || fabsf(dy) < 2 || d < 8);
    if (d >= 60 && d < 70) {
      float a = atan2f(dy, dx);
      F3 c = wood(hx(0x8A5428), a * 120, d * 2, 55);
      float sh = 0.75f + 0.35f * (-dy / d) * 0.5f + 0.25f * sinf((d - 60) / 10 * 3.14159f);
      P.blend(x, y, c * sh, cl01(70 - d + 0.5f));
    } else if (spoke) P.at(x, y) = wood(hx(0x7A4A22), dx + dy, d, 56);
  }
  // dappled light: sunbeam modulated by leaves
  {
    auto leafy = [&](float x, float y) {
      float m = treeGlass(x, y);
      if (m <= 0) return 0.0f;
      float u = (x - 124) / 120.0f, v = (y - 38) / 120.0f;
      return fbm(u * 7, v * 7, 42, 4) > 0.5f ? 0.15f : 1.0f;
    };
    sunbeam(P, g, hour, leafy, hx(0xFFE8A0), 0.6f);
  }
  // vine creeping along the ceiling edge
  for (int i = 0; i < 46; i++) {
    float t = i / 46.0f, x = 56 + t * 120, y = 26 + 6 * sinf(t * 9) + t * 8;
    seg(P, x, y, x + 3, y + 1, 1.2f, hx(0x3A5A24));
    if (i % 3 == 0) { softEllipse(P, x + 2, y + 4, 3.5f, 2.2f, hx(0x4A8A34), 1.0f); softEllipse(P, x + 1, y + 3.5f, 1.5f, 1.0f, hx(0x8ACA5A), 0.8f); }
  }
  table(P, g, T_LOG, 66, 200, hx(0xC89A60), false, hx(0x6A4428));
  cupOnTable(P, g, 66, 200, 30, -4, hx(0xD8F0E8), false);
  grimyLitter(P, 52, 268);   // the cat's litter box & bowls move house with you (same spots in every room)
  grimyBowl(P, 268, 268);
  gradeRoom(P, hour, hx(0xFFC070), 0.65f);
  vignette(P, 0.35f);
}

// ------------------------------------------------------------------ output
void toCanvas(Pic& P, Canvas& hi) {
  static const float bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  for (int y = 0; y < PH; y++) for (int x = 0; x < PW; x++) {
    F3 c = P.at(x, y);
    float t = (bayer[y & 3][x & 3] + 0.5f) / 16.0f;
    int r = (int)(cl01(c.r / 255.f) * 31 + t), gg = (int)(cl01(c.g / 255.f) * 63 + t), b = (int)(cl01(c.b / 255.f) * 31 + t);
    if (r > 31) r = 31; if (gg > 63) gg = 63; if (b > 31) b = 31;
    Color px = (Color)((r << 11) | (gg << 5) | b);
    if (gBake && P.win[y * PW + x] > 0.5f) { px = 0xF81F; if (gGlassOut) gGlassOut[y * PW + x] = 1; }
    hi.set(x, y + TOPY, px);
  }
}

// ------------------------------------------------------------------ STYLE 5: Classy Log Cabin
const Geo GCAB = {184, 112, 50, 22, 318, 190, 290};
float cabGlass(float x, float y) {   // wide picture window, three tall panes
  if (x < 112 || x > 256 || y < 36 || y > 146) return 0;
  if (fabsf(x - 160) < 1.6f || fabsf(x - 208) < 1.6f) return 0;
  return 1;
}
void styleCabin(Pic& P, float hour) {
  const Geo& g = GCAB;
  paintRoom(P, g, [&](const Hit& h, float x, float y) -> F3 {
    switch (h.s) {
      case S_BACK: case S_LEFT: case S_RIGHT: {   // stacked round logs with pale chinking
        float vv = (h.s == S_BACK ? y : h.v + g.vy);   // (side walls: the same height scale as the back wall, so each log row meets its twin in the corners)
        float pu = h.s == S_BACK ? x : h.u;
        const float LH = 17;
        int row = (int)floorf(vv / LH);
        float fv = vv / LH - row;
        if (fv < 0.12f) return hx(0xE6D8BC) * (0.8f + 0.25f * fbm(pu * 0.2f, row, 71, 2)) * (h.s == S_BACK ? 1.0f : 0.85f);   // chinking
        F3 base = lerp(hx(0xB0703A), hx(0xC8884A), rnd2(row, 7, 72));
        base = wood(base, pu + row * 51, vv * 2, 73 + row, 1.1f);
        float r = (fv - 0.12f) / 0.88f;                         // round profile
        base = base * (0.62f + 0.55f * sinf(r * 3.14159f) - 0.12f * r);
        if (fabsf(fmodf(pu + row * 97 + 2000, 260.0f) - 130) < 3 && fabsf(r - 0.5f) < 0.3f) base = base * 0.7f;   // log joints
        return base * (h.s == S_BACK ? 1.0f : (h.s == S_LEFT ? 0.84f : 0.78f));
      }
      case S_FLOOR: {   // polished walnut boards
        float bw = 26;
        int board = (int)floorf(h.u / bw + 100);
        F3 base = lerp(hx(0x5A3820), hx(0x6E4628), rnd2(board, 3, 74));
        base = wood(base, h.v, h.u * 3, board * 5 + 75, 1.0f);
        float fu = h.u / bw + 100 - board;
        if (fu < 0.04f) base = hx(0x2A180C);
        return base;
      }
      default: {   // ceiling: dark beams
        F3 base = wood(hx(0x6A4426), h.v, h.u, 76);
        if (fmodf(h.u + 1000, 48.0f) < 6) base = base * 0.62f;
        return base * 0.8f;
      }
    }
  });
  // Stone fireplace against the LEFT wall, near the back corner (the back wall's bottom-left corner is kept clear for
  // the mouse hole). Everything is built in room space and projected, so the stone courses, the mantel and the
  // hearth run toward the vanishing point with the logs: it sits flush on the wall instead of facing the viewer.
  // Room space: X across (left wall at WX), Y down (floor at FY, ceiling at CY), Z depth (back wall at g.F).
  {
    const float WX = g.bx0 - g.vx, FY = g.by1 - g.vy, CY = g.by0 - g.vy;   // -134, 78, -90
    const float FX = WX + 11;                     // the chimney breast's face stands 11 out from the logs
    const float Z0 = 222, Z1 = 270;               // its near and far ends along the wall
    const float OZ0 = 232, OZ1 = 260, OY = 38;    // the firebox opening (its top edge at Y = OY)
    auto P2 = [&](float X, float Y, float Z, float& x, float& y) { proj(g, X, Y, Z, x, y); };
    auto stone = [&](float a, float b, uint32_t s) -> F3 {   // a = along the face, b = Y (both in room units)
      int row = (int)floorf((b + 200) / 9), col = (int)floorf((a + (row & 1) * 7) / 14);
      float su = fmodf(a + (row & 1) * 7 + 1400, 14.0f), sv = fmodf(b + 200 + 900, 9.0f);
      F3 st = lerp(hx(0x8A8478), hx(0xB4AC9C), rnd2(row, col, s));
      st = st * (0.85f + 0.3f * fbm(a * 0.3f, b * 0.3f, s + 1, 2));
      if (su < 1.3f || sv < 1.3f) st = hx(0x4A443C);   // mortar
      return st;
    };
    float ax, ay, bx, by, cx, cy, dx, dy;
    // the near side of the breast (faces the viewer), X from the wall out to the face
    P2(WX, CY, Z0, ax, ay); P2(FX, CY, Z0, bx, by); P2(FX, FY, Z0, cx, cy); P2(WX, FY, Z0, dx, dy);
    quad(P, ax, ay, bx, by, cx, cy, dx, dy, [&](float x, float y) {
      const float X = (x - g.vx) * Z0 / g.F, Y = (y - g.vy) * Z0 / g.F;
      return stone(X * 1.4f, Y, 177) * 0.7f;         // turned away from the room light
    });
    // the face, parallel to the wall: stones mapped by depth (Z) and height, so the courses follow the perspective
    P2(FX, CY, Z0, ax, ay); P2(FX, CY, Z1, bx, by); P2(FX, FY, Z1, cx, cy); P2(FX, FY, Z0, dx, dy);
    quad(P, ax, ay, bx, by, cx, cy, dx, dy, [&](float x, float y) {
      const float Z = FX * g.F / (x - g.vx), Y = (y - g.vy) * Z / g.F;
      return stone(Z, Y, 77) * 0.9f;
    });
    // a shadow line where the breast meets the logs at its far end
    P2(FX, CY, Z1, ax, ay); P2(FX, FY, Z1, bx, by); seg(P, ax, ay, bx, by, 1.2f, hx(0x2A2018), 0.6f);
    // firebox: a dark opening in the face, with a soot-blackened arch of stone round it
    P2(FX, OY - 4, OZ0 - 4, ax, ay); P2(FX, OY - 4, OZ1 + 4, bx, by); P2(FX, FY, OZ1 + 4, cx, cy); P2(FX, FY, OZ0 - 4, dx, dy);
    quadC(P, ax, ay, bx, by, cx, cy, dx, dy, hx(0x5A4E44), 0.55f);
    P2(FX, OY, OZ0, ax, ay); P2(FX, OY, OZ1, bx, by); P2(FX, FY, OZ1, cx, cy); P2(FX, FY, OZ0, dx, dy);
    quadC(P, ax, ay, bx, by, cx, cy, dx, dy, hx(0x1A100C));
    // the log in the grate and the flames, standing up from the firebox floor
    { float lx0, ly0, lx1, ly1; P2(FX - 3, FY - 3, OZ0 + 3, lx0, ly0); P2(FX - 3, FY - 3, OZ1 - 3, lx1, ly1); seg(P, lx0, ly0, lx1, ly1, 3.0f, hx(0x3A2210)); }
    for (int k = 0; k < 6; k++) {
      const float Z = OZ0 + 3 + k * (OZ1 - OZ0 - 6) / 5.0f, fh = 16 + 12 * rnd2(k, 1, 79);
      float fx, fy; P2(FX - 3, FY - 4, Z, fx, fy);
      const float sc = g.F / Z;                      // nearer flames are drawn a touch bigger
      softEllipse(P, fx, fy - fh * 0.4f * sc, 2.8f * sc, fh * 0.5f * sc, hx(0xFF8A24), 1.0f);
      softEllipse(P, fx, fy - fh * 0.22f * sc, 1.4f * sc, fh * 0.3f * sc, hx(0xFFE070), 1.0f);
    }
    // mantel: a heavy timber shelf across the face, sticking out into the room
    const float MY = OY - 12, MX = FX + 7;
    P2(MX, MY, Z0 - 3, ax, ay); P2(MX, MY, Z1 + 3, bx, by); P2(MX, MY + 5, Z1 + 3, cx, cy); P2(MX, MY + 5, Z0 - 3, dx, dy);
    quadC(P, ax, ay, bx, by, cx, cy, dx, dy, hx(0x5A3A20));                       // its front
    P2(FX, MY, Z0 - 3, ax, ay); P2(FX, MY, Z1 + 3, bx, by); P2(MX, MY, Z1 + 3, cx, cy); P2(MX, MY, Z0 - 3, dx, dy);
    quadC(P, ax, ay, bx, by, cx, cy, dx, dy, hx(0x8A6038));                       // its top
    P2(FX, MY + 5, Z0 - 3, ax, ay); P2(MX, MY + 5, Z0 - 3, bx, by); P2(MX, MY, Z0 - 3, cx, cy); P2(FX, MY, Z0 - 3, dx, dy);
    quadC(P, ax, ay, bx, by, cx, cy, dx, dy, hx(0x3A2414));                       // its near end
    // the hearth: a stone slab on the floor in front of the fire
    P2(FX, FY, OZ0 - 8, ax, ay); P2(FX, FY, OZ1 + 8, bx, by); P2(FX + 16, FY, OZ1 + 8, cx, cy); P2(FX + 16, FY, OZ0 - 8, dx, dy);
    quad(P, ax, ay, bx, by, cx, cy, dx, dy, [&](float x, float y) {
      const float Z = FY * g.F / (y - g.vy), X = (x - g.vx) * Z / g.F;
      return stone(Z, X * 2.2f, 91) * 0.8f;
    });
    // warm firelight, thrown out of the opening into the room (mostly across the floor)
    float gx, gy; P2(FX, FY - 12, (OZ0 + OZ1) * 0.5f, gx, gy);
    glow(P, gx, gy, 70, hx(0xFF9A40), 0.55f);
  }
  // picture window: view of a river winding through the forest (live in game; this is the preview)
  for (int y = 30; y < 152; y++) for (int x = 106; x < 262; x++) {
    float m = cabGlass(x + 0.5f, y + 0.5f);
    if (m <= 0) continue;
    float u = (x - 112) / 144.0f, v = (y - 36) / 110.0f;
    float lk = 0.2f + 0.8f * fmaxf(dayLight(hour), duskAmt(hour) * 0.7f);
    F3 c = skyAt(u, v * 0.8f, hour, 81, 1.3f);
    float far = 0.45f + 0.08f * fbm(u * 8, 0, 82, 3);
    if (v > far) c = lerp(hx(0x3A5A3A), hx(0x5A7A4A), fbm(u * 20, v * 20, 83, 2)) * lk;
    float rc = 0.5f + 0.18f * sinf(v * 7 + 1.0f) * (v - 0.5f) * 2;   // river centre wanders, widens toward us
    float rw = 0.03f + 0.22f * cl01((v - 0.55f) / 0.45f);
    if (v > 0.55f && fabsf(u - rc) < rw) c = lerp(skyAt(u, 0.3f, hour, 81, 1.3f), hx(0x3A6A8A), 0.5f) * (0.6f + 0.4f * lk);
    if (fabsf(u - 0.05f) < 0.08f + 0.1f * v || fabsf(u - 0.95f) < 0.08f + 0.1f * v) { if (v > 0.15f + 0.3f * fbm(u * 10, v * 6, 84, 2)) c = hx(0x22402A) * lk; }
    P.at(x, y) = lerp(P.at(x, y), c, m);
    P.win[y * PW + x] = m;
  }
  // heavy timber frame + sill
  for (int y = 28; y < 156; y++) for (int x = 104; x < 264; x++) {
    bool inG = x >= 112 && x <= 256 && y >= 36 && y <= 146;
    bool mull = inG && (fabsf(x + 0.5f - 160) < 1.6f || fabsf(x + 0.5f - 208) < 1.6f);
    if (!inG || mull) P.at(x, y) = wood(hx(0x6A4024), x * 0.5f + y, y * 0.5f, 85) * (mull ? 0.9f : (y < 36 ? 1.05f : 0.9f));
  }
  quadC(P, 100, 146, 268, 146, 272, 154, 96, 154, hx(0x7A4C2A));
  quadC(P, 100, 146, 268, 146, 268, 147.5f, 100, 147.5f, hx(0xA87040));
  // plaid rug
  for (int y = 236; y < 300; y++) for (int x = 70; x < 300; x++) {
    float dx = (x - 185) / 112.0f, dy = (y - 268) / 30.0f;
    if (dx * dx + dy * dy > 1) continue;
    bool a = ((x / 10) & 1) != 0, b = ((y / 6) & 1) != 0;
    F3 c = a && b ? hx(0x6A1E24) : (a || b ? hx(0x8A2A2E) : hx(0xA83A38));
    if ((x % 10) == 0 || (y % 6) == 0) c = hx(0x2A3A2A);
    P.blend(x, y, c, cl01((1 - (dx * dx + dy * dy)) * 8) * 0.9f);
  }
  floorReflect(P, g, 0.2f);
  if (!gBake) sunbeam(P, g, hour, cabGlass, hx(0xFFE4A8), 0.45f);
  table(P, g, T_RUSTIC, 66, 200, hx(0x8A5A34), false, hx(0x4A2E18));
  cupOnTable(P, g, 66, 200, 30, -4, hx(0xD8F0E8), false);
  grimyLitter(P, 52, 268);   // the cat's litter box & bowls move house with you (same spots in every room)
  grimyBowl(P, 268, 268);
  gradeRoom(P, hour, hx(0xFFB868), 0.6f);
  {   // the fire lights the room: strongest on the floor in front of it and across toward the middle, more at night
    float fx, fy; proj(g, g.bx0 - g.vx + 11, g.by1 - g.vy - 12, 246, fx, fy);
    const float k = 1 - dayLight(hour) * 0.6f;
    glow(P, fx, fy, 95, hx(0xFF9A40), 0.38f * k);
    glow(P, fx + 70, fy + 40, 110, hx(0xFF9A40), 0.18f * k);   // the pool of light it throws out over the floor
  }
  vignette(P, 0.3f);
}

// ------------------------------------------------------------------ STYLE 6: Halo Bridge (secret: both secret cats)
// An angelic starship cabin: pearl hull with gold trim, a huge oval porthole onto deep space, a halo light, a rainbow strip.
const Geo GHAL = {184, 112, 46, 18, 322, 192, 290};
float halGlass(float x, float y) {   // a big oval porthole filling most of the back wall
  const float dx = (x - 184) / 108.0f, dy = (y - 100) / 72.0f, d = dx * dx + dy * dy;
  if (d >= 1) return 0;
  return cl01((1 - d) * 60);   // a soft, anti-aliased rim
}
void styleHalo(Pic& P, float hour) {
  const Geo& g = GHAL;
  paintRoom(P, g, [&](const Hit& h, float x, float y) -> F3 {
    switch (h.s) {
      case S_BACK: {   // pearly panels with a faint shimmer
        F3 c = lerp(hx(0xF2EEFC), hx(0xE6E0F6), cl01((y - 18) / 174.0f));
        c = c * (0.97f + 0.04f * fbm(x * 0.05f, y * 0.05f, 61, 3));
        if (fabsf(fmodf(x + 400, 67.0f)) < 0.9f) c = c * 0.93f;   // panel seams
        return c;
      }
      case S_LEFT: case S_RIGHT: {   // curved hull ribs in soft gold
        F3 c = hx(0xDCD6F0) * (h.s == S_LEFT ? 0.92f : 0.86f);
        float r = fmodf(h.u + 1000, 46.0f);
        if (r < 3.0f) c = lerp(c, hx(0xE6C47A), 0.75f);
        else if (r < 5.0f) c = c * 0.9f;
        return c;
      }
      case S_FLOOR: {   // lavender pearl tiles
        float tu = fmodf(h.u + 1000, 40.0f), tv = fmodf(h.v + 1000, 40.0f);
        F3 c = lerp(hx(0xDCD2F2), hx(0xCEC4EA), rnd2((int)((h.u + 1000) / 40), (int)((h.v + 1000) / 40), 62));
        if (tu < 1.0f || tv < 1.0f) c = c * 0.86f;
        return c;
      }
      default: {   // ceiling
        F3 c = hx(0xF4F0FC) * 0.92f;
        return c;
      }
    }
  });
  // the space view (stands in for the live window, which animates it in the game)
  for (int y = 24; y < 176; y++) for (int x = 72; x < 296; x++) {
    float m = halGlass(x + 0.5f, y + 0.5f);
    if (m <= 0) continue;
    float u = (x - 76) / 216.0f, v = (y - 28) / 144.0f;
    F3 c = lerp(hx(0x100C2E), hx(0x2C1858), v);
    float n = fbm(u * 3.0f, v * 2.4f, 71, 4);
    c = lerp(c, lerp(hx(0xFF8CC8), hx(0x8CB4FF), u), sst(0.5f, 0.8f, n) * 0.35f);
    if (rnd2(x, y, 72) > 0.985f) c = hx(0xFFFFF0);
    P.at(x, y) = lerp(P.at(x, y), c, m);
    P.win[y * PW + x] = m;
  }
  // the porthole frame: a gold ring, a pearl inner rim and a halo of little lights round it
  for (int k = 0; k < 720; k++) {
    float a = k * 3.14159f / 360.0f;
    float ox = 184 + cosf(a) * 111, oy = 100 + sinf(a) * 75;
    seg(P, ox, oy, 184 + cosf(a + 0.009f) * 111, 100 + sinf(a + 0.009f) * 75, 6, hx(0xE2BE6E));
    P.blend((int)(184 + cosf(a) * 106), (int)(100 + sinf(a) * 70), hx(0xFFF6DE), 0.9f);
  }
  for (int k = 0; k < 16; k++) {
    float a = k * 3.14159f / 8.0f;
    float lx = 184 + cosf(a) * 119, ly = 100 + sinf(a) * 82;
    softEllipse(P, lx, ly, 3.2f, 3.2f, hx(0xFFF0C0), 1.0f);
    glow(P, lx, ly, 10, hx(0xFFE6A0), 0.25f);
  }
  // a rainbow light strip where the wall meets the floor
  { static const uint32_t rb[6] = {0xFF96AA, 0xFFC88C, 0xFAF096, 0xA0EBB4, 0x96C8FF, 0xC8A0FF};
    for (int k = 0; k < 6; k++) { float x0 = 50 + k * (268 / 6.0f); seg(P, x0, 190, x0 + 268 / 6.0f, 190, 3, hx(rb[k])); } }
  // the halo light on the ceiling
  for (int k = 0; k < 360; k++) { float a = k * 3.14159f / 180.0f; P.blend((int)(184 + cosf(a) * 46), (int)(8 + sinf(a) * 6), hx(0xFFE6A0), 1.0f); }
  floorReflect(P, g, 0.3f);
  grimyLitter(P, 52, 268);   // the litter box & bowls come along to every home (same spots)
  grimyBowl(P, 268, 268);
  gradeRoom(P, hour, hx(0xFFE6C8), 0.5f);
  { float nightK = 1 - dayLight(hour);   // the porthole lights and the halo glow brighter at night
    glow(P, 184, 10, 70, hx(0xFFE6B0), 0.18f + 0.25f * nightK);
    for (int k = 0; k < 16; k++) { float a = k * 3.14159f / 8.0f; glow(P, 184 + cosf(a) * 119, 100 + sinf(a) * 82, 12, hx(0xFFE6A0), 0.3f * nightK); } }
  vignette(P, 0.16f);
}

}  // namespace

const char* roomStyleName(int s) {
  static const char* n[NUM_ROOM_STYLES] = {"Groovy '70s", "Grimy Cartoon", "Modern Apartment", "Seaside Villa", "Cozy Treehouse", "Classy Log Cabin", "Halo Bridge"};
  return n[s % NUM_ROOM_STYLES];
}

void renderStyledRoomBaked(Canvas& hi, int style, float hour, uint8_t* glassMask) {
  gBake = true; gGlassOut = glassMask;
  renderStyledRoom(hi, style, hour);
  gBake = false; gGlassOut = nullptr;
}

void renderStyledRoom(Canvas& hi, int style, float hour) {
  Pic P;
  switch (style % NUM_ROOM_STYLES) {
    case 0: style70s(P, hour); break;
    case 1: styleToon(P, hour); break;
    case 2: styleModern(P, hour); break;
    case 3: styleVilla(P, hour); break;
    case 4: styleTree(P, hour); break;
    case 6: styleHalo(P, hour); break;
    default: styleCabin(P, hour); break;
  }
  toCanvas(P, hi);
}

}  // namespace cat
