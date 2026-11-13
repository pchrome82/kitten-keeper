// The two playtime games: "Chase the Laser" (finger = laser dot) and "Scratching Post".
#include "game.h"
#include "catart.h"
#include "roomdata.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace cat {

static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

static const int PX0 = 8, PX1 = 176, PY0 = 38, PY1 = 214;

// The laser's play area: the bare floor of the house you live in, seen from above (no rug, no bowls). Its colours are
// picked from the room picture itself (so they follow the time of day too), and the pattern matches the house:
// planks, the '70s shag carpet, the villa's terracotta tiles, or the Halo Bridge's glowing panels.
static uint32_t floorHash(uint32_t x, uint32_t y) { uint32_t h = x * 374761393u + y * 668265263u; h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16); }
static int lum565(uint16_t c) { return ((c >> 11) & 31) * 2 * 30 + ((c >> 5) & 63) * 59 + (c & 31) * 2 * 11; }

void Game::drawPlayFloor(Canvas& cv) {
  ensureRoom();
  const int rid = sv.room < NUM_ROOMS ? sv.room : 0;
  // a clean patch of floor (left of centre, behind the bowls, in front of the wall) in the baked room: sort it by
  // brightness and take a dark, a middle and a light colour
  static int cRid = -1; static float cQ = -1; static Color dark, base, light;
  if (cRid != rid || cQ != roomHour_) {
    cRid = rid; cQ = roomHour_;
    static uint16_t px[19 * 46];
    int n = 0;
    const int w = (int)(roomHour_ * 0.6f * 256.0f + 0.5f);   // night darkens it, but not all the way: the laser still has to show
    for (int y = 222; y < 241; y++) for (int x = 30; x < 122; x += 2) {
      const int i = y * ROOM_W + x;
      px[n++] = mix(ROOMS[rid].day[i], ROOMS[rid].night[i], w);
    }
    for (int i = 1; i < n; i++) { const uint16_t v = px[i]; const int lv = lum565(v); int j = i - 1;   // (insertion sort: small, once)
      while (j >= 0 && lum565(px[j]) > lv) { px[j + 1] = px[j]; j--; } px[j + 1] = v; }
    long sr = 0, sg = 0, sb = 0;   // the carpet's base is its average colour (a speckled carpet's middle pixel can be a grey one)
    for (int i = 0; i < n; i++) { sr += (px[i] >> 11) & 31; sg += (px[i] >> 5) & 63; sb += px[i] & 31; }
    base = rid == 2 ? (Color)(((sr / n) << 11) | ((sg / n) << 5) | (sb / n)) : px[n / 2];   // (boards and tiles: the middle one)
    dark = px[n * 12 / 100]; light = px[n * 92 / 100];
  }
  const Color seam = mix(dark, rgb(0, 0, 0), 70);
  switch (rid) {
    case 2: {   // Groovy '70s: orange shag, a speckle of light and dark tufts (fixed, so it doesn't shimmer)
      cv.clear(base);
      const Color lo = shade(base, 82), hi = shade(base, 116);   // tufts in the carpet's own colour
      for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        const uint32_t h = floorHash(x, y) & 15;
        if (h < 3) cv.set(x, y, lo); else if (h > 12) cv.set(x, y, hi);
      }
      break;
    }
    case 4: {   // Seaside Villa: terracotta tiles with cream grout
      cv.clear(base);
      const int T = 30;
      for (int ty = 0; ty < H; ty += T) for (int tx = -T / 2; tx < W; tx += T) {
        const uint32_t h = floorHash(tx + 50, ty + 50);
        if (h & 1) cv.rect(tx + 2, ty + 2, T - 2, T - 2, mix(base, dark, 40 + (h >> 4) % 40));   // tiles vary a little
        cv.hline(tx + 2, ty + 2, T - 3, mix(base, light, 60));                                      // a glazed edge
      }
      const Color grout = mix(light, rgb(255, 250, 235), 140);
      for (int y = 0; y < H; y += T) cv.rect(0, y, W, 2, grout);
      for (int x = T / 2; x < W; x += T) cv.rect(x, 0, 2, H, grout);
      break;
    }
    case 6: {   // Halo Bridge: pale glowing panels
      cv.clear(base);
      const int T = 36;
      for (int ty = 0; ty < H; ty += T) for (int tx = -T / 2; tx < W; tx += T) cv.rect(tx + 3, ty + 3, T - 8, 5, mix(base, light, 160));   // the sheen
      for (int y = 0; y < H; y += T) cv.hline(0, y, W, light);
      for (int x = T / 2; x < W; x += T) cv.vline(x, 0, H, light);
      break;
    }
    default: {   // planks (Grimy Cartoon, Treehouse, Modern Apartment, Log Cabin), running away from you
      const bool modern = rid == 3, cartoon = rid == 0;
      const int PW = modern ? 30 : 20;
      cv.clear(base);
      for (int c = 0, x = -PW / 3; x < W; c++, x += PW) {
        int y = -(int)(floorHash(c, 7) % 60);
        for (int k = 0; y < H; k++) {   // boards of different lengths, a slightly different tone each
          const int len = (modern ? 90 : 56) + (int)(floorHash(c, k + 11) % 40);
          const uint32_t h = floorHash(c, k);
          const Color tone = (h & 3) == 0 ? mix(base, dark, 70) : ((h & 3) == 1 ? mix(base, light, 60) : base);
          cv.rect(x, y, PW, len, tone);
          if (!modern) for (int g = 0; g < 3; g++) {   // a little grain
            const int gx = x + 3 + (int)((h >> (4 + g * 3)) % (PW - 6)), gy = y + 6 + (int)((h >> (12 + g * 4)) % (len > 14 ? len - 12 : 1));
            cv.vline(gx, gy, 6 + (int)((h >> (20 + g)) % 8), mix(tone, dark, 150));
          }
          cv.hline(x, y, PW, cartoon ? seam : mix(tone, dark, 200));   // the board's end
          y += len;
        }
        cv.vline(x, 0, H, cartoon ? seam : mix(base, dark, 210));      // the gap between rows of boards
        if (cartoon) cv.vline(x + 1, 0, H, seam);                      // (inked, like the room)
      }
      break;
    }
  }
}

static constexpr float LASER_TIME = 20.0f;   // round length (s)
static float LASER_SLOW_REACH = 2.0f;        // how much bigger a slow breed's paw gets (per point of slowness)
static float LASER_HAMSTER_REACH = 30.0f;    // the Hamster's paw (px)

void Game::startLaser() {
  memset(&lg_, 0, sizeof(lg_));
  lg_.timeLeft = LASER_TIME;
  lg_.cx = 92; lg_.cy = 150;
  lg_.cd = 0.5f;
  lg_.hbX = 92; lg_.hbY = 130;   // one bonus hairball in the middle of the floor
  setScreen(S_LASER);
}

void Game::startScratch() {
  memset(&sg_, 0, sizeof(sg_));
  ScratchGame& g = sg_;
  // the chart: the first arrow arrives after a 2.6 s lead-in (it scrolls in while the hint shows), about one a second
  // at first, speeding up to one every ~0.55 s, with the odd quick double. Mostly DOWN (that's how cats scratch).
  float t = 2.6f;
  const float LAST = 15.5f;
  while (g.n < SCR_MAX && t <= LAST) {
    const float prog = (t - 2.6f) / (LAST - 2.6f);
    g.at[g.n] = t;
    g.dir[g.n] = (pet.rnd() % 100) < 70 ? 1 : -1;
    g.n++;
    float gap = 1.0f - 0.45f * prog;
    if (prog > 0.3f && g.n < SCR_MAX && (pet.rnd() % 100) < 18) {   // quick double, same direction
      g.at[g.n] = t + 0.45f; g.dir[g.n] = g.dir[g.n - 1]; g.n++;
      gap += 0.45f;
    }
    t += gap;
  }
  g.endT = g.at[g.n - 1] + 0.8f;
  g.lastHitT = -10;
  setScreen(S_SCRATCH);
}

void Game::finishGame(int kind, int score, int target, int stars, int bonusCoins, int catches) {
  res_.kind = kind;
  res_.score = score;
  res_.catches = catches;
  if (stars >= 0) { res_.stars = stars; res_.win = stars >= 2; }
  else {
    res_.win = score >= target;
    res_.stars = (score >= target * 0.6f ? 1 : 0) + (score >= target ? 1 : 0) + (score >= target * 1.6f ? 1 : 0);
  }
  // hairballs are precious: the hidden pickups (laser rug, maze, knock) and the Scratching Post (1 per star) pay out
  int coins = bonusCoins + (kind == 1 ? res_.stars : 0);
  res_.coins = coins;
  // Fun comes only from the stars you earn: 1 star +17, 2 stars +25, 3 stars +33 (a third of the old 50 / 75 / 100)
  const float fun = gfun(res_.stars >= 3 ? 100.0f : (res_.stars == 2 ? 75.0f : (res_.stars == 1 ? 50.0f : 0.0f)));
  res_.funGain = fun;
  if (res_.stars >= 3) recordMilestone(MS_STARS);
  float chonkLoss = clampf(0.6f + score * (kind == 0 ? 0.12f : 0.03f), 0.5f, 4.0f);
  pet.exercise(fun, kind == 0 ? 14.0f : 17.0f, chonkLoss);
  if (res_.win) pet.d.resent = pet.d.resent > 2 ? pet.d.resent - 2 : 0;
  giveCoins(coins);
  sv.lifeGames++;
  sound(res_.win ? SND_WIN : SND_LOSE);
  autosave(true);
  setScreen(S_RESULT);
}

// ---------------------------------------------------------------------------------------
// laser
// ---------------------------------------------------------------------------------------

void Game::screenLaser(Canvas& cv) {
  LaserGame& g = lg_;
  // The player keeps the dot AWAY from the cat. They get faster and snappier as the round goes on.
  const float a = pet.agility();
  const float ramp = g.started ? clampf(1.0f - g.timeLeft / LASER_TIME, 0, 1) : 0.0f;   // 0 at start -> 1 at the end
  const float speed = (44.0f + 90.0f * a) * (1.0f + 0.8f * ramp) * pet.gameDrag();   // chonky cats lumber                  // quick, and up to 80% quicker
  // Breed balance. Faster cats run the dot down more often; to even it out, SLOWER breeds pounce as sharply as a
  // speed-100 cat and catch with a bigger paw, more so the heavier (weight gain) and fluffier (hair) they are.
  // The Hamster is the exception: it's meant to be hopeless at this one (hamsters don't care about lasers IRL... so
  // here it's a tiny blur that never stops darting at the dot).
  const Breed& br = BREEDS[pet.d.breed < NUM_BREED_SLOTS ? pet.d.breed : 0];
  const bool hamster = pet.d.breed == BR_HAMSTER;
  const float slow = br.speed < 1.0f ? 1.0f - br.speed : 0.0f;          // 0 for speed 100+, 0.2 for speed 80
  const float bulk = (br.gain + br.hair) * 0.5f;                         // ~1: heavier / fluffier = a bigger reach
  float aa = a > 1.2f ? 1.2f : a;
  if (slow > 0 && aa < 1.0f) aa = 1.0f;                                  // slow breeds: speed-100 jump timing
  float windup = (0.10f + (1.2f - aa) * 0.15f) * (1.0f - 0.4f * ramp);
  float recover = (0.22f + (1.2f - aa) * 0.3f) * (1.0f - 0.4f * ramp);
  float range = 58.0f + 14.0f * ramp;   // they leap from farther away
  float catchR = 19.0f * (1.0f + LASER_SLOW_REACH * slow * bulk);        // the paw
  if (hamster) { windup = 0.04f; recover = 0.10f; range += 20.0f; catchR = LASER_HAMSTER_REACH; }
  const float POUNCE_T = 0.17f;   // the leap itself (was 0.30 s)

  // ---- laser pointer = finger ----
  // While touching, the dot follows the finger. If the finger lifts or slides off the play area,
  // the dot stays at its last spot until the player touches again (the cat will go for it!).
  if (g.lockout > 0) g.lockout -= dt;
  const bool touching = in.down && g.lockout <= 0 && in.y >= PY0 - 10;
  const bool swept = touching && g.ptouch;   // (the finger was down last frame too: the line it moved along counts)
  const float fromX = g.lx, fromY = g.ly;
  if (touching) {
    g.lv = true;
    g.lx = clampf((float)in.x, PX0, PX1);
    g.ly = clampf((float)in.y, PY0, PY1);
    if (!g.started) g.started = true;
    for (int i = 5; i > 0; i--) { g.trail[i][0] = g.trail[i - 1][0]; g.trail[i][1] = g.trail[i - 1][1]; }
    g.trail[0][0] = g.lx; g.trail[0][1] = g.ly;
  }
  g.ptouch = touching;
  // did the finger pass over (hx, hy)? Along the whole line from last frame's spot to this one, so a quick swipe can't
  // skip over a hairball between frames (the board draws fewer frames than a browser does)
  auto fingerOver = [&](float hx, float hy) {
    const float R = 11.0f;
    float ax = swept ? fromX : g.lx, ay = swept ? fromY : g.ly;
    const float bx = g.lx - ax, by = g.ly - ay, L2 = bx * bx + by * by;
    float u = L2 > 0.01f ? ((hx - ax) * bx + (hy - ay) * by) / L2 : 1.0f;
    u = u < 0 ? 0 : (u > 1 ? 1 : u);
    const float dx = ax + bx * u - hx, dy = ay + by * u - hy;
    return dx * dx + dy * dy < R * R;
  };
  if (g.started) g.timeLeft -= dt;
  // more hairballs turn up at random spots: one with 8 seconds left, another with 3 seconds left
  auto dropHairball = [&](bool& spawned, float& hx, float& hy, float when) {
    if (spawned || !g.started || g.timeLeft > when) return;
    spawned = true;
    for (int tries = 0; tries < 10; tries++) {
      hx = PX0 + 14 + (float)(pet.rnd() % (PX1 - PX0 - 28)); hy = PY0 + 18 + (float)(pet.rnd() % (PY1 - PY0 - 30));
      if (fabsf(hx - g.cx) + fabsf(hy - g.cy) > 50) break;   // not right under their nose
    }
    sound(SND_POOF);
  };
  dropHairball(g.hb2Spawned, g.hb2X, g.hb2Y, 8.0f);
  dropHairball(g.hb3Spawned, g.hb3X, g.hb3Y, 3.0f);
  auto grab = [&](bool spawned, bool& taken, float hx, float hy) {
    if (!g.lv || !spawned || taken) return;
    if (fingerOver(hx, hy)) { taken = true; sound(SND_COIN); toast("Another hairball! +1"); }
  };
  grab(g.hb2Spawned, g.hb2Taken, g.hb2X, g.hb2Y);
  grab(g.hb3Spawned, g.hb3Taken, g.hb3X, g.hb3Y);
  if (g.lv && !g.hbTaken) {
    if (fingerOver(g.hbX, g.hbY)) {
      g.hbTaken = true;
      sound(SND_COIN);
      toast("Hairball grabbed! +1");
      for (int i = 0; i < 6; i++) spawn(g.hbX + (i - 3) * 3, g.hbY - 4, 1, rgb(255, 240, 120), (i - 3) * 14.0f, -30 - (float)(i % 3) * 8, 0.7f);
    }
  }
  if (g.flash > 0) g.flash -= dt;
  if (g.cd > 0) g.cd -= dt;

  // ---- cat ----
  switch (g.state) {
    case 0: {
      if (g.lv) {
        float dx = g.lx - g.cx, dy = g.ly - g.cy;
        float dist = sqrtf(dx * dx + dy * dy);
        g.idle = 0;
        if (dist > 3.0f) {
          float sp = speed * (dist > 70 ? 1.0f : 0.55f + 0.45f * dist / 70.0f);
          float stp = sp * dt;
          if (stp > dist) stp = dist;
          g.cx += dx / dist * stp;
          g.cy += dy / dist * stp;
          g.vx = dx / dist;
          if (fabsf(dx) > 2) g.left = dx < 0;
        }
        if (dist < range && g.cd <= 0 && LASER_TIME - g.timeLeft >= 3.0f) { g.state = 1; g.stateT = windup; }   // no pouncing in the first 3 s
      } else {
        g.idle += dt;
      }
      break;
    }
    case 1:
      g.stateT -= dt;
      if (g.lv) g.left = g.lx < g.cx;
      if (g.stateT <= 0) {
        g.ptx = g.lv ? g.lx : g.cx + (g.left ? -24 : 24);
        g.pty = g.lv ? g.ly : g.cy;
        g.psx = g.cx; g.psy = g.cy;
        g.state = 2;
        g.stateT = POUNCE_T;
        sound(SND_POUNCE);
      }
      break;
    case 2: {
      g.stateT -= dt;
      float u = clampf(1.0f - g.stateT / POUNCE_T, 0, 1);
      g.cx = g.psx + (g.ptx - g.psx) * u;
      g.cy = g.psy + (g.pty - g.psy) * u;
      if (g.stateT <= 0) {
        g.cx = g.ptx; g.cy = g.pty;
        float d = 9999;
        if (g.lv) { float dx = g.lx - g.ptx, dy = g.ly - g.pty; d = sqrtf(dx * dx + dy * dy); }
        if (d < catchR) {
          g.catches++;   // they got it: costs the player a star
          g.streak++;
          g.lockout = 0.9f; g.lv = false; g.flash = 0.35f;
          g.state = 3; g.stateT = 0.55f;
          sound(SND_COIN);
          for (int i = 0; i < 6; i++) spawn(g.cx + (i - 3) * 3, g.cy - 10, 1, rgb(255, 240, 120), (i - 3) * 14.0f, -30 - (float)(i % 3) * 8, 0.7f);
          spawn(g.cx, g.cy - 30, 0, 0, 0, -20, 1.0f);
        } else if (d < catchR + 27) {
          g.near++; g.dodges++; g.streak = 0;
          g.state = 3; g.stateT = recover;
          spawn(g.cx, g.cy - 20, 5, rgb(255, 255, 255), 0, -10, 0.5f);
        } else {
          g.miss++; g.dodges++; g.streak = 0;
          g.state = 3; g.stateT = recover * 0.9f;
        }
      }
      break;
    }
    default:
      g.stateT -= dt;
      if (g.stateT <= 0) { g.state = 0; g.cd = 0.25f; }
      break;
  }
  g.cx = clampf(g.cx, PX0 + 6, PX1 - 6);
  g.cy = clampf(g.cy, PY0 + 4, PY1);

  // ---- draw ----
  drawPlayFloor(cv);
  float persp = 0.78f + 0.42f * clampf((g.cy - PY0) / (float)(PY1 - PY0), 0, 1);
  CatDraw d;
  d.breed = pet.lookBreed(); d.fat = pet.fatLook(); d.size = pet.visualSize() * persp * pet.gameScale(); d.kit = pet.kitFactor();
  d.faceLeft = g.left; d.t = anim_;
  d.face = g.state == 3 && g.catches > 0 && g.stateT > 0.2f && g.flash > 0 ? FACE_HAPPY : (g.idle > 3 ? FACE_SAD : FACE_NORMAL);
  float lift = 0;
  switch (g.state) {
    case 0: d.pose = (g.lv && g.vx != 0 && !(fabsf(g.lx - g.cx) < 3 && fabsf(g.ly - g.cy) < 3)) ? POSE_WALK : POSE_SIT; break;
    case 1: d.pose = POSE_CROUCH; d.face = FACE_NORMAL; break;
    case 2: {
      d.pose = POSE_POUNCE;
      float u = clampf(1.0f - g.stateT / POUNCE_T, 0, 1);
      lift = sinf(3.14159f * u) * 14.0f;
      break;
    }
    default:
      d.pose = POSE_SIT;
      if (g.flash > 0 || (g.catches > 0 && g.stateT > 0.2f && g.state == 3 && g.lockout > 0)) { d.face = FACE_HAPPY; lift = fabsf(sinf(anim_ * 14)) * 3; }
      break;
  }
  if (!g.hbTaken) {
    cv.ellipse((int)g.hbX, (int)g.hbY + 4, 6, 2, rgb(180, 96, 126));
    drawIcon(cv, 6, (int)g.hbX, (int)g.hbY + (int)(sinf(anim_ * 3) * 1.5f));
  }
  for (int k = 0; k < 2; k++) {   // the 2nd and 3rd hairballs, wherever they landed
    const bool sp = k ? g.hb3Spawned : g.hb2Spawned, tk = k ? g.hb3Taken : g.hb2Taken;
    const float hx = k ? g.hb3X : g.hb2X, hy = k ? g.hb3Y : g.hb2Y;
    if (!sp || tk) continue;
    cv.ellipse((int)hx, (int)hy + 4, 6, 2, rgb(150, 100, 70));
    drawIcon(cv, 6, (int)hx, (int)hy + (int)(sinf(anim_ * 4 + k) * 2.0f));
    if (((int)(anim_ * 6)) & 1) cv.set((int)hx + 7, (int)hy - 8, rgb(255, 255, 200));
  }
  drawCatShadow(cv, (int)g.cx, (int)g.cy, d, rgb(196, 110, 140));
  drawCat(cv, (int)g.cx, (int)(g.cy - lift), d);

  // the dot
  if (g.lv) {
    for (int i = 5; i >= 1; i--) {
      int tx = (int)g.trail[i][0], ty = (int)g.trail[i][1];
      cv.set(tx, ty, rgb(255, 120 + i * 10, 120 + i * 10));
    }
    int fl = (int)(anim_ * 20) % 2;
    cv.disc((int)g.lx, (int)g.ly, 5 + fl, rgb(255, 120, 120));
    cv.disc((int)g.lx, (int)g.ly, 3, rgb(255, 40, 40));
    cv.disc((int)g.lx, (int)g.ly, 1, rgb(255, 230, 230));
  } else if (g.catches > 0) {
    cv.textCenter(W / 2, 100, "GOT IT! Touch again", rgb(255, 255, 255));
  }
  updateParticles(dt);
  drawParticles(cv);

  // HUD
  cv.rect(0, 0, W, 32, rgb(40, 30, 62));
  cv.hline(0, 31, W, rgb(96, 70, 130));
  char b[32];
  snprintf(b, sizeof(b), "CAUGHT %d/3", g.catches);
  cv.textRight(W - CORNER, 7, b, g.catches ? rgb(255, 150, 150) : rgb(250, 230, 150));
  cv.bar(8, 21, W - 16, 7, (int)(g.timeLeft / LASER_TIME * 100), g.timeLeft < 6 ? rgb(236, 64, 64) : rgb(90, 200, 120), rgb(24, 18, 40), rgb(120, 92, 150));
  // BACK (top-left): quit now, no points, no hairballs, no energy used
  if (backButton(cv)) { setScreen(S_HOME); return; }
  drawToasts(cv);
  if (!g.started) {
    cv.panel(14, 84, 156, 64, rgb(30, 22, 46), rgb(250, 230, 150));
    cv.textCenter(W / 2, 92, "Touch the screen!", rgb(255, 255, 255));
    cv.textCenter(W / 2, 104, "Your finger is the laser.", rgb(220, 210, 240));
    cv.textCenter(W / 2, 114, "Don't let them catch it!", rgb(220, 210, 240));
    cv.textCenter(W / 2, 124, "Bonus: grab the hairball", rgb(255, 214, 120));
    cv.textCenter(W / 2, 134, "3 catches and you lose!", rgb(255, 150, 150));
  }
  if (g.catches >= 3) {   // their 3rd catch ends it: you lose
    toast("Caught 3 times! They win.");
    finishGame(0, g.dodges, 0, 0, (g.hbTaken ? 1 : 0) + (g.hb2Taken ? 1 : 0) + (g.hb3Taken ? 1 : 0), g.catches);
  } else if (g.timeLeft <= 0) {
    // 0 catches = 3 stars, 1 = 2, 2 = 1, more = 0. They still loves the chase.
    int stars = g.catches >= 3 ? 0 : 3 - g.catches;
    finishGame(0, g.dodges, 0, stars, (g.hbTaken ? 1 : 0) + (g.hb2Taken ? 1 : 0) + (g.hb3Taken ? 1 : 0), g.catches);
  }
}

// ---------------------------------------------------------------------------------------
// scratching post
// ---------------------------------------------------------------------------------------

namespace {
// a small up/down arrow centred on (x, y)
void scrArrow(Canvas& cv, int x, int y, int dir, Color c, Color edge) {
  const int tip = y + dir * 7, base = y + dir * 1;
  cv.tri(x - 7, base, x + 7, base, x, tip, edge);
  cv.tri(x - 5, base + -dir * 0, x + 5, base, x, tip - dir * 2, c);
  cv.rect(x - 3, dir > 0 ? y - 7 : y - 1, 7, 8, edge);
  cv.rect(x - 2, dir > 0 ? y - 6 : y, 5, 7, c);
}
void scrStar(Canvas& cv, int x, int y, Color c) {
  cv.tri(x, y - 5, x - 2, y + 1, x + 2, y + 1, c);
  cv.tri(x - 5, y - 1, x + 5, y - 1, x, y + 3, c);
  cv.tri(x - 3, y + 4, x, y, x + 3, y + 4, c);
}
}  // namespace

void Game::screenScratch(Canvas& cv) {
  ScratchGame& g = sg_;
  const int postX = 124, postW = 30, postTop = 54, postBot = 196;
  const int LANE_Y = 31, TX = 30;            // target ring on the left of the lane; arrows come in from the right
  const float SPEED = 60.0f;                 // px per second (about 2.6 s of warning)
  const float PERFECT = 0.12f, GOOD = 0.25f; // timing windows (generous: the touch chip only reports ~30x a second)
  g.t += dt;
  if (g.fbT > 0) g.fbT -= dt;
  if (g.sadT > 0) g.sadT -= dt;
  auto judge = [&](int i, int r) {
    g.res[i] = (uint8_t)r;
    g.fb = r; g.fbT = 0.6f;
    if (r == 3) { g.misses++; g.combo = 0; g.sadT = 0.5f; sound(SND_MISS); return; }
    g.hits++; if (r == 1) g.perfects++;
    g.combo++; if (g.combo > g.bestCombo) g.bestCombo = g.combo;
    g.lastHitT = g.t;
    g.pawTarget += 3.14159f;
    sound(SND_SCRATCH);
    for (int k = 0; k < (r == 1 ? 4 : 2); k++)
      spawn(postX - postW / 2 + 2 + (float)(pet.rnd() % 26), 90 + (float)(pet.rnd() % 70), 4, rgb(226, 196, 150), (float)((int)(pet.rnd() % 40) - 20), -30, 0.7f);
  };

  // beat ticks, and arrows that slid past untouched
  for (int i = 0; i < g.n; i++) {
    if (g.res[i] == 0 && !g.ticked[i] && g.t >= g.at[i]) { g.ticked[i] = true; sound(SND_BEAT); }
    // missed only once a late swipe could no longer reach it (a stroke takes a few frames to be recognised)
    if (g.res[i] == 0 && g.t > g.at[i] + GOOD + 0.15f && !(g.swiping && g.swDir == 0 && fabsf(g.at[i] - g.swT) <= GOOD)) judge(i, 3);
  }

  // swipes: anywhere below the lane. A stroke counts once it has moved 14 px; the timing uses when it started.
  // Keep dragging the same way = still the same stroke; reversing (or lifting) starts a new one.
  if (in.down && in.y > 44) {
    if (!g.swiping) { g.swiping = true; g.sy0 = (float)in.y; g.swT = g.t; g.swDir = 0; }
    float dy = in.y - g.sy0;
    int d = dy > 0 ? 1 : -1;
    if (g.swDir != 0 && d == g.swDir) { g.sy0 = (float)in.y; g.swT = g.t; }   // follow the stroke along (a reversal is timed from the turn)
    if (fabsf(dy) >= 14.0f && d != g.swDir) {
      const float when = g.swT - 0.02f;   // the finger landed a little before the chip reported it
      int best = -1; float bd = 99;
      for (int i = 0; i < g.n; i++) {
        if (g.res[i]) continue;
        float e = fabsf(g.at[i] - when);
        if (e < bd) { bd = e; best = i; }
      }
      if (best >= 0 && bd <= GOOD) judge(best, g.dir[best] != d ? 3 : (bd <= PERFECT ? 1 : 2));
      else { g.fb = 3; g.fbT = 0.4f; g.combo = 0; g.sadT = 0.3f; sound(SND_MISS); }   // nothing to hit: breaks the combo only
      g.swDir = d; g.sy0 = (float)in.y; g.swT = g.t;
    }
  } else if (!in.down) {
    g.swiping = false;
  }
  g.pawPhase += (g.pawTarget - g.pawPhase) * clampf(dt * 16.0f, 0, 1);

  // ---- draw ----
  cv.clear(rgb(70, 54, 100));
  cv.rect(0, 160, W, 64, rgb(150, 104, 74));
  for (int y = 160; y < H; y += 10) cv.hline(0, y, W, rgb(126, 86, 60));
  // post
  cv.rect(postX - 34, postBot, postW + 68, 10, rgb(110, 76, 54));
  cv.rect(postX - 30, postTop - 10, postW + 60, 10, rgb(110, 76, 54));
  cv.rect(postX - postW / 2, postTop, postW, postBot - postTop, rgb(212, 178, 126));
  for (int y = postTop; y < postBot; y += 4) cv.hline(postX - postW / 2, y, postW, (y / 4) % 2 ? rgb(178, 142, 96) : rgb(196, 160, 110));
  // a claw mark for every hit: the post gets more and more shredded
  {
    int k = 0;
    for (int i = 0; i < g.n; i++) {
      if (g.res[i] != 1 && g.res[i] != 2) continue;
      const int y = postTop + 8 + (k * 37) % (postBot - postTop - 34);
      const int x = postX - 10 + (k * 7) % 12;
      const Color c = g.res[i] == 1 ? rgb(120, 84, 50) : rgb(150, 112, 72);
      for (int c3 = 0; c3 < 3; c3++) cv.line(x + c3 * 4, y, x + c3 * 4 + 2, y + 13, c);
      if (g.res[i] == 1) for (int c3 = 0; c3 < 3; c3++) cv.line(x + c3 * 4 + 1, y, x + c3 * 4 + 3, y + 13, rgb(236, 210, 166));   // fresh fluff edge
      k++;
    }
  }

  // cat
  CatDraw d;
  d.breed = pet.lookBreed(); d.fat = pet.fatLook(); d.kit = pet.kitFactor();
  d.size = pet.visualSize() * 1.1f * pet.gameScale();
  d.pose = POSE_UPRIGHT; d.t = anim_; d.pawPhase = g.pawPhase; d.faceLeft = false; d.blink = false;
  d.face = g.sadT > 0 ? FACE_SAD : (g.combo >= 3 ? FACE_HAPPY : FACE_NORMAL);
  if (g.t - g.lastHitT > 0.9f) d.pawPhase = 1.5708f * (1 + 2 * ((int)(anim_ * 1.6f) & 1));   // waiting: gentle paw-shuffle
  int catX = (postX - postW / 2) - spriteRightExtent(d) + 3, catY = 198;   // paws reach the post
  cv.ellipse(catX, catY, 30, 4, rgb(100, 68, 50));
  CatMetrics m = drawCat(cv, catX, catY, d);
  if (g.combo >= 5) drawBubble(cv, m.headX, m.topY - 4, "purr!");

  // feedback for the last swipe
  if (g.fbT > 0) {
    const char* t = g.fb == 1 ? "PURRFECT!" : (g.fb == 2 ? "GOOD" : "MISS");
    const Color c = g.fb == 1 ? rgb(120, 245, 150) : (g.fb == 2 ? rgb(250, 230, 150) : rgb(255, 120, 120));
    cv.textCenter(54, 60, t, c, 1);   // in the open space left of the post
  }
  if (g.combo >= 3) { char cb[16]; snprintf(cb, sizeof(cb), "%d in a row!", g.combo); cv.textCenter(54, 72, cb, rgb(250, 230, 150)); }
  updateParticles(dt);
  drawParticles(cv);

  // HUD: row 1 BACK | SCRATCH | stars still possible; row 2 the arrow lane
  cv.rect(0, 0, W, 44, rgb(40, 30, 62));
  cv.hline(0, 43, W, rgb(96, 70, 130));
  cv.textCenter(W / 2, 7, "SCRATCH", rgb(250, 230, 150));
  {   // 3 stars = every beat hit, 2 = 80%, 1 = 60%. A star greys out the moment it can't be reached any more.
    const float best = g.n ? (float)(g.n - g.misses) / g.n : 1.0f;
    const int can = best >= 0.999f ? 3 : (best >= 0.8f ? 2 : (best >= 0.6f ? 1 : 0));
    for (int i = 0; i < 3; i++) scrStar(cv, W - CORNER - 26 + i * 11, 10, i < can ? rgb(252, 226, 72) : rgb(80, 70, 100));
  }
  // lane
  cv.rect(0, LANE_Y - 11, W, 22, rgb(28, 20, 46));
  cv.hline(0, LANE_Y - 11, W, rgb(70, 54, 100)); cv.hline(0, LANE_Y + 10, W, rgb(70, 54, 100));
  {   // target ring with a paw print: swipe when an arrow sits in it
    const bool pulse = g.fbT > 0.45f;
    const Color rc = pulse ? (g.fb == 3 ? rgb(255, 110, 110) : rgb(130, 245, 160)) : rgb(250, 230, 150);
    cv.ring(TX, LANE_Y, 10, rc); cv.ring(TX, LANE_Y, 9, shade(rc, 70));
    cv.disc(TX, LANE_Y + 2, 3, rgb(120, 100, 150));
    cv.disc(TX - 4, LANE_Y - 3, 1, rgb(120, 100, 150)); cv.disc(TX - 1, LANE_Y - 5, 1, rgb(120, 100, 150));
    cv.disc(TX + 2, LANE_Y - 5, 1, rgb(120, 100, 150)); cv.disc(TX + 5, LANE_Y - 3, 1, rgb(120, 100, 150));
  }
  for (int i = g.n - 1; i >= 0; i--) {
    if (g.res[i] == 1 || g.res[i] == 2) continue;   // hit: gone (it popped into a claw mark)
    const int x = TX + (int)((g.at[i] - g.t) * SPEED);
    if (x < -10 || x > W + 10) continue;
    const bool missed = g.res[i] == 3;
    const Color c = missed ? rgb(90, 80, 110) : (g.dir[i] > 0 ? rgb(255, 170, 80) : rgb(110, 190, 255));
    scrArrow(cv, x, LANE_Y, g.dir[i], c, missed ? rgb(50, 44, 66) : rgb(20, 12, 36));
  }
  if (backButton(cv)) { setScreen(S_HOME); return; }   // quit: no points, no energy used

  if (g.t < g.at[0] - 0.3f) {   // how to play, while the first arrow scrolls in
    cv.panel(14, 82, 156, 38, rgb(30, 22, 46), rgb(250, 230, 150));
    cv.textCenter(W / 2, 89, "Swipe UP/DOWN on the post", rgb(255, 255, 255));
    cv.textCenter(W / 2, 101, "when arrows hit the paw", rgb(220, 210, 240));
  }
  if (g.t >= g.endT) {
    const float f = g.n ? (float)g.hits / g.n : 0.0f;
    const int stars = g.hits == g.n ? 3 : (f >= 0.8f ? 2 : (f >= 0.6f ? 1 : 0));
    finishGame(1, (int)(f * 100 + 0.5f), 0, stars);
  }
}

}  // namespace cat
