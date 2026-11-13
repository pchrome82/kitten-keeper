// Tiny software canvas: 184x224 RGB565, drawn at 2x on the 368x448 AMOLED.
// Everything in the game draws through this class so the same code runs on the
// ESP32-S3 and on the PC simulator.
#pragma once
#include <stdint.h>

namespace cat {

constexpr int W = 184;   // logical width  (physical 368 / 2)
constexpr int H = 224;   // logical height (physical 448 / 2)

typedef uint16_t Color;

constexpr Color rgb(int r, int g, int b) {
  return (Color)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xFF) >> 3));
}
inline int cr(Color c) { return ((c >> 11) & 31) * 255 / 31; }
inline int cg(Color c) { return ((c >> 5) & 63) * 255 / 63; }
inline int cb(Color c) { return (c & 31) * 255 / 31; }
Color mix(Color a, Color b, int t256);        // t=0 -> a, t=256 -> b
Color shade(Color c, int pct);                // pct 100 = same, 50 = half brightness, 130 = brighter


class Canvas {
 public:
  explicit Canvas(uint16_t* buffer, int w = W, int h = H) : px(buffer), cw(w), ch(h) {}
  uint16_t* px;
  int cw, ch;                       // buffer size (logical canvas 184x224, or the hi-res 368x448 room cache)

  void clear(Color c);
  inline void set(int x, int y, Color c) {
    if ((unsigned)x < (unsigned)cw && (unsigned)y < (unsigned)ch) px[y * cw + x] = c;
  }
  inline Color get(int x, int y) const {
    return ((unsigned)x < (unsigned)cw && (unsigned)y < (unsigned)ch) ? px[y * cw + x] : 0;
  }

  void rect(int x, int y, int w, int h, Color c);          // filled
  void frame(int x, int y, int w, int h, Color c);         // 1px outline
  void hline(int x, int y, int w, Color c);
  void vline(int x, int y, int h, Color c);
  void line(int x0, int y0, int x1, int y1, Color c);
  void disc(int cx, int cy, int r, Color c);
  void ring(int cx, int cy, int r, Color c);
  void ellipse(int cx, int cy, int rx, int ry, Color c);   // filled
  void tri(int x0, int y0, int x1, int y1, int x2, int y2, Color c);
  void panel(int x, int y, int w, int h, Color fill, Color border);  // rounded-corner panel
  void bar(int x, int y, int w, int h, int pct, Color fill, Color back, Color border);

  // Text: 5x7 font, 6px advance * scale.
  void text(int x, int y, const char* s, Color c, int scale = 1);
  void textShadow(int x, int y, const char* s, Color c, Color shadow, int scale = 1);
  void textCenter(int cx, int y, const char* s, Color c, int scale = 1);
  void textRight(int rx, int y, const char* s, Color c, int scale = 1);
  static int textWidth(const char* s, int scale = 1);


  // Post effects on a rectangle.
  void tintRect(int x, int y, int w, int h, int rPct, int gPct, int bPct);
  void dimRect(int x, int y, int w, int h, int pct);  // pct 100 = unchanged
};

constexpr int HW = W * 2, HH = H * 2;   // 368 x 448 hi-res frame (2x the logical canvas)

}  // namespace cat
