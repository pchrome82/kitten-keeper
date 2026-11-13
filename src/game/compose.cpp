#include "compose.h"
#include "catart.h"
#include "game.h"
#include <string.h>

namespace cat {
// Per-channel lookup tables for the night tint and the shadow (72%), so no divisions per pixel.
// Rebuilt only when the tint changes (a few times an hour).
namespace {
struct Lut { int r = -1, g = -1, b = -1; uint8_t R[32], G[64], B[32];
  void set(int tr, int tg, int tb) {
    if (tr == r && tg == g && tb == b) return;
    r = tr; g = tg; b = tb;
    for (int i = 0; i < 32; i++) { R[i] = (uint8_t)(i * tr / 100); B[i] = (uint8_t)(i * tb / 100); }
    for (int i = 0; i < 64; i++) G[i] = (uint8_t)(i * tg / 100);
  }
  inline uint16_t operator()(uint16_t c) const { return (uint16_t)((R[(c >> 11) & 31] << 11) | (G[(c >> 5) & 63] << 5) | B[c & 31]); }
};
Lut gTint, gShadow, gDim;
inline void put2(uint16_t* o, uint16_t a, uint16_t b) { const uint32_t v = (uint32_t)a | ((uint32_t)b << 16); memcpy(o, &v, 4); }   // two pixels, one store
}  // namespace

int gRoomDim = 100;

void composeRows(const uint16_t* lg, const uint16_t* room, bool keyed, int tr, int tg, int tb, uint16_t* out, int y0, int y1) {
  if (!room) keyed = false;
  const bool tint = keyed && (tr != 100 || tg != 100 || tb != 100);
  if (tint) gTint.set(tr, tg, tb);
  const bool dim = keyed && gRoomDim < 100;
  if (keyed) { const int sh = 70 * (dim ? gRoomDim : 100) / 100; gShadow.set(sh, sh, sh); }
  if (dim) gDim.set(gRoomDim, gRoomDim, gRoomDim);
  for (int y = y0; y < y1; y++) {
    const uint16_t* src = lg + y * W;
    uint16_t* o0 = out + ((y - y0) * 2) * HW;
    uint16_t* o1 = o0 + HW;
    const bool band = keyed && y >= 26 && y < 184;
    if (!band) {   // no room behind this row: double each pixel, then copy the row
      for (int x = 0; x < W; x++) { const uint16_t c = src[x]; put2(o0 + x * 2, c, c); }
      memcpy(o1, o0, HW * sizeof(uint16_t));
      continue;
    }
    const uint16_t* r0 = room + (y * 2) * HW;
    const uint16_t* r1 = r0 + HW;
    for (int x = 0; x < W; x++) {
      uint16_t c = src[x];
      if (c == KEY_COLOR) {            // the hi-res room shows through
        if (dim) { put2(o0 + x * 2, gDim(r0[x * 2]), gDim(r0[x * 2 + 1])); put2(o1 + x * 2, gDim(r1[x * 2]), gDim(r1[x * 2 + 1])); }
        else { memcpy(o0 + x * 2, r0 + x * 2, 4); memcpy(o1 + x * 2, r1 + x * 2, 4); }
      } else if (c == SHADOW_KEY) {    // a soft shadow on the room
        put2(o0 + x * 2, gShadow(r0[x * 2]), gShadow(r0[x * 2 + 1])); put2(o1 + x * 2, gShadow(r1[x * 2]), gShadow(r1[x * 2 + 1]));
      } else {
        if (tint) c = gTint(c);
        put2(o0 + x * 2, c, c); put2(o1 + x * 2, c, c);
      }
    }
  }
}

void composeFrame(const uint16_t* lg, const uint16_t* room, bool keyed, int tr, int tg, int tb, uint16_t* out) {
  composeRows(lg, room, keyed, tr, tg, tb, out, 0, H);
}
}  // namespace cat
