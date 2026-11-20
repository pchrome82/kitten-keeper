// Extra activities: tilt-pour feeding, Lap Cat, Knock-it-off, Maze Run and Explore.
#include "game.h"
#include "gfx.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace cat {

static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static const Color C_PANEL = rgb(44, 34, 70), C_BORDER = rgb(120, 92, 160), C_GOLD = rgb(252, 214, 96);
static const Color C_TXT = rgb(236, 230, 248), C_DIM = rgb(160, 150, 186), C_GREEN = rgb(70, 170, 90);
static const Color C_PURP = rgb(110, 80, 160);

void Game::drawLittleCat(Canvas& cv, int x, int y, Pose pose, Face face, bool left, float size, int topMin) {
  CatDraw d;
  d.breed = pet.lookBreed(); d.fat = pet.fatLook(); d.size = pet.visualSize() * size; d.kit = pet.kitFactor();
  d.pose = pose; d.face = face; d.faceLeft = left; d.t = anim_;
  if (topMin >= 0) fitCatHeight(d, y, topMin);   // a big (chonky, fluffy) cat: shrink just enough that its ears stay below topMin
  d.blink = blinkT_ < 0.0f && blinkT_ > -0.14f;
  gShadowKey = false;
  drawCatShadow(cv, x, y, d, rgb(26, 20, 44));
  drawCat(cv, x, y, d);
}

// Tiny side-view cat for Explore, Maze Run and the neighbours (an ~11x10 pixel sprite, no snout), drawn in the
// breed's own markings: Siamese points, a tuxedo's white bib and socks, calico patches, tabby stripes, the Void
// Cat's yellow eyes, the Sphynx's big ears and whip tail, the Skogkatt's ruff, ear tufts and plume - and the Hamster
// as a hamster. `scale` grows or shrinks the whole sprite (Explore: skinny cats down to 0.8x, chonks up to 1.3x).
// (Kittens never go exploring, so there's no kitten size here.)
float miniCatScale(float fat) {
  if (fat >= 0) return 1.0f + 0.3f * (fat > 1.3f ? 1.0f : fat / 1.3f);   // fatLook tops out at 1.3: that's 1.3x
  return 1.0f - 0.2f * (fat < -0.3f ? 1.0f : -fat / 0.3f);                // ...and bottoms out at -0.3 (skinny): 0.8x
}

void drawMiniCat(Canvas& cv, int x, int y, bool left, float t, int breed, float fat, bool moving, float scale) {
  const Color ol = rgb(28, 18, 30);
  Color coat = rgb(196, 188, 178), coat2 = rgb(150, 140, 132), belly = rgb(236, 230, 220), ear = rgb(240, 160, 170), eye = rgb(30, 24, 30);
  int pat = PAT_SOLID;
  if (breed >= 0 && breed < NUM_BREED_SLOTS) {
    const Breed& b = BREEDS[breed];
    coat = b.coat; coat2 = b.coat2; belly = b.belly; ear = b.earIn; pat = b.pat;
    if (pat == PAT_SOLID && b.coat == b.coat2) coat2 = shade(coat, 80);
  }
  if (breed == BR_RAINBOW) {   // the little Rainbow Cat shimmers through soft pastel colours
    static const Color hues[6] = {rgb(255, 170, 190), rgb(255, 214, 150), rgb(240, 250, 160), rgb(170, 240, 190), rgb(160, 210, 255), rgb(210, 180, 255)};
    const float u = t * 0.6f; const int i = ((int)u) % 6; const int f = (int)((u - (int)u) * 256);
    coat = mix(hues[i], hues[(i + 1) % 6], f); coat2 = mix(hues[(i + 2) % 6], hues[(i + 3) % 6], f); belly = rgb(255, 255, 255);
  }
  const bool skog = breed == 6, voidCat = breed == 3, hamster = breed == BR_HAMSTER, sphynx = pat == PAT_SPHYNX;
  if (voidCat) eye = rgb(250, 214, 80);                 // the Void Cat stares with big yellow eyes
  if (scale < 0.3f) scale = 0.3f;
  const int s = left ? -1 : 1;                          // facing
  const int fw = fat > 1.0f ? 2 : (fat > 0.5f ? 1 : 0);   // chonks are a pixel or two wider (as well as scaled up)
  // one sprite pixel = a little block, so the sprite scales cleanly (scale 1 = one screen pixel, as before)
  auto px = [&](int dx, int dy, Color c) {
    const int x0 = (int)floorf(dx * scale), x1 = (int)floorf((dx + 1) * scale) - 1;
    const int y0 = (int)floorf(dy * scale), y1 = (int)floorf((dy + 1) * scale) - 1;
    if (x1 < x0 || y1 < y0) return;
    const int ax = s > 0 ? x + x0 : x - x1;
    cv.rect(ax, y + y0, x1 - x0 + 1, y1 - y0 + 1, c);
  };
  auto hl = [&](int dx0, int dx1, int dy, Color c) { for (int d = dx0; d <= dx1; d++) px(d, dy, c); };
  const int step = moving ? ((int)(t * 10)) & 1 : 0;

  if (hamster) {   // a round little hamster: no tail, tiny round ears, white tummy, puffy cheeks
    for (int dy = -8; dy <= -1; dy++) { const int w = (dy == -8 || dy == -1) ? 3 : (dy == -7 ? 4 : 5); hl(-w - fw, w + fw, dy, ol); }
    px(-2, -9, ol); px(-1, -9, ol); px(2, -9, ol); px(3, -9, ol);                    // ears
    for (int dy = -7; dy <= -2; dy++) { const int w = (dy == -7 || dy == -2) ? 3 : 4; hl(-w - fw, w + fw, dy, coat); }
    px(-2, -8, ear); px(2, -8, ear);
    hl(-2 - fw, 2 + fw, -3, belly); hl(-1 - fw, 1 + fw, -2, belly);                    // tummy
    px(3 + fw, -5, belly); px(4 + fw, -4, belly);                                      // cheek pouch
    px(2, -6, eye); px(4 + fw, -6, ear);                                               // eye, nose
    px(-3 - fw, -6, coat2); px(-2 - fw, -7, coat2);                                    // darker back
    const int feet[2] = {-2 - fw, 2 + fw};
    for (int i = 0; i < 2; i++) px(feet[i], (step && i == 0) || (!step && moving && i == 1) ? -1 : 0, ear);
    return;
  }

  // ---- outline pass (1 px bigger), then fill ----
  for (int dy = -6; dy <= -1; dy++) hl(-5 - fw, 3 + fw, dy, ol);          // body
  for (int dy = -10; dy <= -4; dy++) hl(1, 7, dy, ol);                     // head
  px(1, -11, ol); px(2, -11, ol); px(5, -11, ol); px(6, -11, ol);           // ear tips
  if (sphynx || skog) { px(2, -12, ol); px(6, -12, ol); }                   // Sphynx: big bat ears / Skogkatt: lynx tufts
  if (skog) hl(1, 3, -3, ol);                                               // the ruff under the chin
  const int tailW = sphynx ? 1 : (skog ? 3 : 2);                           // whip tail / normal / big plume (outline width)
  for (int k = 0; k < 5; k++) for (int w = 0; w < tailW; w++) px(-5 - fw - k / 2 - w, -5 - k, ol);
  // body
  for (int dy = -5; dy <= -2; dy++) hl(-4 - fw, 2 + fw, dy, coat);
  hl(-3 - fw, 1 + fw, -2, belly);
  // head
  for (int dy = -9; dy <= -5; dy++) hl(2, 6, dy, coat);
  px(2, -9, ol); px(6, -9, ol); px(2, -5, ol); px(6, -5, ol);              // round the head's corners
  px(2, -10, coat); px(6, -10, coat); px(3, -10, ear); px(5, -10, ear);   // ears
  if (sphynx) { px(2, -11, coat); px(6, -11, coat); }
  if (skog) { px(2, -4, belly); px(3, -4, belly); hl(1, 2, -3, belly); }   // a fluffy pale ruff
  // markings
  Color tailCol = coat, legCol = coat, pawCol = ol;
  switch (pat) {
    case PAT_TABBY:   // stripes over the back, an M on the forehead, a ringed tail
      for (int k = -3 - fw; k <= 1 + fw; k += 2) { px(k, -5, coat2); px(k, -4, coat2); }
      px(3, -9, coat2); px(5, -9, coat2); px(4, -8, coat2);
      break;
    case PAT_POINTS:  // Siamese: dark ears, mask, legs and tail on a cream coat
      px(2, -10, coat2); px(6, -10, coat2); hl(3, 5, -6, coat2); px(4, -7, coat2);
      tailCol = coat2; legCol = coat2;
      break;
    case PAT_TUX:     // tuxedo: white bib, muzzle and socks
      hl(0, 2 + fw, -4, belly); hl(-1, 2 + fw, -3, belly); hl(3, 5, -5, belly); px(4, -6, belly);
      pawCol = belly;
      break;
    case PAT_CALICO:  // white with orange and black patches
      hl(-4 - fw, -2 - fw, -5, coat2); hl(-4 - fw, -3 - fw, -4, coat2); px(2, -10, coat2); hl(2, 3, -9, coat2);
      hl(0, 1, -5, rgb(54, 46, 52)); px(1, -4, rgb(54, 46, 52)); hl(5, 6, -9, rgb(54, 46, 52)); px(6, -10, rgb(54, 46, 52));
      tailCol = coat2;
      break;
    case PAT_SPHYNX:  // a few wrinkles on the brow
      px(3, -9, coat2); px(5, -9, coat2);
      break;
    default: break;
  }
  px(5, -7, eye); px(3, -7, eye);                                          // eyes
  px(4, -6, pat == PAT_POINTS ? shade(ear, 60) : ear);                     // tiny nose, flush with the face
  const int fillW = tailW > 1 ? tailW - 1 : 1;                             // tail fill (inside its outline)
  for (int k = 0; k < 5; k++) {
    const Color c = pat == PAT_TABBY ? ((k & 1) ? coat2 : coat) : tailCol;
    for (int w = 0; w < fillW; w++) px(-5 - fw - k / 2 - w, -5 - k, c);
  }
  // legs: diagonal pairs lift in turn while walking
  const int legs[4] = {-3 - fw, -1, 1 + fw, 3};
  for (int i = 0; i < 4; i++) {
    const bool lifted = step && (i == 0 || i == 2) ? true : (!step && moving && (i == 1 || i == 3));
    const int foot = lifted ? -1 : 0;
    px(legs[i], -1, legCol);
    px(legs[i], foot, lifted ? (pawCol == ol ? legCol : pawCol) : pawCol);
  }
}

static void dotsBg(Canvas& cv, Color a, Color b) {
  vgrad(cv, 0, 0, W, H, a, b);
}

// ---------------------------------------------------------------------------------------
// TILT-POUR FEEDING
// ---------------------------------------------------------------------------------------

void Game::debugStart(int which) {
  if (!sv.hasCat) return;
  if (which == 7) {   // just the ambulance cutscene (no charge, nothing changes)
    ambPaid_ = VET_FEE; ambOwe_ = 0; ambT_ = 0;
    wake(); sound(SND_SAD); setScreen(S_AMBULANCE);
    return;
  }
  pet.sleeping = false; pet.d.hunting = 0;
  if (pet.d.energy < 60) pet.d.energy = 60;
  switch (which) {
    case 0: startPour(); break;
    case 1: startLap(); break;
    case 2: startKnock(); break;
    case 3: startHeist(); break;
    case 5: startScratch(); break;
    case 6: startLaser(); break;
    case 4: if (pet.stage() < STAGE_ADULT) pet.d.age = AGE_ADULT + 60; sv.exploreTrips = 0; startExplore(); break;
  }
}

void Game::startPour() {
  memset(&pg_, 0, sizeof(pg_));
  pg_.seed = plat->millis() * 2654435761u + 12345u;
  setScreen(S_POUR);
}

// ---- kibble pieces for the pour game ----
namespace {
const int KB_X = 64, KB_Y = 162;   // bowl centre (same as the drawing below)
inline float kr(uint32_t& s) { s = s * 1664525u + 1013904223u; return (float)(s >> 8) / 16777216.0f; }   // 0..1
const Color KIB_COL[4] = {rgb(214, 128, 60), rgb(186, 104, 48), rgb(150, 84, 40), rgb(228, 156, 84)};

// surface of the kibble in the bowl at x: starts at the bottom of the bowl and heaps up past the rim when full
float moundTop(float fill, float x) {
  float u = (x - KB_X) / 19.0f; if (u < -1) u = -1; if (u > 1) u = 1;
  return KB_Y - 2 - fill * 12.0f * (1.0f - 0.5f * u * u);
}

void drawKibble(Canvas& cv, const Kibble& k) {
  int x = (int)k.x, y = (int)k.y;
  Color c = KIB_COL[k.col], d = shade(c, 70), hi = shade(c, 125);
  switch (k.shape) {
    case 0: cv.rect(x, y, 2, 2, c); cv.set(x, y, hi); break;                               // small round
    case 1: cv.rect(x, y, 3, 2, c); cv.set(x, y, hi); cv.set(x + 2, y + 1, d); break;     // oval, lying
    case 2: cv.rect(x, y, 2, 3, c); cv.set(x, y, hi); cv.set(x + 1, y + 2, d); break;     // oval, standing
    default: cv.rect(x, y + 1, 3, 1, c); cv.rect(x + 1, y, 1, 3, c); cv.set(x + 1, y, hi); cv.set(x + 2, y + 2, d); break;   // chunky
  }
}
}  // namespace

// launch one piece: toBowl lands on the mound, otherwise somewhere on the floor either side of the bowl
void Game::pourLaunch(float mx, float my, bool toBowl, int zone) {
  PourGame& g = pg_;
  int slot = -1;
  for (int i = 0; i < KIB_N; i++) if (g.kib[i].state == 0) { slot = i; break; }
  if (slot < 0) for (int i = 0; i < KIB_N; i++) if (g.kib[i].state == 2) { slot = i; break; }   // the mound already shows it
  if (slot < 0) return;
  Kibble& k = g.kib[slot];
  memset(&k, 0, sizeof(k));
  uint32_t& s = g.seed;
  k.x0 = mx + (kr(s) - 0.5f) * 4; k.y0 = my + (kr(s) - 0.5f) * 3;
  if (toBowl) {
    float spread = zone == 2 ? 11.0f : (zone == 1 ? 7.0f : 16.0f);
    k.tx = KB_X + (kr(s) * 2 - 1) * spread + (kr(s) - 0.5f) * 4;
    if (k.tx < KB_X - 18) k.tx = KB_X - 18; if (k.tx > KB_X + 17) k.tx = KB_X + 17;
    k.ty = moundTop(g.p, k.tx) - 2 - kr(s) * 1.5f;
    k.state = 1;
  } else {
    // floor: land either side of the bowl, further out the faster you pour
    float side = kr(s) < 0.6f ? -1.0f : 1.0f;
    float out = 29 + kr(s) * (zone == 3 ? 34 : 16) + kr(s) * 6;
    k.tx = KB_X + side * out;
    if (k.tx > 136) k.tx = 136 - kr(s) * 30;   // don't land on the cat
    if (k.tx < 14) { k.tx = 14 + kr(s) * 16; k.vx *= 0.3f; }   // or pile up against the screen edge
    k.ty = KB_Y + 8 + kr(s) * kr(s) * 16;     // most near the bowl, a few skitter further forward
    k.vx = side * (6 + kr(s) * 44);           // roll after landing
    if (k.tx < 14 + 16) k.vx *= 0.3f;
    k.state = 1;
  }
  k.dur = 0.32f + kr(s) * 0.2f + (toBowl ? 0 : 0.08f);
  k.arc = 6 + kr(s) * 10;
  k.shape = (uint8_t)(kr(s) * 4); if (k.shape > 3) k.shape = 3;
  k.col = (uint8_t)(kr(s) * 4); if (k.col > 3) k.col = 3;
  k.vy = toBowl ? 0 : 1;   // marks a floor piece while flying
  k.x = k.x0; k.y = k.y0;
}

void Game::pourUpdateKibble() {
  PourGame& g = pg_;
  if (g.sndBowl > 0) g.sndBowl -= dt;
  if (g.sndFloor > 0) g.sndFloor -= dt;
  uint32_t& s = g.seed;
  for (int i = 0; i < KIB_N; i++) {
    Kibble& k = g.kib[i];
    if (k.state == 1) {
      k.t += dt;
      float u = k.t / k.dur; if (u > 1) u = 1;
      k.x = k.x0 + (k.tx - k.x0) * u;
      k.y = k.y0 + (k.ty - k.y0) * u * u - k.arc * u * (1 - u) * 0.6f;   // falls faster and faster, slight arc out of the bag
      if (u >= 1) {
        if (k.vy == 0) {   // landed in the bowl
          k.state = 2;
          if (g.sndBowl <= 0) { sound(SND_KIBBLE_BOWL); g.sndBowl = 0.05f + kr(s) * 0.05f; }
        } else {           // hit the floor: one little bounce, then a short roll
          k.state = 3; k.vy = -(14 + kr(s) * 16);
          if (g.sndFloor <= 0) { sound(SND_KIBBLE_FLOOR); g.sndFloor = 0.06f + kr(s) * 0.06f; }
        }
      }
    } else if (k.state == 3) {
      k.vy += 260 * dt;
      k.x += k.vx * dt; k.y += k.vy * dt;
      k.vx *= 1.0f - 3.5f * dt;
      if (k.y >= k.ty) {
        k.y = k.ty;
        if (fabsf(k.vy) > 18) k.vy = -k.vy * 0.3f; else k.vy = 0;
        if (k.vy == 0 && fabsf(k.vx) < 2) k.state = 4;
      }
      if (k.x < 4) { k.x = 4; k.vx = -k.vx * 0.4f; }
      if (k.x > W - 8) { k.x = W - 8; k.vx = -k.vx * 0.4f; }
    }
  }
}

void Game::screenPour(Canvas& cv) {
  PourGame& g = pg_;
  dotsBg(cv, rgb(54, 40, 86), rgb(28, 20, 48));
  // Tilt is measured from however the player is holding the device when the game opens
  // (flat on a table, upright in hand, anything in between): the first ~0.6 s learns that pose.
  const float CALIB = 0.6f;
  float gl = sqrtf(tiltAx_ * tiltAx_ + tiltAy_ * tiltAy_ + tiltAz_ * tiltAz_);
  float cx = 0, cy = 0, cz = 1;
  if (gl > 0.3f) { cx = tiltAx_ / gl; cy = tiltAy_ / gl; cz = tiltAz_ / gl; }
  if (g.calib < CALIB) {
    g.calib += dt;
    g.bx += cx; g.by += cy; g.bz += cz;
  }
  float bl = sqrtf(g.bx * g.bx + g.by * g.by + g.bz * g.bz);
  float ang = 0;
  if (g.calib >= CALIB && bl > 0.01f) {
    float dot = (cx * g.bx + cy * g.by + cz * g.bz) / bl;
    ang = acosf(clampf(dot, -1.0f, 1.0f)) * 57.2958f;
    ang = ang < 4 ? 0 : (ang - 4) * 1.12f;   // small dead zone, slightly more sensitive
  }
  // No working tilt sensor (missing, or not answering): the cat must still be fed, so pouring falls back to the
  // finger: hold and drag down the screen to tip the bag, let go to stop.
  const bool noTilt = imuAge_ > 1.0f;
  if (noTilt) { g.calib = CALIB; ang = in.down ? clampf((in.y - 60) * 0.55f, 0, 80) : 0; }
  g.angle += (ang - g.angle) * 0.18f;   // smoothed needle
  int zone = 0;
  float rate = 0;
  // green is a narrow 8.8 deg window (45% smaller than before) centred on 40 deg
  const float G0 = 35.6f, G1 = 44.4f;
  if (g.angle >= 8 && g.angle < G0) { rate = 0.10f; zone = 1; }        // yellow: trickle (less filling, slims)
  else if (g.angle >= G0 && g.angle <= G1) { rate = 0.20f; zone = 2; } // green: a proper meal
  else if (g.angle > G1) { rate = 0.42f; zone = 3; }                    // red: spills
  if (g.flash > 0) g.flash -= dt;

  const float POUR_TIME = 5.0f;   // the clock starts on the first real tilt
  if (!g.done) {
    g.t += dt;
    if (g.started) g.pourT += dt;
    if (g.calib < CALIB) {
      // still learning the resting pose
    } else if (rate > 0) {
      g.started = true;
      float amt = rate * dt, inb = amt * (zone == 3 ? 0.45f : 1.0f), sp = amt - inb;
      float room = 1.0f - g.p;
      if (inb > room) { sp += inb - room; inb = room; }
      g.p += inb; g.spill += sp;
      g.zAmt[zone] += amt;            // how much kibble left the bag while in each colour
      // the pieces you see match what really went in the bowl / on the floor (~90 pieces per full bowl)
      g.bowlAcc += inb * 90.0f; g.floorAcc += sp * 90.0f;
      { float th = -clampf(g.angle, 0, 85) * 0.01745f;
        float mx = 122 + (-20) * cosf(th) - 12 * sinf(th), my = 98 + (-20) * sinf(th) + 12 * cosf(th);
        while (g.bowlAcc >= 1.0f) { g.bowlAcc -= 1.0f; pourLaunch(mx, my, true, zone); }
        while (g.floorAcc >= 1.0f) { g.floorAcc -= 1.0f; pourLaunch(mx, my, false, zone); } }
      if (zone == 3 || sp > 0) g.flash = 0.25f;
    }
    bool end = (g.started && g.pourT >= POUR_TIME) || (!g.started && g.t > 20.0f);
    if (end) {
      g.done = true;
      float tot = g.zAmt[1] + g.zAmt[2] + g.zAmt[3];
      if (tot < 0.02f) {
        toast("%s is still waiting...", pet.d.name);
        setScreen(S_HOME);
        return;
      }
      // the colour that got the most kibble decides the meal (ties go to green)
      int win = 2;
      if (g.zAmt[1] > g.zAmt[win]) win = 1;
      if (g.zAmt[3] > g.zAmt[win]) win = 3;
      bool over = false;
      const float w0 = pet.d.chonk;
      float amt = g.p < 0.3f ? 0.3f : g.p;
      if (win == 2) { pet.feed(FOOD_KIBBLE, over, amt, 0.0f, 0.0f); g.verdict = over ? 5 : 0; }       // proper meal
      else if (win == 1) { pet.feed(FOOD_KIBBLE, over, amt, 0.0f, amt); g.verdict = over ? 5 : 4; }  // trickle: less filling, slims
      else { pet.feed(FOOD_KIBBLE, over, amt, g.spill < 0.2f ? 0.2f : g.spill, 0.0f); g.verdict = 2; }   // spill: overfed
      sv.lifeFed++;
      recordMilestone(MS_MEAL);
      g.weightChange = pet.d.chonk - w0;
      if (g.verdict == 4) { pet.d.fun -= 2; sound(SND_EAT); }
      else if (g.verdict == 2 || g.verdict == 5) { pet.d.fun -= 2; pet.d.resent += 2; sound(SND_HISS); }
      else { pet.d.fun += 5; pet.d.resent -= 2; sound(SND_PURR); }
      pet.clampAll();
      actorWalkTo(BOWL_X - 18, A_EAT, 3.4f);
      bowl_ = 1.0f;
      autosave(true);
    }
  }

  cv.textCenter(W / 2, 30, g.done ? "FEEDING DONE" : (noTilt ? "DRAG TO POUR" : "TILT TO POUR"), C_GOLD, 1);
  if (gShowTouch) { char dbg[40]; snprintf(dbg, sizeof(dbg), "x%+.2f y%+.2f z%+.2f  %d", tiltAx_, tiltAy_, tiltAz_, (int)g.angle); cv.text(2, 2, dbg, C_DIM); }
  if (!g.done) {
    if (noTilt) { if (!g.started) cv.textCenter(W / 2, 40, "No tilt sensor: drag down!", C_DIM); }
    else if (g.calib < CALIB) cv.textCenter(W / 2, 40, "Hold it how you like...", C_DIM);
    else if (!g.started) cv.textCenter(W / 2, 40, "Tilt to start: 5 sec pour!", C_DIM);
  }

  // gauge 0..80 degrees
  const int gx = 12, gw = 160, gy = 56, gh = 10;
  struct Z { float a, b; Color c; } zs[4] = {{0, 8, rgb(100, 100, 120)}, {8, 35.6f, rgb(220, 190, 60)}, {35.6f, 44.4f, rgb(70, 200, 90)}, {44.4f, 80, rgb(224, 70, 70)}};
  for (auto& z : zs) cv.rect(gx + (int)(z.a / 80 * gw), gy, (int)((z.b - z.a) / 80 * gw), gh, z.c);
  cv.frame(gx - 1, gy - 1, gw + 2, gh + 2, rgb(20, 12, 36));
  int nx = gx + (int)(clampf(g.angle, 0, 80) / 80 * gw);
  cv.tri(nx - 4, gy - 8, nx + 4, gy - 8, nx, gy - 1, rgb(255, 255, 255));
  cv.textCenter(gx + (int)(40 / 80.0f * gw), gy + 13, "POUR", C_DIM);
  cv.text(gx, gy + 13, "none", C_DIM);
  cv.textRight(gx + gw, gy + 13, "SPILL", rgb(240, 130, 130));

  // bag, rotated about its pivot
  float th = -clampf(g.angle, 0, 85) * 0.01745f;
  const float px = 122, py = 98;
  const float ox[4] = {-20, 20, 20, -20}, oy[4] = {-16, -16, 16, 16};
  float qx[4], qy[4];
  for (int i = 0; i < 4; i++) { qx[i] = px + ox[i] * cosf(th) - oy[i] * sinf(th); qy[i] = py + ox[i] * sinf(th) + oy[i] * cosf(th); }
  Color bag = rgb(214, 120, 64);
  cv.tri((int)qx[0], (int)qy[0], (int)qx[1], (int)qy[1], (int)qx[2], (int)qy[2], bag);
  cv.tri((int)qx[0], (int)qy[0], (int)qx[2], (int)qy[2], (int)qx[3], (int)qy[3], shade(bag, 90));
  cv.line((int)qx[0], (int)qy[0], (int)qx[1], (int)qy[1], rgb(250, 190, 130));
  cv.line((int)qx[3], (int)qy[3], (int)qx[0], (int)qy[0], rgb(120, 60, 30));
  float lx = px + 0 * cosf(th) - 2 * sinf(th), ly = py + 0 * sinf(th) + 2 * cosf(th);
  gdisc(cv, (int)lx, (int)ly, 6, rgb(255, 224, 150));
  float mx = px + (-20) * cosf(th) - 12 * sinf(th), my = py + (-20) * sinf(th) + 12 * cosf(th);

  (void)mx; (void)my;
  pourUpdateKibble();
  const int bx = KB_X, by = KB_Y;
  // bowl body
  gellipse(cv, bx, by + 6, 26, 7, rgb(200, 70, 80));
  cv.rect(bx - 24, by - 6, 48, 12, rgb(200, 70, 80));
  gellipse(cv, bx, by - 6, 24, 7, rgb(255, 170, 176));
  gellipse(cv, bx, by - 6, 20, 5, rgb(100, 60, 40));
  // the mound of kibble (grows with the bowl), then the individual pieces on top of it
  if (g.p > 0.02f) {
    for (int x = bx - 19; x <= bx + 19; x++) {
      int top = (int)moundTop(g.p, (float)x), bot = by - 6 + (int)(5.0f * sqrtf(fmaxf(0.0f, 1.0f - ((x - bx) / 20.0f) * ((x - bx) / 20.0f)))) - 1;
      if (top < bot) cv.vline(x, top, bot - top, ((x * 7 + top * 3) % 5) < 2 ? KIB_COL[2] : KIB_COL[1]);
    }
  }
  for (int i = 0; i < KIB_N; i++) if (g.kib[i].state == 2) drawKibble(cv, g.kib[i]);
  // spilled kibble in front of the bowl, and every piece still in the air (drawn last so it falls in front)
  for (int i = 0; i < KIB_N; i++) { const Kibble& k = g.kib[i]; if (k.state == 3 || k.state == 4) drawKibble(cv, k); }
  for (int i = 0; i < KIB_N; i++) if (g.kib[i].state == 1) drawKibble(cv, g.kib[i]);
  char b[40];
  snprintf(b, sizeof(b), "Bowl %d%%", (int)(g.p * 100));
  cv.textCenter(bx, by + 28, b, g.p >= 0.75f ? rgb(120, 230, 140) : C_TXT);
  {   // share of the pour in each colour: the biggest one wins
    float tot = g.zAmt[1] + g.zAmt[2] + g.zAmt[3];
    if (tot > 0.01f) {
      const Color zc[4] = {0, rgb(230, 200, 70), rgb(90, 220, 110), rgb(240, 100, 100)};
      for (int z = 1; z <= 3; z++) {
        snprintf(b, sizeof(b), "%d%%", (int)(g.zAmt[z] / tot * 100 + 0.5f));
        int x = bx - 30 + (z - 1) * 30;
        cv.rect(x - 13, by + 39, 4, 5, zc[z]);
        cv.text(x - 7, by + 38, b, zc[z]);
      }
    }
  }

  // cat spectator
  Face f = g.done ? (g.verdict == 0 ? FACE_HAPPY : (g.verdict == 1 ? FACE_SAD : (g.verdict == 2 ? FACE_ANGRY : FACE_NORMAL))) : (zone == 3 ? FACE_SURPRISE : FACE_NORMAL);
  drawLittleCat(cv, 148, 196, POSE_SIT, f, true, 0.9f);

  if (!g.done) {
    cv.textCenter(W / 2, 80, zone == 3 ? "TOO FAST!" : (zone == 2 ? "Nice!" : (zone == 1 ? "a bit more..." : "tilt the device")), zone == 3 ? rgb(255, 120, 120) : (zone == 2 ? rgb(120, 240, 140) : C_DIM));
    if (backButton(cv)) { setScreen(S_HOME); return; }   // cancel: no meal
    if (g.started) {   // timer bar
      float f = clampf(1.0f - g.pourT / POUR_TIME, 0, 1);
      cv.rect(72, 210, 64, 6, rgb(40, 30, 60));
      cv.rect(72, 210, (int)(64 * f), 6, f < 0.3f ? rgb(240, 100, 100) : rgb(120, 200, 255));
      char tb[16]; snprintf(tb, sizeof(tb), "%.1fs", fmaxf(0.0f, POUR_TIME - g.pourT));
      cv.text(140, 209, tb, f < 0.3f ? rgb(255, 150, 150) : C_TXT);
    }
  } else {
    const char* msg[6] = {"Mostly GREEN: perfect!", "Barely a snack...", "Mostly RED: spilled!", "Good enough.", "Mostly YELLOW: light meal", "Wasn't even hungry!"};
    cv.panel(8, 100, 168, 48, C_PANEL, C_BORDER);
    cv.textCenter(W / 2, 108, msg[g.verdict], g.verdict == 0 ? rgb(120, 240, 140) : (g.verdict == 3 ? C_TXT : (g.verdict == 4 ? rgb(255, 214, 120) : rgb(255, 150, 150))));
    if (g.verdict == 2) cv.textCenter(W / 2, 120, "Ate the spill: chonkier!", rgb(255, 190, 150));
    else if (g.verdict == 1) cv.textCenter(W / 2, 120, "Still hungry and grumpy", rgb(255, 190, 150));
    else if (g.verdict == 4) cv.textCenter(W / 2, 120, "Less filling: slimmer kitty", rgb(255, 214, 120));
    else if (g.verdict == 5) cv.textCenter(W / 2, 120, "Overfed: chonkier!", rgb(255, 190, 150));
    else cv.textCenter(W / 2, 120, "Fed and happy", C_DIM);
    {   // how their weight moved
      char wb[32];
      const float dw = g.weightChange;
      if (fabsf(dw) < 0.05f) snprintf(wb, sizeof(wb), "Weight: no change (%d)", (int)(pet.d.chonk + 0.5f));
      else snprintf(wb, sizeof(wb), "Weight %s%.1f  (now %d)", dw > 0 ? "+" : "-", fabsf(dw), (int)(pet.d.chonk + 0.5f));
      cv.textCenter(W / 2, 134, wb, dw > 0.05f ? rgb(255, 200, 150) : (dw < -0.05f ? rgb(150, 210, 255) : C_DIM));
    }
    if (button(cv, 62, 156, 60, 22, "OK", C_GREEN)) setScreen(S_HOME);
  }
}

// ---------------------------------------------------------------------------------------
// LAP CAT
// ---------------------------------------------------------------------------------------

static const float LAP_TIME = 30.0f;     // the nap lasts up to 30 s
static const int LAP_BUMPS = 3;          // this many bumps and they wake up (the nap is over)

void Game::startLap(bool chosen) {
  memset(&lap_, 0, sizeof(lap_));
  lap_.e0 = pet.d.energy;
  lap_.cap = chosen ? clampf(pet.d.energy + 50.0f, 0, 100) : 100.0f;   // a nap you chose: +50 at most
  setScreen(S_LAP);
}

void Game::screenLap(Canvas& cv) {
  LapGame& g = lap_;
  dotsBg(cv, rgb(62, 44, 84), rgb(34, 24, 52));
  // "Level" = held still in whatever pose the player started with (flat on a table, in the hand,
  // upright...). The first ~0.6 s learns that pose; it then slowly follows gentle drift.
  const float CALIB = 0.6f, LEVEL_DEG = 8.0f;
  float gl = sqrtf(tiltAx_ * tiltAx_ + tiltAy_ * tiltAy_ + tiltAz_ * tiltAz_);
  float cx = 0, cy = 0, cz = 1;
  if (gl > 0.3f) { cx = tiltAx_ / gl; cy = tiltAy_ / gl; cz = tiltAz_ / gl; }
  const bool live = g.begun && !g.done;
  if (live && g.calib < CALIB) { g.calib += dt; g.bx += cx; g.by += cy; g.bz += cz; }
  float bl = sqrtf(g.bx * g.bx + g.by * g.by + g.bz * g.bz);
  float bx = 0, by = 0, bz = 1;
  if (bl > 0.01f) { bx = g.bx / bl; by = g.by / bl; bz = g.bz / bl; }
  float dot = clampf(cx * bx + cy * by + cz * bz, -1.0f, 1.0f);
  g.dev = g.calib >= CALIB ? acosf(dot) * 57.2958f : 0.0f;
  // No working tilt sensor: resting a finger on them stands in for holding still (lifting it just stops the purring;
  // nothing can startle them)
  const bool noTilt = imuAge_ > 1.0f;
  if (noTilt && live) { g.calib = CALIB; g.dev = in.down ? 0.0f : 12.0f; }
  const bool level = live && g.calib >= CALIB && g.dev < LEVEL_DEG;
  const bool jolt = live && g.dev > 25.0f;
  if (level) { float k = 0.05f * dt; g.bx += (cx - bx) * k * bl; g.by += (cy - by) * k * bl; g.bz += (cz - bz) * k * bl; }
  // bubble offset: current gravity seen in a 2-D frame around the resting pose
  const float rx = 0.0f, ry = fabsf(bz) < 0.9f ? 0.0f : 1.0f, rz = fabsf(bz) < 0.9f ? 1.0f : 0.0f;   // reference axis not parallel to b
  float ux = ry * bz - rz * by, uy = rz * bx - rx * bz, uz = rx * by - ry * bx;   // u = ref x b
  float ul = sqrtf(ux * ux + uy * uy + uz * uz); if (ul < 1e-4f) ul = 1;
  ux /= ul; uy /= ul; uz /= ul;
  float vx = by * uz - bz * uy, vy = bz * ux - bx * uz, vz = bx * uy - by * ux;     // v = b x u
  const float BUBBLE = 7.0f / (LEVEL_DEG * 0.01745f);   // inner ring edge == the "level" limit
  float bubX = (cx * ux + cy * uy + cz * uz) * BUBBLE, bubY = (cx * vx + cy * vy + cz * vz) * BUBBLE;
  g.t += dt;
  if (!g.done) idleSec = 0;   // keep the screen awake while they nap
  if (g.stun > 0) g.stun -= dt;
  if (live && g.calib >= CALIB) g.steady += dt;   // the clock: 30 s from the moment the pose is learned
  if (level) {
    g.streak += dt;
    // resting steadily fills their energy: the longer you stay still, the deeper they sleep.  Bumps never take energy away,
    // they only stop it coming in for a moment and reset the deep sleep.
    if (g.stun <= 0) {
      const float mult = 1.0f + clampf(g.streak, 0, 20) / 20.0f;
      const float before = pet.d.energy;
      pet.d.energy = clampf(pet.d.energy + dt * 1.8f * mult, 0, g.cap > before ? g.cap : before);
      g.gain += pet.d.energy - before;
      pet.d.fun = clampf(pet.d.fun + dt * 0.12f, 0, 100);
      pet.d.resent = clampf(pet.d.resent - dt * 0.03f, 0, 100);
      if (((int)(g.t * 1.2f)) != ((int)((g.t - dt) * 1.2f))) spawn(88 + (int)(pet.rnd() % 12), 118, 0, rgb(255, 130, 170), 0, -14, 1.4f);
    }
  } else if (live) {
    g.streak = 0;
    if (jolt) g.shown = 1.0f;
    if (jolt && !g.jolting && g.calib >= CALIB) {   // a new interruption
      g.interrupts++;
      g.stun = 1.2f;
      if (g.interrupts >= LAP_BUMPS) { sound(SND_HISS); } else sound(SND_SAD);
    }
  }
  g.jolting = jolt;
  if (g.shown > 0) g.shown -= dt;
  updateParticles(dt);
  // the nap ends: time up, too many bumps, or all rested
  if (live && g.calib >= CALIB && (g.steady >= LAP_TIME || g.interrupts >= LAP_BUMPS || pet.d.energy >= g.cap - 0.01f)) {
    g.done = true; g.doneT = 0;
    pet.napping = false; pet.sleeping = false;
    if (g.gain >= 10.0f) recordMilestone(MS_LAP);
    sound(g.interrupts >= LAP_BUMPS ? SND_HISS : SND_PURR);
    autosave(true);
  }
  if (g.done) g.doneT += dt;

  cv.textCenter(W / 2, 30, "LAP CAT", C_GOLD);
  cv.textCenter(W / 2, 41, !g.begun ? "" : (g.calib < CALIB ? "Hold it how you like..." : (g.done ? "" : (g.stun > 0 ? "Startled!" : (level ? (noTilt ? "Purring... keep holding" : "Purring... stay steady") : (noTilt ? "No tilt sensor: hold a finger" : "Keep the ball in the middle"))))), level ? rgb(140, 240, 160) : rgb(255, 190, 150));
  // lap
  gellipse(cv, 52, 184, 46, 16, rgb(60, 90, 160));      // two thighs
  gellipse(cv, 132, 184, 46, 16, rgb(56, 84, 150));
  gellipse(cv, 92, 170, 58, 11, rgb(86, 120, 196));     // lap where the cat curls up
  cv.hline(20, 206, 144, rgb(40, 56, 104));
  const bool asleep = level || !g.begun || g.done;
  Pose pose = asleep ? POSE_SLEEP : POSE_SIT;
  Face face = asleep ? FACE_SLEEP : (g.shown > 0 ? FACE_ANGRY : FACE_NORMAL);
  drawLittleCat(cv, 92, 160 - (asleep ? 0 : 1), pose, face, false, 1.25f, 78);   // ears stay clear of the bubble level
  for (int i = 0; i < 32; i++) {
    const Particle& p = parts_[i];
    if (p.life <= 0) continue;
    if (p.kind == 0) cv.disc((int)p.x, (int)p.y, 2, p.col);
    else if (p.kind == 3) gdisc(cv, (int)p.x, (int)p.y, 3, rgb(250, 200, 70));
  }
  // bubble level
  int lx = 92, ly = 76;
  cv.ring(lx, ly, 18, rgb(120, 100, 160));
  cv.ring(lx, ly, 7, level ? rgb(90, 220, 120) : rgb(120, 100, 160));
  int dx = (int)clampf(bubX, -16, 16), dy = (int)clampf(bubY, -16, 16);
  gdisc(cv, lx + dx, ly + dy, 3, level ? rgb(90, 230, 130) : rgb(240, 180, 90));
  char b[48];
  auto label = [&](int y, const char* t, Color c) {   // text with a dark outline, readable on the blue lap
    for (int ox = -1; ox <= 1; ox++) for (int oy = -1; oy <= 1; oy++) if (ox || oy) cv.textCenter(W / 2 + ox, y + oy, t, rgb(24, 30, 60));
    cv.textCenter(W / 2, y, t, c);
  };
  if (live) {
    snprintf(b, sizeof(b), "Time %d", (int)(LAP_TIME - g.steady + 0.99f) > 0 ? (int)(LAP_TIME - g.steady + 0.99f) : 0);
    label(174, b, C_TXT);
    if (g.interrupts > 0) { snprintf(b, sizeof(b), "Bumps: %d of %d", g.interrupts, LAP_BUMPS); label(185, b, rgb(255, 214, 120)); }
    else if (g.streak > 5) { snprintf(b, sizeof(b), "Deep sleep x%.1f", 1.0f + clampf(g.streak, 0, 20) / 20.0f); label(185, b, rgb(250, 190, 110)); }
  }
  cv.text(14, 50, "energy", C_DIM);
  cv.bar(52, 50, 106, 6, (int)pet.d.energy, rgb(100, 180, 255), rgb(24, 18, 40), rgb(120, 92, 150));

  if (!g.begun) {   // what this is
    cv.panel(8, 62, 168, 100, rgb(30, 22, 46), rgb(250, 230, 150));
    cv.textCenter(W / 2, 70, g.cap < 99.9f || g.e0 >= 15 ? "Cat Nap" : "Out of energy!", rgb(255, 255, 255));
    cv.textCenter(W / 2, 84, "Rest without moving to", rgb(220, 210, 240));
    cv.textCenter(W / 2, 94, "gain some. Keep the ball", rgb(220, 210, 240));
    cv.textCenter(W / 2, 104, g.e0 >= 15 ? "in the middle (+50 max)." : "in the middle for 30 s.", rgb(220, 210, 240));
    snprintf(b, sizeof(b), "%d bumps and they wake", LAP_BUMPS);
    cv.textCenter(W / 2, 118, b, rgb(255, 236, 160));
    cv.textCenter(W / 2, 128, "up (less rest, no loss).", rgb(255, 236, 160));
    cv.textCenter(W / 2, 146, "Tap to begin", rgb(255, 214, 120));
    if (in.pressed) { g.begun = true; idleSec = 0; }
    if (backButton(cv)) setScreen(S_HOME);
    return;
  }
  if (g.done) {   // how it went: touch anywhere to go home
    cv.panel(14, 66, 156, 92, C_PANEL, C_BORDER);
    cv.textCenter(W / 2, 74, g.interrupts >= LAP_BUMPS ? "Woke up early" : (pet.d.energy >= 99.99f ? "Fully rested!" : (pet.d.energy >= g.cap - 0.01f ? "Rested! (+50)" : "Nap time over")), C_GOLD);
    snprintf(b, sizeof(b), "Energy +%d", (int)(g.gain + 0.5f));
    cv.textCenter(W / 2, 90, b, rgb(140, 240, 160));
    snprintf(b, sizeof(b), "Energy now %d", (int)(pet.d.energy + 0.5f));
    cv.textCenter(W / 2, 104, b, C_TXT);
    if (g.interrupts == 0) cv.textCenter(W / 2, 118, "Perfectly still!", C_DIM);
    else { snprintf(b, sizeof(b), "%d bump%s: less rest", g.interrupts, g.interrupts == 1 ? "" : "s"); cv.textCenter(W / 2, 118, b, rgb(255, 190, 150)); }
    cv.textCenter(W / 2, 140, "Touch to continue", rgb(255, 214, 120));
    if (g.doneT > 0.4f && in.released) {   // on release, so the lift of the finger is not also a tap on the home screen
      if (g.gain >= 25.0f) toast("%s feels much better!", pet.d.name);
      setScreen(S_HOME);
    }
    return;
  }
  if (backButton(cv)) { autosave(true); setScreen(S_HOME); }   // leave early: what they rested is already theirs
}

// ---------------------------------------------------------------------------------------
// KNOCK IT OFF: the cat strolls along the table to whatever it is about to shove off; tap that item
// while the cat is lined up to stop it.
// ---------------------------------------------------------------------------------------

static const float KNOCK_TIME = 20.0f;
static const int KNOCK_TY = 88;       // the table top
static const int KNOCK_FLOOR = 178;   // where things land

// 0 = daylight .. 1 = night, on the same schedule as the baked room behind
static float nightAmount(float h) {
  return 1.0f - ((h >= 7 && h < 18) ? 1.0f : (h >= 18 && h < 21) ? 1.0f - (h - 18) / 3.0f : (h >= 5 && h < 7) ? (h - 5) / 2.0f : 0.0f);
}
static Color nightCol(Color c, float n) { return mix(c, rgb(22, 26, 60), (int)(n * 140.0f)); }

// The table in front of the room: tinted to match the time of day.
static void drawGameTable(Canvas& cv, int ty, int floorY, float n) {
  cv.rect(10, ty + 12, 6, floorY - ty - 8, nightCol(rgb(120, 80, 56), n)); cv.rect(168, ty + 12, 6, floorY - ty - 8, nightCol(rgb(120, 80, 56), n));
  cv.rect(6, ty, 172, 8, nightCol(rgb(190, 130, 90), n)); cv.hline(6, ty, 172, nightCol(rgb(236, 190, 150), n));
  cv.rect(8, ty + 8, 168, 5, nightCol(rgb(150, 100, 70), n)); cv.hline(8, ty + 12, 168, nightCol(rgb(110, 74, 52), n));
}

void Game::startKnock() {
  memset(&kg_, 0, sizeof(kg_));
  KnockGame& g = kg_;
  g.timeLeft = KNOCK_TIME;
  static const float xs[5] = {38, 67, 96, 125, 154};
  for (int i = 0; i < 5; i++) { g.it[i].x = xs[i]; g.it[i].type = (uint8_t)i; }
  g.cx = 16; g.last = -1; g.tgt = -1; g.cT = 0.8f;
  setScreen(S_KNOCK);
}

static void drawKnockItem(Canvas& cv, int type, int x, int y, bool glow) {
  switch (type) {
    case 0: gellipse(cv, x, y - 6, 6, 7, rgb(220, 70, 70)); cv.ring(x + 7, y - 6, 3, rgb(220, 70, 70)); cv.ellipse(x, y - 11, 5, 2, rgb(255, 200, 200)); break;
    case 1: cv.rect(x - 4, y - 14, 8, 14, rgb(170, 220, 255)); cv.rect(x - 3, y - 13, 6, 3, rgb(240, 250, 255)); cv.ellipse(x, y - 14, 4, 2, rgb(230, 246, 255)); break;
    case 2:   // a cupcake: paper wrapper, pink frosting, cherry
      cv.rect(x - 6, y - 6, 12, 6, rgb(206, 156, 96));
      for (int k = -4; k <= 4; k += 4) cv.vline(x + k, y - 6, 6, rgb(168, 118, 66));
      gellipse(cv, x, y - 9, 7, 4, rgb(255, 150, 190)); cv.ellipse(x, y - 13, 4, 3, rgb(255, 190, 215)); gdisc(cv, x, y - 16, 2, rgb(220, 40, 60));
      break;
    case 3: cv.rect(x - 6, y - 10, 12, 10, rgb(90, 190, 120)); cv.ellipse(x, y - 10, 6, 2, rgb(60, 130, 80)); cv.ring(x + 7, y - 5, 3, rgb(90, 190, 120)); break;
    default:   // two books, stacked
      cv.rect(x - 7, y - 5, 14, 5, rgb(70, 110, 200)); cv.hline(x - 7, y - 3, 14, rgb(240, 236, 220));
      cv.rect(x - 5, y - 10, 12, 5, rgb(200, 80, 90)); cv.hline(x - 5, y - 8, 12, rgb(240, 236, 220)); break;
  }
  if (glow) cv.ring(x, y - 8, 14, rgb(255, 236, 120));
}

void Game::screenKnock(Canvas& cv) {
  KnockGame& g = kg_;
  ensureRoom();
  drawRoom(cv);                      // the house behind (the same room as at home)
  const int ty = KNOCK_TY;
  const float nite = nightAmount(hour + minute / 60.0f);
  const bool wasStarted = g.started;
  const bool over = g.timeLeft <= 0 || g.knocked >= 5;
  if (g.started && !over) g.timeLeft -= dt;
  if (g.msgT > 0) g.msgT -= dt;
  const float ramp = clampf(1.0f - g.timeLeft / KNOCK_TIME, 0, 1);
  const float sp = 1.0f + 1.8f * ramp * ramp;   // the cat speeds up towards the end, like the laser

  // ---- the cat's moves ----
  if (g.started && !over) {
    switch (g.cState) {
      case 0:   // a breath, then pick the next thing to shove
        g.cT -= dt;
        if (g.cT <= 0) {
          int cand[5], n = 0;
          for (int i = 0; i < 5; i++) if (g.it[i].st == 0 && i != g.last) cand[n++] = i;
          if (n == 0 && g.last >= 0 && g.it[g.last].st == 0) cand[n++] = g.last;   // only that one is left
          if (n == 0) break;
          g.tgt = cand[pet.rnd() % n];
          g.dir = g.it[g.tgt].x >= g.cx ? 1 : -1;
          g.cTarget = clampf(g.it[g.tgt].x - g.dir * 17.0f, 14.0f, W - 14.0f);
          g.cState = 1;
        }
        break;
      case 1: {   // stroll over to it
        const float d = g.cTarget - g.cx, st = 115.0f * sp * dt;
        if (fabsf(d) <= st) { g.cx = g.cTarget; g.cState = 2; g.win = clampf(1.35f - 0.85f * ramp, 0.5f, 1.35f); g.cT = g.win; }
        else g.cx += d > 0 ? st : -st;
        break;
      }
      case 2:     // lined up, tail twitching: tap it now or it goes over!
        g.cT -= dt;
        if (g.cT <= 0) {
          KnockItem& it = g.it[g.tgt];
          it.st = 1; it.vx = (float)g.dir * (26.0f + (float)(pet.rnd() % 30)); it.vy = -38.0f;
          g.knocked++; g.last = g.tgt; g.cState = 3; g.cT = 0.28f; g.msgT = 0.9f; g.msgKind = 2;
          sound(SND_SCRATCH);
          spawn(it.x, (float)ty - 8, 5, rgb(255, 255, 255), 0, -10, 0.5f);
        }
        break;
      case 3:     // the shove
        g.cT -= dt;
        if (g.cT <= 0) { g.cState = 0; g.cT = 0.35f / sp; }
        break;
      case 4:     // foiled: a shrug, then it wanders off to try another
        g.cT -= dt;
        if (g.cT <= 0) { g.last = g.tgt; g.cState = 0; g.cT = 0.25f / sp; }
        break;
    }
  }
  // ---- a hairball turns up on the table twice a round (16 s and 8 s from the end): tap it before it rolls away ----
  if (g.started && !over) {
    static const float gaps[4] = {52, 81, 110, 139};
    const float when[2] = {16.0f, 8.0f};
    for (int k = 0; k < 2; k++) {
      if (!g.hbSpawned[k] && g.timeLeft <= when[k]) { g.hbSpawned[k] = true; g.hbX[k] = gaps[pet.rnd() % 4]; g.hbT[k] = 3.2f; }
      if (g.hbT[k] > 0) g.hbT[k] -= dt;
    }
  }
  // ---- your tap ----
  // (reacts the moment the finger lands, not when it lifts, and the target is generous: a big area around the item)
  if (wasStarted && !over && in.pressed && in.py > 34 && in.py < TOOL_Y) {
    int hbHit = -1;   // a hairball on the table, if the finger is nearer to it than to the item the cat is eyeing
    for (int k = 0; k < 2; k++) {
      if (g.hbT[k] <= 0 || fabsf((float)in.px - g.hbX[k]) > 20 || in.py < ty - 60 || in.py > ty + 44) continue;
      if (g.cState == 2 && g.tgt >= 0 && fabsf((float)in.px - g.it[g.tgt].x) < fabsf((float)in.px - g.hbX[k])) continue;
      hbHit = k;
    }
    if (hbHit >= 0) {
      g.hbT[hbHit] = 0; g.hairballs++; sound(SND_COIN);
      spawn(g.hbX[hbHit], (float)ty - 8, 4, rgb(255, 240, 120), 0, -30, 0.6f);
    } else if (g.cState == 2 && g.tgt >= 0 && fabsf((float)in.px - g.it[g.tgt].x) < 30 && in.py > ty - 72 && in.py < ty + 44) {
      g.saved++; g.last = g.tgt; g.cState = 4; g.cT = 0.6f / sp; g.msgT = 0.8f; g.msgKind = 1;   // saved!
      sound(SND_POP);
      spawn(g.it[g.tgt].x, (float)ty - 8, 5, rgb(255, 236, 120), 0, -14, 0.5f);
    } else {
      sound(SND_MISS);
    }
  }
  // ---- things fall ----
  for (int i = 0; i < 5; i++) {
    KnockItem& it = g.it[i];
    if (it.st != 1) continue;
    it.vy += 240.0f * dt; it.fall += it.vy * dt; it.xo += it.vx * dt;
    if (ty + it.fall >= KNOCK_FLOOR) {
      it.st = 2; it.fall = (float)(KNOCK_FLOOR - ty);
      sound(SND_POP); spawn(it.x + it.xo, (float)KNOCK_FLOOR, 4, rgb(255, 200, 170), 0, -20, 0.5f);
    }
  }
  updateParticles(dt);

  // ---- top bar ----
  cv.rect(0, 0, W, 32, rgb(40, 30, 62));
  cv.hline(0, 31, W, rgb(96, 70, 130));
  char b[48];
  snprintf(b, sizeof(b), "ON TABLE %d/5", 5 - g.knocked);
  cv.textRight(W - CORNER, 7, b, rgb(250, 230, 150));
  cv.bar(8, 21, W - 16, 7, (int)(clampf(g.timeLeft, 0, KNOCK_TIME) / KNOCK_TIME * 100), g.timeLeft < 6 ? rgb(236, 64, 64) : rgb(90, 200, 120), rgb(24, 18, 40), rgb(120, 92, 150));
  if (!over && backButton(cv)) { setScreen(S_HOME); return; }   // quit: no fun

  // ---- the table, the things on it, and the cat ----
  drawGameTable(cv, ty, KNOCK_FLOOR, nite);
  for (int k = 0; k < 2; k++) if (g.hbT[k] > 0 && (g.hbT[k] > 1.0f || ((int)(anim_ * 10) % 2))) drawHairball(cv, (int)g.hbX[k], ty - 6, 6);
  {
    const bool left = g.cState != 0 && g.tgt >= 0 ? g.dir < 0 : false;   // faces the item it is about to shove
    const Pose pose = g.cState == 1 ? POSE_WALK : POSE_SIT;
    const Face face = g.cState == 3 ? FACE_HAPPY : FACE_NORMAL;
    CatDraw d;
    d.breed = pet.lookBreed(); d.fat = pet.fatLook(); d.size = pet.visualSize() * 0.85f * pet.gameScale(); d.kit = pet.kitFactor();
    d.pose = pose; d.face = face; d.faceLeft = left; d.t = anim_;
    fitCatHeight(d, ty + 1, 34);
    d.blink = blinkT_ < 0.0f && blinkT_ > -0.14f;
    gShadowKey = false;
    drawCatShadow(cv, (int)g.cx, ty + 1, d, nightCol(rgb(190, 130, 90), nite));   // a soft shadow on the table top
    drawCat(cv, (int)g.cx, ty + 1, d);
    if (g.cState == 3) {   // the paw that does the shoving
      const int px = (int)g.cx + g.dir * 14 + g.dir * (int)((1.0f - g.cT / 0.28f) * 6);
      cv.disc(px, ty - 6, 4, rgb(250, 244, 250)); cv.set(px, ty - 5, rgb(250, 150, 170));
    }
    if (g.cState == 2 && ((int)(anim_ * 8) % 2)) cv.textCenter((int)g.it[g.tgt].x, ty - 52, "TAP!", rgb(255, 236, 120));
    if (g.cState == 4) cv.textCenter((int)g.cx, ty - 46, "hmph", rgb(230, 222, 250));
  }
  // the items on the table are drawn over the cat, so the cat sits behind them
  for (int i = 0; i < 5; i++) {
    const KnockItem& it = g.it[i];
    if (it.st == 0) {
      const bool poised = g.cState == 2 && i == g.tgt;
      const int wob = poised ? (int)(sinf(anim_ * 40) * 1.2f) : 0;
      drawKnockItem(cv, it.type, (int)it.x + wob, ty, poised);
    }
  }
  // ---- falling and fallen things ----
  for (int i = 0; i < 5; i++) {
    const KnockItem& it = g.it[i];
    if (it.st == 1) drawKnockItem(cv, it.type, (int)(it.x + it.xo) + (int)(sinf(it.fall * 0.12f) * 2), ty + (int)it.fall, false);
    else if (it.st == 2) {
      const int fx = (int)(it.x + it.xo);
      cv.ellipse(fx, KNOCK_FLOOR + 1, 9, 2, rgb(60, 40, 40));
      cv.disc(fx - 5, KNOCK_FLOOR - 2, 2, rgb(230, 200, 190)); cv.disc(fx + 5, KNOCK_FLOOR - 1, 2, rgb(230, 200, 190));
    }
  }
  // ---- bottom bar ----
  cv.rect(0, TOOL_Y, W, H - TOOL_Y, rgb(40, 30, 62)); cv.hline(0, TOOL_Y, W, rgb(96, 70, 130));
  const char* hint = !g.started ? "" : (g.msgT > 0 && g.msgKind == 1 ? "Saved it!" : (g.msgT > 0 && g.msgKind == 2 ? "Gravity wins!" : "Tap the item to save it!"));
  cv.textCenter(W / 2, TOOL_Y + 20, hint, rgb(190, 176, 220));
  drawToasts(cv);
  if (!g.started) {
    cv.panel(6, 92, 172, 78, rgb(30, 22, 46), rgb(250, 230, 150));
    cv.textCenter(W / 2, 100, "KNOCK IT OFF!", rgb(255, 255, 255));
    cv.textCenter(W / 2, 114, "The cat is out to shove", rgb(220, 210, 240));
    cv.textCenter(W / 2, 124, "things off the table!", rgb(220, 210, 240));
    cv.textCenter(W / 2, 137, "Tap the item to prevent", rgb(255, 236, 160));
    cv.textCenter(W / 2, 147, "it from being knocked off", rgb(255, 236, 160));
    cv.textCenter(W / 2, 159, "Tap to start", rgb(255, 214, 120));
    if (in.pressed && in.py > 34 && in.py < TOOL_Y) g.started = true;
    return;
  }
  if (over) {
    cv.panel(14, 70, 156, 60, C_PANEL, C_BORDER);
    const int left = 5 - g.knocked;   // things still on the table
    cv.textCenter(W / 2, 78, left >= 5 ? "Nothing fell!" : (left >= 3 ? "Most things survived" : (left > 0 ? "Some things survived" : "Everything fell!")), C_GOLD);
    snprintf(b, sizeof(b), "Saved %d, %d on the table", g.saved, left);
    cv.textCenter(W / 2, 92, b, C_TXT);
    if (button(cv, 62, 104, 60, 22, "OK", C_GREEN)) {
      // stars like the other games: 1 star +11 fun, 2 stars +17, 3 stars +34 (and the hairballs you picked up)
      const int stars = (left >= 4 && g.saved >= 6) ? 3 : ((left >= 2 && g.saved >= 4) ? 2 : (g.saved >= 2 ? 1 : 0));
      finishGame(4, g.saved, 0, stars, g.hairballs);
    }
  }
}

// ---------------------------------------------------------------------------------------
// MAZE RUN
// ---------------------------------------------------------------------------------------

static const int HZ_T = 11, HZ_X0 = 9, HZ_Y0 = 34, HZ_L = 7;   // 15x15 tiles = 7x7 cell lattice

static bool hzWall(const HeistGame& g, float px, float py, float r) {
  px -= HZ_X0; py -= HZ_Y0;
  int x0 = (int)floorf((px - r) / HZ_T), x1 = (int)floorf((px + r) / HZ_T), y0 = (int)floorf((py - r) / HZ_T), y1 = (int)floorf((py + r) / HZ_T);
  for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
    if (x < 0 || y < 0 || x >= HZ_W || y >= HZ_H) return true;
    if (g.maze[y][x]) return true;
  }
  return false;
}

void Game::startHeist() {
  HeistGame& g = hz_;
  memset(&g, 0, sizeof(g));
  for (int y = 0; y < HZ_H; y++) for (int x = 0; x < HZ_W; x++) g.maze[y][x] = 1;
  // recursive backtracker on the 7x7 lattice
  int stackX[64], stackY[64], sp = 0;
  g.maze[1][1] = 0; stackX[sp] = 0; stackY[sp] = 0; sp++;
  bool seen[HZ_L][HZ_L] = {};
  seen[0][0] = true;
  while (sp > 0) {
    int cx = stackX[sp - 1], cy = stackY[sp - 1];
    int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}, cand[4], nc = 0;
    for (int d = 0; d < 4; d++) {
      int nx = cx + dirs[d][0], ny = cy + dirs[d][1];
      if (nx >= 0 && ny >= 0 && nx < HZ_L && ny < HZ_L && !seen[ny][nx]) cand[nc++] = d;
    }
    if (!nc) { sp--; continue; }
    int d = cand[pet.rnd() % nc];
    int nx = cx + dirs[d][0], ny = cy + dirs[d][1];
    seen[ny][nx] = true;
    g.maze[ny * 2 + 1][nx * 2 + 1] = 0;
    g.maze[cy * 2 + 1 + dirs[d][1]][cx * 2 + 1 + dirs[d][0]] = 0;
    stackX[sp] = nx; stackY[sp] = ny; sp++;
  }
  // a handful of loops (shortcuts) so there is more than one route, but plenty of dead ends stay
  for (int k = 0; k < 7; k++) {
    int x = 1 + (int)(pet.rnd() % (HZ_W - 2)), y = 1 + (int)(pet.rnd() % (HZ_H - 2));
    if (((x ^ y) & 1) && !g.maze[y][x] == false) g.maze[y][x] = 0;
  }
  g.x = HZ_X0 + HZ_T * 1.5f; g.y = HZ_Y0 + HZ_T * 1.5f;
  g.timeLeft = 30.0f;
  // loot: 0 coin, 1 mouse, 2 cheese, 3 fish (heavy but valuable), 4 clock (+time).  Best loot hides far from home.
  g.nloot = 0;
  while (g.nloot < 14) {
    int tx = 1 + 2 * (int)(pet.rnd() % HZ_L), ty = 1 + 2 * (int)(pet.rnd() % HZ_L);
    if (tx == 1 && ty == 1) continue;
    bool dup = false;
    for (int i = 0; i < g.nloot; i++) if (g.loot[i].tx == tx && g.loot[i].ty == ty) dup = true;
    if (dup) continue;
    int far = (tx + ty) - 2;                       // 0..24
    int r = (int)(pet.rnd() % 10) + far / 5;       // far tiles roll better
    uint8_t type = r < 4 ? 0 : (r < 7 ? 1 : (r < 10 ? 2 : 3));
    if (g.nloot == 13) type = 4;   // exactly one clock (+10 s) per maze
    if (g.nloot == 11 || g.nloot == 12) type = 5;   // exactly two hairballs (+1 each) per maze...
    if (g.nloot == 10 && BREEDS[pet.d.breed].hair <= 0.0f) type = 5;   // ...three for a hairless cat (Sphynx), who never coughs any up
    g.loot[g.nloot++] = {(int8_t)tx, (int8_t)ty, type, false};
  }
  setScreen(S_HEIST);
}

void Game::screenHeist(Canvas& cv) {
  HeistGame& g = hz_;
  const float fat = clampf(pet.fatLook(), 0, 1.3f);
  const float gf = pet.gameFat();
  const float catR = 3.6f + 1.3f * gf;                            // a chonky cat fills the 11 px corridor: it has to squeeze round the corners
  float speed = (40.0f * pet.weightSpeed() + 26.0f * pet.agility()) * pet.gameDrag();   // heavy cats are slower still
  float pen = g.carry * (0.07f - 0.03f * clampf(fat, 0, 1));   // loot slows them down gently
  speed *= clampf(1.0f - pen, 0.55f, 1.0f);
  float cap = 6.5f + 2.5f * clampf(fat, 0, 1);                  // more room in their pockets

  if (g.stun > 0) g.stun -= dt;
  if (g.flash > 0) g.flash -= dt;
  if (g.msgT > 0) g.msgT -= dt;
  if (g.fullT > 0) g.fullT -= dt;
  bool over = g.timeLeft <= 0;
  if (!over && g.started && !g.quitAsk) g.timeLeft -= dt;

  // ---- cat follows the finger through the maze (anywhere on the screen steers) (shortest path to the nearest reachable tile) ----
  if (!over && !g.quitAsk && in.down && g.stun <= 0) {
    int sx = (int)((g.x - HZ_X0) / HZ_T), sy = (int)((g.y - HZ_Y0) / HZ_T);
    float fxl = (in.x - HZ_X0) / (float)HZ_T, fyl = (in.y - HZ_Y0) / (float)HZ_T;
    int par[HZ_H][HZ_W];
    for (int y = 0; y < HZ_H; y++) for (int x = 0; x < HZ_W; x++) par[y][x] = -2;
    int qx[HZ_W * HZ_H], qy[HZ_W * HZ_H], qh = 0, qt = 0;
    qx[qt] = sx; qy[qt] = sy; qt++; par[sy][sx] = -1;
    int bestX = sx, bestY = sy; float bestD = 1e9f;
    while (qh < qt) {
      int x = qx[qh], y = qy[qh]; qh++;
      float dd = (x + 0.5f - fxl) * (x + 0.5f - fxl) + (y + 0.5f - fyl) * (y + 0.5f - fyl);
      if (dd < bestD) { bestD = dd; bestX = x; bestY = y; }
      static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
      for (int d = 0; d < 4; d++) {
        int nx = x + D[d][0], ny = y + D[d][1];
        if (nx < 0 || ny < 0 || nx >= HZ_W || ny >= HZ_H || g.maze[ny][nx] || par[ny][nx] != -2) continue;
        par[ny][nx] = y * HZ_W + x; qx[qt] = nx; qy[qt] = ny; qt++;
      }
    }
    float aimX = (float)in.x, aimY = (float)in.y;
    if (!(bestX == sx && bestY == sy)) {
      int cx2 = bestX, cy2 = bestY;
      while (par[cy2][cx2] >= 0 && par[cy2][cx2] != sy * HZ_W + sx) { int p2 = par[cy2][cx2]; cx2 = p2 % HZ_W; cy2 = p2 / HZ_W; }
      aimX = HZ_X0 + (cx2 + 0.5f) * HZ_T; aimY = HZ_Y0 + (cy2 + 0.5f) * HZ_T;
      // keep to the corridor centre while changing direction
      float ccx = HZ_X0 + (sx + 0.5f) * HZ_T, ccy = HZ_Y0 + (sy + 0.5f) * HZ_T;
      const float tol = fmaxf(0.25f, 1.5f - 1.2f * gf);   // the fatter the cat, the more exactly it must line up with a doorway
      if (cx2 != sx && fabsf(g.y - ccy) > tol) { aimX = g.x; aimY = ccy; }
      else if (cy2 != sy && fabsf(g.x - ccx) > tol) { aimX = ccx; aimY = g.y; }
    } else if (bestD > 0.0f) {
      aimX = clampf((float)in.x, HZ_X0 + sx * HZ_T + 3.0f, HZ_X0 + (sx + 1) * HZ_T - 3.0f);
      aimY = clampf((float)in.y, HZ_Y0 + sy * HZ_T + 3.0f, HZ_Y0 + (sy + 1) * HZ_T - 3.0f);
    }
    float tx = aimX - g.x, ty = aimY - g.y;
    float dist = sqrtf(tx * tx + ty * ty);
    if (dist > 0.3f) {
      g.started = true;
      float stp = fminf(speed * dt, dist);
      float vx = tx / dist, vy = ty / dist;
      // squeezing: while the fat cat is not lined up with the corridor it crawls
      float ccx2 = HZ_X0 + (sx + 0.5f) * HZ_T, ccy2 = HZ_Y0 + (sy + 0.5f) * HZ_T;
      const float off = fmaxf(fabsf(g.x - ccx2), fabsf(g.y - ccy2));
      if (gf > 0.3f && off > 0.5f && (hzWall(g, g.x + 2.0f, g.y, catR) || hzWall(g, g.x - 2.0f, g.y, catR) || hzWall(g, g.x, g.y + 2.0f, catR) || hzWall(g, g.x, g.y - 2.0f, catR))) stp *= 1.0f - 0.25f * gf;
      if (!hzWall(g, g.x + vx * stp, g.y, catR)) g.x += vx * stp;
      if (!hzWall(g, g.x, g.y + vy * stp, catR)) g.y += vy * stp;
      if (fabsf(vx) > 0.1f) g.left = vx < 0;
    }
  }
  // loot pickup
  for (int i = 0; i < g.nloot; i++) {
    HeistLoot& l = g.loot[i];
    if (l.taken || over) continue;
    float cx = HZ_X0 + (l.tx + 0.5f) * HZ_T, cy = HZ_Y0 + (l.ty + 0.5f) * HZ_T;
    if (fabsf(cx - g.x) < 6 && fabsf(cy - g.y) < 6) {
      if (l.type == 4) { l.taken = true; g.timeLeft += 10.0f; sound(SND_COIN); snprintf(g.msg, sizeof(g.msg), "+10 seconds!"); g.msgT = 0.8f; continue; }
      if (l.type == 5) { l.taken = true; g.hairballs++; sound(SND_COIN); snprintf(g.msg, sizeof(g.msg), "+1 hairball!"); g.msgT = 0.9f; continue; }
      static const float W_[4] = {0.5f, 1.0f, 2.0f, 3.0f}; static const int V_[4] = {2, 3, 5, 9};
      float w = W_[l.type]; int v = V_[l.type];
      if (g.carry + w > cap) { g.fullT = 1.5f; continue; }   // too full: banner at the top
      l.taken = true; g.carry += w; g.carried += v; sound(SND_COIN);
      snprintf(g.msg, sizeof(g.msg), "+%d loot", v); g.msgT = 0.7f;
    }
  }
  // home
  // the house tile counts a few pixels beyond its edges, so hauling loot home is forgiving
  const float homeCx = HZ_X0 + 1.5f * HZ_T, homeCy = HZ_Y0 + 1.5f * HZ_T, homeReach = HZ_T * 0.5f + 5.0f;
  if (fabsf(g.x - homeCx) < homeReach && fabsf(g.y - homeCy) < homeReach && g.carried > 0) {
    g.banked += g.carried; g.items++;
    snprintf(g.msg, sizeof(g.msg), "Hauled %d home!", g.carried); g.msgT = 1.0f; g.flash = 0.4f;
    g.carried = 0; g.carry = 0; sound(SND_WIN);
  }
  // everything picked up and hauled home: no point waiting out the clock, the run ends right away
  if (!over && g.started && g.carried == 0) {
    bool all = true;
    for (int i = 0; i < g.nloot; i++) if (!g.loot[i].taken) { all = false; break; }
    if (all) { g.timeLeft = 0; snprintf(g.msg, sizeof(g.msg), "Cleaned out the maze!"); g.msgT = 1.5f; }
  }
  // ---- draw ----
  dotsBg(cv, rgb(40, 32, 60), rgb(24, 18, 40));
  cv.textCenter(W / 2, 27, "MAZE RUN", C_GOLD);
  for (int y = 0; y < HZ_H; y++) for (int x = 0; x < HZ_W; x++) {
    int px = HZ_X0 + x * HZ_T, py = HZ_Y0 + y * HZ_T;
    if (g.maze[y][x]) {
      vgrad(cv, px, py, HZ_T, HZ_T, rgb(120, 86, 120), rgb(78, 56, 84));
      cv.hline(px, py, HZ_T, rgb(176, 136, 170));
      cv.hline(px, py + 5, HZ_T, rgb(60, 42, 66));
      cv.vline(px + ((y & 1) ? 3 : 8), py, 5, rgb(60, 42, 66));
    } else {
      cv.rect(px, py, HZ_T, HZ_T, ((x + y) & 1) ? rgb(210, 170, 120) : rgb(196, 154, 106));
    }
  }
  // home door
  {   // home: a little house with an H on the door
    const int hx = HZ_X0 + HZ_T, hy = HZ_Y0 + HZ_T, c = hx + HZ_T / 2;
    const Color ol = rgb(30, 20, 36);
    cv.tri(c, hy, hx - 1, hy + 5, hx + HZ_T, hy + 5, ol);                 // roof outline
    cv.tri(c, hy + 1, hx + 1, hy + 5, hx + HZ_T - 2, hy + 5, rgb(220, 70, 70));   // red roof
    cv.rect(hx + 1, hy + 5, HZ_T - 2, HZ_T - 5, ol);
    cv.rect(hx + 2, hy + 5, HZ_T - 4, HZ_T - 6, rgb(250, 230, 170));       // walls
    cv.vline(c - 2, hy + 6, 4, rgb(60, 140, 80)); cv.vline(c + 1, hy + 6, 4, rgb(60, 140, 80)); cv.hline(c - 2, hy + 7, 4, rgb(60, 140, 80));   // "H"
  }
  for (int i = 0; i < g.nloot; i++) {
    const HeistLoot& l = g.loot[i];
    if (l.taken) continue;
    int cx = HZ_X0 + l.tx * HZ_T + HZ_T / 2, cy = HZ_Y0 + l.ty * HZ_T + HZ_T / 2 + ((int)(anim_ * 4 + i) % 2 ? 0 : -1);
    if (l.type == 0) { gdisc(cv, cx, cy, 3, rgb(252, 200, 60)); cv.set(cx - 1, cy - 1, rgb(255, 250, 200)); }
    else if (l.type == 1) { cv.ellipse(cx, cy + 1, 3, 2, rgb(170, 170, 184)); cv.disc(cx - 2, cy - 1, 1, rgb(200, 150, 170)); cv.line(cx + 3, cy + 2, cx + 5, cy + 3, rgb(220, 160, 170)); }
    else if (l.type == 2) { cv.tri(cx - 4, cy + 3, cx + 4, cy + 3, cx + 4, cy - 3, rgb(252, 210, 70)); cv.set(cx + 1, cy + 1, rgb(200, 150, 30)); }
    else if (l.type == 3) { cv.ellipse(cx - 1, cy, 4, 2, rgb(40, 200, 230)); cv.tri(cx + 2, cy, cx + 5, cy - 2, cx + 5, cy + 2, rgb(30, 150, 200)); cv.set(cx - 3, cy - 1, rgb(255, 255, 255)); }
    else if (l.type == 4) { gdisc(cv, cx, cy, 4, rgb(240, 240, 255)); cv.ring(cx, cy, 4, rgb(60, 60, 90)); cv.line(cx, cy, cx, cy - 2, rgb(60, 60, 90)); cv.line(cx, cy, cx + 2, cy, rgb(60, 60, 90)); }
    else drawIconS(cv, 6, cx, cy, 0.6f);   // the bonus hairball
  }
  // cat
  drawMiniCat(cv, (int)g.x, (int)g.y + 5, g.left, anim_, pet.lookBreed(), pet.fatLook(), in.down && !over);
  // HUD
  char b[40];
  snprintf(b, sizeof(b), "%ds", (int)ceilf(fmaxf(g.timeLeft, 0)));
  cv.text(6, 27, b, g.timeLeft < 10 ? rgb(255, 120, 120) : C_TXT);
  snprintf(b, sizeof(b), "Haul %d", g.banked);
  cv.textRight(W - 6, 27, b, C_GOLD);
  snprintf(b, sizeof(b), "Carrying %d  (%d%% load)", g.carried, (int)(g.carry / cap * 100));
  if (g.msgT > 0) cv.textCenter(W / 2, 214, g.msg, rgb(255, 240, 160));
  else if (!g.started) cv.textCenter(W / 2, 214, "Drag to move. Loot to H!", rgb(255, 255, 255));
  else cv.textCenter(W / 2, 214, b, g.carry > cap * 0.8f ? rgb(255, 190, 120) : C_DIM);
  if (!over && (g.fullT > 0 || g.carry >= cap - 0.49f)) {   // can't carry more: say so at the top
    bool blink = ((int)(anim_ * 3)) % 2 == 0;
    cv.panel(62, 4, 112, 15, rgb(120, 40, 50), blink ? rgb(255, 200, 120) : rgb(255, 140, 120));
    cv.textCenter(118, 8, "Too full! Go home!", rgb(255, 245, 220));
  }
  if (!over) {
    if (!g.quitAsk) { if (backButton(cv)) g.quitAsk = true; }
    else {   // paused: the whole screen steers, so BACK asks first
      cv.panel(24, 80, 136, 64, C_PANEL, C_BORDER);
      cv.textCenter(W / 2, 88, "QUIT THE MAZE?", C_GOLD);
      cv.textCenter(W / 2, 100, "You keep no loot.", C_DIM);
      if (button(cv, 30, 114, 60, 22, "QUIT", rgb(200, 80, 80))) { setScreen(S_HOME); return; }
      if (button(cv, 94, 114, 60, 22, "PLAY ON", C_GREEN)) g.quitAsk = false;
    }
  }
  if (over) {
    cv.panel(24, 80, 136, 52, C_PANEL, C_BORDER);
    bool cleared = g.carried == 0;
    for (int i = 0; i < g.nloot && cleared; i++) if (!g.loot[i].taken) cleared = false;
    cv.textCenter(W / 2, 88, cleared ? "MAZE CLEARED!" : "TIME'S UP", C_GOLD);
    snprintf(b, sizeof(b), "Banked loot: %d", g.banked);
    cv.textCenter(W / 2, 101, b, C_TXT);
    if (button(cv, 62, 111, 60, 18, "OK", C_GREEN)) finishGame(2, g.banked, 14, -1, g.hairballs);
  }
}

// EXPLORE lives in explore.cpp


}  // namespace cat
