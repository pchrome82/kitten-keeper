#include "gfx.h"
#include <math.h>
namespace cat {

static inline Color dark(Color c, int p = 45) { return shade(c, p); }

void vgrad(Canvas& cv, int x, int y, int w, int h, Color top, Color bot) {
  for (int j = 0; j < h; j++) cv.hline(x, y + j, w, mix(top, bot, h > 1 ? j * 256 / (h - 1) : 0));
}

void gellipse(Canvas& cv, float cx, float cy, float rx, float ry, Color base) {
  Color ol = mix(shade(base, 40), rgb(20, 10, 40), 90);
  for (int y = (int)floorf(cy - ry - 1); y <= (int)ceilf(cy + ry + 1); y++)
    for (int x = (int)floorf(cx - rx - 1); x <= (int)ceilf(cx + rx + 1); x++) {
      float nx = (x + 0.5f - cx) / rx, ny = (y + 0.5f - cy) / ry;
      float d2 = nx * nx + ny * ny;
      float o = 1.0f + 1.0f / (rx < ry ? rx : ry);
      if (d2 > o * o) continue;
      if (d2 > 1.0f) { cv.set(x, y, ol); continue; }
      float light = -(nx * 0.45f + ny * 0.7f);
      int pct = 100 + (int)(light * 42);
      if (d2 > 0.62f && light < 0) pct -= 8;       // darker rim on the shadow side
      cv.set(x, y, shade(base, pct));
    }
  int sx = (int)(cx - rx * 0.42f), sy = (int)(cy - ry * 0.5f);
  if (rx >= 3.5f) { cv.set(sx, sy, rgb(255, 255, 255)); if (rx >= 5) cv.set(sx + 1, sy, rgb(255, 255, 255)); }
}
void gdisc(Canvas& cv, int cx, int cy, int r, Color base) { gellipse(cv, cx + 0.0f, cy + 0.0f, (float)r, (float)r, base); }

static int cornerSkip(int row, int rad) { return row == 0 ? rad : (row == 1 && rad >= 3 ? 1 : (row == 2 && rad >= 4 ? 1 : 0)); }

static void roundedGrad(Canvas& cv, int x, int y, int w, int h, Color top, Color bot, int rad) {
  for (int j = 0; j < h; j++) {
    int sk = j < rad ? cornerSkip(j, rad) : (h - 1 - j < rad ? cornerSkip(h - 1 - j, rad) : 0);
    cv.hline(x + sk, y + j, w - 2 * sk, mix(top, bot, h > 1 ? j * 256 / (h - 1) : 0));
  }
}

void glossBtn(Canvas& cv, int x, int y, int w, int h, Color base, bool pressed) {
  int lip = pressed ? 1 : 3;
  int fy = pressed ? y + 2 : y, fh = h - 3;
  Color edge = mix(shade(base, 38), rgb(18, 10, 34), 100);
  roundedGrad(cv, x, fy, w, fh + lip + 1, edge, edge, 3);                               // silhouette
  roundedGrad(cv, x + 1, fy + fh - 2, w - 2, lip + 2, shade(base, 62), shade(base, 54), 2);  // lip
  roundedGrad(cv, x + 1, fy + 1, w - 2, fh - 1, shade(base, 132), shade(base, 88), 3);       // face
  // gloss band on the upper half
  for (int j = 2; j < fh / 2 + 1; j++) cv.hline(x + 4, fy + j, w - 8, mix(shade(base, 142), shade(base, 122), j * 256 / (fh / 2 + 1)));
  cv.hline(x + 4, fy + 1, w - 8, shade(base, 175));                                      // top highlight
  cv.set(x + 2, fy + 3, shade(base, 160));
}

void gbar(Canvas& cv, int x, int y, int w, int h, int pct, Color fill) {
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  roundedGrad(cv, x, y, w, h, rgb(14, 10, 30), rgb(14, 10, 30), 2);
  roundedGrad(cv, x + 1, y + 1, w - 2, h - 2, rgb(52, 44, 86), rgb(36, 30, 64), 1);
  int fw = (w - 2) * pct / 100;
  if (fw > 0) {
    roundedGrad(cv, x + 1, y + 1, fw, h - 2, shade(fill, 140), shade(fill, 85), 1);
    cv.hline(x + 2, y + 2, fw > 2 ? fw - 2 : 0, shade(fill, 185));
  }
}

// ------------------------------------------------------------------ icons
void drawIcon(Canvas& cv, int id, int cx, int cy) {
  switch (id) {
    case IC_FEED: {  // kibble bowl
      Color red = rgb(236, 64, 84);
      gellipse(cv, cx, cy + 6, 9.5f, 2.6f, rgb(200, 150, 90));
      for (int j = 0; j < 8; j++) {   // bowl body (trapezoid, gradient)
        int hw = 9 - j / 3 * 0 - (j > 5 ? 2 : 0);
        cv.hline(cx - hw, cy + j - 1, hw * 2, mix(shade(red, 130), shade(red, 80), j * 256 / 7));
      }
      cv.hline(cx - 9, cy - 1, 18, rgb(255, 150, 160));
      gellipse(cv, cx, cy - 1, 9, 2.8f, rgb(120, 70, 40));
      for (int i = 0; i < 6; i++) gdisc(cv, cx - 7 + i * 3, cy - 3 - (i % 2), 2, i % 2 ? rgb(214, 128, 60) : rgb(176, 98, 48));
      break; }
    case IC_PLAY: {  // yarn ball
      gdisc(cv, cx, cy, 8, rgb(255, 80, 170));
      for (int k = -1; k <= 1; k++) cv.line(cx - 6, cy + k * 3 + 2, cx + 5, cy + k * 3 - 3, rgb(255, 170, 220));
      break; }
    case IC_BOX: {   // litter tray + scoop
      Color blue = rgb(52, 150, 255);
      for (int j = 0; j < 9; j++) cv.hline(cx - 9 + (j > 6 ? 1 : 0), cy - 3 + j, 18 - (j > 6 ? 2 : 0), mix(shade(blue, 135), shade(blue, 80), j * 256 / 8));
      cv.hline(cx - 9, cy - 3, 18, rgb(180, 220, 255));
      cv.rect(cx - 7, cy - 2, 14, 3, rgb(244, 214, 150));
      for (int i = 0; i < 4; i++) cv.rect(cx - 5 + i * 3, cy - 2 + (i % 2), 2, 1, rgb(176, 130, 80));
      cv.rect(cx - 9, cy + 6, 18, 1, shade(blue, 45));
      cv.rect(cx + 5, cy - 9, 2, 8, rgb(255, 210, 60)); cv.rect(cx + 3, cy - 10, 6, 2, rgb(255, 210, 60));
      break; }
    case IC_SHOP: {  // shopping bag
      Color pur = rgb(170, 90, 255);
      for (int j = 0; j < 14; j++) cv.hline(cx - 8, cy - 4 + j, 16, mix(shade(pur, 135), shade(pur, 75), j * 256 / 13));
      cv.frame(cx - 8, cy - 4, 16, 14, shade(pur, 40));
      cv.ellipse(cx, cy - 5, 4, 3, rgb(255, 214, 70)); cv.ellipse(cx, cy - 5, 2, 1, shade(pur, 90));
      gdisc(cv, cx, cy + 3, 3, rgb(255, 214, 70));
      break; }
    case IC_MENU: case IC_PAW: {
      Color t = id == IC_MENU ? rgb(40, 210, 190) : rgb(255, 150, 120);
      gellipse(cv, cx, cy + 3, 6, 5, t);
      gdisc(cv, cx - 7, cy - 2, 2, t); gdisc(cv, cx - 3, cy - 6, 2, t); gdisc(cv, cx + 3, cy - 6, 2, t); gdisc(cv, cx + 7, cy - 2, 2, t);
      break; }
    case IC_COIN: {
      gdisc(cv, cx, cy, 8, rgb(255, 196, 30));
      cv.ring(cx, cy, 5, rgb(214, 140, 10));
      cv.rect(cx - 1, cy - 3, 2, 6, rgb(214, 140, 10));
      cv.set(cx - 4, cy - 4, rgb(255, 250, 200));
      break; }
    case IC_HEART: {
      Color r = rgb(255, 52, 90);
      gdisc(cv, cx - 4, cy - 3, 5, r); gdisc(cv, cx + 4, cy - 3, 5, r);
      for (int j = 0; j < 9; j++) { int hw = 9 - j; cv.hline(cx - hw, cy + j - 1, hw * 2, shade(r, 100 - j * 4)); }
      cv.set(cx - 5, cy - 5, rgb(255, 255, 255)); cv.set(cx - 4, cy - 5, rgb(255, 255, 255));
      break; }
    case IC_FISH: {
      Color teal = rgb(40, 200, 230);
      cv.tri(cx + 6, cy, cx + 12, cy - 5, cx + 12, cy + 5, rgb(30, 150, 200));
      gellipse(cv, cx - 1, cy, 8, 5, teal);
      cv.tri(cx - 2, cy - 4, cx + 3, cy - 8, cx + 4, cy - 3, rgb(30, 150, 200));
      gdisc(cv, cx - 6, cy - 1, 1, rgb(255, 255, 255)); cv.set(cx - 6, cy - 1, rgb(20, 10, 40));
      break; }
    case IC_MOUSE: {
      Color gr = rgb(176, 180, 200);
      cv.line(cx + 6, cy + 3, cx + 11, cy - 3, rgb(255, 150, 170));
      gdisc(cv, cx - 5, cy - 5, 3, rgb(255, 160, 180)); gdisc(cv, cx + 1, cy - 6, 3, rgb(255, 160, 180));
      gellipse(cv, cx, cy + 1, 8, 6, gr);
      cv.set(cx - 5, cy, rgb(20, 10, 40)); cv.set(cx - 5, cy - 1, rgb(20, 10, 40)); cv.set(cx - 8, cy + 2, rgb(255, 120, 150));
      break; }
    case IC_CHEESE: {
      Color y = rgb(255, 205, 40);
      cv.tri(cx - 9, cy + 6, cx + 9, cy + 6, cx + 9, cy - 5, shade(y, 40));
      cv.tri(cx - 8, cy + 5, cx + 8, cy + 5, cx + 8, cy - 3, y);
      cv.tri(cx - 8, cy + 5, cx + 8, cy - 3, cx + 8, cy - 3, shade(y, 120));
      cv.rect(cx - 8, cy + 3, 16, 3, shade(y, 85));
      gdisc(cv, cx + 3, cy + 1, 1, rgb(220, 150, 20)); gdisc(cv, cx - 3, cy + 4, 1, rgb(220, 150, 20)); cv.set(cx + 5, cy - 1, rgb(220, 150, 20));
      break; }
    case IC_HAIRBALL: {
      gdisc(cv, cx, cy, 7, rgb(150, 124, 96));
      for (int i = 0; i < 7; i++) { cv.set(cx - 6 + i * 2, cy + (i % 3) - 2, rgb(100, 80, 58)); cv.set(cx - 5 + i * 2, cy + 3 - (i % 2), rgb(200, 176, 140)); }
      cv.line(cx + 5, cy - 6, cx + 9, cy - 9, rgb(150, 124, 96));
      break; }
    case IC_STAR: {
      Color y = rgb(255, 224, 60);
      cv.tri(cx, cy - 9, cx - 3, cy - 2, cx + 3, cy - 2, y);
      cv.tri(cx, cy + 8, cx - 3, cy + 1, cx + 3, cy + 1, y);
      cv.tri(cx - 9, cy - 3, cx - 1, cy - 1, cx - 4, cy + 3, y);
      cv.tri(cx + 9, cy - 3, cx + 1, cy - 1, cx + 4, cy + 3, y);
      cv.disc(cx, cy, 4, shade(y, 105)); cv.set(cx - 1, cy - 2, rgb(255, 255, 240));
      break; }
    case IC_BIRD: {
      Color b = rgb(70, 160, 255);
      cv.tri(cx + 4, cy + 1, cx + 11, cy - 2, cx + 10, cy + 5, rgb(40, 100, 200));
      gellipse(cv, cx, cy, 7, 5, b);
      gdisc(cv, cx - 5, cy - 4, 3, b);
      cv.tri(cx - 9, cy - 4, cx - 7, cy - 5, cx - 7, cy - 2, rgb(255, 180, 50));
      cv.set(cx - 5, cy - 5, rgb(255, 255, 255)); cv.ellipse(cx + 1, cy + 1, 3, 2, rgb(40, 100, 200));
      break; }
    case IC_BOLT: {
      Color y = rgb(255, 220, 40);
      cv.tri(cx + 2, cy - 9, cx - 6, cy + 2, cx, cy + 2, y);
      cv.tri(cx - 1, cy + 9, cx + 6, cy - 2, cx, cy - 2, y);
      cv.rect(cx - 1, cy - 2, 3, 4, y);
      break; }
    case IC_BONE: {
      Color w = rgb(250, 240, 220);
      cv.rect(cx - 6, cy - 2, 12, 4, w);
      gdisc(cv, cx - 7, cy - 3, 3, w); gdisc(cv, cx - 7, cy + 3, 3, w); gdisc(cv, cx + 7, cy - 3, 3, w); gdisc(cv, cx + 7, cy + 3, 3, w);
      break; }
  }
}


void drawIconScaled(Canvas& cv, int id, int cx, int cy, float s) {
  static uint16_t tmp[40 * 40];
  Canvas t(tmp, 40, 40);
  const Color K = 0xF81F;
  t.clear(K);
  drawIcon(t, id, 20, 20);
  for (int y = 0; y < 40; y++) for (int x = 0; x < 40; x++) {
    Color p = tmp[y * 40 + x];
    if (p == K) continue;
    cv.set(cx + (int)floorf((x - 20) * s + 0.5f), cy + (int)floorf((y - 20) * s + 0.5f), p);
  }
}
}  // namespace cat
