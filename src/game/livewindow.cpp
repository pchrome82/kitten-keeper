#include "livewindow.h"
#include "roomdata.h"
#include "canvas.h"
#include <math.h>
#include <string.h>

namespace cat {
namespace {

// Glass bounding box of the current room (band coords) + per-pixel scene layer. Rebuilt when the room changes.
int GX0 = 0, GY0 = 0, GW = 1, GH = 1;
float gU0 = 0, gUS = 1, gV0 = 0, gVS = 1;   // u = (x - gU0) / gUS, v = (y - gV0) / gVS across the window
constexpr int CW = 128, CH = 40;              // cloud texture
uint8_t* gLayer = nullptr;   // bit7 = glass; low bits = scene layer id
uint8_t gCloud[CW * CH];
int gRoom = -1;
const RoomImg* gR = nullptr;

enum { L_SKY = 0, L_BLDG_A = 1, L_BLDG_B = 2, L_WIN = 3, L_WALL = 4, L_WIRE = 5, L_SEA = 6, L_TREE_A = 7, L_TREE_B = 8, L_RIVER = 9 };

inline uint32_t hsh(int a, int b, uint32_t s) { uint32_t h = (uint32_t)a * 374761393u + (uint32_t)b * 668265263u + s * 2246822519u; h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16); }
inline float rnd(int a, int b, uint32_t s) { return (hsh(a, b, s) & 0xFFFF) / 65536.0f; }
inline float cl(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline float sst(float e0, float e1, float x) { float t = cl((x - e0) / (e1 - e0)); return t * t * (3 - 2 * t); }
struct V3 { float r, g, b; };
inline V3 mixv(V3 a, V3 b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
inline V3 hx(uint32_t h) { return {(float)((h >> 16) & 255), (float)((h >> 8) & 255), (float)(h & 255)}; }
inline Color toC(V3 c) { return rgb((int)(c.r < 0 ? 0 : (c.r > 255 ? 255 : c.r)), (int)(c.g < 0 ? 0 : (c.g > 255 ? 255 : c.g)), (int)(c.b < 0 ? 0 : (c.b > 255 ? 255 : c.b))); }

float vnoise(float x, float y, uint32_t s) {
  int xi = (int)floorf(x), yi = (int)floorf(y); float fx = x - xi, fy = y - yi;
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
  float a = rnd(xi, yi, s), b = rnd(xi + 1, yi, s), c = rnd(xi, yi + 1, s), d = rnd(xi + 1, yi + 1, s);
  return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

void init(int room) {
  gRoom = room;
  gR = &ROOMS[room];
  int x0 = 9999, y0 = 9999, x1 = -1, y1 = -1;
  for (int i = 0; i < gR->glassN; i++) {
    const GlassSpan& s = gR->glass[i];
    if (s.x0 < x0) x0 = s.x0;
    if (s.x1 > x1) x1 = s.x1;
    if (s.y < y0) y0 = s.y;
    if (s.y > y1) y1 = s.y;
  }
  if (x1 < x0) { x0 = y0 = 0; x1 = y1 = 1; }
  GX0 = x0; GY0 = y0; GW = x1 - x0 + 1; GH = y1 - y0 + 1;
  gU0 = (float)x0; gUS = (float)(x1 - x0 > 8 ? x1 - x0 : 8);
  gV0 = (float)y0; gVS = (float)(y1 - y0 > 8 ? y1 - y0 : 8);
  delete[] gLayer;
  gLayer = new uint8_t[GW * GH];
  memset(gLayer, 0, (size_t)GW * GH);
  const int view = gR->view;
  for (int i = 0; i < gR->glassN; i++) {
    const GlassSpan& s = gR->glass[i];
    for (int x = s.x0; x <= s.x1; x++) {
      int lx = x - GX0, ly = s.y - GY0;
      if (lx < 0 || ly < 0 || lx >= GW || ly >= GH) continue;
      float u = (x - gU0) / gUS, v = (s.y - gV0) / gVS;
      uint8_t id = L_SKY;
      if (view == 1) {            // sea: a flat horizon
        if (v > 0.62f) id = L_SEA;
      } else if (view == 2) {     // forest: bumpy treeline of two depths
        float far = 0.48f + 0.10f * vnoise(u * 9, 0, 61) + 0.05f * vnoise(u * 23, 1, 62);
        float nearL = 0.66f + 0.12f * vnoise(u * 6, 2, 63) + 0.06f * vnoise(u * 17, 3, 64);
        if (v > nearL) id = L_TREE_B; else if (v > far) id = L_TREE_A;
      } else if (view == 3) {     // a river winding out of the forest toward the cabin
        float far = 0.42f + 0.08f * vnoise(u * 8, 4, 65) + 0.04f * vnoise(u * 21, 5, 66);
        float rc = 0.5f + 0.16f * sinf(v * 7.0f + 1.0f) * (v - 0.5f) * 2.0f;
        float rw = 0.025f + 0.22f * cl((v - 0.52f) / 0.48f);
        float edge = 0.07f + 0.12f * v;
        if (v > far) id = L_TREE_A;
        if (v > 0.52f && fabsf(u - rc) < rw) id = L_RIVER;
        if ((u < edge || u > 1.0f - edge) && v > 0.12f + 0.25f * vnoise(u * 10, v * 6, 67)) id = L_TREE_B;   // near pines framing the view
      } else {                    // city skyline
        float bh = 0.55f + 0.18f * rnd((int)(u * 6), 0, 44) + (u - 0.5f) * 0.12f;
        float lean = (v - 0.6f) * 0.06f;
        int bi = (int)((u + lean) * 6);
        if (v > bh) {
          id = rnd(bi, 2, 45) > 0.5f ? L_BLDG_A : L_BLDG_B;
          float wu = fmodf((u + lean) * 36, 1.0f), wv = fmodf(v * 22, 1.0f);
          if (wu > 0.35f && wu < 0.75f && wv > 0.3f && wv < 0.75f) id = rnd((int)((u + lean) * 36), (int)(v * 22), 46) > 0.55f ? L_WIN : L_WALL;
        }
        for (int w = 0; w < 2; w++) { float wy = 0.36f + w * 0.05f + 0.06f * (u - 0.5f) * (u - 0.5f) * 4; if (fabsf(v - wy) < 0.006f) id = L_WIRE; }
      }
      gLayer[ly * GW + lx] = 0x80 | id;
    }
  }
  static bool cloudsMade = false;
  if (cloudsMade) return;
  cloudsMade = true;
  for (int y = 0; y < CH; y++) for (int x = 0; x < CW; x++) {
    // tileable in x: blend two shifted samples
    float fx = x / (float)CW, n1 = vnoise(x * 0.11f, y * 0.19f, 7), n2 = vnoise((x - CW) * 0.11f, y * 0.19f, 7);
    float n = n1 * (1 - fx) + n2 * fx;
    n += 0.5f * vnoise(x * 0.27f, y * 0.4f, 8);
    gCloud[y * CW + x] = (uint8_t)(cl(n / 1.5f) * 255);
  }
}

}  // namespace

int weatherForDay(uint32_t d) { uint32_t h = hsh((int)d, 7, 99) % 100; return h < 70 ? 0 : (h < 86 ? 1 : 2); }

// Deep space (the Halo Bridge): a drifting nebula, twinkling stars, a slowly turning ringed planet. No weather up here.
static void drawSpaceWindow(uint16_t* room, float t) {
  const float drift = t * 0.004f;
  const float pu = 0.70f + 0.03f * sinf(t * 0.02f), pv = 0.58f, pr = 0.15f;   // the planet
  for (int i = 0; i < gR->glassN; i++) {
    const GlassSpan& s = gR->glass[i];
    uint16_t* row = room + (s.y + ROOM_TOP) * ROOM_W;
    const float v = (s.y - gV0) / gVS;
    for (int x = s.x0; x <= s.x1; x++) {
      const float u = (x - gU0) / gUS;
      V3 c = mixv(hx(0x100C2E), hx(0x2C1858), v);
      // the cloud texture's puffs are ~9 texels wide and ~5 tall: sample it so they come out round, not as streaks
      int cx = ((int)((u * 0.8f + drift) * CW)) % CW; if (cx < 0) cx += CW;
      int cy = (int)(v * CH); if (cy >= CH) cy = CH - 1;
      const float n = gCloud[cy * CW + cx] / 255.0f;
      c = mixv(c, mixv(hx(0xFF8CC8), hx(0x8CB4FF), u), sst(0.45f, 0.85f, n) * 0.38f);
      if (rnd(x, s.y, 5) > 0.988f) c = mixv(c, hx(0xFFFFF0), 0.55f + 0.45f * sinf(t * (1.5f + rnd(x, s.y, 6) * 2) + x));
      const float dx = (u - pu) * gUS / gVS, dy = v - pv, d = sqrtf(dx * dx + dy * dy);
      if (d < pr) {
        const float lit = cl(0.75f - (dx + dy) * 2.2f);   // lit from the upper left
        c = mixv(hx(0xB86A70), hx(0xFFD4B4), lit);
        if (fabsf(dy + 0.03f * sinf(dx * 20 + t * 0.1f)) < 0.012f) c = mixv(c, hx(0xFFF0E0), 0.4f);   // a cloud band
      }
      const float rr = sqrtf(dx * dx * 0.23f + dy * dy * 5.5f);   // the ring (in front of the planet's lower half)
      if (fabsf(rr - pr * 0.95f) < 0.012f && (d >= pr || dy > 0)) c = mixv(c, hx(0xE0D0FF), 0.8f);
      row[x] = toC(c);
    }
  }
}

void drawLiveWindow(uint16_t* room, int roomId, float hour, float t, int weather) {
  if (roomId < 0 || roomId >= NUM_ROOMS) roomId = 0;
  if (roomId != gRoom) init(roomId);
  if (gR->view == 4) { drawSpaceWindow(room, t); return; }
  float day = sst(5.5f, 7.5f, hour) * (1 - sst(18.0f, 20.0f, hour));
  float dusk = cl(fmaxf(1 - fabsf(hour - 19.0f) / 1.6f, 1 - fabsf(hour - 6.5f) / 1.2f));
  float night = 1 - fmaxf(day, dusk * 0.8f);
  V3 top = mixv(hx(0x1A1440), hx(0x5ED0C8), day), hor = mixv(hx(0x3A2050), hx(0xF4A0B8), day);
  hor = mixv(hor, hx(0xFF8030), dusk); top = mixv(top, hx(0x8040A0), dusk * 0.6f);
  float grey = weather == 0 ? 0.0f : (weather == 1 || weather == 3 ? 0.55f : 0.8f);
  V3 gcol = mixv(hx(0x3A3A4A), hx(0x9AA0AA), day);
  top = mixv(top, gcol, grey); hor = mixv(hor, mixv(gcol, hx(0xC8CCD2), 0.3f), grey);
  // sun / moon
  float tt = cl((hour - 6.0f) / 12.0f);
  float su = 0.12f + tt * 0.76f, sv = 0.78f - sinf(tt * 3.14159f) * 0.62f;
  bool sunUp = hour > 6.2f && hour < 19.5f;
  float mu = 0.74f, mv = 0.22f;
  float cloudShift = t * 0.012f;
  V3 cloudCol = mixv(hx(0x4A3A66), hx(0xF8EEDA), day);
  cloudCol = mixv(cloudCol, hx(0xFFC4A0), dusk * 0.6f);
  cloudCol = mixv(cloudCol, mixv(gcol, hx(0xB4B8C0), 0.4f), grey);
  V3 bA = mixv(hx(0x2A2028), hx(0x4A3C48), day * 0.6f), bB = mixv(hx(0x1E161E), hx(0x3C3038), day * 0.6f);
  bA = mixv(bA, hx(0x3A2A40), dusk * 0.5f);
  float cover = weather == 0 ? 0.66f : (weather == 1 || weather == 3 ? 0.40f : 0.30f);   // lower = more cloud

  V3 seaA = mixv(hx(0x14203A), hx(0x2E7FB8), day), seaB = mixv(hx(0x1C2C4C), hx(0x7CC8E8), day);
  seaA = mixv(seaA, hx(0x6A4A70), dusk * 0.4f);
  V3 trA = mixv(hx(0x10180F), hx(0x4C7A3C), day), trB = mixv(hx(0x0A100A), hx(0x2E5426), day);
  trA = mixv(trA, gcol, grey * 0.3f);
  for (int i = 0; i < gR->glassN; i++) {
    const GlassSpan& s = gR->glass[i];
    uint16_t* row = room + (s.y + ROOM_TOP) * ROOM_W;
    float v = (s.y - gV0) / gVS;
    for (int x = s.x0; x <= s.x1; x++) {
      int lx = x - GX0, ly = s.y - GY0;
      uint8_t id = (lx >= 0 && ly >= 0 && lx < GW && ly < GH) ? (gLayer[ly * GW + lx] & 0x7F) : 0;
      float u = (x - gU0) / gUS;
      V3 c;
      if (id == L_SKY) {
        c = mixv(top, hor, v);
        if (night > 0.2f && grey < 0.5f && rnd(x, s.y, 5) > 0.992f) c = mixv(c, hx(0xFFFFF0), night * (0.6f + 0.4f * sinf(t * 2 + x)));
        if (sunUp && weather < 2) {
          float dx = (u - su) * 1.3f, dy = v - sv, d = sqrtf(dx * dx + dy * dy);
          c.r += 255 * 0.35f * expf(-d * 9) * (day + dusk) * (1 - grey); c.g += 230 * 0.3f * expf(-d * 9) * (day + dusk) * (1 - grey); c.b += 150 * 0.2f * expf(-d * 9) * (day + dusk) * (1 - grey);
          if (d < 0.06f && grey < 0.7f) c = mixv(c, mixv(hx(0xFFE070), hx(0xFFFBEA), day), 1.0f);
        } else if (!sunUp) {
          float dx = (u - mu) * 1.3f, dy = v - mv, d = sqrtf(dx * dx + dy * dy);
          if (d < 0.055f && grey < 0.8f) c = hx(0xF0F0E0);
          else c.b += 70 * expf(-d * 10) * (1 - grey);
        }
        // clouds
        // (the texture's puffs are ~9 texels wide and ~5 tall: across the window at this rate they come out round and
        //  fluffy; three times faster, as before, squeezed them into thin vertical streaks that read as rain)
        float cu = u * 0.8f + cloudShift;
        int cx = ((int)(cu * CW)) % CW; if (cx < 0) cx += CW;
        int cy = (int)(v * 1.3f * CH); if (cy >= CH) cy = CH - 1;
        float cn = gCloud[cy * CW + cx] / 255.0f;
        if (v < 0.7f) {
          float m = sst(cover, cover + 0.07f, cn) * (1 - sst(0.5f, 0.7f, v));
          if (m > 0.01f) { V3 cc = cn > cover + 0.18f ? mixv(cloudCol, hx(0x222030), 0.18f) : cloudCol; c = mixv(c, cc, m); }
        }
      } else if (id == L_BLDG_A) c = bA;
      else if (id == L_BLDG_B) c = bB;
      else if (id == L_WALL) c = mixv(bB, bA, 0.5f);
      else if (id == L_WIN) {
        bool lit = night > 0.35f || (weather == 2 && day < 0.6f);
        float flick = rnd(x / 4, s.y / 4, 77 + (uint32_t)(t * 0.05f));
        c = lit && flick > 0.12f ? hx(0xF8D860) : mixv(bB, hx(0x504858), 0.5f);
      } else if (id == L_SEA) {
        float dv = (v - 0.62f) / 0.38f;   // 0 at the horizon -> 1 at the sill
        c = mixv(seaB, seaA, dv);
        float sp = sinf(x * 0.9f + t * 2.0f + s.y * 0.7f) * sinf(x * 0.23f - t * 1.3f);   // glints
        if (sp > 0.92f - 0.1f * dv && grey < 0.7f) c = mixv(c, hx(0xFFF6E0), 0.55f * (day + dusk * 0.6f + 0.25f));
      } else if (id == L_RIVER) {
        c = mixv(mixv(top, hor, 0.5f), seaA, 0.55f);   // reflects the sky
        if (sinf(x * 0.7f + t * 3.0f + s.y * 1.3f) > 0.93f && grey < 0.7f) c = mixv(c, hx(0xFFF6E0), 0.5f * (day + 0.3f));
      } else if (id == L_TREE_A) c = trA;
      else if (id == L_TREE_B) { c = trB; if (rnd(x / 3, s.y / 3, 91) > 0.82f) c = mixv(trB, trA, 0.6f); }
      else c = hx(0x1A1018);   // wire
      if (weather == 2) { c.r *= 0.88f; c.g *= 0.9f; c.b *= 0.95f; }
      row[x] = toC(c);
    }
  }
  // a bird crosses every ~28 s (not in the rain or at night)
  if (weather < 2 && day > 0.4f) {
    float ph = fmodf(t, 28.0f);
    if (ph < 7.0f) {
      float u = -0.08f + ph / 7.0f * 1.2f, v = 0.2f + 0.04f * sinf(ph * 1.7f);
      int bx = (int)(gU0 + u * gUS), by = (int)(gV0 + v * gVS);
      int fl = ((int)(ph * 6)) & 1;
      for (int k = -3; k <= 3; k++) {
        int yy = by + (fl ? -abs(k) / 2 : abs(k) / 2 - 1);
        int xx = bx + k;
        int lx = xx - GX0, ly = yy - GY0;
        if (lx < 0 || ly < 0 || lx >= GW || ly >= GH || !(gLayer[ly * GW + lx] & 0x80)) continue;
        room[(yy + ROOM_TOP) * ROOM_W + xx] = rgb(24, 18, 28);
      }
    }
  }
  // snow: soft flakes drifting down, swaying
  if (weather == 3) {
    for (int n = 0; n < 60; n++) {
      const float sp = 14 + rnd(n, 3, 5) * 16;
      float fx = rnd(n, 1, 5) * GW + sinf(t * (0.8f + rnd(n, 4, 5)) + n) * 3.0f;
      float fy = fmodf(rnd(n, 2, 5) * GH + t * sp, (float)GH);
      const int big = rnd(n, 6, 5) < 0.3f;
      for (int k = 0; k <= big; k++) for (int j = 0; j <= big; j++) {
        int lx = (int)fx + k, ly = (int)fy + j;
        if (lx < 0 || ly < 0 || lx >= GW || ly >= GH || !(gLayer[ly * GW + lx] & 0x80)) continue;
        room[(ly + GY0 + ROOM_TOP) * ROOM_W + lx + GX0] = big ? rgb(250, 252, 255) : rgb(226, 234, 248);
      }
    }
  }
  // rain streaks
  if (weather == 2) {
    for (int n = 0; n < 70; n++) {
      float fx = rnd(n, 1, 3) * GW, fy = fmodf(rnd(n, 2, 3) * GH + t * (150 + rnd(n, 3, 3) * 80), (float)GH);
      for (int k = 0; k < 4; k++) {
        int lx = (int)(fx - k * 0.5f), ly = (int)(fy - k * 2);
        if (lx < 0 || ly < 0 || lx >= GW || ly >= GH || !(gLayer[ly * GW + lx] & 0x80)) continue;
        room[(ly + GY0 + ROOM_TOP) * ROOM_W + lx + GX0] = rgb(170 - k * 12, 196 - k * 10, 226 - k * 6);
      }
    }
  }
}

}  // namespace cat
