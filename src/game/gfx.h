// Shaded UI primitives: gradients, glossy buttons, sphere-shaded icons.
#pragma once
#include "canvas.h"
namespace cat {
void vgrad(Canvas& cv, int x, int y, int w, int h, Color top, Color bot);
void gdisc(Canvas& cv, int cx, int cy, int r, Color base);                 // lit sphere with outline + specular
void gellipse(Canvas& cv, float cx, float cy, float rx, float ry, Color base);
void glossBtn(Canvas& cv, int x, int y, int w, int h, Color base, bool pressed = false);
void gbar(Canvas& cv, int x, int y, int w, int h, int pct, Color fill);   // glossy meter

enum IconId : uint8_t {
  IC_FEED, IC_PLAY, IC_BOX, IC_SHOP, IC_MENU, IC_COIN, IC_HEART, IC_FISH, IC_MOUSE, IC_CHEESE,
  IC_HAIRBALL, IC_STAR, IC_BIRD, IC_BOLT, IC_PAW, IC_BONE, IC_COUNT
};
void drawIcon(Canvas& cv, int id, int cx, int cy);   // ~20 px, centred
void drawIconScaled(Canvas& cv, int id, int cx, int cy, float s);   // nearest-neighbour shrink (HUD)
}  // namespace cat
