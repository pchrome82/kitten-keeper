// Menus, shop, settings, litter box, endings.
#include "game.h"
#include "gfx.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace cat {

static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

static const Color BG = rgb(36, 28, 58);
static const Color PANEL = rgb(58, 44, 88);
static const Color BORDER = rgb(130, 100, 170);
static const Color TXT = rgb(236, 228, 250);
static const Color DIM = rgb(160, 146, 190);
static const Color GOLD = rgb(250, 230, 150);
static const Color GREEN = rgb(60, 150, 90);
static const Color BLUE = rgb(70, 110, 200);
static const Color RED = rgb(190, 70, 80);
static const Color PURPLE = rgb(110, 80, 160);

// ---------------------------------------------------------------------------------------
// widgets
// ---------------------------------------------------------------------------------------

// Touch targets are a little bigger than they look: a bit of slack all round, and anything near the
// top or bottom edge reaches all the way to it (fingers there land partly off the glass and read jittery).
// The finger may drift a bit while lifting, so the release point gets extra slack.
void touchBox(int& x, int& y, int& w, int& h) {
  x -= 1; w += 2; y -= 2; h += 4;   // 1 px sideways so tightly packed rows never overlap
  if (x <= 9) { w += x + 4; x = -4; }                   // the corners of the glass are hard to hit: left and right edges reach out to the edge too
  if (x + w >= W - 9) w = W - x + 4;
  if (y + h >= H - 16) { h = H - y + 8 + 4; y -= 4; }   // near the bottom edge: reach the edge and a little higher
  if (y <= 8) { h += y + 4; y = -4; }                   // near the top edge: reach right up to (and past) it
}
static void hitBox(int& x, int& y, int& w, int& h) { touchBox(x, y, w, h); }

bool Game::tapIn(int x, int y, int w, int h) const {
  if (nHits_ < MAX_HITS) hits_[nHits_++] = {(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h};   // remember this target
  if (!in.tap) return false;
  if (tapMode_ == 1) return x == tapWin_.x && y == tapWin_.y && w == tapWin_.w && h == tapWin_.h;   // the nearest target only
  hitBox(x, y, w, h);
  const int s = 8;
  return in.px >= x && in.px < x + w && in.py >= y && in.py < y + h &&
         in.x >= x - s && in.x < x + w + s && in.y >= y - s && in.y < y + h + s;
}

bool Game::button(Canvas& cv, int x, int y, int w, int h, const char* label, Color fill, bool enabled, Color textCol) {
  int hx = x, hy = y, hw = w, hh = h; hitBox(hx, hy, hw, hh);
  bool press = enabled && in.down && in.px >= hx && in.px < hx + hw && in.py >= hy && in.py < hy + hh;
  glossBtn(cv, x, y, w, h, enabled ? fill : rgb(78, 70, 96), press);
  int tw = Canvas::textWidth(label);
  int ty = y + (h - 3 - 7) / 2 + (press ? 2 : 0);
  Color tc = enabled ? textCol : rgb(140, 132, 160);
  if (enabled) cv.text(x + (w - tw) / 2 + 1, ty + 1, label, shade(fill, 40));
  cv.text(x + (w - tw) / 2, ty, label, tc);
  if (enabled && tapIn(x, y, w, h)) { sound(strcmp(label, "BACK") == 0 ? SND_BACK : SND_TAP); return true; }
  return false;
}

bool Game::backButton(Canvas& cv) {
  return button(cv, CORNER - 2, 3, 38, 15, "BACK", rgb(110, 80, 150));
}

void Game::panelTitle(Canvas& cv, const char* title, int y) {
  cv.textCenter(W / 2, y, title, GOLD, 1);
  cv.hline(W / 2 - 40, y + 10, 80, BORDER);
}

void Game::drawBackdrop(Canvas& cv, bool dim) {
  (void)dim;
  cv.clear(BG);
  for (int y = 0; y < H; y += 16)
    for (int x = ((y / 16) % 2) * 16; x < W; x += 32) cv.set(x + 4, y + 4, rgb(52, 42, 78));
}

bool Game::stepper(Canvas& cv, int x, int y, int w, const char* label, int& val, int lo, int hi, int step, bool wrap) {
  static float acc = 0;
  cv.text(x, y + 8, label, TXT);
  int bx = x + w - 88;
  bool changed = false;
  char b[16];
  snprintf(b, sizeof(b), "%d", val);
  bool dn = button(cv, bx, y, 24, 22, "-", PURPLE);
  bool up = button(cv, bx + 64, y, 24, 22, "+", PURPLE);
  cv.textCenter(bx + 44, y + 8, b, GOLD);
  bool holdDn = in.down && in.px >= bx && in.px < bx + 24 && in.py >= y && in.py < y + 22 && in.heldMs > 450;
  bool holdUp = in.down && in.px >= bx + 64 && in.px < bx + 88 && in.py >= y && in.py < y + 22 && in.heldMs > 450;
  if (holdDn || holdUp) {
    acc += dt;
    if (acc > 0.09f) { acc = 0; if (holdDn) dn = true; else up = true; }
  }
  if (dn) { val -= step; changed = true; }
  if (up) { val += step; changed = true; }
  if (val < lo) val = wrap ? hi : lo;
  if (val > hi) val = wrap ? lo : hi;
  return changed;
}

// The title screen (first run / after a reset): logo, a kitten, a little parade, and START.
// The title screen's "Neon Night" sky: violet gradient, twinkling stars, drifting aurora ribbons, a crescent moon and
// (optionally) the city rooftops. Shared by the title and the breed picker so they match.
static void drawNeonSky(Canvas& cv, float anim_, int auroraY, bool roofs, int moonX = 156, int moonY = 112) {
  // "Neon Night": a deep violet sky, twinkling stars, glowing aurora ribbons, neon logo, city rooftops
  for (int y = 0; y < H; y++) {
    const int t = y * 512 / (H - 1);   // 0..512 across two colour stops
    const Color c = t < 256 ? mix(rgb(14, 10, 40), rgb(40, 20, 90), t) : mix(rgb(40, 20, 90), rgb(90, 30, 120), t - 256);
    cv.hline(0, y, W, c);
  }
  for (int i = 0; i < 40; i++) {
    const uint32_t h = (uint32_t)(i + 1) * 2654435761u;
    const int x = (int)(h % W), y = (int)((h >> 8) % 150);
    const float tw = sinf(anim_ * (1.4f + (i % 5) * 0.3f) + i);
    if (tw > 0) cv.set(x, y, tw > 0.7f ? rgb(255, 255, 230) : rgb(150, 150, 210));
    if (tw > 0.95f) { const Color g = rgb(170, 160, 215); cv.set(x - 1, y, g); cv.set(x + 1, y, g); cv.set(x, y - 1, g); cv.set(x, y + 1, g); }
  }
  {   // aurora ribbons that drift slowly
    static const uint8_t col[3][3] = {{60, 230, 170}, {90, 160, 255}, {230, 90, 220}};
    static const int amp[3] = {6, 5, 4}; const int off[3] = {auroraY, auroraY + 10, auroraY + 20};
    for (int b = 0; b < 3; b++)
      for (int x = 0; x < W; x++) {
        const int y = off[b] + (int)(amp[b] * sinf(x / 20.0f + anim_ * 0.8f + b));
        for (int k = 0; k < 3; k++) {
          const float f = 0.55f - 0.15f * k;
          cv.set(x, y + k, rgb((int)(col[b][0] * f) + 20, (int)(col[b][1] * f) + 20, (int)(col[b][2] * f) + 20));
        }
      }
  }
  cv.disc(moonX, moonY, 10, rgb(255, 246, 200)); cv.disc(moonX + 5, moonY - 4, 9, mix(rgb(14, 10, 40), rgb(40, 20, 90), moonY * 512 / (H - 1) > 256 ? 256 : moonY * 512 / (H - 1)));   // crescent moon
  if (roofs) {
  {   // rooftops with lit windows
    static const int bx[5] = {0, 38, 66, 116, 144}, bw[5] = {40, 30, 52, 30, 40}, bh[5] = {40, 28, 22, 34, 26};
    static const int wOff[5] = {0, 0, 0, 12, 0};   // the 4th building's lit windows sit over at its right side, clear of the kitten's name
    for (int k = 0; k < 5; k++) {
      const int top = H - 20 - bh[k];
      cv.rect(bx[k], top, bw[k], H - top, rgb(26, 16, 50));
      for (int wy = top + 6; wy < H - 30; wy += 9)
        for (int wx = bx[k] + 4; wx < bx[k] + bw[k] - 4; wx += 8)
          if (((wx * 7 + wy) % 5) < 2) cv.rect(wx + wOff[k], wy, 3, 4, rgb(255, 214, 120));
    }
  }
  }
}

void Game::screenMode(Canvas& cv) {
  drawNeonSky(cv, anim_, 70, true);
  // neon logo: cyan KITTEN, magenta KEEPER, each with a glow outline, bobbing in a wave
  auto word = [&](const char* w, int y, float ph, Color fill, Color glow) {
    const int n = (int)strlen(w), x0 = W / 2 - Canvas::textWidth(w, 3) / 2;
    for (int i = 0; i < n; i++) {
      const char c[2] = {w[i], 0};
      const int x = x0 + i * 18, yy = y + (int)roundf(sinf(anim_ * 3.0f + i * 0.6f + ph) * 1.2f);
      for (int ox = -2; ox <= 2; ox++)
        for (int oy = -2; oy <= 2; oy++)
          if (ox || oy) cv.text(x + ox, yy + oy, c, glow, 3);
      cv.text(x, yy, c, fill, 3);
    }
  };
  word("KITTEN", 10, 0.0f, rgb(140, 255, 250), rgb(30, 160, 200));
  word("KEEPER", 46, 1.6f, rgb(255, 150, 245), rgb(170, 40, 170));
  // the star of the show: a kitten, hearts floating up
  cv.ellipse(92, 162, 22, 3, rgb(14, 8, 30));
  CatDraw d;
  d.breed = 0; d.pose = POSE_SIT; d.size = 0.9f; d.kit = 0.9f; d.face = FACE_HAPPY; d.t = anim_;
  d.blink = ((int)(anim_ * 2.2f) % 7) == 0;
  drawCat(cv, 92, 162, d);
  static const Color hcol[3] = {rgb(255, 150, 245), rgb(140, 255, 250), rgb(255, 240, 160)};
  for (int k = 0; k < 3; k++) {
    const float p = fmodf(anim_ * 0.45f + k / 3.0f, 1.0f);
    if (p > 0.85f) continue;
    const int hx = 92 + (int)(sinf(p * 9.0f + k * 2.0f) * 14) + (k - 1) * 18, hy = 124 - (int)(p * 34);
    cv.disc(hx - 2, hy, 2, hcol[k]); cv.disc(hx + 2, hy, 2, hcol[k]); cv.tri(hx - 4, hy + 1, hx + 4, hy + 1, hx, hy + 5, hcol[k]);
  }
  // a little parade trotting along the rooftops
  for (int k = 0; k < 3; k++) {
    static const int breeds[3] = {0, 2, 4};
    const float sp = 22.0f + k * 5.0f;
    const int x = (int)fmodf(anim_ * sp + k * 75.0f, (float)(W + 60)) - 30;
    CatDraw c; c.breed = breeds[k]; c.pose = POSE_WALK; c.size = 0.42f; c.t = anim_ + k; c.fat = k == 2 ? 0.6f : 0.1f;
    drawCat(cv, x, 182, c);
  }
  // START (a tap anywhere works too)
  const bool go = button(cv, 30, 190, 124, 22, "START", rgb(200, 70, 200)) || in.tap;
  if (((int)(anim_ * 1.5f)) & 1) { cv.disc(36, 201, 1, rgb(255, 255, 220)); cv.disc(148, 201, 1, rgb(255, 255, 220)); }
  cv.textCenter(W / 2, 215, "v1.3.2", rgb(140, 120, 180));
  if (go) {
    sv.mode = MODE_EASY;
    { DateTime t = fromEpoch(epoch); clockSel_[0] = t.year; clockSel_[1] = t.month; clockSel_[2] = t.day; clockSel_[3] = t.hour; clockSel_[4] = t.minute; }
    clockNext_ = S_NAME;
    sound(SND_BLOOP);
    setScreen(sv.clockSet && clockOk ? S_NAME : S_CLOCK);
  }
}

static int daysInMonth(int y, int m) {
  static const int dm[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int d = dm[(m - 1) % 12];
  if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) d = 29;
  return d;
}

void Game::screenClock(Canvas& cv) {
  drawBackdrop(cv, false);
  panelTitle(cv, "SET THE CLOCK", 8);
  cv.textCenter(W / 2, 24, "Used for the alarm & day/night", DIM);
  int* c = clockSel_;
  stepper(cv, 10, 40, 164, "Year", c[0], 2025, 2099);
  stepper(cv, 10, 68, 164, "Month", c[1], 1, 12, 1, true);
  int dim = daysInMonth(c[0], c[1]);
  if (c[2] > dim) c[2] = dim;
  stepper(cv, 10, 96, 164, "Day", c[2], 1, dim, 1, true);
  stepper(cv, 10, 124, 164, "Hour", c[3], 0, 23, 1, true);
  stepper(cv, 10, 152, 164, "Minute", c[4], 0, 59, 1, true);
  char b[24];
  snprintf(b, sizeof(b), "%04d-%02d-%02d  %02d:%02d", c[0], c[1], c[2], c[3], c[4]);
  cv.textCenter(W / 2, 180, b, GOLD);
  bool cancel = (clockNext_ == S_SETTINGS);
  if (button(cv, cancel ? 96 : 24, 194, cancel ? 78 : 136, 26, "SAVE", GREEN)) {
    DateTime d = {(uint16_t)c[0], (uint8_t)c[1], (uint8_t)c[2], (uint8_t)c[3], (uint8_t)c[4], 0};
    plat->rtcSet(d);
    sv.clockSet = 1;
    turboOffset_ = 0;
    sv.fallbackClock = toEpoch(d) - nowMs / 1000;
    updateClock();
    autosave(true);
    setScreen(clockNext_);
  }
  if (cancel && button(cv, 10, 194, 78, 26, "CANCEL", PURPLE)) setScreen(S_SETTINGS);
}

void Game::screenBday(Canvas& cv) {
  drawBackdrop(cv, false);
  panelTitle(cv, "BIRTHDAY", 8);
  cv.textCenter(W / 2, 28, pet.d.name, GOLD);
  cv.textCenter(W / 2, 42, "Pick the party day!", DIM);
  int* c = bdaySel_;
  stepper(cv, 10, 62, 164, "Month", c[0], 1, 12, 1, true);
  const int dim = daysInMonth(2024, c[0]);   // (a leap year: February 29th is allowed)
  if (c[1] > dim) c[1] = dim;
  stepper(cv, 10, 92, 164, "Day", c[1], 1, dim, 1, true);
  static const char* MN[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
  char b[40];
  snprintf(b, sizeof(b), "%s %d", MN[c[0] - 1], c[1]);
  cv.textCenter(W / 2, 130, b, rgb(255, 240, 160));
  cv.textCenter(W / 2, 148, "A party every year, with a", DIM);
  cv.textCenter(W / 2, 158, "hat, confetti and a gift!", DIM);
  if (button(cv, 10, 190, 78, 26, "CANCEL", PURPLE)) setScreen(S_SETTINGS);
  if (button(cv, 96, 190, 78, 26, "SAVE", GREEN)) {
    sv.x8.bdayMonth = (uint8_t)c[0]; sv.x8.bdayDay = (uint8_t)c[1];
    toast("Birthday set!");
    autosave(true);
    setScreen(S_SETTINGS);
  }
  drawToasts(cv);
}

// The on-screen keyboard: type any name up to 11 letters (capitals follow the start of each word; ABC toggles the next letter).
void Game::screenNameKb(Canvas& cv) {
  drawNeonSky(cv, anim_, 22, true, 160, 30);
  cv.textCenter(W / 2, 6, "NAME YOUR KITTEN", rgb(140, 255, 250));
  const int cap = (int)sizeof(nameBuf_) - 1;
  int len = (int)strlen(nameBuf_);
  cv.panel(10, 20, 164, 26, rgb(30, 18, 60), rgb(110, 80, 170));
  { // the name is drawn on its own (centred); the blinking cursor sits after it, so the text never shifts
    cv.textCenter(W / 2, 26, nameBuf_, GOLD, 2);
    if (len < cap && ((int)(anim_ * 2) & 1)) cv.text(W / 2 + Canvas::textWidth(nameBuf_, 2) / 2 + 1, 26, "_", GOLD, 2); }
  auto addc = [&](char ch) {
    if (len >= cap) { sound(SND_MISS); return; }
    nameBuf_[len++] = ch; nameBuf_[len] = 0;
    kbShift_ = (ch == ' ' || ch == '.');   // the next word starts with a capital
  };
  static const char* ROW1 = "qwertyuiop"; static const char* ROW2 = "asdfghjkl"; static const char* ROW3 = "zxcvbnm";
  const Color keyc = rgb(96, 70, 150), spec = rgb(70, 110, 200);
  char lab[2] = {0, 0};
  for (int i = 0; i < 10; i++) {
    lab[0] = kbShift_ ? (char)(ROW1[i] - 32) : ROW1[i];
    if (button(cv, 2 + i * 18, 54, 17, 26, lab, keyc)) addc(lab[0]);
  }
  for (int i = 0; i < 9; i++) {
    lab[0] = kbShift_ ? (char)(ROW2[i] - 32) : ROW2[i];
    if (button(cv, 11 + i * 18, 82, 17, 26, lab, keyc)) addc(lab[0]);
  }
  if (button(cv, 2, 110, 26, 26, kbShift_ ? "ABC" : "abc", kbShift_ ? rgb(220, 150, 60) : spec)) kbShift_ = !kbShift_;
  for (int i = 0; i < 7; i++) {
    lab[0] = kbShift_ ? (char)(ROW3[i] - 32) : ROW3[i];
    if (button(cv, 30 + i * 18, 110, 17, 26, lab, keyc)) addc(lab[0]);
  }
  if (button(cv, 157, 110, 25, 26, "<-", rgb(190, 70, 80)) && len > 0) { nameBuf_[--len] = 0; if (len == 0) kbShift_ = true; }
  if (button(cv, 2, 138, 34, 26, ".", spec)) addc('.');
  if (button(cv, 38, 138, 108, 26, "SPACE", spec)) { if (len > 0 && nameBuf_[len - 1] != ' ') addc(' '); }
  if (button(cv, 148, 138, 34, 26, "-", spec)) addc('-');
  if (button(cv, 8, 172, 80, 28, "RANDOM", rgb(110, 80, 160))) { stashName(); strncpy(nameBuf_, randomName(), cap); nameBuf_[cap] = 0; }
  if (button(cv, 96, 172, 80, 28, "OK", GREEN)) {
    int n = (int)strlen(nameBuf_);
    while (n > 0 && nameBuf_[n - 1] == ' ') nameBuf_[--n] = 0;      // no trailing spaces
    if (n == 0) { strncpy(nameBuf_, randomName(), cap); nameBuf_[cap] = 0; toast("Picked one for you!"); }
    else stashName();
    setScreen(S_NAME);
  }
  drawToasts(cv);
}

void Game::screenName(Canvas& cv) {
  // the same Neon Night as the title screen
  drawNeonSky(cv, anim_, 22, true, 160, 30);
  {   // neon title, glowing like the logo
    const char* t = "A KITTEN NEEDS A HOME!";
    const int x0 = W / 2 - Canvas::textWidth(t) / 2;
    for (int ox = -1; ox <= 1; ox++) for (int oy = -1; oy <= 1; oy++) if (ox || oy) cv.text(x0 + ox, 6 + oy, t, rgb(30, 160, 200));
    cv.text(x0, 6, t, rgb(140, 255, 250));
    cv.textCenter(W / 2, 17, "Pick your kitten", rgb(200, 190, 230));
  }
  uint8_t& sel = sv.pad[0];                         // 0..6 breeds, 8/9 = secret breeds (7, the old "surprise me", is gone)
  if (sel >= NUM_BREED_SLOTS || sel == BR_KITTEN) sel = 0;
  // the arrows first: a press changes the pick BEFORE anything is drawn, so a locked secret never shows its name
  for (int dir = -1; dir <= 1; dir += 2) {
    if (button(cv, dir < 0 ? 4 : 146, 36, 34, 44, dir < 0 ? "<" : ">", rgb(170, 60, 180))) {
      for (int k = 0; k < NUM_BREED_SLOTS; k++) {
        sel = (uint8_t)((sel + dir + NUM_BREED_SLOTS) % NUM_BREED_SLOTS);
        if (sel == BR_KITTEN) continue;                      // (no more "let it evolve")
        break;                                               // every breed shows: the ones you don't own can be bought right here
      }
      sound(SND_TAP);
    }
  }
  const bool mystery = isSecretBreed(sel) && !(sv.unlocked & (1u << sel));   // a secret not found yet: shadow only
  // the kitten, on a glowing spot
  CatDraw d;
  d.breed = sel; d.pose = POSE_FRONT; d.size = 0.9f; d.kit = 0.0f; d.fat = 0.2f; d.face = FACE_HAPPY; d.t = anim_;
  d.blink = ((int)(anim_ * 2.2f) % 7) == 0;
  if (mystery) { d.silhouette = rgb(14, 10, 26); d.face = FACE_NORMAL; }
  cv.ellipse(92, 80, 36, 6, rgb(60, 30, 100)); cv.ellipse(92, 80, 28, 4, rgb(90, 50, 140));
  drawCat(cv, 92, 80, d);
  // the breed's name, big and neon
  {
    const char* nm = mystery ? "????" : BREEDS[sel].name;
    const Color fill = isSecretBreed(sel) ? rgb(255, 150, 245) : rgb(140, 255, 250);
    const Color glow = isSecretBreed(sel) ? rgb(170, 40, 170) : rgb(30, 160, 200);
    const int x0 = W / 2 - Canvas::textWidth(nm, 2) / 2;
    for (int ox = -1; ox <= 1; ox++) for (int oy = -1; oy <= 1; oy++) if (ox || oy) cv.text(x0 + ox, 88 + oy, nm, glow, 2);
    cv.text(x0, 88, nm, fill, 2);
  }
  // its stats: hair (how often they cough up a hairball), speed, strength
  cv.panel(14, 106, 156, 40, rgb(30, 18, 60), rgb(110, 80, 170));
  {
    const Breed& b = BREEDS[sel];
    const float strength = b.strength;   // (as Pet::power)
    struct Row { const char* label; float v; Color c; } rows[3] = {
      {"Hair", b.hair, rgb(250, 200, 110)}, {"Speed", b.speed, rgb(110, 220, 255)}, {"Strength", strength, rgb(255, 130, 170)}};
    for (int i = 0; i < 3; i++) {
      const int y = 111 + i * 11;
      cv.text(20, y, rows[i].label, rgb(210, 200, 235));
      const int bx = 74, bw = 64;
      cv.rect(bx, y + 1, bw, 6, rgb(50, 36, 86));
      if (!mystery) cv.rect(bx, y + 1, (int)(bw * (rows[i].v / 1.5f > 1 ? 1 : rows[i].v / 1.5f)), 6, rows[i].c);
      char v[8]; if (mystery) snprintf(v, sizeof(v), "???"); else snprintf(v, sizeof(v), "%d", (int)(rows[i].v * 100 + 0.5f));
      cv.textRight(164, y, v, mystery ? rgb(150, 136, 180) : rgb(236, 228, 250));
    }
  }
  // the kitten's name
  cv.textCenter(W / 2, 148, nameBuf_, GOLD, 2);
  const bool canUndo = customName_[0] && strcmp(customName_, nameBuf_) != 0;
  const int bw = canUndo ? 53 : 82;
  if (button(cv, 8, 166, bw, 20, canUndo ? "TYPE" : "TYPE NAME", rgb(70, 110, 200))) { kbShift_ = nameBuf_[0] == 0; setScreen(S_NAMEKB); }
  if (button(cv, 8 + bw + 4, 166, bw, 20, canUndo ? "RANDOM" : "RANDOM NAME", rgb(110, 80, 160))) {
    stashName();
    strncpy(nameBuf_, randomName(), sizeof(nameBuf_) - 1);
  }
  if (canUndo && button(cv, 8 + 2 * (bw + 4), 166, bw, 20, "MINE", rgb(60, 150, 90))) {
    strncpy(nameBuf_, customName_, sizeof(nameBuf_) - 1);      // bring the typed name back
  }
  const bool notOwned = mystery;                    // every regular breed is free to adopt; a secret one must be found first
  if (button(cv, 8, 196, 168, 26, notOwned ? "LOCKED" : "ADOPT!", rgb(200, 70, 200), !notOwned) && !notOwned) {
    if (sel < NUM_BREEDS) sv.unlocked |= 1u << sel;
    newKitten(false);
    customName_[0] = 0;
    toast("Welcome home, %s!", pet.d.name);
    toast("They seem %s!", traitName(sv.x8.trait));
    sound(SND_MEOW_SOFT);
    clockT_ = 1.0f;
    autosave(true);
    setScreen(S_HOME);
  }
}

// ---------------------------------------------------------------------------------------
// popups on top of the home room
// ---------------------------------------------------------------------------------------

void Game::screenPlay(Canvas& cv) {
  dimHome(cv, HOME_DIM);
  int px = 8, py = 26, pw = 168, ph = 158;
  cv.panel(px, py, pw, ph, PANEL, BORDER);
  const char* why = nullptr;
  if (pet.sleeping) why = "Shh, sleeping!";
  else if (pet.d.hunting) why = "Out hunting...";
  else if (pet.d.energy < 15) { toast("Too tired to play... cat nap time!"); startLap(); return; }   // too tired: a nap instead
  bool ok = why == nullptr;
  int y = py + 20, bh = 17, gap = 2;
  if (button(cv, px + 8, y, pw - 16, bh, "CHASE THE LASER", GREEN, ok)) { startLaser(); return; }
  y += bh + gap;
  if (button(cv, px + 8, y, pw - 16, bh, "SCRATCHING POST", BLUE, ok)) { startScratch(); return; }
  y += bh + gap;
  if (button(cv, px + 8, y, pw - 16, bh, "MAZE RUN", rgb(200, 110, 60), ok)) { startHeist(); return; }
  y += bh + gap;
  if (button(cv, px + 8, y, pw - 16, bh, "KNOCK IT OFF", rgb(170, 110, 200), ok)) { startKnock(); return; }
  y += bh + gap;
  if (button(cv, px + 8, y, pw - 16, bh, sv.boxType ? "MAGIC CAT" : "MAGIC CAT (NEEDS BOX)", rgb(120, 90, 200), ok && sv.boxType != BT_NONE)) { startMagic(); return; }
  y += bh + gap;
  const char* xwhy = nullptr;
  bool xok = ok && exploreAllowed(&xwhy);
  if (button(cv, px + 8, y, pw - 16, bh, "EXPLORE (adult stage)", rgb(60, 140, 90), xok)) { startExplore(); return; }
  y += bh + gap;
  {   // TOGETHER (up to 4 devices; the radio comes on only once it's tapped), with a Wi-Fi symbol after "requires"
    static const char* TL = "TOGETHER - requires   ";
    const int bx = px + 8, bw = pw - 16;
    const bool hit = button(cv, bx, y, bw, bh, TL, rgb(70, 110, 200), xok);
    const int tx = bx + (bw - Canvas::textWidth(TL)) / 2 + Canvas::textWidth("TOGETHER - requires ");
    const int ty = y + (bh - 3 - 7) / 2;   // top of the label's letters
    const Color wc = xok ? rgb(255, 255, 255) : rgb(140, 132, 160);
    // Wi-Fi: two arcs over a dot, as tall as the letters
    static const char* WIFI[7] = {"  #####  ", " #     # ", "#  ###  #", "  #   #  ", "         ", "    #    ", "         "};
    for (int r = 0; r < 7; r++) for (int c = 0; c < 9; c++) if (WIFI[r][c] == '#') cv.set(tx + c, ty + r, wc);
    if (hit) { startLink(); return; }
  }
  if (button(cv, px + pw - 20, py + 2, 18, 14, "x", RED) || (in.tap && (in.py < py - 10 || in.py > py + ph + 4))) setScreen(S_HOME);
}

void Game::screenConfirm(Canvas& cv) {
  dimHome(cv, HOME_DIM);
  if (confirm_ == CF_SKIN) {   // switching breed mid-game
    const int px = 6, py = 56, pw = 172, ph = 112;
    cv.panel(px, py, pw, ph, PANEL, BORDER);
    char l[40];
    snprintf(l, sizeof(l), "Switch to %s?", BREEDS[skinSel_].name);
    cv.textCenter(W / 2, py + 9, l, GOLD);
    snprintf(l, sizeof(l), "It costs %d hairballs.", SKIN_FEE);
    cv.textCenter(W / 2, py + 25, l, TXT);
    cv.textCenter(W / 2, py + 37, "Switching back later will", TXT);
    snprintf(l, sizeof(l), "cost another %d.", SKIN_FEE);
    cv.textCenter(W / 2, py + 49, l, TXT);
    cv.textCenter(W / 2, py + 61, "Use it for this playthrough?", DIM);
    if (button(cv, px + 8, py + 78, 70, 26, "YES", GREEN)) {
      confirm_ = CF_NONE;
      if (sv.coins >= (uint32_t)SKIN_FEE && skinSel_ != pet.d.breed) {
        sv.coins -= SKIN_FEE;
        if (isSecretBreed(pet.d.breed) && !isSecretBreed(skinSel_) && pet.d.age < AGE_ADULT) pet.d.age = AGE_ADULT;   // a secret breed is always grown up: leaving it, they stay grown up
        pet.d.breed = (uint8_t)skinSel_;
        sv.pad[0] = (uint8_t)skinSel_;
        toast("%s is now a %s!", pet.d.name, BREEDS[skinSel_].name);
        sound(SND_MEOW);
        autosave(true);
      }
      setScreen(S_SKINS);
      return;
    }
    if (button(cv, px + pw - 78, py + 78, 70, 26, "NO", PURPLE)) { confirm_ = CF_NONE; setScreen(S_SKINS); }
    return;
  }
  int px = 14, py = 70, pw = 156, ph = 84;
  cv.panel(px, py, pw, ph, PANEL, BORDER);
  const char* l1 = "Are you sure?";
  const char* l2 = "";
  if (confirm_ == CF_RESET) { l1 = "Erase EVERYTHING and"; l2 = "start from scratch?"; }
  cv.textCenter(W / 2, py + 12, l1, TXT);
  cv.textCenter(W / 2, py + 24, l2, DIM);
  if (button(cv, px + 8, py + 48, 64, 26, "YES", RED)) {
    if (confirm_ == CF_RESET) {
      uint32_t keepClock = sv.fallbackClock;
      uint8_t keepSet = sv.clockSet;
      saveDefaults(sv);
      sv.fallbackClock = keepClock;
      sv.clockSet = keepSet;
      pet.d = PetData();
      memset(&pet.d, 0, sizeof(pet.d));
      a_ = Actor();
      holdBox_ = holdFloor_ = holdHair_ = holdPrey_ = 0;
      autosave(true);
      setScreen(S_MODE);
    }
    confirm_ = CF_NONE;
  }
  if (button(cv, px + pw - 72, py + 48, 64, 26, "NO", PURPLE)) setScreen(confirmBack_);
}

// ---------------------------------------------------------------------------------------
// litter box
// ---------------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------------
// shop
// ---------------------------------------------------------------------------------------

static bool spend(Game& g, uint32_t& coins, int price) {
  (void)g;
  if (coins < (uint32_t)price) return false;
  coins -= (uint32_t)price;
  return true;
}

struct ShopItem { const char* name; const char* desc; int price; int id; };
// Shop text shows the upside as a bonus ("Fun +10%"); under the hood it's the same drain reduction (fun drains 10% slower).
static const ShopItem FURN[5] = {
  {"Floor Cat Bed", "Nap +7%", 5, ITEM_BED},
  {"Ripped Chair", "Nap+15% Fun+5%", 10, ITEM_RCHAIR},
  {"Cat Tree", "Nap+15% Fun+15%", 15, ITEM_TREE},
  {"Luxury Chair", "Nap+30% Fun+10%", 25, ITEM_LCHAIR},
  {"Cat Tree + Bed", "Nap+50% Fun+20%", 45, ITEM_TREEBED}};

void Game::updateToys() {
  float m = 1.0f;
  if (hasItem(ITEM_TREEBED)) m *= 0.80f; else if (hasItem(ITEM_TREE)) m *= 0.85f;   // the big purchases
  float tb = 0;
  if (hasItem(ITEM_MOUSE)) { m *= 0.95f; tb += 0.05f; }
  if (hasItem(ITEM_YARN)) { m *= 0.94f; tb += 0.06f; }
  if (hasItem(ITEM_TUNNEL)) { m *= 0.92f; tb += 0.08f; }
  if (hasItem(ITEM_WAND)) { m *= 0.90f; tb += 0.10f; }
  pet.toyBonus = tb;
  pet.allToys = hasItem(ITEM_MOUSE) && hasItem(ITEM_YARN) && hasItem(ITEM_TUNNEL) && hasItem(ITEM_WAND);
  if (hasItem(ITEM_RCHAIR)) m *= 0.95f;   // a chair they're allowed to shred
  if (hasItem(ITEM_LCHAIR)) m *= 0.90f;
  pet.funMul = m;
  pet.decayMul = sv.slowItem ? 0.85f : 1.0f;
  pet.boost = (sv.ex.flags & 64) ? 1.15f : 1.0f;   // the Hidden Hermit's Wild Amulet
  float sm = hasItem(ITEM_TREEBED) ? 1.5f : (hasItem(ITEM_BED) ? 1.07f : (sv.boxType ? 1.03f : 1.0f));   // a box is a so-so nap spot (+3%): the bed (+7%) is better
  if (hasItem(ITEM_LCHAIR) && sm < 1.30f) sm = 1.30f;   // best nap spot they own wins
  if ((hasItem(ITEM_RCHAIR) || hasItem(ITEM_TREE)) && sm < 1.15f) sm = 1.15f;
  pet.sleepMul = sm;
}

void Game::screenShop(Canvas& cv) {
  drawBackdrop(cv, false);
  static const char* TITLES[5] = {"GOODS", "FURNITURE", "BOXES", "", "HOMES"};
  panelTitle(cv, shopPage_ < 0 ? "HAIRBALL SHOP" : TITLES[shopPage_], 6);
  drawIcon(cv, 6, 14, 24);
  char b[48];
  coinText(b, sizeof(b));
  cv.text(22, 21, b, coinColor(GOLD));
  if (shopPage_ < 0) {   // the shop menu
    struct Opt { const char* label; Color col; };
    const Opt opts[5] = {{"WARDROBE", rgb(190, 90, 150)}, {"GOODS", GREEN}, {"FURNITURE", BLUE}, {"BOXES", rgb(190, 140, 70)}, {"HOMES", rgb(40, 150, 160)}};
    for (int i = 0; i < 5; i++) {
      if (button(cv, 20, 34 + i * 32, 144, 28, opts[i].label, opts[i].col)) {
        if (i == 0) { wardFromShop_ = true; wardTab_ = 0; wardSel_ = sv.wornHat; setScreen(S_WARDROBE); return; }
        static const int PAGE[5] = {0, 0, 1, 2, 4};
        shopPage_ = PAGE[i]; if (shopPage_ == 4) homeSel_ = sv.room;
      }
    }
    updateParticles(dt);
    drawToasts(cv);
    if (button(cv, 40, 196, 104, 24, "BACK", PURPLE)) setScreen(S_HOME);
    return;
  }
  const int y0 = 34;
  if (shopPage_ == 0) {
  struct Row { const char* name; const char* desc; int price; int icon; bool avail; };
  int tier = sv.litterTier;
  Row rows[3] = {   // (treats and diet kibble are gone: pouring kibble is how a cat gets chonky or lean)
    {"Catnip Treat", "+40 energy, zoom!", 6, 4, true},
    {"Vet Visit", "+45 health", 10, 2, true},
    {"Auto Litter-Bot", "Cleans each visit", 120, 3, tier < 2}};   // (the Sifting Box, tier 1, is no longer sold: a save that has one keeps it)
  for (int i = 0; i < 3; i++) {
    int y = y0 + i * 28;
    cv.panel(4, y, 176, 26, PANEL, BORDER);
    int ix = 16, iy = y + 13;
    switch (rows[i].icon) {
      case 2: drawIcon(cv, 5, ix, iy); break;
      case 3: drawIcon(cv, 2, ix, iy); break;
      default:   // catnip leaf
        cv.ellipse(ix, iy, 6, 4, rgb(60, 170, 80)); cv.ellipse(ix - 1, iy - 1, 4, 2, rgb(120, 220, 120)); cv.line(ix - 7, iy + 3, ix + 7, iy - 3, rgb(30, 110, 50)); cv.set(ix + 8, iy - 6, rgb(255, 255, 160)); break;
    }
    cv.text(30, y + 3, rows[i].name, TXT);
    cv.text(30, y + 14, rows[i].desc, DIM);
    bool owned = (rows[i].icon == 3 && !rows[i].avail);   // (the icon number doubles as the item id)
    if (owned) { cv.text(136, y + 10, "OWNED", rgb(150, 255, 190)); continue; }
    snprintf(b, sizeof(b), "%d", rows[i].price);
    bool can = sv.coins >= (uint32_t)rows[i].price;
    if (button(cv, 134, y + 2, 42, 21, b, can ? GREEN : rgb(120, 70, 80), true)) {
      if (rows[i].icon == 4 && pet.sleeping) { toast("Shh, sleeping!"); sound(SND_LOSE); }
      else if (rows[i].icon == 4 && pet.d.energy > 80) { toast("Already full of energy!"); sound(SND_LOSE); }
      else if (!spend(*this, sv.coins, rows[i].price)) { toast("Not enough hairballs!"); sound(SND_LOSE); }
      else {
        sound(SND_COIN);
        const int id = rows[i].icon;
        if (id == 2) { pet.vetVisit(); toast("%s feels much better!", pet.d.name); }
        else if (id == 3) { sv.litterTier = 2; toast("Auto Litter-Bot online!"); }
        else if (id == 4) {
          pet.d.energy += 40; pet.d.fun += 10; pet.clampAll();
          actorWalkTo(roamX(), A_ZOOM, 0.5f); a_.speed = 120;
          toast("%s goes bananas!", pet.d.name);
        }
        autosave(true);
      }
    }
  }
  } else if (shopPage_ == 4) {
    shopHomes(cv, y0);
  } else if (shopPage_ == 2) {
    shopBoxes(cv, y0);
  } else {
    const ShopItem* its = FURN;
    int n = 5;
    for (int i = 0; i < n; i++) {
      int y = y0 + i * 29;
      const ShopItem& it = its[i];
      cv.panel(4, y, 176, 27, PANEL, BORDER);
      cv.rect(8, y + 2, 36, 23, rgb(46, 34, 60));
      int ix = 26, iy = y + 23;
      // thumbnails fit the 36x23 box (y+2 .. y+25)
      if (it.id == ITEM_TREE || it.id == ITEM_TREEBED) { drawItem(cv, it.id + 100, ix, y + 23); }
      else if (it.id == ITEM_RCHAIR || it.id == ITEM_LCHAIR) drawItem(cv, it.id == ITEM_RCHAIR ? 104 : 105, ix, y + 23);
      else if (it.id == ITEM_BED) drawItem(cv, 103, ix, y + 17);
      else if (it.id == ITEM_WAND) drawItem(cv, ITEM_WAND, ix - 4, y + 25);
      else drawItem(cv, it.id, ix, iy - 4);
      cv.text(46, y + 4, it.name, TXT);
      cv.text(46, y + 15, it.desc, DIM);
      if (hasItem(it.id)) { cv.text(142, y + 10, "OWNED", rgb(150, 255, 190)); continue; }
      snprintf(b, sizeof(b), "%d", it.price);
      bool can = sv.coins >= (uint32_t)it.price;
      if (button(cv, 138, y + 3, 38, 21, b, can ? GREEN : rgb(120, 70, 80), true)) {
        if (!spend(*this, sv.coins, it.price)) { toast("Not enough hairballs!"); sound(SND_LOSE); }
        else {
          sv.items |= (uint16_t)(1u << it.id); sound(SND_WIN); updateToys();
          if (shopPage_ == 1) recordMilestone(MS_FURNI);
          if (shopPage_ == 1 && sv.boxType < BT_CARD) { giveBox(BT_CARD); toast("%s + free cardboard box!", it.name); }   // every furniture purchase comes with a box
          else toast("%s bought!", it.name);
          autosave(true);
        }
      }
    }
  }
  updateParticles(dt);
  drawToasts(cv);
  if (button(cv, 40, 196, 104, 24, "BACK", PURPLE)) shopPage_ = -1;
}

// ---------------------------------------------------------------------------------------
// skins
// ---------------------------------------------------------------------------------------

void Game::screenSkins(Canvas& cv) {
  drawBackdrop(cv, false);
  panelTitle(cv, "CHANGE BREED", 6);
  int list[NUM_BREED_SLOTS], nList = 0;
  for (int i = 0; i < NUM_BREEDS; i++) list[nList++] = i;
  for (int i = BR_RAINBOW; i <= BR_HAMSTER; i++) if (sv.unlocked & (1u << i)) list[nList++] = i;
  if (skinSel_ < 0 || skinSel_ >= NUM_BREED_SLOTS || skinSel_ == BR_KITTEN || (isSecretBreed(skinSel_) && !(sv.unlocked & (1u << skinSel_)))) skinSel_ = 0;
  for (int li = 0; li < nList; li++) {
    const int i = list[li];
    int cx = 4 + (li % 3) * 59, cy = 22 + (li / 3) * 46;
    bool sel = skinSel_ == i;
    cv.panel(cx, cy, 57, 44, sel ? rgb(88, 66, 130) : PANEL, sel ? GOLD : BORDER);
    CatDraw d;
    d.breed = i; d.pose = POSE_FRONT; d.size = 0.5f; d.t = anim_; d.face = FACE_NORMAL;
    drawCat(cv, cx + 28, cy + 40, d);
    if (pet.d.breed == i) cv.text(cx + 3, cy + 3, "*", GOLD);
    if (tapIn(cx, cy, 57, 44)) { skinSel_ = i; sound(SND_TAP); }
  }
  // big preview of the selected breed (in the free space beside the last row, while there is one)
  if (nList <= NUM_BREEDS) {
    CatDraw d;
    d.breed = skinSel_; d.pose = POSE_FRONT; d.size = 0.9f; d.t = anim_; d.face = FACE_HAPPY;
    d.blink = ((int)(anim_ * 2.2f) % 7) == 0;
    cv.ellipse(122, 158, 26, 3, rgb(26, 20, 44));
    drawCat(cv, 122, 157, d);
  }
  const Breed& b = BREEDS[skinSel_];
  cv.rect(4, 162, 176, 30, PANEL);
  cv.frame(4, 162, 176, 30, BORDER);
  cv.text(8, 164, b.name, GOLD);
  cv.text(8, 173, b.blurb, TXT);
  char s[48];
  snprintf(s, sizeof(s), "Spd %d  Gain %d  Hair %d", (int)(b.speed * 100), (int)(b.gain * 100), (int)(b.hair * 100));
  cv.text(8, 182, s, DIM);
  {   // switching breed mid-game costs 10 hairballs each time (even back to one you've had before)
    const bool cur = pet.d.breed == skinSel_;
    char t[24];
    snprintf(t, sizeof(t), "SWITCH  %d", SKIN_FEE);
    if (button(cv, 4, 196, 112, 24, cur ? "EQUIPPED" : t, cur ? PURPLE : (sv.coins >= (uint32_t)SKIN_FEE ? GREEN : rgb(120, 70, 80)), !cur)) {
      if (sv.coins >= (uint32_t)SKIN_FEE) { confirm_ = CF_SKIN; confirmBack_ = S_SKINS; setScreen(S_CONFIRM); return; }
      toast("Switching breed costs %d hairballs", SKIN_FEE); sound(SND_LOSE);
    }
  }
  updateParticles(dt);
  drawToasts(cv);
  if (button(cv, 122, 196, 58, 24, "BACK", PURPLE)) setScreen(S_MENU);
}

// ---------------------------------------------------------------------------------------
// menu / stats / settings / alarm / hall
// ---------------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------------
// TOUCH FIX: tap 5 targets; a straight-line fit per axis corrects where the touch chip says the finger is.
// The correction lives on the device (Platform::touchCalSet), so it survives a game reset.
// ---------------------------------------------------------------------------------------
static const int CAL_PTS[5][2] = {{28, 34}, {156, 34}, {92, 112}, {28, 206}, {156, 206}};   // the low pair sits on the ADOPT row: the bottom is measured, not guessed

void Game::startTouchCal() {
  plat->touchCalGet(calOld_[0], calOld_[1], calOld_[2], calOld_[3]);
  plat->touchCalSet(1, 0, 1, 0);   // measure the raw readings
  calStep_ = 0; calMsgT_ = 0; calFail_ = false;
  if (scr != S_TOUCHCAL) calReturn_ = scr;   // come back here afterwards
  setScreen(S_TOUCHCAL);
}

void Game::screenTouchCal(Canvas& cv) {
  cv.clear(rgb(18, 12, 40));
  if (calMsgT_ > 0) calMsgT_ -= dt;
  if (calStep_ < 5) {
    const int tx = CAL_PTS[calStep_][0], ty = CAL_PTS[calStep_][1];
    const int r = 7 + (int)(2 * sinf(anim_ * 6));
    cv.ring(tx, ty, r + 4, rgb(255, 120, 200)); cv.ring(tx, ty, r, rgb(255, 230, 120));
    cv.hline(tx - 14, ty, 29, rgb(255, 255, 255)); cv.vline(tx, ty - 14, 29, rgb(255, 255, 255));
    char b[32]; snprintf(b, sizeof(b), "Tap the centre (%d of 5)", calStep_ + 1);
    const int my = ty > 112 ? 84 : 140;
    cv.textCenter(W / 2, my, "TOUCH FIX", GOLD);
    cv.textCenter(W / 2, my + 12, b, TXT);
    cv.textCenter(W / 2, my + 22, calFail_ ? "That didn't line up. Again!" : "Use a fingertip, gently.", calFail_ ? rgb(255, 170, 150) : DIM);
    if (in.released) {   // where the finger first touched (raw, since the old correction is off)
      calPts_[calStep_][0] = (float)in.px; calPts_[calStep_][1] = (float)in.py;
      sound(SND_TAP);
      calStep_++;
      if (calStep_ == 5) {
        // a reading stuck on the edge of the glass (a chip that reads long can't report past it) is left out of the fit
        auto calUse = [&](int i, int axis) { const float m = calPts_[i][axis]; return m > 0.5f && m < (axis ? H - 1.5f : W - 1.5f); };
        // least-squares line per axis: true = a * measured + b
        auto fit = [&](int axis, float& a, float& b) {
          float sm = 0, st = 0, smm = 0, smt = 0, n = 0;
          for (int i = 0; i < 5; i++) {
            if (!calUse(i, axis)) continue;   // pinned to the glass edge: says nothing about the slope
            const float m = calPts_[i][axis], t = (float)CAL_PTS[i][axis]; sm += m; st += t; smm += m * m; smt += m * t; n += 1;
          }
          const float den = n * smm - sm * sm;
          a = fabsf(den) < 1e-3f ? 1.0f : (n * smt - sm * st) / den;
          b = n > 0 ? (st - a * sm) / n : 0;
        };
        float ax, bx, ay, by; fit(0, ax, bx); fit(1, ay, by);
        float err = 0;
        for (int i = 0; i < 5; i++) {
          const float dx = calUse(i, 0) ? ax * calPts_[i][0] + bx - CAL_PTS[i][0] : 0, dy = calUse(i, 1) ? ay * calPts_[i][1] + by - CAL_PTS[i][1] : 0;
          err = fmaxf(err, sqrtf(dx * dx + dy * dy));
        }
        const bool ok = ax > 0.7f && ax < 1.4f && ay > 0.7f && ay < 1.4f && fabsf(bx) < 40 && fabsf(by) < 40 && err < 12;
        if (ok) {
          plat->touchCalSet(ax, bx * 2, ay, by * 2);   // stored in physical pixels (2x the game's)
          calResult_[0] = ax; calResult_[1] = bx; calResult_[2] = ay; calResult_[3] = by;
          sound(SND_WIN);
        } else { calFail_ = true; calStep_ = 0; sound(SND_MISS); }   // a sloppy tap: start over
      }
    }
    if (button(cv, 62, ty > 112 ? 150 : 196, 60, 20, "CANCEL", PURPLE)) {   // (kept away from the targets)
      plat->touchCalSet(calOld_[0], calOld_[1], calOld_[2], calOld_[3]);   // (also marks it done: no asking again)
      setScreen((Screen)calReturn_);
    }
    return;
  }
  panelTitle(cv, "TOUCH FIXED!", 30);
  char b[40];
  snprintf(b, sizeof(b), "Across: x%.2f %+.0f px", calResult_[0], calResult_[1]); cv.textCenter(W / 2, 60, b, TXT);
  snprintf(b, sizeof(b), "Down:   x%.2f %+.0f px", calResult_[2], calResult_[3]); cv.textCenter(W / 2, 72, b, TXT);
  cv.textCenter(W / 2, 96, "Try tapping these:", DIM);
  static int tried = 0;
  if (button(cv, 14, 110, 74, 22, "TEST A", BLUE)) { tried = 1; sound(SND_POP); }
  if (button(cv, 96, 110, 74, 22, "TEST B", BLUE)) { tried = 2; sound(SND_POP); }
  if (tried) cv.textCenter(W / 2, 138, tried == 1 ? "You tapped A" : "You tapped B", rgb(160, 255, 180));
  if (button(cv, 14, 196, 74, 22, "REDO", rgb(200, 120, 60))) { tried = 0; startTouchCal(); return; }
  if (button(cv, 96, 196, 74, 22, "DONE", GREEN)) { tried = 0; setScreen((Screen)calReturn_); }
}

void Game::screenMenu(Canvas& cv) {
  drawBackdrop(cv, false);
  panelTitle(cv, "MENU", 6);
  const int bh = 22, g = 3; int y = 20;
  if (button(cv, 20, y, 144, bh, "STATS", BLUE)) setScreen(S_STATS);
  y += bh + g;
  if (button(cv, 20, y, 144, bh, "CHANGE BREED", BLUE)) { skinSel_ = pet.d.breed; setScreen(S_SKINS); }
  y += bh + g;
  if (button(cv, 20, y, 144, bh, "ALARM CLOCK", BLUE)) {
    alarmH_ = sv.alarmH; alarmM_ = sv.alarmM; setScreen(S_ALARM_SET);
  }
  y += bh + g;
  if (button(cv, 20, y, 144, bh, "JOURNAL", rgb(200, 140, 60))) { journalPage_ = 0; setScreen(S_JOURNAL); }   // (with the Hall of Cats)
  y += bh + g;
  if (button(cv, 20, y, 144, bh, "SETTINGS", BLUE)) setScreen(S_SETTINGS);
  y += bh + g + 2;
  if (button(cv, 20, y, 68, 22, "HELP", rgb(200, 140, 60))) { helpPage_ = 0; setScreen(S_HELP); }
  if (button(cv, 96, y, 68, 22, "BACK", GREEN)) setScreen(S_HOME);
}

// ---------------------------------------------------------------------------------------
// HELP: what the bars, numbers and game scores mean
// ---------------------------------------------------------------------------------------
void Game::screenHelp(Canvas& cv) {
  drawBackdrop(cv, false);
  const int PAGES = 4;
  if (helpPage_ < 0 || helpPage_ >= PAGES) helpPage_ = 0;
  static const char* titles[PAGES] = {"HELP: TOP BAR", "HELP: THE 3 BARS", "HELP: WEIGHT & MOOD", "HELP: GAME SCORES"};
  panelTitle(cv, titles[helpPage_], 6);
  const Color H1 = rgb(255, 230, 150);
  int y = 24;
  auto row = [&](int icon, const char* head, const char* l1, const char* l2) {
    if (icon >= 0) drawIconS(cv, icon, 14, y + 5, 0.8f);
    cv.text(icon >= 0 ? 26 : 8, y, head, H1);
    if (l1) cv.text(8, y + 10, l1, TXT);
    if (l2) cv.text(8, y + 19, l2, DIM);
    y += 32;
  };
  // icon ids match home.cpp's OldIcon: 5 heart, 6 hairball, 7 fish, 8 ball, 9 moon
  switch (helpPage_) {
    case 0:
      row(6, "Hairballs", "Money for the shop.", "Win them in games & trips.");
      row(-1, "Clock (middle)", "Real time. Day & night show", "in the window.");
      row(5, "Health", "0-100. Drops if they're", "hungry, dirty or too chonky.");
      row(-1, "Red dot on a button", "That need wants you now!", nullptr);
      break;
    case 1:
      row(7, "Fullness", "Feed kibble. Empty = they", "lose health & weight.");
      row(8, "Fun", "Play games, pet them, toys.", "Furniture slows the drain.");
      row(9, "Energy", "Games use it, naps refill.", "Under 12 they nap. Play 15+");
      row(-1, "A bar turns RED", "when it drops below 25.", "Explore uses 50 energy.");
      break;
    case 2:
      row(-1, "WEIGHT (Stats screen)", "Heavier = slower speed.", "Weight 50 is ideal.");
      row(-1, "Grumpy (Stats screen)", "Goes up when ignored.", "Too high and they run away!");
      row(-1, "Mood & life", "Happy cats stay longer.", "Check STATS in the Menu.");
      break;
    default:
      row(-1, "Feeding / Lap Cat", "5 sec pour. Biggest colour", "wins. Out of energy? Lap nap");
      row(-1, "Laser", "Keep the dot AWAY. 0 catch", "= 3 stars. 3 catches = lose");
      row(-1, "Scratch / Maze Run", "Swipe with the arrows. /", "Bring loot to the house.");
      row(-1, "Earning hairballs", "Pickups in Laser & Maze,", "3* Scratch, Magic 3+ right");
      break;
  }
  char b[12];
  snprintf(b, sizeof(b), "%d/%d", helpPage_ + 1, PAGES);
  cv.textCenter(W / 2, 156, b, DIM);
  if (button(cv, 20, 168, 68, 22, "< PREV", PURPLE, helpPage_ > 0)) helpPage_--;
  if (button(cv, 96, 168, 68, 22, "NEXT >", PURPLE, helpPage_ < PAGES - 1)) helpPage_++;
  if (button(cv, 40, 196, 104, 22, "DONE", GREEN)) setScreen(S_MENU);
}

static void statBar(Canvas& cv, int y, const char* label, float v, Color c, const char* extra = nullptr) {
  cv.text(6, y, label, TXT);
  cv.bar(60, y - 1, 88, 9, (int)v, c, rgb(24, 18, 40), rgb(120, 92, 150));
  char b[12];
  snprintf(b, sizeof(b), "%d", (int)(v + 0.5f));
  cv.text(152, y, extra ? extra : b, DIM);
}

void Game::screenStats(Canvas& cv) {
  drawBackdrop(cv, false);
  CatDraw d;
  d.breed = pet.lookBreed(); d.fat = pet.fatLook(); d.size = pet.visualSize() * 0.9f; d.kit = pet.kitFactor();
  d.pose = POSE_FRONT; d.face = pet.mood() == 0 ? FACE_HAPPY : (pet.mood() >= 2 && pet.mood() != 4 ? FACE_SAD : FACE_NORMAL);
  d.t = anim_;
  cv.ellipse(40, 66, 26, 4, rgb(26, 20, 44));
  drawCat(cv, 40, 66, d);
  cv.text(76, 8, pet.d.name, GOLD, 2);
  cv.text(76, 26, BREEDS[pet.d.breed].name, DIM);
  char b[48];
  snprintf(b, sizeof(b), "%s  %.1f days", stageName(pet.stage()), pet.ageDays());
  cv.text(76, 36, b, TXT);
  static const char* moods[] = {"Happy", "Content", "Unhappy", "GRUMPY", "Sleeping"};
  snprintf(b, sizeof(b), "Mood: %s", moods[pet.mood()]);
  cv.text(76, 50, b, TXT);
  snprintf(b, sizeof(b), "Trait: %s", traitName(pet.trait));
  cv.text(76, 62, b, rgb(250, 200, 120));
  statBar(cv, 80, "Fullness", pet.d.hunger, rgb(244, 150, 40));
  statBar(cv, 93, "Fun", pet.d.fun, rgb(240, 100, 150));
  statBar(cv, 106, "Energy", pet.d.energy, rgb(90, 170, 250));
  statBar(cv, 119, "Health", pet.d.health, rgb(90, 220, 120));
  const float ideal = pet.idealWeight();
  const float wv = pet.d.chonk;
  Color cc = wv > 90 ? rgb(240, 60, 60) : (wv > ideal + 35 ? rgb(250, 150, 60) : ((wv > ideal + 18 || wv < ideal - 13) ? rgb(240, 220, 90) : rgb(90, 220, 120)));
  statBar(cv, 132, "Weight", wv, cc);
  statBar(cv, 145, "Grumpy", pet.d.resent, rgb(190, 90, 220));
  cv.vline(60 + 1 + (int)(86 * ideal / 100.0f), 131, 11, rgb(255, 255, 255));  // ideal-weight marker
  char tipBuf[40];
  if (pet.stage() >= STAGE_ADULT) snprintf(tipBuf, sizeof(tipBuf), "Weight 50 is ideal.");
  else if (pet.stage() == STAGE_JUNIOR) snprintf(tipBuf, sizeof(tipBuf), "Growing: weight %d is ideal.", (int)(ideal + 0.5f));
  else snprintf(tipBuf, sizeof(tipBuf), "Weight 35 is ideal.");
  const char* tip = tipBuf;
  if (((int)(anim_ / 4.0f)) & 1) tip = "Good care = a longer life.";   // (takes turns with the weight tip)
  if (wv > 90) tip = "DANGER: Gravity Severe!!";
  else if (wv > ideal + 35) tip = "Too heavy: slow & unhealthy.";
  else if (wv > ideal + 18) tip = "Heavier = slower. Go easy!";
  else if (wv < ideal - 13) tip = "Too skinny! Pour in the green.";
  else if (pet.d.resent > 80) tip = "About to run away!!";
  else if (pet.d.resent > 55) tip = "Cat is getting fed up with you.";
  cv.textCenter(W / 2, 160, tip, pet.d.chonk > 90 || pet.d.resent > 80 ? rgb(255, 150, 150) : DIM);
  {   // hair, speed and strength as bars, like the breed picker's (speed and strength are this cat's, right now:
      // weight, health, age and charms all move them; full bar = 150)
    struct Row { const char* label; float v; Color c; } rows[3] = {
      {"Hair", BREEDS[pet.d.breed].hair, rgb(250, 200, 110)}, {"Speed", pet.agility(), rgb(110, 220, 255)}, {"Str", pet.power() * 0.5f, rgb(255, 130, 170)}};
    for (int i = 0; i < 3; i++) {
      const int x0 = 1 + i * 60;
      cv.textRight(x0 + 31, 172, rows[i].label, rgb(210, 200, 235));
      cv.rect(x0 + 34, 173, 24, 6, rgb(50, 36, 86));
      const float f = rows[i].v / 1.5f;
      cv.rect(x0 + 34, 173, (int)(24 * (f > 1 ? 1 : (f < 0 ? 0 : f)) + 0.5f), 6, rows[i].c);
    }
  }
  snprintf(b, sizeof(b), "Catches: %d", pet.d.preyCount);
  cv.textCenter(W / 2, 183, b, DIM);
  if (button(cv, 40, 196, 104, 24, "BACK", PURPLE)) setScreen(S_MENU);
}

// Battery details: level, charging or not, and roughly how long to full / empty.
void Game::screenBattery(Canvas& cv) {
  dimHome(cv, HOME_DIM);
  const int px = 18, py = 40, pw = 148, ph = 124;
  cv.panel(px, py, pw, ph, PANEL, BORDER);
  cv.textCenter(W / 2, py + 6, "BATTERY", GOLD);
  const int pct = batPct_ < 0 ? 0 : (batPct_ > 100 ? 100 : batPct_);
  {   // a big battery
    const int bx = W / 2 - 30, by = py + 22, bw = 56, bh = 26;
    const Color fill = pct > 50 ? rgb(110, 220, 120) : (pct > 20 ? rgb(240, 210, 80) : rgb(240, 80, 80));
    cv.rect(bx - 1, by - 1, bw + 2, bh + 2, rgb(230, 222, 250));
    cv.rect(bx, by, bw, bh, rgb(30, 22, 46));
    cv.rect(bx + bw + 1, by + 8, 4, bh - 16, rgb(230, 222, 250));
    const int fw = (bw - 4) * pct / 100;
    if (fw > 0) cv.rect(bx + 2, by + 2, fw, bh - 4, fill);
    if (batChg_) {   // lightning bolt
      const int cx = bx + bw / 2;
      cv.tri(cx + 3, by + 3, cx - 5, by + 14, cx + 1, by + 14, rgb(255, 250, 140));
      cv.tri(cx - 1, by + 12, cx + 5, by + 12, cx - 3, by + 23, rgb(255, 250, 140));
    }
  }
  char b[32];
  snprintf(b, sizeof(b), "%d%%", pct);
  cv.textCenter(W / 2, py + 54, b, rgb(255, 255, 255), 2);
  if (!batOk_) cv.textCenter(W / 2, py + 76, "No battery info", DIM);
  else {
    cv.textCenter(W / 2, py + 74, batChg_ ? (pct >= 100 ? "Fully charged" : "Charging") : "On battery", batChg_ ? rgb(150, 230, 160) : TXT);
    int kind = 0; const int m = batEstimate(kind);
    if (kind == 0) snprintf(b, sizeof(b), "Working it out...");
    else if (kind == 4) snprintf(b, sizeof(b), "Ready to unplug");
    else if (kind == 3) snprintf(b, sizeof(b), "Over %dh %02dm left", m / 60, m % 60);
    else if (m >= 60) snprintf(b, sizeof(b), "About %dh %02dm %s", m / 60, m % 60, kind == 1 ? "to full" : "left");
    else snprintf(b, sizeof(b), "About %d min %s", m < 1 ? 1 : m, kind == 1 ? "to full" : "left");
    cv.textCenter(W / 2, py + 86, b, GOLD);
    cv.textCenter(W / 2, py + 97, kind == 0 ? "(give it a few minutes)" : "(based on last 30 min)", DIM);
  }
  if (button(cv, W / 2 - 30, py + ph - 20, 60, 16, "OK", PURPLE) || (in.tap && (in.py < py - 4 || in.py > py + ph + 4))) setScreen(S_HOME);
}

void Game::screenSettings(Canvas& cv) {
  drawBackdrop(cv, false);
  panelTitle(cv, "SETTINGS", 6);
  int vol = sv.volume, br = sv.brightness;
  char b[24];
  stepper(cv, 10, 26, 164, vol ? "Volume" : "Volume (off)", vol, 0, 10);
  stepper(cv, 10, 54, 164, "Bright", br, 1, 10);
  if (vol != sv.volume) { sv.volume = (uint8_t)vol; sound(SND_BLOOP); }   // a little bloop at the new level
  sv.brightness = (uint8_t)br;
  {   // how long until the screen turns itself off
    static const char* TL[9] = {"1 sec", "3 sec", "5 sec", "10 sec", "15 sec", "30 sec", "1 min", "5 min", "never"};
    int ti = sv.eq[2] < 9 ? sv.eq[2] : 6;
    const int bx = 10 + 164 - 88;
    cv.text(10, 90, "Screen off", TXT);
    if (button(cv, bx, 82, 24, 22, "-", PURPLE) && ti > 0) ti--;
    if (button(cv, bx + 64, 82, 24, 22, "+", PURPLE) && ti < 8) ti++;
    cv.textCenter(bx + 44, 90, TL[ti], GOLD);
    sv.eq[2] = (uint8_t)ti;
  }
  float cax, cbx, cay, cby;
  const bool hasTouch = plat->touchCalGet(cax, cbx, cay, cby);   // (the board has a touch screen to calibrate; the web page doesn't)
  if (hasTouch && button(cv, 96, 114, 78, 24, "TOUCH FIX", rgb(200, 120, 60))) startTouchCal();
  if (button(cv, 10, 114, hasTouch ? 80 : 164, 24, "SET CLOCK", BLUE)) {
    DateTime d = fromEpoch(epoch);
    clockSel_[0] = d.year; clockSel_[1] = d.month; clockSel_[2] = d.day; clockSel_[3] = d.hour; clockSel_[4] = d.minute;
    clockNext_ = S_SETTINGS;
    setScreen(S_CLOCK);
  }
  // auto-rotate: ON (the tilt sensor decides) -> OFF (always the normal way up) -> FLIP (always upside down: a manual
  // turn, for holding it the other way round with rotation off, or when there is no working tilt sensor)
  const char* rotLbl = !(oriCfg_ & 16) ? "ROTATE: ON" : ((oriCfg_ & 32) ? "ROTATE: FLIP" : "ROTATE: OFF");
  if (hasTouch && button(cv, 10, 140, 80, 24, rotLbl, (oriCfg_ & 16) ? rgb(110, 100, 130) : rgb(60, 150, 120))) {
    if (!(oriCfg_ & 16)) oriCfg_ = (oriCfg_ | 16) & ~32;       // ON -> OFF
    else if (!(oriCfg_ & 32)) oriCfg_ |= 32;                   // OFF -> FLIP
    else oriCfg_ &= ~(16 | 32);                                // FLIP -> ON
    if (!(oriCfg_ & 16)) {
      oriCfg_ &= ~7;                                           // switched on: learn which way is up again
      if (imuAge_ > 1.0f) { setFlip(false); toast("No tilt sensor found"); } else toast("Hold it upright a moment");
    } else setFlip((oriCfg_ & 32) != 0);
    plat->orientSet(oriCfg_);
  }
  if (button(cv, hasTouch ? 96 : 10, 140, hasTouch ? 78 : 164, 24, "RESET GAME", RED)) { confirm_ = CF_RESET; confirmBack_ = S_SETTINGS; setScreen(S_CONFIRM); }
  if (sv.hasCat && button(cv, 10, 167, 164, 20, "BIRTHDAY", rgb(220, 90, 150))) {
    ensureBirthday();
    bdaySel_[0] = sv.x8.bdayMonth ? sv.x8.bdayMonth : 1; bdaySel_[1] = sv.x8.bdayDay ? sv.x8.bdayDay : 1;
    setScreen(S_BDAY);
  }
  {   // developer (hidden): hold the version line for 3 seconds -> +100 hairballs
    static float held = 0;
    const bool on = in.down && in.px >= 20 && in.px < 164 && in.py >= 185 && in.py < 197;
    held = on ? held + dt : 0;
    if (on) { const int w = (int)(144 * clampf(held / 3.0f, 0, 1)); cv.rect(20, 198, w, 1, rgb(255, 120, 120)); }   // (a thin progress line)
    if (held >= 3.0f) { held = -100; devAddHairballs(100); }   // (once per hold: it counts up from -100 until the finger lifts)
  }
  if (timeScale > 1) { snprintf(b, sizeof(b), "DEBUG x%d", timeScale); cv.textCenter(W / 2, 190, b, rgb(255, 120, 120)); }
  else cv.textCenter(W / 2, 190, "Kitten Keeper v1.3.2", DIM);
  updateParticles(dt);
  drawToasts(cv);
  if (button(cv, 40, 199, 104, 21, "BACK", GREEN)) { autosave(true); setScreen(S_MENU); }
}

void Game::screenAlarmSet(Canvas& cv) {
  drawBackdrop(cv, false);
  panelTitle(cv, "ALARM CLOCK", 6);
  cv.textCenter(W / 2, 22, "Your cat will wake you up!", DIM);
  stepper(cv, 10, 40, 164, "Hour", alarmH_, 0, 23, 1, true);
  stepper(cv, 10, 70, 164, "Minute", alarmM_, 0, 59, 1, true);
  char b[16];
  snprintf(b, sizeof(b), "%02d:%02d", alarmH_, alarmM_);
  cv.textCenter(W / 2, 108, b, GOLD, 3);
  if (button(cv, 20, 140, 144, 28, sv.alarmOn ? "ALARM: ON" : "ALARM: OFF", sv.alarmOn ? GREEN : RED)) {
    sv.alarmOn = !sv.alarmOn;
    sv.alarmH = (uint8_t)alarmH_; sv.alarmM = (uint8_t)alarmM_;
    alarmDayDone_ = -1;
    autosave(true);
  }
  cv.textCenter(W / 2, 176, "They meow you awake at", DIM);
  cv.textCenter(W / 2, 186, "this time every morning.", DIM);
  if (button(cv, 40, 196, 104, 24, "BACK", PURPLE)) {
    sv.alarmH = (uint8_t)alarmH_; sv.alarmM = (uint8_t)alarmM_;
    alarmDayDone_ = -1;
    autosave(true);
    setScreen(S_MENU);
  }
}

// ---------------------------------------------------------------------------------------
// result / alarm / endings
// ---------------------------------------------------------------------------------------

void Game::screenResult(Canvas& cv) {
  drawBackdrop(cv, false);
  panelTitle(cv, res_.win ? "YOU WIN!" : "GOOD TRY!", 10);
  for (int i = 0; i < 3; i++) {
    int sx = 52 + i * 40, sy = 40;
    bool on = i < res_.stars && scrT_ > 0.3f + i * 0.3f;
    if (i < res_.stars && scrT_ - dt <= 0.3f + i * 0.3f && scrT_ > 0.3f + i * 0.3f) sound(SND_POP);   // pop... pop... pop!
    Color c = on ? rgb(252, 226, 72) : rgb(70, 62, 90);
    cv.tri(sx, sy - 12, sx - 5, sy + 4, sx + 5, sy + 4, c);
    cv.tri(sx - 12, sy - 3, sx + 12, sy - 3, sx, sy + 9, c);
    cv.tri(sx - 7, sy + 11, sx, sy + 1, sx + 7, sy + 11, c);
  }
  CatDraw d;
  d.breed = pet.lookBreed(); d.fat = pet.fatLook(); d.size = pet.visualSize() * 0.95f; d.kit = pet.kitFactor();
  d.pose = POSE_SIT; d.face = res_.win ? FACE_HAPPY : FACE_NORMAL; d.t = anim_; d.faceLeft = false;
  float hop = res_.win ? -fabsf(sinf(anim_ * 8)) * 4 : 0;
  cv.ellipse(92, 126, 22, 4, rgb(26, 20, 44));
  drawCat(cv, 92, 126 + (int)hop, d);
  char b[48];
  if (res_.kind == 0) { if (res_.catches >= 3) snprintf(b, sizeof(b), "Caught 3 times: they win!"); else snprintf(b, sizeof(b), "Caught %d of 3", res_.catches); }
  else if (res_.kind == 1) snprintf(b, sizeof(b), res_.score >= 100 ? "Every beat: PURRFECT!" : "Beats hit: %d%%", res_.score);
  else if (res_.kind == 4) snprintf(b, sizeof(b), "Knock It Off: %d saved", res_.score);
  else snprintf(b, sizeof(b), "Maze: %d pts", res_.score);
  cv.textCenter(W / 2, 138, b, TXT);
  drawIcon(cv, 6, 70, 156);
  snprintf(b, sizeof(b), "+%d hairballs", res_.coins);
  cv.text(78, 153, b, GOLD);
  snprintf(b, sizeof(b), "Fun +%d", (int)res_.funGain);
  cv.textCenter(W / 2, 166, b, rgb(250, 130, 170));
  bool tired = pet.d.energy < 15;
  if (button(cv, 8, 184, 80, 30, tired ? "CAT NAP" : "AGAIN", tired ? BLUE : GREEN)) {   // too tired for another round: nap instead
    if (tired) startLap();
    else if (res_.kind == 0) startLaser(); else if (res_.kind == 1) startScratch(); else if (res_.kind == 4) startKnock(); else startHeist();
  }
  if (button(cv, 96, 184, 80, 30, "HOME", PURPLE)) setScreen(S_HOME);
}

void Game::screenAlarmRing(Canvas& cv) {
  alarmRing_ += dt;
  if (alarmRing_ >= 2.4f) { alarmRing_ = 0; sound(SND_ALARM); }
  bool flash = ((int)(anim_ * 4) % 2) == 0;
  cv.clear(flash ? rgb(120, 40, 80) : rgb(70, 24, 60));
  cv.textCenter(W / 2, 10, "WAKE UP, HUMAN!", flash ? rgb(255, 240, 150) : rgb(255, 255, 255), 1);
  char b[16];
  snprintf(b, sizeof(b), "%02d:%02d", hour, minute);
  cv.textCenter(W / 2, 24, b, rgb(255, 255, 255), 3);
  CatDraw d;
  d.breed = pet.lookBreed(); d.fat = pet.fatLook(); d.size = pet.visualSize() * 1.7f; d.kit = pet.kitFactor();
  d.pose = POSE_FRONT; d.face = FACE_MEOW; d.t = anim_;
  fitCatHeight(d, 160, 48);   // a big chonk's ears stay under the clock
  int sh = (int)(sinf(anim_ * 30) * 2);
  cv.ellipse(92 + sh, 160, 44, 6, rgb(40, 14, 40));
  drawCat(cv, 92 + sh, 160, d);
  drawBubble(cv, 92, 78, ((int)(anim_ * 3) % 2) ? "MEOW!!" : "MROW!");
  if (button(cv, 8, 184, 80, 32, "STOP", GREEN)) {
    toast("Good morning!");
    pet.petIt(4);
    setScreen(S_HOME);
  }
  if (button(cv, 96, 184, 80, 32, "SNOOZE 5m", PURPLE)) {
    snoozeEpoch_ = epoch + 300;
    setScreen(S_HOME);
  }
  if (scrT_ > 300.0f) setScreen(S_HOME);
}

static void drawBlackHole(Canvas& cv, int cx, int cy, float r, float t) {
  if (r < 1) return;
  for (int arm = 0; arm < 4; arm++) {
    for (int s = 0; s < 60; s++) {
      float ang = arm * 1.5708f + s * 0.16f + t * 2.2f;
      float rad = r * (0.2f + 0.8f * s / 60.0f);
      int x = cx + (int)(cosf(ang) * rad), y = cy + (int)(sinf(ang) * rad * 0.55f);
      Color c = s < 20 ? rgb(255, 170, 60) : (s < 40 ? rgb(220, 70, 170) : rgb(110, 60, 200));
      cv.set(x, y, c);
      cv.set(x + 1, y, shade(c, 70));
    }
  }
  cv.ellipse(cx, cy, (int)(r * 0.28f), (int)(r * 0.16f), rgb(0, 0, 0));
  cv.ellipse(cx, cy, (int)(r * 0.28f), (int)(r * 0.16f), rgb(0, 0, 0));
}

// The cat ambulance: health hit 0, so a tiny ambulance takes them to the vet (9 seconds; the bill is already paid in checkCollapse).
void Game::screenAmbulance(Canvas& cv) {
  ambT_ += dt;
  const float rt = ambT_;                    // real seconds (siren, wheels, blinking)
  const float t = rt * (4.0f / AMB_SECS);    // the choreography below runs on a 4-second timeline, stretched to AMB_SECS
  Canvas& c = cv;
  if (t >= 4.0f) { setScreen(S_HOME); return; }
  auto ease = [](float u) { u = clampf(u, 0, 1); return u * u * (3.0f - 2.0f * u); };
  const float drive = ease((t - 2.1f) / 1.5f);              // 2.1 .. 3.6 s: off to the clinic (the camera follows)
  const int cam = (int)(130.0f * drive);
  // sky, clouds
  for (int y = 0; y < 134; y++) c.hline(0, y, W, mix(rgb(112, 184, 244), rgb(214, 238, 252), y * 256 / 134));
  for (int i = 0; i < 3; i++) {
    const int cx = ((i * 83 + 20 - cam / 3) % (W + 60) + (W + 60)) % (W + 60) - 30, cy = 20 + i * 17;
    c.ellipse(cx, cy, 15, 5, rgb(255, 255, 255)); c.ellipse(cx + 9, cy - 3, 9, 5, rgb(255, 255, 255));
  }
  // the vet clinic (far along the street)
  { const int x0 = 238 - cam;
    c.rect(x0, 62, 92, 76, rgb(250, 226, 196)); c.frame(x0, 62, 92, 76, rgb(196, 160, 130));
    c.rect(x0 - 4, 52, 100, 11, rgb(190, 84, 90)); c.hline(x0 - 4, 62, 100, rgb(130, 52, 60));
    c.rect(x0 + 12, 66, 40, 15, rgb(255, 255, 255)); c.frame(x0 + 12, 66, 40, 15, rgb(190, 84, 90));
    c.rect(x0 + 18, 69, 3, 9, rgb(224, 50, 62)); c.rect(x0 + 15, 72, 9, 3, rgb(224, 50, 62));
    c.text(x0 + 28, 70, "VET", rgb(190, 50, 60));
    c.rect(x0 + 58, 88, 22, 18, rgb(170, 220, 250)); c.frame(x0 + 58, 88, 22, 18, rgb(130, 100, 90)); c.vline(x0 + 69, 88, 18, rgb(130, 100, 90));
    c.rect(x0 + 24, 100, 20, 38, t >= 3.6f ? rgb(255, 238, 170) : rgb(150, 104, 74)); c.frame(x0 + 24, 100, 20, 38, rgb(110, 76, 56));
    if (t >= 3.6f) { c.rect(x0 + 24, 100, 6, 38, rgb(120, 84, 60)); }   // the door swings open
    else c.disc(x0 + 40, 120, 1, rgb(250, 220, 120));
  }
  // street
  c.rect(0, 134, W, 4, rgb(180, 182, 190)); c.hline(0, 134, W, rgb(214, 216, 222));
  c.rect(0, 138, W, 30, rgb(84, 86, 100));
  for (int wx = -40; wx < 420; wx += 36) { const int x = wx - cam; if (x > -20 && x < W) c.rect(x, 154, 18, 2, rgb(236, 214, 110)); }
  // the ambulance and the stretcher
  const float axw = 50.0f + 140.0f * drive;                 // parked (siren on) until it drives off to the vet
  const int ax = (int)axw - cam;
  const float load = ease((t - 0.5f) / 1.3f);               // 0.5 .. 1.8 s: the stretcher is wheeled in from off-screen
  const float sxw = t < 1.8f ? -44.0f + 100.0f * load : axw + 6.0f;
  const int sx = (int)sxw - cam;
  const bool moving = t > 2.1f && t < 3.6f;
  const int bob = moving ? (int)(sinf(rt * 34.0f) * 0.9f + 0.9f) : 0;
  if (sx < W) {   // stretcher (drawn first: the ambulance's side covers the part that has rolled inside)
    c.rect(sx, 134, 36, 3, rgb(196, 200, 212)); c.rect(sx, 131, 36, 3, rgb(96, 168, 224));
    c.line(sx + 6, 137, sx + 3, 153, rgb(150, 154, 168)); c.line(sx + 30, 137, sx + 33, 153, rgb(150, 154, 168));
    c.disc(sx + 3, 154, 2, rgb(50, 50, 60)); c.disc(sx + 33, 154, 2, rgb(50, 50, 60));
    CatDraw d;
    d.breed = pet.lookBreed(); d.fat = pet.fatLook(); d.size = pet.visualSize() * 0.5f; d.kit = pet.kitFactor();
    d.pose = POSE_SLEEP; d.face = FACE_SAD; d.t = rt; d.faceLeft = false;
    CatMetrics m = drawCat(c, sx + 18, 131, d);
    (void)m;
  }
  { const int y = 118 + bob;
    c.rect(ax, y, 46, 30, rgb(246, 246, 250)); c.frame(ax, y, 46, 30, rgb(168, 174, 192));       // box
    c.rect(ax + 46, y + 10, 20, 20, rgb(246, 246, 250)); c.frame(ax + 46, y + 10, 20, 20, rgb(168, 174, 192));   // cab
    c.rect(ax + 54, y + 13, 10, 8, rgb(150, 205, 240)); c.rect(ax + 64, y + 22, 2, 3, rgb(255, 236, 140));
    c.rect(ax + 1, y + 20, 64, 3, rgb(226, 58, 70));                                          // stripe
    c.rect(ax + 21, y + 4, 5, 13, rgb(224, 50, 62)); c.rect(ax + 17, y + 8, 13, 5, rgb(224, 50, 62));   // cross
    const bool flip = ((int)(rt * 6.0f)) % 2 == 0;                                             // siren
    const Color l1 = flip ? rgb(255, 70, 70) : rgb(120, 40, 50), l2 = flip ? rgb(60, 90, 140) : rgb(80, 150, 255);
    c.rect(ax + 14, y - 3, 22, 3, rgb(120, 124, 140));
    c.rect(ax + 15, y - 6, 9, 3, l1); c.rect(ax + 26, y - 6, 9, 3, l2);
    if (flip) c.disc(ax + 20, y - 8, 4, rgb(255, 150, 150)); else c.disc(ax + 31, y - 8, 4, rgb(150, 190, 255));
    if (t > 0.2f && t < 2.0f) { c.rect(ax - 6, y + 2, 6, 26, rgb(214, 218, 230)); c.frame(ax - 6, y + 2, 6, 26, rgb(150, 156, 176)); }   // rear doors open
    else c.vline(ax + 1, y + 2, 26, rgb(190, 194, 208));
    const float spin = axw / 6.0f;
    for (int k = 0; k < 2; k++) {
      const int wx = ax + (k ? 54 : 12), wy = 150 + bob;
      c.disc(wx, wy, 6, rgb(36, 36, 44)); c.disc(wx, wy, 3, rgb(196, 198, 210));
      c.line(wx, wy, wx + (int)(cosf(spin) * 5), wy + (int)(sinf(spin) * 5), rgb(36, 36, 44));
    }
  }
  if (t >= 3.6f) { const float u = (t - 3.6f) / 0.4f;   // "+45" floats up from the door
    char hb[12]; snprintf(hb, sizeof(hb), "+45");
    c.textCenter(142, 99 - (int)(u * 10), hb, rgb(110, 240, 130));
  }
  // text panel
  c.rect(0, 168, W, H - 168, rgb(40, 30, 62)); c.hline(0, 168, W, rgb(120, 96, 168));
  char b[48];
  if (t < 2.1f) { snprintf(b, sizeof(b), "%s isn't well!", pet.d.name); c.textCenter(W / 2, 174, b, rgb(255, 150, 150)); c.textCenter(W / 2, 187, t < 0.5f ? "Calling the cat ambulance..." : "Careful... onto the stretcher", rgb(230, 222, 250)); }
  else if (t < 3.6f) { c.textCenter(W / 2, 174, "Rushing to the VET!", rgb(255, 230, 150)); c.textCenter(W / 2, 187, ((int)(rt * 6) % 2) ? "WEE-OO  WEE-OO" : "wee-oo  wee-oo", rgb(230, 222, 250)); }
  else { c.textCenter(W / 2, 174, "Checked in at the VET", rgb(255, 230, 150)); c.textCenter(W / 2, 187, "Health back up to 45", rgb(150, 240, 160)); }
  if (t >= 1.5f) {
    snprintf(b, sizeof(b), "Vet bill: %d hairballs", ambPaid_ + ambOwe_);
    c.textCenter(W / 2, 202, b, rgb(250, 230, 150));
    if (ambOwe_ > 0) { snprintf(b, sizeof(b), "IN DEBT! You owe %d", (int)sv.x8.debt); c.textCenter(W / 2, 213, b, rgb(255, 96, 96)); }
    else c.textCenter(W / 2, 213, "Paid in full", rgb(170, 160, 200));
  }
}

void Game::screenEnding(Canvas& cv) {
  endT_ += dt;
  const float t = endT_;
  Canvas& c = cv;
  bool showText = false;
  if (endingKind_ == END_RAN_AWAY) {
    // a moonlit meadow: they bolt across the grass and out of sight
    c.clear(rgb(30, 26, 56));
    for (int i = 0; i < 24; i++) c.set((i * 53) % W, (i * 31) % 110, rgb(255, 255, 220));
    c.disc(150, 40, 14, rgb(250, 244, 200));
    c.ellipse(40, 118, 60, 14, rgb(36, 50, 66)); c.ellipse(150, 124, 70, 12, rgb(32, 46, 60));   // far hills
    c.rect(0, 126, W, H - 126, rgb(44, 92, 64)); c.hline(0, 126, W, rgb(70, 124, 84));
    const int scroll = (int)(t * 70.0f);
    for (int i = 0; i < 70; i++) {   // blades of grass streaming past
      const int gy = 134 + (i * 29) % 80;
      const int gx = ((i * 47 - scroll * (1 + gy / 70)) % (W + 10) + (W + 10)) % (W + 10) - 5;
      const Color gc = (i % 3) ? rgb(70, 140, 86) : rgb(96, 164, 100);
      c.vline(gx, gy - 4, 4, gc); c.vline(gx + 2, gy - 3, 3, gc); c.vline(gx - 2, gy - 2, 2, gc);
    }
    const float x = -30 + t * 45.0f;
    const int hop = (int)(fabsf(sinf(t * 13.0f)) * 4.0f);   // a galloping bounce
    CatDraw d;
    d.breed = pet.lookBreed(); d.fat = pet.fatLook(); d.size = pet.visualSize(); d.kit = pet.kitFactor();
    d.pose = POSE_WALK; d.face = FACE_SAD; d.t = t * 2.6f; d.faceLeft = false;
    if (x < W + 40) {
      drawCatShadow(c, (int)x, 172, d, rgb(44, 92, 64));
      for (int k = 1; k <= 4; k++) c.disc((int)x - 14 - k * 7, 170 - hop / 2 - k, 3 - k / 2, mix(rgb(44, 92, 64), rgb(150, 190, 150), 256 - k * 56));   // dust puffs behind
      for (int k = 0; k < 3; k++) c.hline((int)x - 30 - k * 4, 150 + k * 9 - hop, 12 + k * 3, rgb(150, 190, 160));   // speed lines
      drawCat(c, (int)x, 172 - hop, d);
    }
    showText = t > 5.8f;
  } else if (endingKind_ == END_BLACK_HOLE) {
    // 0.0-1.8 the chonk swells, wobbling; 1.8 it bursts into specks of itself; 2.6-4.0 they swirl inward and collapse
    // into a black hole; 4.0-4.6 the hole churns (a portal); 4.6-6.0 a Rainbow Cat comes through it; 6.0-6.6 it closes
    const Color BG = rgb(8, 6, 20);
    const int hx = 92, hy = 100;   // where the hole opens
    struct Speck { float x, y, vx, vy, spin; Color c; };
    static Speck sp[420]; static int nsp = 0; static bool got = false;
    if (t < 0.05f) { got = false; nsp = 0; }
    c.clear(BG);
    auto stars = [&]() { for (int i = 0; i < 40; i++) c.set((i * 47 + (int)(t * (i % 3 + 1))) % W, (i * 29) % 200, rgb(200, 200, 255)); };
    CatDraw d;
    d.breed = pet.lookBreed(); d.fat = 1.3f; d.kit = pet.kitFactor(); d.pose = POSE_STAND; d.t = t;
    if (t < 1.8f || (!got && t < 4.0f)) {   // swelling up (and the moment it bursts: read its own pixels as specks)
      const float k = clampf(t / 1.8f, 0, 1);
      d.size = pet.visualSize() * (1.0f + 0.75f * k * k);
      d.face = k > 0.5f ? FACE_SURPRISE : FACE_NORMAL;
      const int wob = k > 0.3f ? (int)(sinf(t * (18.0f + 30.0f * k)) * (1.0f + 2.5f * k)) : 0;
      const int by = 150 + (int)(k * 12);
      if (t >= 1.8f && !got) {
        drawCat(c, hx, by, d);
        nsp = 0;
        int all = 0;   // how many pixels the cat covers: spread the specks evenly over all of it
        for (int y = 10; y < 214; y++) for (int x = 0; x < W; x++) if (c.get(x, y) != BG) all++;
        const int every = all / 400 + 1;
        int seen = 0;
        for (int y = 10; y < 214 && nsp < 420; y++) for (int x = 0; x < W && nsp < 420; x++) {
          const Color px = c.get(x, y);
          if (px == BG) continue;
          if ((seen++ % every) != 0) continue;
          Speck& s = sp[nsp++];
          const float dx = x - hx, dy = y - (by - 30), dl = sqrtf(dx * dx + dy * dy) + 1.0f;
          const float push = 30.0f + (float)((x * 31 + y * 17) % 40);
          s.x = (float)x; s.y = (float)y; s.vx = dx / dl * push; s.vy = dy / dl * push; s.c = px;
          s.spin = 2.0f + (float)((x * 13 + y * 7) % 30) / 10.0f;
        }
        got = true;
        c.clear(BG);
      } else {
        stars();
        if (k > 0.6f) for (int i = 0; i < 6; i++) c.set(hx - 40 + (int)((t * 97 + i * 31)) % 80, by - 70 + (i * 23) % 70, rgb(255, 220, 160));   // straining
        drawCat(c, hx + wob, by, d);
      }
    }
    if (got && t >= 1.8f) {
      stars();
      // the specks: a puff outward, then a swirl into the middle; the hole grows as they fall in
      const float k = clampf((t - 2.6f) / 1.4f, 0, 1);
      float holeR = 3.0f + 27.0f * k * k;
      if (t > 6.0f) holeR = 30.0f * clampf(1.0f - (t - 6.0f) / 0.6f, 0, 1);   // ...and it closes behind the new cat
      else if (t > 4.0f) holeR = 30.0f + 2.0f * sinf((t - 4.0f) * 9.0f);
      if (t > 2.4f) drawBlackHole(c, hx, hy, holeR, t);
      if (t < 4.0f) for (int i = 0; i < nsp; i++) {
        const Speck& s = sp[i];
        const float burst = clampf(t - 1.8f, 0, 0.8f);
        float x = s.x + s.vx * burst, y = s.y + s.vy * burst;
        if (k > 0) {   // spiral in: the radius shrinks, the angle winds faster the closer they get
          const float dx = x - hx, dy = y - hy;
          const float r0 = sqrtf(dx * dx + dy * dy), a0 = atan2f(dy, dx);
          const float r = r0 * powf(1.0f - k, 1.6f), a = a0 + s.spin * k * k * 3.0f;
          if (r < holeR * 0.7f) continue;   // swallowed
          x = hx + cosf(a) * r; y = hy + sinf(a) * r * 0.8f;
        }
        const Color col = k > 0.5f ? mix(s.c, rgb(150, 90, 255), (int)((k - 0.5f) * 2.0f * 200)) : s.c;   // stretched into the glow
        c.set((int)x, (int)y, col);
        if (k < 0.3f) c.set((int)x + 1, (int)y, col);
      }
      if (t > 4.6f) {   // a Rainbow Cat comes through the portal: small in the middle, growing as it steps out and down
        const float u = clampf((t - 4.6f) / 1.4f, 0, 1), e = u * u * (3.0f - 2.0f * u);
        CatDraw rk;
        rk.breed = BR_RAINBOW; rk.pose = POSE_FRONT; rk.size = 0.12f + 0.58f * e; rk.kit = 1.0f; rk.face = FACE_SURPRISE; rk.t = t;
        const int ry = hy + 14 + (int)(e * 24);
        for (int i = 0; i < 10; i++) {   // rainbow light pouring out
          static const Color RB[6] = {rgb(255, 80, 80), rgb(255, 170, 60), rgb(255, 240, 90), rgb(90, 230, 120), rgb(90, 160, 255), rgb(190, 110, 255)};
          const float ang = t * 3.0f + i * 0.628f, rr = 6.0f + (1.0f - u) * 30.0f + (i & 1) * 6.0f;
          c.set(hx + (int)(cosf(ang) * rr), hy + (int)(sinf(ang) * rr * 0.8f), RB[i % 6]);
        }
        drawCat(c, hx, ry, rk);
        if (((int)(t * 8)) % 2) c.set(80 + (int)(t * 13) % 24, 96 + (int)(t * 7) % 30, rgb(255, 255, 180));
      }
    }
    showText = t > 6.9f;
  } else if (endingKind_ == END_SUN) {
    // they shrink into a tiny bright sun... which pops, and out tumbles a hamster
    for (int y = 0; y < H; y++) c.hline(0, y, W, mix(rgb(70, 40, 120), rgb(255, 170, 120), y * 256 / H));
    for (int i = 0; i < 24; i++) if (sinf(t * 2 + i) > 0.2f) c.set((i * 47) % W, (i * 29) % 120, rgb(255, 240, 220));
    c.rect(0, 142, W, H - 142, rgb(90, 60, 120));   // (the ground sits above the text panel, so the hamster lands in view)
    const int sx = 92, sy = 96;
    const float shrink = clampf((t - 1.0f) / 2.4f, 0, 1);      // 1.0 .. 3.4 s: the cat shrinks into the sun
    float sunR = 2 + 7 * clampf((t - 1.0f) / 2.4f, 0, 1);
    if (t > 4.0f) sunR = 9 + 3 * sinf((t - 4.0f) * 6.0f) * clampf(1.0f - (t - 4.0f) / 2.0f, 0, 1);
    if (t < 3.4f) {
      CatDraw d;
      d.breed = pet.lookBreed(); d.fat = -0.3f; d.size = pet.visualSize() * (1.0f - shrink); d.kit = pet.kitFactor();
      d.pose = POSE_SIT; d.face = FACE_HAPPY; d.t = t;
      const int cy = 142 - (int)(shrink * (142 - sy - 4));
      if (d.size > 0.1f) drawCat(c, sx, cy, d);
    }
    if (t > 1.0f) {   // the sun: a warm glow, turning rays, a white-hot core
      const int R = (int)sunR;
      c.disc(sx, sy, R + 6, rgb(255, 200, 110)); c.disc(sx, sy, R + 3, rgb(255, 224, 140));
      for (int k = 0; k < 8; k++) {
        const float a = t * 1.5f + k * 0.785f; const int L = R + 6 + ((k & 1) ? 3 : 6);
        c.line(sx + (int)(cosf(a) * (R + 4)), sy + (int)(sinf(a) * (R + 4)), sx + (int)(cosf(a) * L), sy + (int)(sinf(a) * L), rgb(255, 236, 150));
      }
      c.disc(sx, sy, R, rgb(255, 246, 200)); c.disc(sx, sy, R > 3 ? R - 3 : 1, rgb(255, 255, 255));
    }
    if (t > 3.3f && t < 3.6f) c.disc(sx, sy, 30, rgb(255, 255, 240));   // pop!
    if (t > 4.4f) {   // a hamster tumbles out and lands on the ground
      const float u = clampf((t - 4.4f) / 0.8f, 0, 1);
      CatDraw k; k.breed = BR_HAMSTER; k.pose = POSE_FRONT; k.size = 0.75f; k.face = FACE_SURPRISE; k.t = t;
      k.blink = ((int)(t * 2.2f) % 7) == 0;
      const int hy = sy + (int)((142 - sy) * u) - (int)(sinf(u * 3.14159f) * 18);
      c.ellipse(sx, 143, 14, 2, rgb(60, 40, 90));
      drawCat(c, sx, hy, k);
      if (((int)(t * 8)) % 2) c.set(80 + (int)(t * 13) % 24, 112 + (int)(t * 7) % 24, rgb(255, 255, 200));
    }
    showText = t > 6.4f;
  } else {
    c.clear(rgb(140, 210, 250));
    c.rect(0, 150, W, 74, rgb(92, 180, 90));
    c.disc(150, 36, 16, rgb(255, 236, 110));
    for (int i = 0; i < 9; i++) {
      int fx = 10 + i * 21, fy = 160 + (i * 17) % 40;
      c.line(fx, fy, fx, fy + 6, rgb(40, 120, 60));
      c.disc(fx, fy - 1, 3, i % 2 ? rgb(250, 110, 150) : rgb(255, 230, 90));
    }
    c.rect(40, 130, 104, 6, rgb(150, 100, 60));
    c.rect(46, 136, 5, 14, rgb(110, 74, 44));
    c.rect(132, 136, 5, 14, rgb(110, 74, 44));
    CatDraw d;
    d.breed = pet.lookBreed(); d.fat = pet.fatLook(); d.size = pet.visualSize(); d.kit = 0;
    d.pose = POSE_SIT; d.face = FACE_HAPPY; d.t = t; d.blink = ((int)(t * 2) % 5) == 0;
    drawCat(c, 92, 132, d);
    if (((int)(t * 5)) % 3 == 0) c.set(70 + (int)(t * 17) % 50, 80 + (int)(t * 11) % 40, rgb(255, 255, 255));
    cv.text(100, 70 + (int)(sinf(t * 2) * 3), "z", rgb(250, 250, 255));
    showText = t > 3.0f;
  }
  if (showText) {
    if (endingKind_ == END_RAN_AWAY) {
      c.panel(6, 118, 172, 70, rgb(30, 22, 46), GOLD);
      for (int i = 0; i < 6; i++) c.textCenter(W / 2, 124 + i * 10, endMsg_[i], i == 5 ? GOLD : (i == 4 ? rgb(255, 190, 170) : TXT));
    } else {
      c.panel(6, 150 - 4, 172, 66, rgb(30, 22, 46), GOLD);
      for (int i = 0; i < 3; i++) c.textCenter(W / 2, 158 + i * 11, endMsg_[i], TXT);
    }
    if (secretNew_ && (endingKind_ == END_BLACK_HOLE || endingKind_ == END_SUN)) {   // first time: a secret breed!
      const Color sc = mix(rgb(255, 214, 120), rgb(255, 140, 220), (int)((0.5f + 0.5f * sinf(t * 4)) * 256));
      c.panel(10, 6, 164, 26, rgb(30, 22, 46), sc);
      c.textCenter(W / 2, 10, "SECRET UNLOCKED!", sc);
      c.textCenter(W / 2, 20, endingKind_ == END_SUN ? "Hamster: the fastest" : "Rainbow Cat: the strongest", TXT);
    }
    const float textAt = endingKind_ == END_RAN_AWAY ? 5.8f : (endingKind_ == END_BLACK_HOLE ? 6.9f : (endingKind_ == END_SUN ? 6.4f : 3.0f));
    if (t < textAt + 1.5f) { if (((int)(t * 2)) & 1) cv.textCenter(W / 2, 200, "...", DIM); }   // (a moment to read, before the button)
    else if (button(cv, 22, 192, 140, 24, "A NEW KITTEN!", GREEN)) {
      strncpy(nameBuf_, randomName(), sizeof(nameBuf_) - 1);
      setScreen(S_NAME);
    }
  } else if (t > 1.0f && in.tap) {
    endT_ += 2.0f;  // tap to skip ahead
  }
}


// ---------------------------------------------------------------------------------------
// wardrobe
// ---------------------------------------------------------------------------------------

void Game::updateDecor() { updateToys(); }


struct Costume { uint8_t slot, id; const char* name; int price; };   // price 0 = reward only (Explore: quests, secrets, the King rematch), -1 = only from a gift box
static const Costume COSTUMES[] = {   // (at most 7 per slot: the wardrobe shows "none" + 7)
  {0, HAT_TOP, "Top Hat", 30}, {0, HAT_PARTY, "Party Hat", 15}, {0, HAT_CROWN, "Crown", 0}, {0, HAT_WIZARD, "Wizard Hat", 0},
  {0, HAT_SANTA, "Santa Hat", 20}, {0, HAT_FLOWER, "Flower Crown", 0}, {0, HAT_COWBOY, "Cowboy Hat", 0},
  {1, GL_SUN, "Sunglasses", 25}, {1, GL_HEART, "Heart Glasses", 25}, {1, GL_STAR, "Star Shades", -1}, {1, GL_MONOCLE, "Monocle", -1}, {1, GL_3D, "3-D Specs", -1}, {1, GL_GOGGLES, "Cave Goggles", 0},
  {2, NK_BOW, "Bow Tie", 15}, {2, NK_SCARF, "Scarf", 20}, {2, NK_BELL, "Bell Collar", 10}, {2, NK_BANDANA, "Bandana", 0}, {2, NK_PEARLS, "Pearls", -1}, {2, NK_TIE, "Necktie", -1}, {2, NK_LACE, "Lace Collar", 0},
  {3, BD_CAPE, "Hero Cape", 30}, {3, BD_SADDLE, "Hamster Saddle", 45}, {3, BD_WINGS, "Angel Wings", 0}, {3, BD_SHIRT, "T-Shirt", 20}, {3, BD_BFLY, "Butterfly Wings", 35}};
static const char* WARD_TAB[4] = {"HAT", "EYES", "NECK", "BODY"};

static uint8_t& ownOf(SaveData& s, int slot) { return slot == 0 ? s.ownHat : slot == 1 ? s.ownGl : slot == 2 ? s.ownNeck : s.ownCollar; }
static uint8_t& wornOf(SaveData& s, int slot) { return slot == 0 ? s.wornHat : slot == 1 ? s.wornGl : slot == 2 ? s.wornNeck : s.wornCollar; }

// ---------------------------------------------------------------------------------------
// Gift boxes. A wrapped box turns up on the floor at home on the cat's birthday, at Halloween / the holidays / New
// Year, and after GIFT_STREAK well-cared-for days in a row. Tapping it opens one of the gift-only costumes (price -1
// above) that isn't owned yet, and the cat puts it on; once they are all owned it holds hairballs instead.
// State lives in sv.pad9: [0] the box waiting (0 none, else why: 1 birthday, 2 holiday, 3 good care),
// [1] good days in a row so far, [2] the day last counted (epoch day % 255 + 1; 0 = never).
// ---------------------------------------------------------------------------------------
void Game::giftArrives(int why) {
  if (!sv.hasCat || sv.pad9[0]) return;   // (one box at a time)
  sv.pad9[0] = (uint8_t)why;
  toast(why == 3 ? "Such good care: a gift box!" : "A gift box has arrived!");
  sound(SND_POP);
  dirty_ = true;
}

void Game::openGift() {
  if (!sv.pad9[0]) return;
  sv.pad9[0] = 0;
  const Costume* pick[16]; int n = 0;
  for (const Costume& c : COSTUMES) if (c.price < 0 && !((ownOf(sv, c.slot) >> c.id) & 1) && n < 16) pick[n++] = &c;
  static const Color cc[4] = {rgb(255, 120, 170), rgb(255, 220, 80), rgb(120, 220, 255), rgb(160, 255, 140)};
  for (int i = 0; i < 14; i++) spawn((float)GIFT_X + (float)((int)(pet.rnd() % 30) - 15), (float)GIFT_Y - 12, 7, cc[i & 3], (float)((int)(pet.rnd() % 60) - 30), -40.0f - (pet.rnd() % 30), 1.5f);
  if (n) {
    const Costume& c = *pick[pet.rnd() % n];
    ownOf(sv, c.slot) |= (uint8_t)(1u << c.id);
    wornOf(sv, c.slot) = c.id;   // they try it on straight away
    toast("A gift: the %s!", c.name);
    toast("It's in the Wardrobe now.");
    if (a_.act == A_IDLE || a_.act == A_GROOM || a_.act == A_STARE) { a_.act = A_HAPPY; a_.actT = 1.8f; }
  } else {
    toast("A gift: +10 hairballs!");
    giveCoins(10);
  }
  pet.d.fun = clampf(pet.d.fun + 10, 0, 100);
  sound(SND_WIN);
  autosave(true);
}

// The box on the floor (front left corner, clear of the furniture), with a bobbing "tap me" marker like a hairball's.
void Game::drawGiftBox(Canvas& cv) {
  if (!sv.pad9[0]) return;
  const Color ol = rgb(30, 20, 36), box = rgb(92, 180, 226), boxD = rgb(58, 132, 188), rib = rgb(255, 110, 170), ribL = rgb(255, 190, 215);
  const int x = GIFT_X, y = GIFT_Y - (fmodf(anim_, 2.4f) < 0.25f ? 1 : 0);   // a little hop now and then
  cv.ellipse(x, GIFT_Y + 1, 10, 2, rgb(40, 30, 30));                                        // shadow
  cv.rect(x - 9, y - 12, 18, 13, ol); cv.rect(x - 8, y - 11, 16, 11, box); cv.rect(x - 8, y - 4, 16, 4, boxD);   // the box
  cv.rect(x - 10, y - 15, 20, 5, ol); cv.rect(x - 9, y - 14, 18, 3, box); cv.hline(x - 9, y - 14, 18, rgb(150, 214, 244));   // its lid
  cv.rect(x - 1, y - 14, 3, 14, rib); cv.vline(x - 1, y - 14, 14, ribL);                    // ribbon
  cv.disc(x - 3, y - 17, 2, rib); cv.disc(x + 4, y - 17, 2, rib); cv.set(x - 3, y - 18, ribL); cv.set(x + 4, y - 18, ribL); cv.rect(x, y - 17, 2, 2, ribL);   // bow
  if (((int)(anim_ * 3)) % 4 == 0) cv.set(x + 7, y - 20, rgb(255, 255, 255));               // sparkle
  cv.text(x + 12, GIFT_Y - 22 - (int)((0.5f + 0.5f * sinf(anim_ * 4)) * 2), "!", rgb(255, 226, 120));   // "tap me", beside the box (above it is the litter tray)
}

void Game::devGift() { sv.pad9[0] = 0; giftArrives(1); }   // dev: a gift box, now (the web page's developer panel)

void Game::screenWardrobe(Canvas& cv) {
  drawBackdrop(cv, false);
  panelTitle(cv, "WARDROBE", 4);
  char b[48];
  coinText(b, sizeof(b));
  drawIcon(cv, 6, CORNER + 4, 9);
  cv.text(CORNER + 12, 5, b, coinColor(GOLD));
  const int tab = wardTab_;
  for (int i = 0; i < 4; i++) if (button(cv, 4 + i * 45, 18, 43, 15, WARD_TAB[i], tab == i ? GREEN : PURPLE)) { wardTab_ = i; wardSel_ = wornOf(sv, i); sound(SND_TAP); }
  // cells: "none" + the items of this slot
  int ids[8], n = 0; ids[n++] = 0;
  for (const Costume& c : COSTUMES) if (c.slot == tab) ids[n++] = c.id;
  if (wardSel_ != 0) { bool ok = false; for (int k = 0; k < n; k++) ok |= ids[k] == wardSel_; if (!ok) wardSel_ = 0; }
  uint8_t pv[4] = {sv.wornHat, sv.wornGl, sv.wornNeck, sv.wornCollar};
  for (int k = 0; k < n; k++) {
    int cx = 4 + (k % 4) * 44, cy = 38 + (k / 4) * 38;
    bool sel = wardSel_ == ids[k];
    bool owned = (ownOf(sv, tab) >> ids[k]) & 1;
    cv.panel(cx, cy, 42, 36, sel ? rgb(88, 66, 130) : PANEL, sel ? GOLD : BORDER);
    CatDraw d = catLook(POSE_FRONT, FACE_HAPPY);
    d.size = 0.5f; d.kit = 0; d.t = anim_; d.blink = false; d.faceLeft = false;
    d.hat = pv[0]; d.glasses = pv[1]; d.neck = pv[2]; d.body = pv[3];
    if (tab == 0) d.hat = ids[k]; else if (tab == 1) d.glasses = ids[k]; else if (tab == 2) d.neck = ids[k]; else d.body = ids[k];
    drawCat(cv, cx + 21, cy + 33, d);
    if (!owned && ids[k]) { cv.dimRect(cx + 1, cy + 1, 40, 34, 50); }
    if (pv[tab] == ids[k]) cv.text(cx + 3, cy + 3, "*", GOLD);
    if (tapIn(cx, cy, 42, 36)) { wardSel_ = ids[k]; sound(SND_TAP); }
  }
  int sid = wardSel_;
  const Costume* sc = nullptr;
  for (const Costume& c : COSTUMES) if (c.slot == tab && c.id == sid) sc = &c;
  bool owned = sid == 0 || ((ownOf(sv, tab) >> sid) & 1);
  cv.rect(4, 116, 176, 34, PANEL); cv.frame(4, 116, 176, 34, BORDER);
  cv.text(8, 119, sc ? sc->name : "No costume", GOLD);
  if (!sc) cv.text(8, 130, "Plain and fluffy.", TXT);
  else if (owned) cv.text(8, 130, "In your wardrobe.", rgb(150, 255, 190));
  else if (sc->price > 0) { snprintf(b, sizeof(b), "Costs %d hairballs.", sc->price); cv.text(8, 130, b, TXT); }
  else if (sc->price < 0) cv.text(8, 130, "Rare! Comes in a gift box.", rgb(255, 170, 210));
  else cv.text(8, 130, "Rare! Found while Exploring.", rgb(255, 214, 120));
  int set = 0; for (int k = 0; k < 3; k++) if (pv[k]) set++;
  if (set + (pv[3] ? 1 : 0) >= 3) cv.text(8, 140, "Dressed to impress!", rgb(255, 180, 220));
  // preview
  {
    CatDraw d = catLook(POSE_SIT, FACE_HAPPY);
    d.size = 0.9f; d.t = anim_; d.blink = false; d.faceLeft = false;
    d.hat = pv[0]; d.glasses = pv[1]; d.neck = pv[2]; d.body = pv[3];
    if (tab == 0) d.hat = sid; else if (tab == 1) d.glasses = sid; else if (tab == 2) d.neck = sid; else d.body = sid;
    cv.ellipse(42, 216, 24, 4, rgb(26, 20, 44));
    drawCat(cv, 42, 216, d);
  }
  bool worn = pv[tab] == sid;
  if (owned) {
    if (button(cv, 84, 156, 96, 22, worn ? "WEARING" : (sid ? "WEAR" : "TAKE OFF"), worn ? PURPLE : GREEN, !worn)) { wornOf(sv, tab) = (uint8_t)sid; sound(SND_MEOW); autosave(true); }
  } else if (sc && sc->price > 0) {
    snprintf(b, sizeof(b), "BUY  %d", sc->price);
    bool can = sv.coins >= (uint32_t)sc->price;
    if (button(cv, 84, 156, 96, 22, b, can ? GREEN : rgb(120, 70, 80), true)) {
      if (!can) { toast("Not enough hairballs!"); sound(SND_LOSE); }
      else { sv.coins -= sc->price; ownOf(sv, tab) |= (uint8_t)(1u << sid); wornOf(sv, tab) = (uint8_t)sid; sound(SND_COIN); toast("Bought %s!", sc->name); autosave(true); }
    }
  } else button(cv, 84, 156, 96, 22, sc && sc->price < 0 ? "GIFT BOX ONLY" : "EXPLORE REWARD", rgb(100, 80, 110), false);
  if (button(cv, 84, 196, 96, 22, "BACK", BLUE)) { if (wardFromShop_) { wardFromShop_ = false; shopPage_ = -1; setScreen(S_SHOP); } else setScreen(S_MENU); }
  updateParticles(dt);
  drawToasts(cv);
}

}  // namespace cat
