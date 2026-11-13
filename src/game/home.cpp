// The home room: scenery, the cat "actor" that wanders / eats / sleeps, HUD, toolbar, taps.
#include "game.h"
#include "gfx.h"
#include "roomdata.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace cat {

static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static const Color kOutline = rgb(24, 20, 32);

enum OldIcon { OI_FEED, OI_PLAY, OI_LITTER, OI_SHOP, OI_MENU, OI_HEART, OI_HB, OI_FISH, OI_BALL, OI_MOON };

// ---------------------------------------------------------------------------------------
// small drawing helpers
// ---------------------------------------------------------------------------------------

void Game::drawHairball(Canvas& cv, int cx, int cy, int r) {
  cv.disc(cx, cy, r + 1, kOutline);
  cv.disc(cx, cy, r, rgb(168, 140, 112));
  cv.disc(cx - r / 3, cy - r / 3, r / 2, rgb(196, 172, 146));
  cv.set(cx + r, cy - 1, rgb(168, 140, 112));
  cv.set(cx + r + 1, cy - 2, rgb(168, 140, 112));
  cv.set(cx - r, cy + 1, rgb(140, 112, 90));
  cv.set(cx + 1, cy + r, rgb(140, 112, 90));
}

void Game::drawPrey(Canvas& cv, int type, int cx, int cy) {
  switch (type) {
    case PREY_MOUSE:
    case PREY_GOLDEN: {
      bool g = type == PREY_GOLDEN;
      Color body = g ? rgb(252, 214, 70) : rgb(168, 168, 182);
      Color dark = g ? rgb(196, 140, 30) : rgb(110, 110, 128);
      cv.line(cx - 6, cy, cx - 12, cy - 2, rgb(240, 150, 170));
      cv.line(cx - 12, cy - 2, cx - 14, cy - 5, rgb(240, 150, 170));
      cv.ellipse(cx, cy, 7, 4, kOutline);
      cv.ellipse(cx, cy, 6, 3, body);
      cv.disc(cx + 3, cy - 3, 2, dark);
      cv.set(cx + 5, cy - 1, kOutline);
      cv.set(cx + 7, cy, rgb(240, 120, 140));
      if (g) {
        int s = (int)(anim_ * 6) % 3;
        cv.set(cx - 7, cy - 6 + s, rgb(255, 255, 200));
        cv.set(cx + 8, cy - 7, rgb(255, 255, 255));
        cv.set(cx + 2, cy - 8 + s, rgb(255, 255, 200));
      }
      break;
    }
    case PREY_BIRD:
      cv.line(cx - 1, cy + 4, cx - 1, cy + 7, rgb(240, 180, 60));
      cv.line(cx + 2, cy + 4, cx + 2, cy + 7, rgb(240, 180, 60));
      cv.ellipse(cx, cy, 6, 5, kOutline);
      cv.ellipse(cx, cy, 5, 4, rgb(84, 154, 244));
      cv.ellipse(cx - 1, cy + 1, 3, 2, rgb(56, 108, 200));
      cv.disc(cx + 4, cy - 3, 3, rgb(84, 154, 244));
      cv.tri(cx + 6, cy - 4, cx + 6, cy - 1, cx + 9, cy - 2, rgb(252, 200, 60));
      cv.set(cx + 4, cy - 4, kOutline);
      cv.tri(cx - 5, cy, cx - 10, cy - 2, cx - 9, cy + 2, rgb(56, 108, 200));
      break;
    case PREY_LIZARD:
      cv.tri(cx - 6, cy, cx - 14, cy + 2, cx - 6, cy + 2, kOutline);
      cv.tri(cx - 6, cy, cx - 13, cy + 2, cx - 6, cy + 1, rgb(84, 200, 92));
      cv.ellipse(cx, cy, 7, 3, kOutline);
      cv.ellipse(cx, cy, 6, 2, rgb(84, 200, 92));
      cv.line(cx - 3, cy + 2, cx - 4, cy + 4, rgb(40, 130, 72));
      cv.line(cx + 3, cy + 2, cx + 4, cy + 4, rgb(40, 130, 72));
      cv.disc(cx + 6, cy - 1, 2, rgb(84, 200, 92));
      cv.set(cx + 6, cy - 2, kOutline);
      break;
    case PREY_FISH:
      cv.tri(cx - 5, cy, cx - 11, cy - 4, cx - 11, cy + 4, kOutline);
      cv.tri(cx - 5, cy, cx - 10, cy - 3, cx - 10, cy + 3, rgb(240, 150, 60));
      cv.ellipse(cx, cy, 7, 4, kOutline);
      cv.ellipse(cx, cy, 6, 3, rgb(120, 190, 240));
      cv.ellipse(cx, cy + 1, 5, 1, rgb(220, 236, 250));
      cv.set(cx + 4, cy - 1, kOutline);
      break;
    default: break;
  }
}

void Game::drawIcon(Canvas& cv, int id, int cx, int cy) {
  drawIconS(cv, id, cx, cy, 1.0f);
}
void Game::drawIconS(Canvas& cv, int id, int cx, int cy, float s) {
  static const uint8_t map[10] = {cat::OI_FEED, cat::OI_PLAY, cat::IC_BOX, cat::OI_SHOP, cat::OI_MENU, cat::IC_HEART, cat::IC_HAIRBALL, cat::OI_FISH, cat::OI_PLAY, cat::IC_BOLT};
  int m = map[id < 0 || id > 9 ? 0 : id];
  if (s >= 0.99f) cat::drawIcon(cv, m, cx, cy); else cat::drawIconScaled(cv, m, cx, cy, s);
}

void Game::drawBubble(Canvas& cv, int x, int y, const char* s) {
  int w = Canvas::textWidth(s) + 8;
  int bx = x - w / 2;
  if (bx < 2) bx = 2;
  if (bx + w > W - 2) bx = W - 2 - w;
  cv.panel(bx, y - 12, w, 12, rgb(255, 255, 255), kOutline);
  cv.text(bx + 4, y - 9, s, kOutline);
  cv.tri(x - 2, y, x + 2, y, x, y + 4, kOutline);
  cv.tri(x - 1, y - 1, x + 1, y - 1, x, y + 2, rgb(255, 255, 255));
}

// ---------------------------------------------------------------------------------------
// scenery
// ---------------------------------------------------------------------------------------

static float nightness(int hour, int minute) {
  float t = hour + minute / 60.0f;
  if (t >= 7 && t < 18) return 0;
  if (t >= 18 && t < 21) return (t - 18) / 3.0f;
  if (t >= 5 && t < 7) return 1.0f - (t - 5) / 2.0f;
  return 1.0f;
}


// ---- furniture and toys (small procedural sprites; the room itself is static) ----
void Game::drawItem(Canvas& cv, int item, int x, int y) {
  const Color ol = rgb(30, 20, 36);
  switch (item) {
    case 103: {
      cv.ellipse(x, y + 1, 15, 5, ol); cv.ellipse(x, y, 14, 4, rgb(168, 64, 92)); cv.ellipse(x, y - 1, 10, 3, rgb(228, 120, 150)); break; }
    case ITEM_BED: {
      cv.ellipse(x, y + 1, 21, 7, ol);
      cv.ellipse(x, y, 20, 6, rgb(168, 64, 92));
      cv.ellipse(x, y - 1, 15, 4, rgb(228, 120, 150));
      cv.ellipse(x - 5, y - 2, 6, 2, rgb(246, 170, 190));
      cv.hline(x - 18, y + 4, 36, rgb(120, 40, 64));
      break; }
    case ITEM_TREE: case ITEM_TREEBED: {
      Color carpet = rgb(196, 160, 120), dark = rgb(150, 116, 82), top = rgb(176, 136, 98);
      cv.ellipse(x, y + 1, 19, 5, ol);
      cv.rect(x - 17, y - 5, 34, 6, dark); cv.rect(x - 17, y - 5, 34, 2, carpet); cv.frame(x - 17, y - 5, 34, 6, ol);   // base
      cv.rect(x - 4, y - 52, 8, 48, carpet); cv.vline(x - 4, y - 52, 48, ol); cv.vline(x + 3, y - 52, 48, ol);          // trunk
      for (int j = y - 48; j < y - 6; j += 4) cv.hline(x - 3, j, 6, dark);                                                // sisal rope
      cv.rect(x + 4, y - 28, 14, 4, top); cv.frame(x + 4, y - 28, 14, 4, ol);                                               // low step
      cv.rect(x - 20, y - 56, 40, 5, top); cv.rect(x - 20, y - 56, 40, 2, carpet); cv.frame(x - 20, y - 56, 40, 5, ol);   // top platform
      if (item == ITEM_TREEBED) {
        cv.ellipse(x, y - 60, 15, 6, ol); cv.ellipse(x, y - 60, 14, 5, rgb(168, 64, 92)); cv.ellipse(x, y - 61, 10, 3, rgb(228, 120, 150));
      } else { cv.disc(x + 11, y - 31, 2, rgb(240, 100, 150)); cv.line(x + 11, y - 29, x + 11, y - 24, rgb(240, 100, 150)); }
      break; }
    case 101: case 102: {   // shop thumbnails of the tree
      Color carpet = rgb(196, 160, 120), dark = rgb(150, 116, 82), top = rgb(176, 136, 98);
      cv.rect(x - 8, y - 2, 16, 3, dark); cv.rect(x - 2, y - 15, 4, 14, carpet); cv.vline(x - 2, y - 15, 14, ol); cv.vline(x + 1, y - 15, 14, ol);
      cv.rect(x - 10, y - 18, 20, 3, top); cv.frame(x - 10, y - 18, 20, 3, ol);
      if (item == 102) { cv.ellipse(x, y - 21, 7, 3, rgb(168, 64, 92)); cv.ellipse(x, y - 22, 5, 2, rgb(228, 120, 150)); }
      break; }
    case ITEM_MOUSE: {
      cv.ellipse(x, y, 6, 3, ol); cv.ellipse(x, y - 1, 5, 3, rgb(176, 176, 194)); cv.disc(x + 3, y - 3, 1, rgb(240, 150, 170));
      cv.set(x + 4, y - 2, rgb(30, 20, 30)); cv.line(x - 5, y, x - 9, y - 2, rgb(240, 150, 170)); break; }
    case ITEM_YARN: {
      cv.disc(x, y - 4, 5, ol); cv.disc(x, y - 4, 4, rgb(255, 80, 170));
      cv.line(x - 3, y - 3, x + 3, y - 6, rgb(255, 170, 220)); cv.line(x - 3, y - 5, x + 2, y - 2, rgb(255, 170, 220)); cv.line(x + 4, y, x + 9, y + 1, rgb(255, 80, 170)); break; }
    case ITEM_TUNNEL: {
      cv.ellipse(x, y - 4, 14, 8, ol); cv.ellipse(x, y - 4, 13, 7, rgb(86, 160, 230)); cv.ellipse(x - 9, y - 3, 3, 5, rgb(30, 40, 90));
      for (int k = -2; k <= 3; k++) cv.vline(x + k * 3, y - 10, 5, rgb(140, 200, 250)); break; }
    case ITEM_RCHAIR: case 104: {   // old armchair, side view facing the table; clawed to bits
      const bool th = item == 104;   // shop thumbnail: a bit smaller
      const int s = th ? 1 : 0;
      Color fab = rgb(150, 72, 62), fabD = rgb(110, 48, 44), fabL = rgb(184, 100, 84), wood = rgb(96, 62, 40), stuff = rgb(244, 234, 214);
      int sw = th ? 18 : 24, bh = th ? 14 : 20, seatY = y - (th ? 8 : 11);
      int L = x - sw / 2;
      cv.rect(L + 2, y - 4 + s, 2, 4 - s, wood); cv.rect(L + sw - 4, y - 4 + s, 2, 4 - s, wood);   // legs
      cv.rect(L, seatY - bh, 6, bh + 1, fabD); cv.frame(L, seatY - bh, 6, bh + 1, ol);              // back (left, they face the table)
      cv.rect(L, seatY, sw, 6, fab); cv.hline(L, seatY, sw, fabL); cv.frame(L, seatY, sw, 6, ol);     // seat cushion
      cv.rect(L + sw - 5, seatY - 4, 5, 5, fabD); cv.frame(L + sw - 5, seatY - 4, 5, 5, ol);          // arm
      // claw marks + stuffing poking out
      cv.line(L + 1, seatY - bh + 3, L + 4, seatY - bh + 7, fabL); cv.line(L + 1, seatY - bh + 6, L + 4, seatY - bh + 10, fabL);
      cv.set(L + 2, seatY - 3, stuff); cv.set(L + 3, seatY - 4, stuff); cv.set(L + 8, seatY + 2, stuff); cv.set(L + 9, seatY + 1, stuff);
      cv.set(L + sw - 3, seatY - 5, stuff); cv.line(L + 12, seatY + 3, L + 15, seatY + 5, fabD);
      if (!th) { cv.set(L - 1, seatY - bh + 2, stuff); cv.set(L - 2, seatY - bh + 1, stuff); cv.disc(L + 14, seatY + 2, 1, stuff); }
      break; }
    case ITEM_LCHAIR: case 105: {   // velvet wingback with gold trim and curvy legs
      const bool th = item == 105;
      Color vel = rgb(112, 58, 156), velD = rgb(78, 36, 114), velL = rgb(156, 104, 204), gold = rgb(236, 192, 84), goldD = rgb(170, 128, 40);
      int sw = th ? 18 : 26, bh = th ? 15 : 22, seatY = y - (th ? 8 : 11);
      int L = x - sw / 2;
      cv.line(L + 2, y, L + 3, seatY + 5, goldD); cv.line(L + sw - 2, y, L + sw - 3, seatY + 5, goldD);   // curvy legs
      cv.set(L + 1, y, gold); cv.set(L + sw - 1, y, gold);
      cv.ellipse(L + 4, seatY - bh / 2, 5, bh / 2 + 1, ol); cv.ellipse(L + 4, seatY - bh / 2, 4, bh / 2, vel);   // plush rounded back
      cv.vline(L + 3, seatY - bh + 3, bh - 4, velL);
      for (int k = 0; k < 3; k++) cv.set(L + 4, seatY - bh + 4 + k * (bh / 3), gold);                          // buttons
      cv.rect(L + 1, seatY, sw - 1, 6, velD); cv.frame(L + 1, seatY, sw - 1, 6, ol);                            // base
      cv.ellipse(L + sw / 2 + 2, seatY, sw / 2 - 2, 3, vel); cv.hline(L + 6, seatY - 2, sw - 10, velL);        // puffy cushion
      cv.hline(L + 1, seatY + 5, sw - 1, gold);                                                                    // gold trim
      cv.ellipse(L + sw - 3, seatY - 3, 3, 4, ol); cv.ellipse(L + sw - 3, seatY - 3, 2, 3, vel);                // rolled arm
      if (!th) { cv.vline(L + sw - 3, seatY + 6, 3, gold); cv.set(L + sw - 3, seatY + 9, goldD); }                // tassel
      break; }
    case ITEM_WAND: {
      cv.line(x, y, x - 3, y - 16, rgb(160, 110, 70)); cv.line(x - 3, y - 16, x + 5, y - 22, rgb(200, 160, 110));
      cv.tri(x + 5, y - 22, x + 11, y - 27, x + 9, y - 19, rgb(250, 90, 90)); cv.tri(x + 5, y - 22, x + 9, y - 19, x + 4, y - 18, rgb(250, 210, 60)); break; }
  }
}

void Game::drawFloorX(int i, int& x, int& y) const {
  static const int xs[4] = {64, 118, 84, 146};   // (the first: clear of the Luxury Chair's legs)
  static const int ys[4] = {167, 170, 174, 166};
  x = xs[i & 3];
  y = ys[i & 3];
}

void Game::drawBox(Canvas& cv, int cx, int baseY, int drops) {
  // The box body is part of the hi-res room; draw only what changes: droppings, smell, upgrades.
  uint8_t tier = sv.litterTier;
  int top = baseY - 18, x0 = cx - 19;
  static const int dxs[6] = {-10, 2, 12, -4, 8, -14};
  static const int dys[6] = {-1, 0, -2, 1, 1, 1};
  for (int i = 0; i < drops && i < 6; i++) {
    int dx = cx + dxs[i], dy = top + 3 + dys[i];
    cv.ellipse(dx, dy, 3, 2, rgb(96, 60, 34));
    cv.ellipse(dx - 1, dy - 1, 1, 1, rgb(150, 104, 66));
  }
  if (drops >= 3 && tier < 2) {
    int s = (int)(anim_ * 5);
    for (int k = -1; k <= 1; k++) {
      int sx = cx + k * 9;
      for (int j = 0; j < 4; j++) cv.set(sx + ((j + s + k) % 2), top - 5 - j * 2, rgb(150, 200, 70));
    }
  }
  if (tier >= 1) {
    bool on = ((int)(anim_ * 2) % 2) == 0;
    cv.rect(x0 + 30, top + 11, 4, 3, tier == 2 ? (on ? rgb(90, 240, 120) : rgb(40, 120, 60)) : rgb(250, 200, 70));
  }
}

void Game::drawRoom(Canvas& cv) {
  // The room itself is the hi-res cache; here we only mark its band as "see-through".
  cv.rect(0, HUD_H, W, TOOL_Y - HUD_H, KEY_COLOR);
  fi.keyed = true;
  gShadowKey = true;
}

void Game::drawHud(Canvas& cv) {
  vgrad(cv, 0, 0, W, HUD_H, rgb(66, 48, 104), rgb(34, 24, 58));
  cv.hline(0, HUD_H - 1, W, rgb(20, 12, 36));
  cv.hline(0, 0, W, rgb(120, 96, 168));
  char b[24];
  // top row sits inside the rounded corners
  drawIconS(cv, OI_HB, CORNER + 3, 8, 0.62f);
  coinText(b, sizeof(b));
  cv.textShadow(CORNER + 11, 3, b, coinColor(rgb(255, 236, 160)), rgb(30, 20, 50));
  if (clockOk) snprintf(b, sizeof(b), "%02d:%02d", hour, minute);
  else snprintf(b, sizeof(b), "--:--");
  cv.textCenter(W / 2, 3, b, rgb(230, 222, 250));
  if (batOk_) {   // battery: little cell right of the clock, colour by charge, bolt while charging
    const int bx = W / 2 + 19, by = 3, bw = 13, bh = 7;
    Color fill = batPct_ > 50 ? rgb(110, 220, 120) : (batPct_ > 20 ? rgb(240, 210, 80) : rgb(240, 80, 80));
    if (batPct_ <= 10 && !batChg_ && ((int)(anim_ * 2) & 1)) fill = rgb(120, 40, 40);   // blink when nearly empty
    cv.frame(bx, by, bw, bh, rgb(230, 222, 250));
    cv.rect(bx + bw, by + 2, 1, 3, rgb(230, 222, 250));
    int fw = (bw - 2) * (batPct_ < 0 ? 0 : (batPct_ > 100 ? 100 : batPct_)) / 100;
    if (fw > 0) cv.rect(bx + 1, by + 1, fw, bh - 2, fill);
    if (batChg_) { cv.line(bx + 7, by + 1, bx + 5, by + 3, rgb(255, 250, 120)); cv.line(bx + 5, by + 3, bx + 8, by + 3, rgb(255, 250, 120)); cv.line(bx + 8, by + 3, bx + 6, by + 5, rgb(255, 250, 120)); }
  }
  snprintf(b, sizeof(b), "%d", (int)(pet.d.health + 0.5f));
  {
    int hx = W - CORNER + 4 - Canvas::textWidth(b);
    drawIconS(cv, OI_HEART, hx - 7, 8, 0.62f);
    cv.textShadow(hx, 3, b, rgb(255, 170, 190), rgb(30, 20, 50));
  }
  struct Bar { int icon; float v; Color c; };
  Bar bars[3] = {{OI_FISH, pet.d.hunger, rgb(250, 160, 50)}, {OI_BALL, pet.d.fun, rgb(246, 110, 160)}, {OI_MOON, pet.d.energy, rgb(100, 180, 255)}};
  for (int i = 0; i < 3; i++) {
    int x = 2 + i * 61;
    drawIconS(cv, bars[i].icon, x + 8, 19, 0.55f);
    Color c = bars[i].v < 25 ? rgb(240, 70, 70) : bars[i].c;
    gbar(cv, x + 17, 15, 40, 7, (int)bars[i].v, c);
  }
}

void Game::drawToolbar(Canvas& cv, bool interactive) {
  vgrad(cv, 0, TOOL_Y, W, TOOL_H, rgb(48, 36, 78), rgb(24, 16, 42));
  cv.hline(0, TOOL_Y, W, rgb(120, 96, 168));
  static const char* labels[5] = {"FEED", "PLAY", "POO", "SHOP", "MENU"};
  static const int icons[5] = {OI_FEED, OI_PLAY, OI_LITTER, OI_SHOP, OI_MENU};
  static const Color cols[5] = {rgb(224, 120, 60), rgb(222, 80, 140), rgb(70, 140, 220), rgb(150, 100, 210), rgb(90, 90, 150)};
  bool badge[5] = {
    pet.d.hunger < 35 && !pet.sleeping,
    pet.d.fun < 35 && !pet.sleeping,
    pet.d.droppings - holdBox_ >= 1 || pet.d.floorMess - holdFloor_ > 0,   // the dot matches what you can see: any poop in the box or on the floor
    false, false};
  for (int i = 0; i < 5; i++) {
    int x = i * W / 5 + 1, w = W / 5 - 2, y = TOOL_Y + 2, h = 37;
    bool press = interactive && in.down && in.px >= x && in.px < x + w && in.py >= y && in.py < y + h;
    glossBtn(cv, x, y, w, h, cols[i], press);
    int o = press ? 2 : 0;
    drawIcon(cv, icons[i], x + w / 2, y + 12 + o);
    cv.textCenter(x + w / 2 + 1, y + 24 + o, labels[i], rgb(30, 20, 40));
    cv.textCenter(x + w / 2, y + 23 + o, labels[i], rgb(255, 250, 255));
    if (badge[i] && ((int)(anim_ * 2) % 2 == 0)) gdisc(cv, x + w - 5, y + 5, 3, rgb(240, 60, 60));
    if (interactive && tapIn(x, y, w, h)) {
      sound(SND_TAP);
      switch (i) {
        case 0:
          if (pet.sleeping) toast("%s is asleep! Shh.", pet.d.name);
          else if (pet.d.hunting) toast("%s is out hunting.", pet.d.name);
          else if (!pet.canFeed()) toast("%s is too full to eat!", pet.d.name);
          else startPour();   // FEED goes straight to pouring the kibble
          break;
        case 1:
          if (!pet.sleeping && !pet.d.hunting && pet.d.energy < 15) { toast("Too tired to play... cat nap time!"); startLap(); }   // straight into a nap
          else setScreen(S_PLAY);
          break;
        case 2: setScreen(S_LITTER); break;
        case 3: shopPage_ = 0; setScreen(S_SHOP); break;
        case 4: setScreen(S_MENU); break;
      }
    }
  }
}

// ---------------------------------------------------------------------------------------
// particles / toasts
// ---------------------------------------------------------------------------------------

void Game::spawn(float x, float y, uint8_t kind, Color c, float vx, float vy, float life) {
  int idx = -1;
  for (int i = 0; i < 32; i++) if (parts_[i].life <= 0) { idx = i; break; }
  if (idx < 0) idx = (int)(pet.rnd() % 32);
  parts_[idx] = {x, y, vx, vy, life, life, kind, c};
}

void Game::updateParticles(float d) {
  for (int i = 0; i < 32; i++) {
    Particle& p = parts_[i];
    if (p.life <= 0) continue;
    p.life -= d;
    p.x += p.vx * d;
    p.y += p.vy * d;
    if (p.kind == 4) p.vy += 120 * d;  // fibers fall
    if (p.kind == 0 || p.kind == 2) p.vx = sinf(p.life * 6 + i) * 8;
  }
  for (int i = 0; i < 3; i++) if (toasts_[i].t > 0) toasts_[i].t -= d;
  if (bubbleT_ > 0) bubbleT_ -= d;
  if (bowl_ > 0 && a_.act != A_EAT) bowl_ = bowl_ > 0.02f ? bowl_ - d * 0.01f : bowl_;
}

void Game::drawParticles(Canvas& cv) {
  for (int i = 0; i < 32; i++) {
    const Particle& p = parts_[i];
    if (p.life <= 0) continue;
    int x = (int)p.x, y = (int)p.y;
    switch (p.kind) {
      case 0:
        cv.disc(x - 1, y, 1, rgb(250, 90, 130)); cv.disc(x + 1, y, 1, rgb(250, 90, 130));
        cv.tri(x - 2, y + 1, x + 2, y + 1, x, y + 3, rgb(250, 90, 130));
        break;
      case 1: case 7:
        cv.set(x, y, p.col); cv.set(x - 1, y, p.col); cv.set(x + 1, y, p.col); cv.set(x, y - 1, p.col); cv.set(x, y + 1, p.col);
        break;
      case 2: {
        int s = p.life / p.maxLife > 0.5f ? 1 : 1;
        cv.text(x, y, "z", rgb(200, 220, 255), s);
        break;
      }
      case 3:
        cv.disc(x, y, 3, rgb(252, 226, 72)); cv.disc(x, y, 1, rgb(210, 160, 30));
        break;
      case 4:
        cv.set(x, y, p.col); cv.set(x + 1, y + 1, p.col);
        break;
      case 5: {
        int r = 2 + (int)((1.0f - p.life / p.maxLife) * 4);
        cv.ring(x, y, r, p.col ? p.col : rgb(240, 240, 240));
        break;
      }
      default: break;
    }
  }
}

void Game::drawToasts(Canvas& cv) {
  const int MAXW = 164;   // text wider than this wraps onto a second line (so it stays on the screen)
  int y = HUD_H + 3;
  for (int i = 0; i < 3; i++) {
    if (toasts_[i].t <= 0) continue;
    char l1[64], l2[64];
    strncpy(l1, toasts_[i].text, sizeof(l1) - 1); l1[sizeof(l1) - 1] = 0; l2[0] = 0;
    if (Canvas::textWidth(l1) > MAXW) {
      int cut = -1;   // the last space that leaves the first line short enough
      for (int k = 0; l1[k]; k++) { if (l1[k] == ' ') { char c = l1[k]; l1[k] = 0; const int w = Canvas::textWidth(l1); l1[k] = c; if (w <= MAXW) cut = k; else break; } }
      if (cut < 0) { cut = 26; strncpy(l2, l1 + cut, sizeof(l2) - 1); l2[sizeof(l2) - 1] = 0; }
      else { strncpy(l2, l1 + cut + 1, sizeof(l2) - 1); l2[sizeof(l2) - 1] = 0; }
      l1[cut] = 0;
    }
    int w = Canvas::textWidth(l1), w2 = l2[0] ? Canvas::textWidth(l2) : 0;
    if (w2 > w) w = w2;
    w += 10;
    const int x = (W - w) / 2, h = l2[0] ? 24 : 13;
    cv.panel(x, y, w, h, rgb(30, 22, 46), rgb(250, 230, 150));
    cv.textCenter(W / 2, y + 3, l1, rgb(255, 245, 220));
    if (l2[0]) cv.textCenter(W / 2, y + 14, l2, rgb(255, 245, 220));
    y += h + 2;
  }
}

// ---------------------------------------------------------------------------------------
// the cat actor
// ---------------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------------
// the mouse hole (every home has one, on the baseboard) and the mouse that lives in it
// ---------------------------------------------------------------------------------------
int Game::mouseHoleY() const {   // where the wall meets the floor (the modern loft's glass wall and the Log Cabin's logs end a little higher)
  static const int HY[NUM_ROOMS] = {124, 124, 124, 122, 124, 120, 124};
  return HY[sv.room < NUM_ROOMS ? sv.room : 0];
}
int Game::mouseRunY() const { return mouseHoleY() + 3; }   // the mouse runs along the floor just in front of its hole

int Game::mouseHoleX() const {
  static const int HX[NUM_ROOMS] = {40, 44, 44, 25, 40, 40, 40};   // on the back wall, away from the corners and clear of the table (the Log Cabin's fireplace is on the left wall)
  return HX[sv.room < NUM_ROOMS ? sv.room : 0];
}

// The hole is painted into the room picture itself (full resolution, smoothly edged, shaded from the wall's own colours),
// so it looks like part of the house rather than a sticker. It is redone whenever the room is re-lit for the time of day.
void Game::bakeMouseHole() {
  if (!roomBuf_) return;
  const int cx = mouseHoleX() * 2, base = mouseHoleY() * 2 + 1;   // physical pixels; base = the bottom edge of the opening
  const float a = 9.5f, total = 18.5f, ycen = base - total + a;   // an arch: half-width a, total height
  const int sx = cx - 13, ex = cx + 13, sy = base - 26, ey = base + 3;
  auto at = [&](int x, int y) -> uint16_t& { return roomBuf_[y * HW + x]; };
  const Color wall = at(cx, sy - 4);                      // the wall just above, for the colour of the rim and the shadows
  for (int y = sy; y <= ey; y++) for (int x = sx; x <= ex; x++) {
    if (x < 0 || x >= HW || y < 0 || y >= HH) continue;
    const float dx = (float)(x - cx), dy = (float)y + 0.5f - ycen;
    float d;                                              // signed distance to the arch: negative inside
    if (dy <= 0) d = sqrtf(dx * dx + dy * dy) - a;
    else d = fmaxf(fabsf(dx) - a, ((float)y + 0.5f) - base);
    Color px = at(x, y);
    if (d > 0 && d < 5.0f) px = mix(px, shade(wall, 62), (int)(70 * (1.0f - d / 5.0f)));      // a soft shadow around it
    if (d <= 0.5f) {
      const float cov = d <= -0.5f ? 1.0f : 0.5f - d;     // anti-aliased edge
      const float depth = (float)(y - (base - total)) / total;   // 0 at the top, 1 at the floor
      const Color inner = rgb(10 + (int)(18 * depth), 6 + (int)(12 * depth), 8 + (int)(14 * depth));
      Color c = inner;
      if (d > -2.6f) c = mix(inner, shade(wall, 45), 256 - (int)(256 * (-d) / 2.6f) / 1);   // the broken plaster edge, darker than the wall
      px = mix(px, c, (int)(256 * cov));
    }
    at(x, y) = px;
  }
  // a faint crumb of plaster on the floor line
  at(cx + 13, base + 1) = shade(wall, 80); at(cx - 13, base + 1) = shade(wall, 75);
}

// The Modern Apartment's three recessed ceiling lights, painted into the room picture like the mouse hole. They sit
// on the strip of ceiling at the top (room rows 0..13): one in the middle, the other two out near the window's left
// and right edges (the frame spans x 60..307), above the three pools of light on the floor (x 72 / 184 / 296).
// By day they are off (a pale lens in a trim ring); as night comes they light up warm and glow on the ceiling.
void Game::bakeCeilingLights(int night) {
  if (!roomBuf_) return;
  static const int LX[3] = {60, 184, 307};
  const int LY = 7, CEIL_ROWS = 14;                       // the ceiling meets the back wall at row 14
  const float RX = 9.0f, RY = 2.6f;                       // seen from below at a shallow angle: a flat ellipse
  const Color warm = rgb(255, 236, 184);
  auto at = [&](int x, int y) -> uint16_t& { return roomBuf_[(ROOM_TOP + y) * HW + x]; };
  for (int i = 0; i < 3; i++) {
    const int cx = LX[i];
    const Color ceil = at(cx, LY + 4);                    // the ceiling's own colour right now (day or night)
    for (int y = 0; y < CEIL_ROWS; y++) for (int x = cx - 34; x <= cx + 34; x++) {
      if (x < 0 || x >= HW) continue;
      const float dx = (float)(x - cx), dy = (float)(y - LY);
      Color px = at(x, y);
      // the glow on the ceiling around a lit lamp
      const float h = sqrtf(dx * dx / (34.0f * 34.0f) + dy * dy / (8.0f * 8.0f));
      if (h < 1.0f && night > 0) px = mix(px, warm, (int)(120.0f * (1.0f - h) * (1.0f - h)) * night / 256);
      const float d = sqrtf(dx * dx / (RX * RX) + dy * dy / (RY * RY));
      if (d < 1.0f + 0.5f / RY) {
        const int cov = (int)(256.0f * clampf((1.0f - d) * RY + 0.5f, 0, 1));   // soft edge
        const Color off = dy < -0.5f ? shade(ceil, 78) : mix(ceil, rgb(255, 255, 255), 90);   // unlit: frosted glass, the recess shading its far edge
        const Color lens = mix(off, mix(warm, rgb(255, 255, 255), dy > -0.5f && fabsf(dx) < 4 ? 120 : 0), night);
        const Color trim = mix(shade(ceil, 70), mix(ceil, warm, 110), night);                 // the ring round it
        px = mix(px, d > 0.72f ? trim : lens, cov);
      }
      at(x, y) = px;
    }
  }
}

void Game::updateMouse() {
  HouseMouse& m = mouse_;
  if (m.room != sv.room) { m.room = sv.room; m.st = 0; m.next = 60.0f + pet.rndf() * 120.0f; }
  const float hx = (float)mouseHoleX();
  m.t += dt;
  // a finger on (or just lifting off) the screen makes the mouse freeze, so its tap target holds still
  const bool still = in.down || in.tap;
  switch (m.st) {
    case 0:   // the mouse is at home; it only comes out when the cat is up and about
      if (!pet.sleeping && a_.act != A_HIDDEN && a_.act != A_SLEEP && !pet.d.hunting) m.next -= dt;
      if (m.next <= 0) {
        m.st = 1; m.t = 0; m.x = hx; m.left = false;
        const float dist = 28.0f + pet.rndf() * 50.0f;
        m.tx = clampf(hx + dist, 30, 172);
        sound(SND_POP);
      }
      break;
    case 1:   // scurries out along the baseboard
      if (!still) m.x += (m.tx > m.x ? 1 : -1) * 38.0f * dt;
      m.left = m.tx < m.x;
      if (fabsf(m.tx - m.x) < 2.0f) { m.st = 2; m.t = 0; }
      break;
    case 2:   // sniffs about
      if (m.t > 1.3f) { m.st = 3; m.t = 0; }
      break;
    case 3:   // and scampers back into the hole
      m.left = hx < m.x; if (!still) m.x += (hx > m.x ? 1 : -1) * 46.0f * dt;
      if (fabsf(hx - m.x) < 2.0f) { m.st = 0; m.next = 100.0f + pet.rndf() * 200.0f; }
      break;
    case 4:   // caught on sight: it freezes, the cat comes running
      if (pet.sleeping || a_.act == A_SLEEP) { m.st = 3; m.t = 0; break; }
      if (fabsf(a_.x - m.x) < 16.0f || m.t > 3.5f) {
        m.st = 0; m.next = 120.0f + pet.rndf() * 240.0f;
        pet.d.energy += 12.0f; pet.d.fun += 3.0f; pet.clampAll();
        pet.petIt(2.0f);
        giveCoins(1);
        a_.act = A_HAPPY; a_.actT = 1.6f; a_.left = m.x < a_.x;
        for (int i = 0; i < 3; i++) spawn(m.x + (float)(i * 6 - 6), mouseRunY() - 8, 0, 0, 0, -22, 1.0f);
        spawn(m.x, mouseRunY() - 4, 5, rgb(220, 220, 230), 0, -6, 0.5f);
        spawn(a_.x, a_.y - 40, 7, 0, 0, -26, 0.9f);
        sound(SND_COIN);
        toast("%s caught the mouse!", pet.d.name);
        toast("+12 energy, +1 hairball");
        autosave(false);
      }
      break;
  }
}

void Game::drawMouse(Canvas& cv) {
  const HouseMouse& m = mouse_;
  if (m.st == 0) return;
  if (m.st == 1 && m.t < 0.25f) { const int ex = mouseHoleX(); cv.set(ex - 2, mouseHoleY() - 5, rgb(255, 232, 150)); cv.set(ex + 1, mouseHoleY() - 5, rgb(255, 232, 150)); }   // two eyes peek out of the dark first
  const int x = (int)m.x, s = m.left ? -1 : 1;
  const bool run = m.st == 1 || m.st == 3;
  const int MOUSE_Y = mouseRunY();
  const int y = MOUSE_Y + (run && ((int)(anim_ * 14) & 1) ? -1 : 0);
  const Color body = rgb(170, 168, 180), dk = rgb(110, 106, 122), pink = rgb(236, 150, 170), ol = rgb(30, 22, 34);
  cv.ellipse(x, MOUSE_Y + 2, 5, 1, rgb(60, 44, 40));                                   // shadow
  cv.ellipse(x, y - 2, 4, 3, ol); cv.ellipse(x, y - 2, 3, 2, body);                    // body
  cv.disc(x + s * 4, y - 2, 2, ol); cv.set(x + s * 4, y - 2, body); cv.set(x + s * 5, y - 2, pink);   // head + nose
  cv.disc(x + s * 3, y - 5, 1, pink); cv.set(x + s * 3, y - 5, pink);                  // ear
  cv.set(x + s * 4, y - 3, ol);                                                         // eye
  cv.line(x - s * 4, y - 1, x - s * 8, y - ((int)(anim_ * 6) & 1 ? 3 : 1), pink);       // tail
  cv.set(x - s, y, dk); cv.set(x + s * 2, y, dk);                                       // feet
  if (m.st != 4) { const int by = y - 14 - (int)(sinf(anim_ * 5) * 1.5f); cv.tri(x - 3, by, x + 3, by, x, by + 4, rgb(255, 226, 120)); }   // "tap me"
  else if (((int)(anim_ * 12)) & 1) cv.set(x + s * 4, y - 6, rgb(255, 255, 255));   // it trembles
}

// The middle of this room's window glass (logical x), so they sit right in front of it.
int Game::windowX() const {
  const RoomImg& r = ROOMS[sv.room < NUM_ROOMS ? sv.room : 0];
  long sx = 0; int n = 0;
  for (int i = 0; i < r.glassN; i++) { sx += (r.glass[i].x0 + r.glass[i].x1) / 2; n++; }
  int x = n ? (int)(sx / n) / 2 : 92;
  return x < CAT_X_MIN + 6 ? CAT_X_MIN + 6 : (x > CAT_X_MAX - 6 ? CAT_X_MAX - 6 : x);
}

int Game::windowBottomY() const {   // glass rows are stored from physical row 52 (= logical 26)
  const RoomImg& r = ROOMS[sv.room < NUM_ROOMS ? sv.room : 0];
  int y = 0;
  for (int i = 0; i < r.glassN; i++) if (r.glass[i].y > y) y = r.glass[i].y;
  return r.glassN ? y / 2 + 26 : 100;
}

// The Hamster's wheel. Drawn in two layers so the hamster runs INSIDE it: the stand, rim and spokes behind, the front rim on top.
void Game::drawWheel(Canvas& cv, bool front) {
  const int cx = WHEEL_X, cy = WHEEL_Y, R = WHEEL_R;
  const Color rim = rgb(250, 196, 84), rimD = rgb(196, 140, 52), metal = rgb(150, 150, 170);
  if (front) {   // just the near edge of the rim, so it looks like they're in it
    for (int a = 20; a <= 160; a += 4) { const float r = a * 0.0174533f; cv.set(cx + (int)(cosf(r) * R), cy + (int)(sinf(r) * R), rim); }
    return;
  }
  cv.ellipse(cx, cy + R + 4, R - 2, 2, rgb(60, 40, 30));                     // its shadow on the floor
  cv.line(cx, cy, cx - 9, cy + R + 3, metal); cv.line(cx, cy, cx + 9, cy + R + 3, metal);   // the stand
  cv.hline(cx - 12, cy + R + 3, 7, rimD); cv.hline(cx + 6, cy + R + 3, 7, rimD);
  cv.ring(cx, cy, R, rimD); cv.ring(cx, cy, R - 1, rim);
  for (int k = 0; k < 8; k++) {                                              // spokes (turning while they run)
    const float a = wheelSpin_ + k * 0.785398f;
    cv.line(cx, cy, cx + (int)(cosf(a) * (R - 2)), cy + (int)(sinf(a) * (R - 2)), rgb(255, 226, 150));
  }
  for (int k = 0; k < 16; k++) {                                             // the rungs round the rim
    const float a = wheelSpin_ + k * 0.392699f;
    cv.set(cx + (int)(cosf(a) * (R - 3)), cy + (int)(sinf(a) * (R - 3)), rimD);
  }
  cv.disc(cx, cy, 2, rgb(255, 240, 180));
}

CatDraw Game::catLook(Pose pose, Face face) const {
  CatDraw d;
  d.breed = pet.lookBreed();
  d.fat = pet.fatLook();
  d.size = pet.visualSize();
  d.kit = pet.kitFactor();
  d.pose = pose;
  d.face = face;
  d.faceLeft = a_.left;
  d.t = anim_;
  d.blink = blinkT_ < 0.0f && blinkT_ > -0.14f;
  d.hat = sv.wornHat; d.glasses = sv.wornGl; d.neck = sv.wornNeck; d.body = sv.wornCollar;   // wornCollar now holds the BODY slot
  if (!d.hat) d.hat = (uint8_t)seasonalHat();   // a birthday party hat, a pumpkin at Halloween, a Santa hat...
  return d;
}

int Game::roamX() {
  // a big cat walking about shouldn't end up half off screen: at most ~12 px of fur past the edge
  const int reach = catReach(catLook(POSE_WALK, FACE_NORMAL)) - 12;
  const int lo = reach > CAT_X_MIN ? reach : CAT_X_MIN, hi = W - reach < CAT_X_MAX ? W - reach : CAT_X_MAX;
  return lo < hi ? (int)pet.rndRange((float)lo, (float)hi) : W / 2;
}

void Game::actorWalkTo(float x, Act after, float dur) {
  a_.tx = clampf(x, 14, 170);
  a_.after = after;
  a_.afterT = dur;
  a_.act = A_WALK;
  a_.speed = 34.0f + 40.0f * pet.agility();
  a_.left = a_.tx < a_.x;
}

void Game::actorEvent(const Event& e) {
  switch (e.type) {
    case EV_POOP:
      if (e.arg == 1) { holdBox_++; actorWalkTo(BOX_X, A_POOP, 3.0f); }
      else { holdFloor_++; int fx, fy; drawFloorX(pet.d.floorMess - 1, fx, fy); actorWalkTo((float)fx, A_POOP, 2.5f); }
      break;
    case EV_HAIRBALL:
      holdHair_++;
      a_.act = A_HACK;
      a_.actT = 3.2f;
      sound(SND_HACK);
      break;
    case EV_HUNT_GO:
      actorWalkTo(FLAP_X - 8, A_EXIT, 0.8f);
      break;
    case EV_PREY_BACK:
      a_.x = FLAP_X - 4;
      a_.y = CAT_BASE_Y;
      a_.left = true;
      if (e.arg2) {
        holdPrey_++;
        actorWalkTo(130, A_MEOW, 2.2f);
        sound(SND_MEOW);
        toast("%s caught you something!", pet.d.name);
      } else {
        actorWalkTo(120, A_SULK, 3.0f);
        toast("%s came back empty-pawed.", pet.d.name);
      }
      break;
    case EV_MEOW:
      if (!pet.sleeping && a_.act != A_HIDDEN && a_.act != A_EXIT) {
        a_.act = A_MEOW;
        a_.actT = 1.8f;
        static const char* t[] = {"MEOW!", "FEED ME", "BOX!!", "PLAY?"};
        int r = e.arg2 >= 1 && e.arg2 <= 3 ? e.arg2 : 0;
        strncpy(bubble_, t[r], sizeof(bubble_) - 1);
        bubbleT_ = 2.5f;
      }
      break;
    default: break;
  }
}

void Game::updateActor() {
  Actor& a = a_;
  const float d = dt;
  a.t += d;
  blinkT_ -= d;
  if (blinkT_ < -0.16f) blinkT_ = 2.5f + (pet.rnd() % 350) / 100.0f;

  // sync with the pet's sleep / hunt state
  if (pet.d.hunting) {
    if (a.act != A_EXIT && a.act != A_HIDDEN && !(a.act == A_WALK && a.after == A_EXIT)) {
      a.act = A_HIDDEN;
    }
  } else if (a.act == A_HIDDEN) {
    a.x = FLAP_X - 4;
    a.left = true;
    actorWalkTo(110, A_IDLE, 0);
  }
  if (pet.sleeping && a.act != A_HIDDEN) {
    if (a.act != A_SLEEP && !(a.act == A_WALK && a.after == A_SLEEP)) actorWalkTo(hasItem(ITEM_TREEBED) ? TREE_X : (hasItem(ITEM_BED) ? BED_X : (sv.boxType ? BOX2_X : BED_X)), A_SLEEP, 0);
  } else if (!pet.sleeping && (a.act == A_SLEEP || (a.act == A_WALK && a.after == A_SLEEP))) {
    a.act = A_YAWN;
    a.actT = 1.6f;
  }

  switch (a.act) {
    case A_WALK: {
      float dx = a.tx - a.x;
      float step = a.speed * d;
      if (fabsf(dx) <= step) {
        a.x = a.tx;
        a.act = a.after;
        a.actT = a.afterT;
        if (a.act == A_EAT || a.act == A_POOP) a.left = (a.act == A_POOP) ? false : false;
        if (a.act == A_IDLE) a.idleT = 1.0f + (pet.rnd() % 300) / 100.0f;
        if (a.act == A_POOP) { sound(SND_SCOOP); }
      } else {
        a.x += dx > 0 ? step : -step;
        a.left = dx < 0;
      }
      break;
    }
    case A_EXIT:
      a.actT -= d;
      a.x += 30 * d;
      if (a.actT <= 0) { a.act = A_HIDDEN; }
      break;
    case A_SLEEP:
      if (((int)(a.t * 1.0f)) % 3 == 0 && (pet.rnd() % 40) == 0) spawn(a.x + 10, a.y - 26, 2, 0, 4, -10, 2.2f);
      break;
    case A_IDLE: {
      a.idleT -= d;
      if (a.idleT > 0) break;
      a.idleT = 2.0f + (pet.rnd() % 400) / 100.0f;
      float r = pet.rndf();
      if (pet.d.resent >= 55 && r < 0.3f) { a.act = A_SULK; a.actT = 6.0f; break; }
      if (pet.d.hunger < 30 && r < 0.45f) { actorWalkTo(BOWL_X - 18, A_MEOW, 1.5f); break; }
      if (pet.d.droppings >= 3 && r < 0.45f) { actorWalkTo(BOX_X + 24, A_SULK, 3.0f); break; }
      const bool hamster = pet.d.breed == BR_HAMSTER, rainbow = pet.d.breed == BR_RAINBOW;
      if (hamster && r < 0.24f) {   // off for a run in the wheel
        a.actLen = 6.0f + pet.rndf() * 7.0f;
        actorWalkTo((float)WHEEL_X, A_WHEEL, a.actLen);
        break;
      }
      if (rainbow && r >= 0.78f && r < 0.88f && pet.rnd() % 100 < 70) {   // floats up to the window and gazes out, hovering
        a.actLen = a.actT = 9.0f + pet.rndf() * 6.0f;
        a.act = A_FLOAT; a.x0 = a.x; a.tx = (float)windowX();
        a.winGlanceAt = pet.rnd() % 100 < 50 ? a.actLen * (0.3f + pet.rndf() * 0.3f) : -1.0f;
        break;
      }
      {   // personality: now and then they do the thing they're like
        const float q = pet.rndf();
        switch (pet.trait) {
          case TRAIT_LAZY: if (q < 0.20f) { a.act = q < 0.10f ? A_YAWN : A_GROOM; a.actT = q < 0.10f ? 1.6f : 4.5f; } break;
          case TRAIT_CURIOUS:
            if (q < 0.10f) { a.act = A_STARE; a.stareLen = a.actT = 3.0f + pet.rndf() * 2.0f; a.stareDir = (pet.rnd() & 1) ? 1 : -1; }
            else if (q < 0.18f) { const float len = 10.0f + pet.rndf() * 8.0f; actorWalkTo((float)windowX(), A_WINDOW, len); a.winTracker = pet.rnd() % 100 < 50; a.winGlanceAt = -1.0f; }
            break;
          case TRAIT_PLAYFUL: if (q < 0.10f && !hamster) { actorWalkTo(roamX(), A_ZOOM, 0.6f); a.speed = 110; } break;
          case TRAIT_FOODIE: if (q < 0.12f && pet.d.hunger < 75) actorWalkTo(BOWL_X - 18, A_MEOW, 1.5f); break;
          case TRAIT_CUDDLY: if (q < 0.12f) { actorWalkTo(92, A_MEOW, 1.2f); } break;   // comes to the front for attention
          default: break;
        }
        if (a.act != A_IDLE) break;
      }
      if (r < 0.40f) actorWalkTo(roamX(), A_IDLE, 0);
      else if (r < 0.62f) { a.act = A_GROOM; a.actT = 3.0f; }
      else if (r < 0.70f) { a.act = A_YAWN; a.actT = 1.6f; }
      else if (r < 0.78f) {   // they quietly freeze and stare at something off screen (touch them to break it)
        a.act = A_STARE; a.stareLen = a.actT = 2.5f + pet.rndf() * 2.0f;
        a.stareDir = (pet.rnd() & 1) ? 1 : -1;
      }
      else if (r < 0.88f) {   // wanders over and watches the world go by out of the window
        const float len = 8.0f + pet.rndf() * 7.0f;
        actorWalkTo((float)windowX(), A_WINDOW, len);
        a.winTracker = pet.rnd() % 100 < 25;                                                // now and then: a bird to follow
        a.winGlanceAt = (!a.winTracker && pet.rnd() % 100 < 60) ? len * (0.3f + pet.rndf() * 0.4f) : -1.0f;   // a little look back at you
      }
      else if (r < 0.97f && (sv.items & ((1u << ITEM_MOUSE) | (1u << ITEM_YARN) | (1u << ITEM_TUNNEL) | (1u << ITEM_WAND)))) {
        static const int tx[4] = {46, 128, TUNNEL_X + 38, 162};   // walk-to spots (minus 12) beside mouse, yarn, tunnel, wand
        int pick[4], n = 0;
        for (int k = 0; k < 4; k++) if (hasItem(ITEM_MOUSE + k)) pick[n++] = k;
        int k = pick[pet.rnd() % n];
        actorWalkTo(tx[k] - 12, A_HAPPY, 2.2f);
        pet.d.fun += 1.5f; if (pet.d.fun > 100) pet.d.fun = 100;
      }
      break;
    }
    case A_HIDDEN:
      break;
    default: {  // timed acts
      a.actT -= d;
      if (a.act == A_EAT) bowl_ = a.actT > 0 ? bowl_ : bowl_;
      if (a.act == A_HAPPY && ((int)(a.t * 8)) % 6 == 0) spawn(a.x + (pet.rnd() % 20) - 10, a.y - 36, 0, 0, 0, -18, 1.0f);
      if (a.act == A_WHEEL) {   // the wheel spins while they run (after the hop in, before the hop out)
        const float el = a.actLen - a.actT;
        if (el > 0.45f && a.actT > 0.45f) { wheelSpin_ += d * (6.0f + 4.0f * pet.agility()); if (((int)(a.t * 6)) % 9 == 0 && pet.rnd() % 4 == 0) sound(SND_TAP); }
        a.left = false;
      }
      if (a.act == A_FLOAT) {   // drift across to the window on the way up
        const float el = a.actLen - a.actT, u = clampf(el / 1.6f, 0, 1), s = u * u * (3 - 2 * u);
        a.x = a.x0 + (a.tx - a.x0) * s;
        a.left = a.tx < a.x0;
        if (((int)(a.t * 10)) % 4 == 0 && pet.rnd() % 3 == 0) {   // a trail of soft sparkles
          static const Color sp[3] = {rgb(255, 190, 230), rgb(180, 230, 255), rgb(255, 250, 200)};
          const float h = clampf(fminf(el, a.actT) / 1.6f, 0, 1);
          spawn(a.x + (float)((int)(pet.rnd() % 24) - 12), a.y - 6 - h * (CAT_BASE_Y - windowBottomY() - 14), 1, sp[pet.rnd() % 3], 0, 10, 0.9f);
        }
      }
      if (a.actT <= 0) {
        if (a.act == A_POOP) {
          if (holdBox_ > 0 && fabsf(a.x - BOX_X) < 3) holdBox_--;
          else if (holdFloor_ > 0) holdFloor_--;
          spawn(a.x, a.y - 4, 5, rgb(180, 150, 100), 0, -6, 0.6f);
        } else if (a.act == A_HACK) {
          if (holdHair_ > 0) holdHair_--;
          spawn(a.x + 14, a.y - 6, 1, rgb(255, 240, 200), 0, -10, 0.7f);
        } else if (a.act == A_MEOW && holdPrey_ > 0) {
          holdPrey_--;
        } else if (a.act == A_SULK && holdPrey_ > 0) {
          holdPrey_--;
        } else if (a.act == A_FLOAT) {
          a.x = a.tx;   // landed under the window
        }
        a.act = A_IDLE;
        a.idleT = 1.0f + (pet.rnd() % 200) / 100.0f;
      }
      break;
    }
  }
  if (holdPrey_ > 0 && a.act != A_MEOW && a.act != A_WALK && a.act != A_SULK) holdPrey_ = 0;
  // never leave a mess hidden: if the cat is not on its way to / in the middle of the act, show everything
  if (a.act != A_POOP && !(a.act == A_WALK && a.after == A_POOP)) { holdBox_ = 0; holdFloor_ = 0; }
  if (a.act != A_HACK && !(a.act == A_WALK && a.after == A_HACK)) holdHair_ = 0;
  if (a.act == A_WALK && a.after == A_MEOW && holdPrey_ <= 0 && false) {}
}

void Game::drawActor(Canvas& cv) {
  const Actor& a = a_;
  if (a.act == A_HIDDEN) return;
  Pose pose = POSE_SIT;
  int m = pet.mood();
  Face face = m == 0 ? FACE_HAPPY : (m == 2 ? FACE_SAD : (m == 3 ? FACE_ANGRY : FACE_NORMAL));
  float yoff = 0;
  switch (a.act) {
    case A_WALK: case A_ENTER: case A_EXIT: pose = POSE_WALK; break;
    case A_ZOOM: pose = POSE_WALK; face = FACE_HAPPY; break;
    case A_EAT: pose = POSE_EAT; face = FACE_EAT; break;
    case A_POOP: pose = POSE_CROUCH; face = FACE_NORMAL; break;
    case A_HACK: pose = POSE_HACK; face = FACE_SAD; break;
    case A_SLEEP: pose = POSE_SLEEP; face = FACE_SLEEP; break;
    case A_GROOM: pose = POSE_SIT; face = FACE_HAPPY; break;
    case A_MEOW: pose = POSE_SIT; face = FACE_MEOW; break;
    case A_YAWN: pose = POSE_SIT; face = FACE_MEOW; break;
    case A_HAPPY: pose = POSE_SIT; face = FACE_HAPPY; yoff = -fabsf(sinf(a.t * 10)) * 5; break;
    case A_SULK: pose = POSE_BACK; break;
    case A_STARE: pose = POSE_SIT; face = FACE_NORMAL; break;
    case A_WINDOW: pose = POSE_BACK; break;
    case A_WHEEL: pose = POSE_WALK; face = FACE_HAPPY; break;
    case A_FLOAT: pose = POSE_SIT; face = FACE_HAPPY; break;
    default: break;
  }
  if (m == 3 && a.act == A_IDLE) pose = POSE_SIT;
  CatDraw d = catLook(pose, face);
  if (a.act == A_GROOM) d.t = a.t;
  if (a.act == A_STARE) { d.look = a.stareDir; d.blink = false; }
  if (pose == POSE_BACK) {   // facing away: window watching or sulking
    const float t = a.t;
    if (a.act == A_WINDOW && a.winTracker) d.backFrame = (int8_t)(4 + (((int)(t * 2.5f)) & 3));                        // tail swish + head tilt
    else if (a.act == A_WINDOW && a.winGlanceAt > 0 && a.actT <= a.winGlanceAt && a.actT > a.winGlanceAt - 1.4f) d.backFrame = 3;   // a glance back at you
    else if (fmodf(t, 3.7f) < 0.25f) d.backFrame = 1;    // an ear swivels
    else if (fmodf(t, 2.3f) < 0.3f) d.backFrame = 2;     // the tail tip flicks
    else d.backFrame = 0;
    d.faceLeft = false;   // the glance looks back over their right shoulder
  }
  int ox = (int)a.x, oy = (int)(a.y + yoff);
  int shadowY = (int)a.y;
  float shadowScale = 1.0f;
  if (a.act == A_WHEEL) {   // hop up into the wheel, run (on the spot: the wheel turns), hop back down
    const float el = a.actLen - a.actT, h = clampf(fminf(el, a.actT) / 0.45f, 0, 1);
    const float runY = WHEEL_Y + WHEEL_R - 3;
    oy = (int)(a.y + (runY - a.y) * h - sinf(h * 3.14159f) * 12);
    d.size *= 1.0f - 0.32f * h;          // a little smaller, to fit inside the wheel
    if (h < 1) d.pose = POSE_POUNCE;     // mid-hop
    else d.t = a.t * 1.8f;               // scampering
    d.faceLeft = false;
    shadowScale = 1.0f - 0.8f * h;
  } else if (a.act == A_FLOAT) {   // rising up to the window, hovering there looking out, floating back down
    const float el = a.actLen - a.actT, h = clampf(fminf(el, a.actT) / 1.6f, 0, 1), s = h * h * (3 - 2 * h);
    const float lift = (float)(CAT_BASE_Y - (windowBottomY() + 14));
    oy = (int)(a.y - s * lift + (h >= 1 ? sinf(a.t * 1.8f) * 2.5f : 0));
    shadowScale = 1.0f - 0.55f * s;
    if (h >= 1) {   // up there: facing the window, sometimes glancing back at you
      d.pose = POSE_BACK; d.faceLeft = false;
      if (a.winGlanceAt > 0 && a.actT <= a.winGlanceAt && a.actT > a.winGlanceAt - 1.4f) d.backFrame = 3;
      else if (fmodf(a.t, 3.1f) < 0.25f) d.backFrame = 1;
      else d.backFrame = 0;
    }
  }
  if (a.act == A_SLEEP && hasItem(ITEM_TREEBED) && fabsf(a.x - TREE_X) < 4) { const int up = 56 + (168 - TREE_Y); oy -= up; shadowY -= up; }   // up in the bed on top of the tree
  else if (a.act == A_SLEEP && sv.boxType && !hasItem(ITEM_BED) && !hasItem(ITEM_TREEBED) && fabsf(a.x - BOX2_X) < 4) { static const int BH[5] = {0, 12, 13, 16, 18}; oy = 180 - BH[sv.boxType] + 5; shadowY = oy + 40; }
  if (a.act == A_POOP && fabsf(a.x - BOX_X) < 4) { oy -= 3; shadowY -= 3; }
  { CatDraw sd = d; sd.size *= shadowScale; if (a.act == A_WHEEL && shadowScale < 0.5f) sd.size = 0; if (sd.size > 0) drawCatShadow(cv, ox, shadowY, sd, rgb(184, 132, 90)); }
  CatMetrics cm = drawCat(cv, ox, oy, d);
  if (a.act == A_WHEEL) drawWheel(cv, true);   // the near rim goes over them
  if (a.act == A_GROOM) {  // paw lick sparkle
    int s = (int)(a.t * 6) % 2;
    cv.set(cm.headX + (a.left ? -6 : 6), cm.headY + 4 + s, rgb(255, 255, 255));
  }
  if (bubbleT_ > 0 && a.act == A_MEOW) drawBubble(cv, cm.headX, cm.topY - 5, bubble_);
  else if (a.act == A_SULK && pet.d.resent >= 55 && !holdPrey_) drawBubble(cv, cm.headX, cm.topY - 5, "...");

}

// ---------------------------------------------------------------------------------------
// the home screen
// ---------------------------------------------------------------------------------------

void Game::drawHome(Canvas& cv, bool interactive) {
  ensureRoom();
  updateActor();
  if (interactive) updateMouse();
  updateParticles(dt);
  drawRoom(cv);

  int boxDrops = pet.d.droppings - holdBox_;
  if (boxDrops < 0) boxDrops = 0;
  bool catInBox = a_.act == A_POOP && fabsf(a_.x - BOX_X) < 4;

  // messes, hairballs, gift
  int messes = pet.d.floorMess - holdFloor_;
  for (int i = 0; i < messes; i++) {
    int fx, fy;
    drawFloorX(i, fx, fy);
    drawFloorPoop(cv, fx, fy, anim_, i);   // the same lump as on the Box game's floor
    if (((int)(anim_ * 2)) % 2 == 0) cv.text(fx - 2, fy - 18, "!", rgb(255, 120, 90));   // tap me
  }
  int hairs = pet.d.hairballsOnFloor - holdHair_;
  for (int i = 0; i < hairs; i++) {
    static const int hx[3] = {40, 70, 150}, hy[3] = {172, 175, 172};
    // it turns into a mess after 30 min: for the last 10 it wobbles and the marker goes red
    const bool old = pet.d.hbAge > 1200.0f;
    const int wob = old ? (int)(sinf(anim_ * 14 + i) * 1.5f) : 0;
    const int x = hx[i] + wob, y = hy[i];
    const float pulse = 0.5f + 0.5f * sinf(anim_ * 4 + i);
    cv.ellipse(x, y + 5, 7, 2, rgb(40, 30, 30));                                              // contact shadow
    cv.ring(x, y, 9 + (int)(pulse * 2), old ? rgb(255, 110, 90) : rgb(255, 226, 120));         // glow ring
    drawHairball(cv, x, y, 6);
    if (((int)(anim_ * 3) + i) % 4 == 0) cv.set(x + 5, y - 7, rgb(255, 255, 255));            // sparkle
    const int by = y - 22 - (int)(pulse * 2);                                                  // bobbing "tap me" marker
    const Color mc = old ? rgb(255, 110, 90) : rgb(255, 226, 120);
    cv.tri(x - 4, by, x + 4, by, x, by + 5, mc);
    if (old && ((int)(anim_ * 3)) % 2 == 0) cv.text(x - 2, by - 10, "!", mc);
  }
  bool prey = pet.d.preyOnFloor && holdPrey_ <= 0;
  if (prey) {
    int by = (int)(sinf(anim_ * 6) > 0.8f ? -1 : 0);
    drawPrey(cv, pet.d.preyOnFloor, 112, 168 + by);
    cv.text(110, 150, "!", rgb(255, 220, 80));
  }

  // furniture and toys (tree is drawn behind the cat, the rest sit on the floor)
  // the passive toys live at the back of the room (drawn first, so the cat and the furniture pass in front of them)
  if (hasItem(ITEM_MOUSE)) drawItem(cv, ITEM_MOUSE, 20, 138);     // against the back wall, behind the litter box
  if (hasItem(ITEM_WAND)) drawItem(cv, ITEM_WAND, 166, 146);      // propped in the far right corner
  if (hasItem(ITEM_YARN)) {   // the yarn ball has rolled to the back of the room: under the little table, behind everything
    const RoomImg::Sprite& tb = ROOMS[sv.room < NUM_ROOMS ? sv.room : 0].table;
    if (tb.w) drawItem(cv, ITEM_YARN, (tb.x + tb.w / 2) / 2, (ROOM_TOP + tb.y + tb.h) / 2 - 10);
    else drawItem(cv, ITEM_YARN, 120, 134);
  }
  // the tree stands at the back, against the wall: left of the middle, its trunk and step just right of the mouse hole
  if (hasItem(ITEM_TREEBED)) drawItem(cv, ITEM_TREEBED, TREE_X, TREE_Y); else if (hasItem(ITEM_TREE)) drawItem(cv, ITEM_TREE, TREE_X, TREE_Y);
  if (hasItem(ITEM_BED)) drawItem(cv, ITEM_BED, BED_X, 172);
  const bool lchair = hasItem(ITEM_LCHAIR);   // the comfy one replaces the ripped one; it stands front left, over the litter box's corner
  if (!lchair && hasItem(ITEM_RCHAIR)) drawItem(cv, ITEM_RCHAIR, CHAIR_X, CHAIR_Y);
  if (hasItem(ITEM_TUNNEL)) drawItem(cv, ITEM_TUNNEL, TUNNEL_X, TUNNEL_Y);   // mid screen, on the floor in front of the tree's foot
  if (pet.d.breed == BR_HAMSTER) drawWheel(cv, false);   // their wheel comes along to every apartment (the box, nearer, goes in front of it)
  bool inBox = sv.boxType && a_.act == A_SLEEP && fabsf(a_.x - BOX2_X) < 4 && !hasItem(ITEM_BED) && !hasItem(ITEM_TREEBED);
  if (sv.boxType) drawBoxProp(cv, sv.boxType, BOX2_X, 180, inBox ? 1 : 0);
  if (!catInBox) { drawBox(cv, BOX_X, 160, boxDrops); if (lchair) drawItem(cv, ITEM_LCHAIR, LCHAIR_X, LCHAIR_Y); }
  drawSeasonalDecor(cv);
  drawGiftBox(cv);
  drawActor(cv);
  drawMouse(cv);
  if (inBox) drawBoxProp(cv, sv.boxType, BOX2_X, 180, 2);
  if (catInBox) { drawBox(cv, BOX_X, 160, boxDrops); if (lchair) drawItem(cv, ITEM_LCHAIR, LCHAIR_X, LCHAIR_Y); }   // (the chair is nearer than the box)

  // night: the room cache is already dark; tint what is drawn on top (cats, props) to match
  float n = nightness(hour, minute);
  fi.tintR = (int)(100 - 42 * n); fi.tintG = (int)(100 - 36 * n); fi.tintB = (int)(100 - 14 * n);

  drawParticles(cv);
  drawHud(cv);
  if (interactive && batOk_ && tapIn(W / 2 - 17, 0, 68, 17)) setScreen(S_BATTERY);   // the clock + battery: battery details
  drawToolbar(cv, interactive);
  drawToasts(cv);
  if (interactive) homeInput();
}

void Game::homeInput() {
  // hold on the cat = petting
  bool onCat = in.down && in.py >= HUD_H && in.py < TOOL_Y && a_.act != A_HIDDEN &&
               fabsf(in.px - a_.x) < 24 && in.py > a_.y - 46 * pet.visualSize() - 6 && in.py < a_.y + 6;
  if (onCat && !pet.sleeping && in.heldMs > 450 && in.px > 0) {
    petHold_ += dt;
    if (a_.act != A_HAPPY && a_.act != A_WALK) { a_.act = A_HAPPY; a_.actT = 0.6f; }
    else if (a_.act == A_HAPPY) a_.actT = 0.6f;
    if (petHold_ >= 1.0f) {
      petHold_ = 0;
      pet.petIt(1.5f);
      // a hairless cat (Sphynx) never coughs up hairballs, so a good petting session earns one instead: at most one
      // every 3 hours (about what a furry cat coughs up: one every 2-5 h, ~7 a day)
      if (BREEDS[pet.d.breed].hair <= 0.0f && (sv.x8.petHbAt == 0 || epoch - sv.x8.petHbAt >= PET_HB_EVERY || epoch < sv.x8.petHbAt)) {
        sv.x8.petHbAt = epoch ? epoch : 1;
        giveCoins(1);
        sv.lifeHairballs++;
        toast("Soft and warm! +1 hairball");
        sound(SND_COIN);
        spawn(a_.x, a_.y - 30, 3, 0, 0, -35, 0.8f);
        recordMilestone(MS_HAIRBALL);
        autosave(true);
      }
      spawn(a_.x + (float)((int)(pet.rnd() % 18) - 9), a_.y - 38, 0, 0, 0, -20, 1.1f);
      sound(SND_PURR);
    }
  } else {
    petHold_ = 0;
  }
  // Everything tappable in the room registers as a touch target (tapIn), so a tap near the toolbar snaps to the
  // nearest thing (a hairball right above PLAY used to open PLAY instead).
  int x = in.x, y = in.y;
  const bool inRoom = in.tap && y >= HUD_H && y < TOOL_Y;
  // the mouse
  if (mouse_.st >= 1 && mouse_.st <= 3 && tapIn((int)mouse_.x - 12, mouseRunY() - 17, 24, 22) && inRoom) {
    if (pet.sleeping || a_.act == A_SLEEP || a_.act == A_EAT || a_.act == A_POOP || a_.act == A_HACK || a_.act == A_HIDDEN || a_.act == A_EXIT || a_.act == A_ENTER) {
      toast("%s is busy right now.", pet.d.name);
    } else {
      mouse_.st = 4; mouse_.t = 0;
      actorWalkTo(mouse_.x, A_HAPPY, 0.5f);
      a_.speed *= 2.2f;   // a mad dash
      sound(SND_TAP);
    }
    return;
  }
  // a gift box waiting to be opened
  if (sv.pad9[0] && tapIn(GIFT_X - 13, GIFT_Y - 24, 26, 28) && inRoom) { openGift(); return; }
  // the catch they brought home
  if (pet.d.preyOnFloor && holdPrey_ <= 0 && tapIn(96, 152, 32, 28) && inRoom) {
    int v = preyValue(pet.d.preyOnFloor);
    toast("%s! +%d hairballs", preyName(pet.d.preyOnFloor), v);
    giveCoins(v);
    pet.takePrey();
    recordMilestone(MS_GIFT);
    pet.petIt(6);
    sound(SND_COIN);
    for (int i = 0; i < 4; i++) spawn(112 + i * 4 - 6, 160, 3, 0, (i - 2) * 10.0f, -40, 0.8f);
    autosave(true);
    return;
  }
  // hairballs
  int hairs = pet.d.hairballsOnFloor - holdHair_;
  for (int i = 0; i < hairs; i++) {
    static const int hx[3] = {40, 70, 150}, hy[3] = {172, 175, 172};
    if (tapIn(hx[i] - 13, hy[i] - 13, 27, 27) && inRoom) {
      pet.collectHairball();
      giveCoins(1);
      recordMilestone(MS_HAIRBALL);
      sv.lifeHairballs++;
      sound(SND_COIN);
      spawn((float)hx[i], (float)hy[i] - 4, 3, 0, 0, -35, 0.7f);
      return;
    }
  }
  // floor mess
  int messes = pet.d.floorMess - holdFloor_;
  for (int i = 0; i < messes; i++) {
    int fx, fy;
    drawFloorX(i, fx, fy);
    if (tapIn(fx - 11, fy - 9, 23, 19) && inRoom) {
      pet.cleanFloor();
      sound(SND_SCOOP);
      spawn((float)fx, (float)fy - 3, 5, rgb(200, 220, 255), 0, -4, 0.6f);
      spawn((float)fx, (float)fy - 3, 1, rgb(160, 240, 255), 4, -14, 0.7f);
      pet.petIt(1.0f);
      return;
    }
  }
  // cat's box -> Magic Cat
  if (sv.boxType && tapIn(BOX2_X - 21, 161, 43, 23) && inRoom) { startMagic(); return; }
  { const int ct = (int)(a_.y - 46 * pet.visualSize() - 6);   // the cat moves: register it (so taps near it aren't
    if (a_.act != A_HIDDEN) tapIn((int)a_.x - 23, ct, 47, (int)a_.y + 6 - ct);   // snapped elsewhere), test it below
  }
  if (!inRoom) return;
  // tap the cat
  if (a_.act != A_HIDDEN && fabsf(x - a_.x) < 24 && y > a_.y - 46 * pet.visualSize() - 6 && y < a_.y + 6) {
    if (pet.sleeping) {
      toast("Shh... %s is sleeping.", pet.d.name);
      spawn(a_.x + 8, a_.y - 24, 2, 0, 4, -10, 2.0f);
    } else {
      pet.petIt(2.0f);
      sound(SND_MEOW_SOFT);
      if (a_.act == A_IDLE || a_.act == A_GROOM || a_.act == A_SULK || a_.act == A_STARE || a_.act == A_WINDOW || a_.act == A_WHEEL || a_.act == A_FLOAT) { a_.act = A_HAPPY; a_.actT = 1.4f; }   // (petting snaps them out of a stare too)
      for (int i = 0; i < 2; i++) spawn(a_.x + (i * 10 - 5), a_.y - 36, 0, 0, 0, -22, 1.0f);
    }
    return;
  }
}

}  // namespace cat
