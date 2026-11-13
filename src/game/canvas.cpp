#include "canvas.h"
#include "font5x7.h"
#include <string.h>
#include <stdlib.h>

namespace cat {

Color mix(Color a, Color b, int t) {
  if (t <= 0) return a;
  if (t >= 256) return b;
  int r = (cr(a) * (256 - t) + cr(b) * t) >> 8;
  int g = (cg(a) * (256 - t) + cg(b) * t) >> 8;
  int bl = (cb(a) * (256 - t) + cb(b) * t) >> 8;
  return rgb(r, g, bl);
}

Color shade(Color c, int pct) {
  int r = cr(c) * pct / 100, g = cg(c) * pct / 100, b = cb(c) * pct / 100;
  if (r > 255) r = 255;
  if (g > 255) g = 255;
  if (b > 255) b = 255;
  return rgb(r, g, b);
}


void Canvas::clear(Color c) {
  for (int i = 0; i < cw * ch; i++) px[i] = c;
}

void Canvas::rect(int x, int y, int w, int h, Color c) {
  int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
  int x1 = x + w > cw ? cw : x + w, y1 = y + h > ch ? ch : y + h;
  for (int yy = y0; yy < y1; yy++) {
    uint16_t* p = px + yy * cw + x0;
    for (int xx = x0; xx < x1; xx++) *p++ = c;
  }
}

void Canvas::frame(int x, int y, int w, int h, Color c) {
  hline(x, y, w, c);
  hline(x, y + h - 1, w, c);
  vline(x, y, h, c);
  vline(x + w - 1, y, h, c);
}

void Canvas::hline(int x, int y, int w, Color c) { rect(x, y, w, 1, c); }
void Canvas::vline(int x, int y, int h, Color c) { rect(x, y, 1, h, c); }

void Canvas::line(int x0, int y0, int x1, int y1, Color c) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (int guard = 0; guard < 1000; guard++) {
    set(x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void Canvas::disc(int cx, int cy, int r, Color c) { ellipse(cx, cy, r, r, c); }

void Canvas::ring(int cx, int cy, int r, Color c) {
  int x = r, y = 0, err = 1 - r;
  while (x >= y) {
    set(cx + x, cy + y, c); set(cx - x, cy + y, c);
    set(cx + x, cy - y, c); set(cx - x, cy - y, c);
    set(cx + y, cy + x, c); set(cx - y, cy + x, c);
    set(cx + y, cy - x, c); set(cx - y, cy - x, c);
    y++;
    if (err < 0) err += 2 * y + 1;
    else { x--; err += 2 * (y - x) + 1; }
  }
}

void Canvas::ellipse(int cx, int cy, int rx, int ry, Color c) {
  if (rx <= 0 || ry <= 0) return;
  for (int dy = -ry; dy <= ry; dy++) {
    // width of this scanline
    long num = (long)rx * rx * ((long)ry * ry - (long)dy * dy);
    int w = 0;
    if (num > 0) {
      long q = num / ((long)ry * ry);
      w = 0;
      while ((long)(w + 1) * (w + 1) <= q) w++;
    }
    hline(cx - w, cy + dy, 2 * w + 1, c);
  }
}

void Canvas::tri(int x0, int y0, int x1, int y1, int x2, int y2, Color c) {
  int minY = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
  int maxY = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
  for (int y = minY; y <= maxY; y++) {
    int xl = 100000, xr = -100000;
    int xs[3] = {x0, x1, x2}, ys[3] = {y0, y1, y2};
    for (int i = 0; i < 3; i++) {
      int j = (i + 1) % 3;
      int ya = ys[i], yb = ys[j];
      if (ya == yb) {
        if (y == ya) {
          if (xs[i] < xl) xl = xs[i];
          if (xs[j] < xl) xl = xs[j];
          if (xs[i] > xr) xr = xs[i];
          if (xs[j] > xr) xr = xs[j];
        }
        continue;
      }
      if ((y < ya && y < yb) || (y > ya && y > yb)) continue;
      int x = xs[i] + (xs[j] - xs[i]) * (y - ya) / (yb - ya);
      if (x < xl) xl = x;
      if (x > xr) xr = x;
    }
    if (xl <= xr) hline(xl, y, xr - xl + 1, c);
  }
}

void Canvas::panel(int x, int y, int w, int h, Color fill, Color border) {
  rect(x + 1, y + 1, w - 2, h - 2, fill);
  hline(x + 1, y, w - 2, border);
  hline(x + 1, y + h - 1, w - 2, border);
  vline(x, y + 1, h - 2, border);
  vline(x + w - 1, y + 1, h - 2, border);
}

void Canvas::bar(int x, int y, int w, int h, int pct, Color fill, Color back, Color border) {
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  rect(x, y, w, h, border);
  rect(x + 1, y + 1, w - 2, h - 2, back);
  int fw = (w - 2) * pct / 100;
  if (fw > 0) rect(x + 1, y + 1, fw, h - 2, fill);
}

int Canvas::textWidth(const char* s, int scale) {
  int n = (int)strlen(s);
  return n ? (n * 6 - 1) * scale : 0;
}

void Canvas::text(int x, int y, const char* s, Color c, int scale) {
  for (; *s; s++, x += 6 * scale) {
    unsigned char ch = (unsigned char)*s;
    const uint8_t* g = &kFont5x7[ch * 5];
    for (int col = 0; col < 5; col++) {
      uint8_t bits = g[col];
      for (int row = 0; row < 8; row++) {
        if (bits & (1 << row)) {
          if (scale == 1) set(x + col, y + row, c);
          else rect(x + col * scale, y + row * scale, scale, scale, c);
        }
      }
    }
  }
}

void Canvas::textShadow(int x, int y, const char* s, Color c, Color shadow, int scale) {
  text(x + 1, y + 1, s, shadow, scale);
  text(x, y, s, c, scale);
}

void Canvas::textCenter(int cx, int y, const char* s, Color c, int scale) {
  text(cx - textWidth(s, scale) / 2, y, s, c, scale);
}

void Canvas::textRight(int rx, int y, const char* s, Color c, int scale) {
  text(rx - textWidth(s, scale), y, s, c, scale);
}

void Canvas::tintRect(int x, int y, int w, int h, int rP, int gP, int bP) {
  int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
  int x1 = x + w > cw ? cw : x + w, y1 = y + h > ch ? ch : y + h;
  for (int yy = y0; yy < y1; yy++) {
    uint16_t* p = px + yy * cw + x0;
    for (int xx = x0; xx < x1; xx++, p++) {
      uint16_t c = *p;
      int r = ((c >> 11) & 31) * rP / 100;
      int g = ((c >> 5) & 63) * gP / 100;
      int b = (c & 31) * bP / 100;
      if (r > 31) r = 31;
      if (g > 63) g = 63;
      if (b > 31) b = 31;
      *p = (uint16_t)((r << 11) | (g << 5) | b);
    }
  }
}

void Canvas::dimRect(int x, int y, int w, int h, int pct) { tintRect(x, y, w, h, pct, pct, pct); }

}  // namespace cat
