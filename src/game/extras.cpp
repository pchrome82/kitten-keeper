// Boxes (tissue / shoe / cardboard / crate), Magic Cat, and the scoop-and-tilt litter game.
#include "game.h"
#include "roomdata.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace cat {

static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static const Color PANEL = rgb(58, 44, 88), BORDER = rgb(130, 100, 170), TXT = rgb(236, 228, 250), DIM = rgb(160, 146, 190);
static const Color GOLD = rgb(250, 230, 150), GREEN = rgb(60, 150, 90), BLUE = rgb(70, 110, 200), PURPLE = rgb(110, 80, 160), RED = rgb(190, 70, 80);

// ---------------------------------------------------------------------------------------
// boxes
// ---------------------------------------------------------------------------------------

static const uint32_t BOX_LIFE[5] = {0, 30 * 60, 2 * 3600, 6 * 3600, 0};   // (a quarter of the original 2 h / 8 h / 24 h)
static const int BOX_PRICE[5] = {0, 2, 8, 14, 50};

const char* Game::boxName(int t) const {
  static const char* n[5] = {"No box", "Tissue Box", "Shoebox", "Cardboard Box", "Sturdy Crate"};
  return n[t < 0 || t > 4 ? 0 : t];
}

void Game::giveBox(uint8_t type) {
  sv.boxType = type;
  sv.boxExp = BOX_LIFE[type] ? epoch + BOX_LIFE[type] : 0;
}

void Game::updateBox() {
  if (!sv.hasCat) return;
  if (sv.boxType != BT_NONE && sv.boxType != BT_CRATE && epoch >= sv.boxExp) {
    if (sv.boxType == BT_TISSUE) toast("%s shredded the tissue box!", pet.d.name);
    else toast("The %s fell apart!", boxName(sv.boxType));
    sv.boxType = BT_NONE;
    sound(SND_POOF);
    autosave(false);
  }
  // (no free shoebox when they grow up any more: boxes come free with furniture, or from the shop)
}

void Game::drawBoxProp(Canvas& cv, int type, int x, int y, int part) {
  if (type == BT_NONE) return;
  static const uint8_t W_[5] = {0, 24, 34, 36, 40}, H_[5] = {0, 12, 13, 16, 18};
  int w = W_[type], h = H_[type], hw = w / 2;
  Color body, dark, light;
  switch (type) {
    case BT_TISSUE: body = rgb(112, 186, 236); dark = rgb(60, 120, 180); light = rgb(176, 224, 250); break;
    case BT_SHOE: body = rgb(196, 124, 76); dark = rgb(120, 70, 44); light = rgb(228, 162, 110); break;
    case BT_CARD: body = rgb(214, 170, 108); dark = rgb(140, 100, 60); light = rgb(236, 206, 150); break;
    default: body = rgb(112, 124, 146); dark = rgb(60, 68, 86); light = rgb(160, 172, 194); break;
  }
  const Color ol = rgb(30, 20, 36);
  if (part == 0 || part == 1) {
    cv.ellipse(x, y + 1, hw + 3, 4, rgb(30, 22, 40));                    // shadow
    cv.ellipse(x, y - h, hw, 4, ol);
    cv.ellipse(x, y - h, hw - 1, 3, dark);
    if (type == BT_TISSUE) { cv.tri(x - 4, y - h, x - 1, y - h - 8, x + 2, y - h, rgb(250, 250, 255)); cv.tri(x, y - h, x + 3, y - h - 6, x + 6, y - h, rgb(235, 238, 250)); }
  }
  if (part == 0 || part == 2) {
    for (int j = 0; j < h; j++) cv.hline(x - hw, y - h + j, w, j < 2 ? light : (j > h - 3 ? dark : body));
    cv.frame(x - hw, y - h, w, h + 1, ol);
    if (type == BT_TISSUE) { cv.ellipse(x, y - h / 2, 6, 3, rgb(30, 40, 90)); cv.ellipse(x, y - h / 2, 5, 2, rgb(250, 250, 255)); }
    else if (type == BT_SHOE) { cv.rect(x - hw + 3, y - h + 4, 12, 5, rgb(240, 70, 70)); cv.hline(x - hw + 4, y - h + 6, 10, rgb(255, 210, 210)); }
    else if (type == BT_CARD) { cv.vline(x, y - h + 2, h - 2, dark); cv.hline(x - hw + 2, y - h + 5, 9, dark); cv.rect(x + 8, y - h + 5, 10, 6, rgb(240, 240, 230)); }
    else { for (int k = -1; k <= 1; k += 2) { cv.disc(x + k * (hw - 4), y - h + 4, 1, rgb(220, 226, 240)); cv.disc(x + k * (hw - 4), y - 4, 1, rgb(220, 226, 240)); } cv.hline(x - hw + 2, y - h / 2, w - 4, dark); }
  }
}

void Game::shopBoxes(Canvas& cv, int y0) {
  char b[48];
  static const char* shortN[5] = {"", "Tissue", "Shoebox", "Cardboard", "Crate"};
  if (sv.boxType == BT_NONE) snprintf(b, sizeof(b), "No box right now");
  else if (sv.boxType == BT_CRATE) snprintf(b, sizeof(b), "Crate: forever");
  else {
    int left = (int)(sv.boxExp > epoch ? sv.boxExp - epoch : 0);
    if (left >= 3600) snprintf(b, sizeof(b), "%s %dh left", shortN[sv.boxType], (left + 1800) / 3600);
    else snprintf(b, sizeof(b), "%s %dm left", shortN[sv.boxType], (left + 30) / 60);
  }
  cv.text(6, y0 + 2, b, TXT);
  static const char* desc[5] = {"", "Lasts 30 min", "Lasts 2 hours", "Lasts 6 hours", "Lasts forever!"};   // any cat can use any box
  for (int i = 1; i <= 4; i++) {
    int y = y0 + 14 + (i - 1) * 30;
    cv.panel(4, y, 176, 28, PANEL, BORDER);
    cv.rect(8, y + 3, 34, 22, rgb(46, 34, 60));
    drawBoxProp(cv, i, 25, y + 24, 0);
    cv.text(46, y + 3, boxName(i), TXT);
    const bool locked = false;
    cv.text(46, y + 15, desc[i], DIM);
    snprintf(b, sizeof(b), "%d", BOX_PRICE[i]);
    bool can = !locked && sv.coins >= (uint32_t)BOX_PRICE[i];
    if (button(cv, 138, y + 3, 38, 22, b, locked ? rgb(70, 60, 80) : (can ? GREEN : rgb(120, 70, 80)), !locked)) {
      if (sv.coins < (uint32_t)BOX_PRICE[i]) { toast("Not enough hairballs!"); sound(SND_LOSE); }
      else { sv.coins -= (uint32_t)BOX_PRICE[i]; giveBox((uint8_t)i); sound(SND_WIN); toast("%s ready!", boxName(i)); autosave(true); }
    }
  }
}

// ---------------------------------------------------------------------------------------
// HOMES: buy a new place and move in (the cat's stuff comes along)
// ---------------------------------------------------------------------------------------

static const char* HOME_NAME[NUM_ROOMS] = {"Grimy Cartoon", "Cozy Treehouse", "Groovy '70s", "Modern Apartment", "Seaside Villa", "Classy Log Cabin", "Halo Bridge"};
static const char* HOME_VIEW[NUM_ROOMS] = {"City view, cheap rent", "Forest view, all wood", "Shag carpet, city view", "Floor-to-ceiling glass", "Sea view!", "River & forest, fireplace", "A starship! Space view"};
static const int HOME_PRICE[NUM_ROOMS] = {0, 15, 25, 35, 45, 55, 100};   // ... + the secret Halo Bridge   // Grimy, Treehouse, '70s, Modern, Villa, Log Cabin

void Game::shopHomes(Canvas& cv, int y0) {
  // the Halo Bridge only shows up once both secret cats have been found
  const bool halo = (sv.unlocked & (1u << BR_RAINBOW)) && (sv.unlocked & (1u << BR_HAMSTER));
  const int nHomes = halo ? NUM_ROOMS : NUM_ROOMS - 1;
  if (homeSel_ < 0 || homeSel_ >= nHomes) homeSel_ = 0;
  const int i = homeSel_;
  const bool owned = i == 0 || ((sv.roomsOwned >> i) & 1);
  const bool here = sv.room == i;
  // preview: the baked daytime room at quarter size (window shown as plain sky)
  const int tw = ROOM_W / 4, th = ROOM_H / 4, tx = (W - tw) / 2, ty = y0 + 3;
  cv.frame(tx - 2, ty - 2, tw + 4, th + 4, here ? GOLD : BORDER);
  cv.frame(tx - 1, ty - 1, tw + 2, th + 2, rgb(20, 12, 36));
  const uint16_t* img = ROOMS[i].day;
  const Color sky = ROOMS[i].view == 4 ? rgb(34, 22, 70) : (ROOMS[i].view == 1 ? rgb(120, 200, 236) : (ROOMS[i].view >= 2 ? rgb(150, 210, 170) : rgb(140, 210, 220)));
  for (int y = 0; y < th; y++)
    for (int x = 0; x < tw; x++) {
      uint16_t p = img[(y * 4 + 1) * ROOM_W + x * 4 + 1];
      cv.set(tx + x, ty + y, p == 0xF81F ? sky : p);
    }
  if (button(cv, 6, ty + th / 2 - 12, 30, 24, "<", PURPLE)) { homeSel_ = (i + nHomes - 1) % nHomes; sound(SND_TAP); }
  if (button(cv, W - 36, ty + th / 2 - 12, 30, 24, ">", PURPLE)) { homeSel_ = (i + 1) % nHomes; sound(SND_TAP); }
  int y = ty + th + 5;
  cv.textCenter(W / 2, y, HOME_NAME[i], i == NUM_ROOMS - 1 ? rgb(255, 214, 120) : TXT);
  cv.textCenter(W / 2, y + 10, HOME_VIEW[i], DIM);
  char b[24];
  y += 22;
  if (here) {
    button(cv, 36, y, 112, 22, "YOU LIVE HERE", PURPLE, false);
  } else if (owned) {
    if (button(cv, 36, y, 112, 22, "MOVE IN", GREEN)) {
      sv.room = (uint8_t)i; sound(SND_WIN); toast("Moved to the %s!", HOME_NAME[i]); autosave(true);
    }
  } else {
    snprintf(b, sizeof(b), "BUY  %d", HOME_PRICE[i]);
    bool can = sv.coins >= (uint32_t)HOME_PRICE[i];
    if (button(cv, 36, y, 112, 22, b, can ? GREEN : rgb(120, 70, 80))) {
      if (!can) { toast("Not enough hairballs!"); sound(SND_LOSE); }
      else {
        sv.coins -= (uint32_t)HOME_PRICE[i];
        sv.roomsOwned |= (uint8_t)(1u << i);
        sv.room = (uint8_t)i;
        sound(SND_WIN); toast("Welcome to the %s!", HOME_NAME[i]); autosave(true);
      }
    }
  }
}

// ---------------------------------------------------------------------------------------
// MAGIC CAT: is the cat under the upside-down box?
// ---------------------------------------------------------------------------------------

static void mgNewRound(MagicGame& g, Pet& p) {
  g.state = 0; g.t = 0; g.guess = -1;
  g.inside = (p.rnd() & 1) != 0;
  g.tell = g.inside ? p.rndf() < 0.75f : p.rndf() < 0.2f;     // a wobble gives it away... mostly
}

void Game::startMagic() {
  if (sv.boxType == BT_NONE) { toast("You need a box first!"); return; }
  if (pet.sleeping) { toast("Shh, sleeping!"); return; }
  memset(&mg_, 0, sizeof(mg_));
  mgNewRound(mg_, pet);
  setScreen(S_MAGIC);
}

void Game::screenMagic(Canvas& cv) {
  MagicGame& g = mg_;
  const int ROUNDS = 5;
  drawBackdrop(cv, false);
  ensureRoom();
  drawRoom(cv);   // the house behind (the same room as at home, at this time of day)
  cv.panel(4, 156, 176, 34, PANEL, BORDER);   // (the words below the table sit on a panel, not on the floor)
  float prevT = g.t;
  g.t += dt;
  cv.textCenter(W / 2, 8, "MAGIC CAT", GOLD, 1);
  char b[40];
  snprintf(b, sizeof(b), "Round %d/%d  Right %d", g.round + 1 > ROUNDS ? ROUNDS : g.round + 1, ROUNDS, g.correct);
  cv.textCenter(W / 2 + 18, 17, b, DIM);   // (above the room, so it reads on any house; clear of BACK)
  static const Color BC[5] = {rgb(0, 0, 0), rgb(112, 186, 236), rgb(196, 124, 76), rgb(214, 170, 108), rgb(112, 124, 146)};
  Color body = BC[sv.boxType > 4 ? 0 : sv.boxType];
  Color dk = shade(body, 60);
  // table
  cv.rect(10, 150, 164, 8, rgb(130, 90, 60)); cv.hline(10, 150, 164, rgb(170, 124, 86));
  if (g.state < 3) {
    int bx = 92, by = 150;
    float lift = 0;
    if (g.state == 1) lift = clampf(g.t / 0.8f, 0, 1) * 62;
    if (g.state == 1 && g.t > 0.8f) lift = 62;
    int wob = 0;
    if (g.state == 0 && g.tell) { float ph = fmodf(g.t, 1.7f); if (ph < 0.5f) wob = (int)(sinf(ph * 60) * 3); }
    // what is underneath
    if (g.state == 1 && lift > 4) {
      if (g.inside) drawLittleCat(cv, bx, by, POSE_SIT, g.t > 1.0f ? FACE_HAPPY : FACE_SURPRISE, false, 0.95f, by - 62 + 3);   // fits under the box once it is fully lifted
      else { cv.ellipse(bx, by - 1, 22, 4, rgb(100, 70, 46)); }
    }
    // the upside-down box
    int top = by - 46 - (int)lift;
    cv.ellipse(bx + wob, by - (int)lift + 1, 34, 5, rgb(30, 22, 40));
    cv.rect(bx - 32 + wob, top, 64, 46, body);
    cv.rect(bx - 32 + wob, top, 64, 5, shade(body, 120));
    cv.rect(bx - 32 + wob, top + 40, 64, 6, dk);
    cv.frame(bx - 32 + wob, top, 64, 46, rgb(30, 20, 36));
    cv.vline(bx + wob, top + 6, 8, dk);
    cv.textCenter(bx + wob, top + 18, "THIS SIDE", dk);
    cv.textCenter(bx + wob, top + 28, "UP", dk);
  }
  if (g.state == 0) {
    cv.textCenter(W / 2, 168 - 6, "Is the cat under the box?", TXT);
    cv.textCenter(W / 2, 176 - 3, "Watch for a wobble...", DIM);
    if (button(cv, 8, 190, 82, 22, "IN THERE!", GREEN)) { g.guess = 1; g.state = 1; g.t = 0; sound(SND_TAP); }
    if (button(cv, 94, 190, 82, 22, "EMPTY", BLUE)) { g.guess = 0; g.state = 1; g.t = 0; sound(SND_TAP); }
  } else if (g.state == 1) {
    if (prevT < 0.8f && g.t >= 0.8f) sound(g.inside ? SND_MEOW_SOFT : SND_SAD);   // the empty box gets a disappointed meow
    if (prevT < 1.5f && g.t >= 1.5f) sound((g.guess == 1) == g.inside ? SND_CHIME : SND_MISS);
    if (g.t > 0.9f) {
      bool right = (g.guess == 1) == g.inside;
      cv.textCenter(W / 2, 172, g.inside ? "They were inside!" : "Empty... meow.", g.inside ? rgb(150, 255, 190) : rgb(255, 190, 150));
      cv.textCenter(W / 2, 186, right ? "You guessed right!" : "Wrong guess!", right ? rgb(150, 255, 190) : rgb(255, 170, 170));
    }
    if (g.t > 2.6f) {
      if ((g.guess == 1) == g.inside) g.correct++;
      g.round++;
      if (g.round >= ROUNDS) { g.state = 3; g.t = 0; sound(SND_WIN); } else mgNewRound(g, pet);
    }
  } else {
    cv.panel(18, 54, 148, 100, PANEL, BORDER);
    cv.textCenter(W / 2, 62, "ALL DONE", GOLD);
    snprintf(b, sizeof(b), "Right: %d of %d", g.correct, ROUNDS);
    cv.textCenter(W / 2, 80, b, TXT);
    // Stars: 3 right = 1 star, 4 right = 2, 5 right = 3 (a hairball each, and fun +17/+25/+33 like the other games)
    const int stars = g.correct >= 3 ? g.correct - 2 : 0;
    const int reward = stars;
    for (int i = 0; i < 3; i++) {
      const int sx = W / 2 - 24 + i * 24, sy = 100;
      const Color sc = i < stars ? GOLD : rgb(60, 54, 80);
      cv.rect(sx - 2, sy - 7, 5, 15, sc); cv.rect(sx - 7, sy - 2, 15, 5, sc); cv.rect(sx - 5, sy - 5, 11, 11, sc);
    }
    snprintf(b, sizeof(b), "Prize: %d hairball%s", reward, reward == 1 ? "" : "s");
    cv.textCenter(W / 2, 114, b, rgb(255, 240, 160));
    const int fun = (int)gfun((float)(reward >= 3 ? 100 : (reward == 2 ? 75 : (reward == 1 ? 50 : 0))));   // same star rule as the other games
    snprintf(b, sizeof(b), "Fun +%d", fun);
    cv.textCenter(W / 2, 126, b, rgb(250, 130, 170));
    cv.textCenter(W / 2, 136, "Wobble = someone inside", DIM);
    if (button(cv, 52, 164 - 6, 80, 22, "DONE", GREEN)) {
      if (reward) giveCoins(reward);
      if (reward >= 3) recordMilestone(MS_STARS);
      pet.d.fun += (float)fun; pet.clampAll();
      sv.lifeGames++;
      autosave(true);
      setScreen(S_HOME);
    }
  }
  updateParticles(dt);
  drawToasts(cv);
  if (g.state != 3 && backButton(cv)) { setScreen(S_HOME); return; }   // quit: no prize
}

// ---------------------------------------------------------------------------------------
// LITTER: scoop a poop with the finger, carry it to the can, tilt the device to drop it in
// ---------------------------------------------------------------------------------------

static const int CAN_X = 152, CAN_RIM = 114, CAN_BOT = 170, FLOOR_Y = 174;

// Does a dropped lump (a 10 x 6 px blob centred on x, y) count as "in the bin"? Yes when at least 55% of it
// overlaps the can's mouth (28 px wide), even if it's hanging over an edge, and it isn't below the can.
static bool lumpInCan(float x, float y) {
  const float half = 5.0f, mouth = 14.0f;
  const float ov = fminf(x + half, CAN_X + mouth) - fmaxf(x - half, CAN_X - mouth);
  return ov >= 0.55f * (2 * half) && y < CAN_BOT && y > CAN_RIM - 60;
}

// A poop on the floor (missed in the Box game, or left there): the same little lump everywhere, with a whiff of stink.
// x, y = where it sits on the floor; k staggers the stink between several.
void drawFloorPoop(Canvas& cv, int x, int y, float anim, int k) {
  cv.ellipse(x, y + 1, 6, 3, rgb(40, 24, 16)); cv.ellipse(x, y - 1, 5, 3, rgb(96, 60, 34)); cv.ellipse(x - 1, y - 3, 3, 2, rgb(126, 84, 50));
  if (((int)(anim * 5) + k) % 3 == 0) cv.set(x + 2, y - 8, rgb(150, 200, 70));
}

void Game::screenLitter(Canvas& cv) {
  // Poops are buried in the sand. Run a finger through it to dig them up, then drag each one into the bin.
  LitterGame& L = lt_;
  const int TX = 11, TY = 31, TW = 110, TH = 102;   // tray sand area
  drawBackdrop(cv, false);
  panelTitle(cv, "LITTER BOX", 6);
  // Where to bury something: the best of 12 random spots, the one farthest from every other buried thing, so they
  // spread over the whole tray instead of bunching up.  (self = the lump being placed, -1 for the hairball;
  // only lumps 0..upto-1 count as already placed)
  auto pickLumpSpot = [&](int self, int upto) {
    float bestX = (float)(TX + TW / 2), bestY = (float)(TY + TH / 2), bestD = -1;
    for (int c = 0; c < 12; c++) {
      const float cx = (float)TX + 10.0f + pet.rndf() * (float)(TW - 20), cy = (float)TY + 10.0f + pet.rndf() * (float)(TH - 20);
      float md = 1e9f;
      for (int j = 0; j < upto && j < 6; j++) {
        if (j == self || dropGone_[j]) continue;
        const float dx = cx - dropX_[j], dy = cy - dropY_[j];
        md = fminf(md, dx * dx + dy * dy);
      }
      if (self >= 0 && L.hbHere) { const float dx = cx - L.hbX, dy = cy - L.hbY; md = fminf(md, dx * dx + dy * dy); }
      if (md > bestD) { bestD = md; bestX = cx; bestY = cy; }
    }
    if (self >= 0) { dropX_[self] = bestX; dropY_[self] = bestY; } else { L.hbX = bestX; L.hbY = bestY; }
  };
  // every visit starts with smooth, freshly raked sand; whatever is in the box is buried again somewhere new.
  // A hairball you didn't find stays put, and a new one can only turn up after they're used the box since.
  // (the first frame after the box was opened: a slow frame on the device used to skip this and leave
  //  six blank lumps parked outside the tray - "6 left" with nothing to find)
  if (litterSerial_ != scrSerial_) {
    litterSerial_ = scrSerial_;
    const bool keepHb = L.init && L.hbHere && !L.hbFound;
    const float hbX = L.hbX, hbY = L.hbY;
    const bool mayRoll = !L.init || pet.d.droppings > L.known;
    memset(&L, 0, sizeof(L));
    L.init = true;
    memset(L.sand, 255, sizeof(L.sand));
    L.carryIdx = -1;
    litterHits_ = 0;
    int n = pet.d.droppings;
    for (int i = 0; i < 6; i++) { dropGone_[i] = i >= n; dropX_[i] = dropY_[i] = 0; }
    for (int i = 0; i < n && i < 6; i++) pickLumpSpot(i, i);   // hidden spots, spread out over the whole tray, different every visit
    // now and then a lost hairball is buried in there too (at most 6 finds per day)
    const uint8_t day = (uint8_t)((epoch / 86400u) % 32u);
    if ((sv.pad[1] >> 3) != day) sv.pad[1] = (uint8_t)(day << 3);
    L.known = pet.d.droppings;   // (a second set-up frame mustn't roll again)
    if (keepHb) { L.hbHere = true; L.hbX = hbX; L.hbY = hbY; }
    else if (mayRoll && (sv.pad[1] & 7) < 6 && pet.rnd() % 100 < 6) {
      L.hbHere = true;
      pickLumpSpot(-1, 6);
    }
  }
  if (L.msgT > 0) L.msgT -= dt;
  // keep the lumps in the sand in step with the real count, both ways: they can poop while you're here,
  // and a sifting / self-cleaning box can empty itself while you're here
  { const bool holding = (L.carry || L.falling) && L.carryIdx >= 0;
    int alive = holding ? 1 : 0;
    for (int i = 0; i < 6; i++) if (!dropGone_[i]) alive++;
    for (int i = 5; i >= 0 && alive > pet.d.droppings; i--) if (!dropGone_[i]) { dropGone_[i] = true; alive--; }
    if (holding && alive > pet.d.droppings) { L.carry = L.falling = false; L.carryIdx = -1; alive--; }
    while (alive < pet.d.droppings && alive < 6) {
      for (int i = 0; i < 6; i++) if (dropGone_[i] && i != L.carryIdx) {
        dropGone_[i] = false; alive++;
        pickLumpSpot(i, 6);
        for (int y = 0; y < LT_GY; y++) for (int x = 0; x < LT_GX; x++) {   // freshly covered
          float dx = TX + x * 2 - dropX_[i], dy = TY + y * 2 - dropY_[i];
          if (dx * dx + dy * dy < 100) L.sand[y][x] = 255;
        }
        break;
      }
    }
  }

  // ---- input: dig / pick up / carry ----
  const bool inTray = in.x >= TX && in.x < TX + TW && in.y >= TY && in.y < TY + TH;
  const float lastX = L.sx, lastY = L.sy;
  const bool wasDown = in.down && !in.pressed;   // (the finger was already down: dig along the line it moved)
  if (in.down) { L.sx = (float)in.x; L.sy = (float)in.y; }
  if (in.down && inTray && !L.carry) {   // digging moves sand out of the way
    const int R = 9;
    // a quick swipe moves the finger a long way between frames: dig at points along the whole line, sharing this
    // frame's digging between them, so it uncovers an even strip instead of a row of holes
    const float ddx = wasDown ? in.x - lastX : 0, ddy = wasDown ? in.y - lastY : 0;
    const int steps = 1 + (int)(sqrtf(ddx * ddx + ddy * ddy) / 4.0f);
    for (int s = 0; s < steps && s < 24; s++) {
      const float f = (float)(s + 1) / steps;
      const int cx = (int)(in.x - ddx * (1.0f - f)), cy = (int)(in.y - ddy * (1.0f - f));
      const float share = steps > 1 ? 2.2f / steps : 1.0f;   // (a long line still digs a firm strip)
      for (int y = -R; y <= R; y += 2) for (int x = -R; x <= R; x += 2) {
        int gx = (cx + x - TX) / 2, gy = (cy + y - TY) / 2;
        if (gx < 0 || gy < 0 || gx >= LT_GX || gy >= LT_GY) continue;
        float k = 1.0f - (x * x + y * y) / (float)(R * R + 1);
        if (k <= 0) continue;
        int v = L.sand[gy][gx] - (int)(4200.0f * dt * k * (share > 1.0f ? 1.0f : share) * (steps > 1 ? 1.6f : 1.0f));   // one firm swipe uncovers a strip
        L.sand[gy][gx] = (uint8_t)(v < 0 ? 0 : v);
      }
    }
    if (((int)(anim_ * 12)) % 3 == 0) spawn((float)in.x, (float)in.y, 4, rgb(226, 204, 150), (float)((int)(pet.rnd() % 40) - 20), -25, 0.4f);
    if ((int)(anim_ * 5) != (int)((anim_ - dt) * 5)) sound(SND_SCRATCH);   // shff... shff...
  }
  auto sandAt = [&](float px, float py) -> int {   // thinnest sand over a lump-sized spot (so partly dug = found)
    int best = 255;
    for (int oy = -2; oy <= 2; oy += 2) for (int ox = -4; ox <= 4; ox += 2) {
      int gx = (int)(px + ox - TX) / 2, gy = (int)(py + oy - TY) / 2;
      int v = (gx < 0 || gy < 0 || gx >= LT_GX || gy >= LT_GY) ? 0 : L.sand[gy][gx];
      if (v < best) best = v;
    }
    return best;
  };
  if (L.hbHere && !L.hbFound && sandAt(L.hbX, L.hbY) <= 110) {   // dug up the treasure!
    L.hbFound = true; L.hbT = 0;
    giveCoins(1); sound(SND_WIN);
    sv.pad[1]++;   // today's finds
    toast("You found a hairball! +1");
    for (int i = 0; i < 10; i++) spawn(L.hbX, L.hbY, 1, rgb(255, 240, 120), (float)((i - 5) * 12), -40 - (float)(i % 3) * 10, 0.9f);
    autosave(false);
  }
  if (in.down && !L.carry && !L.falling) {
    for (int i = 0; i < 6 && !L.carry; i++) {   // an uncovered lump sticks to the finger
      if (dropGone_[i] || sandAt(dropX_[i], dropY_[i]) > 110) continue;
      float dx = dropX_[i] - in.x, dy = dropY_[i] - in.y;
      if (dx * dx + dy * dy < 9.0f * 9.0f) {
        dropGone_[i] = true; L.carry = true; L.carryIdx = i; sound(SND_SCOOP);
        snprintf(L.msg, sizeof(L.msg), "Found one! Into the bin"); L.msgT = 1.4f;
      }
    }
    for (int i = 0; i < L.floorN && !L.carry; i++) {
      float dx = L.floorX[i] - in.x, dy = (float)FLOOR_Y - in.y;
      if (dx * dx + dy * dy < 12.0f * 12.0f) { L.carry = true; L.carryIdx = -1; for (int k = i; k + 1 < L.floorN; k++) L.floorX[k] = L.floorX[k + 1]; L.floorN--; sound(SND_SCOOP); }
    }
  }
  if (L.carry && !in.down) {   // let go: over the sand it just drops back in; over the bin it goes in; else it falls
    L.carry = false;
    const bool overTray = L.sx >= TX && L.sx < TX + TW && L.sy >= TY && L.sy < TY + TH;
    if (overTray && L.carryIdx >= 0) {
      dropGone_[L.carryIdx] = false; dropX_[L.carryIdx] = L.sx; dropY_[L.carryIdx] = L.sy; L.carryIdx = -1;
      for (int y = -3; y <= 3; y++) for (int x = -4; x <= 4; x++) {   // it sits on top, still visible
        int gx = ((int)L.sx - TX) / 2 + x, gy = ((int)L.sy - TY) / 2 + y;
        if (gx >= 0 && gy >= 0 && gx < LT_GX && gy < LT_GY && L.sand[gy][gx] > 60) L.sand[gy][gx] = 60;
      }
      snprintf(L.msg, sizeof(L.msg), "Drag it to the bin!"); L.msgT = 1.2f;
    } else {
      L.falling = true; L.fx = L.sx; L.fy = L.sy; L.fvx = 0; L.fvy = 0;
      L.fy = L.sy - 8;   // (it is drawn 8 px above the fingertip while carried)
      L.fallToCan = lumpInCan(L.sx, L.fy);
    }
  }

  // ---- draw: floor, bin, tray ----
  ensureRoom();                                          // the floor is the current house's own floor (and its time of day)
  cv.rect(0, 146, W, TOOL_Y - 146, KEY_COLOR);
  fi.keyed = true;
  cv.rect(0, TOOL_Y, W, 8, rgb(40, 30, 62));
  cv.panel(TX - 5, TY - 5, TW + 10, TH + 10, rgb(104, 144, 188), rgb(64, 92, 136));
  // lumps first (under the sand), then sand on top where it still covers them
  for (int i = 0; i < 6; i++) {
    if (dropGone_[i]) continue;
    int x = (int)dropX_[i], y = (int)dropY_[i];
    cv.ellipse(x, y, 5, 3, rgb(86, 54, 30)); cv.ellipse(x - 1, y - 1, 3, 2, rgb(124, 84, 52)); cv.set(x - 2, y - 2, rgb(170, 124, 84));
  }
  for (int gy = 0; gy < LT_GY; gy++) for (int gx = 0; gx < LT_GX; gx++) {
    int v = L.sand[gy][gx], px = TX + gx * 2, py = TY + gy * 2;
    bool speck = ((gx * 37 + gy * 53) % 11) == 0;
    if (v > 110) {   // sand still covers this spot
      Color c = speck ? rgb(206, 184, 128) : (v > 200 ? rgb(232, 214, 160) : rgb(214, 192, 136));
      cv.rect(px, py, 2, 2, c);
    } else {
      // dug: we can see what's underneath; draw darker damp sand only where there's no lump
      bool lump = false;
      for (int i = 0; i < 6 && !lump; i++) if (!dropGone_[i] && fabsf(px - dropX_[i]) < 6 && fabsf(py - dropY_[i]) < 4) lump = true;
      if (!lump) cv.rect(px, py, 2, 2, v > 40 ? rgb(196, 172, 118) : rgb(176, 150, 100));
    }
  }
  // the found hairball pops up, spins and floats away
  if (L.hbFound && L.hbT < 1.6f) {
    L.hbT += dt;
    float k = L.hbT / 1.6f;
    int x = (int)L.hbX + (int)(sinf(L.hbT * 12) * 3), y = (int)(L.hbY - 30 * sinf(k * 3.14159f * 0.5f));
    if (((int)(L.hbT * 20)) % 2 == 0 || k < 0.7f) drawIconS(cv, 6, x, y, 1.0f + 0.4f * sinf(k * 3.14159f));
    cv.textCenter(x, y - 14, "+1", rgb(255, 240, 120));
  }
  // stink still gives buried lumps away
  for (int i = 0; i < 6; i++) {
    if (dropGone_[i] || pet.d.droppings < 2) continue;
    if (((int)(anim_ * 4) + i) % 4 == 0) { int y = (int)dropY_[i] - 6 - ((int)(anim_ * 8) % 6); cv.set((int)dropX_[i] + ((y & 1) ? 1 : 0), y, rgb(150, 200, 70)); }
  }
  // trash can
  cv.ellipse(CAN_X, CAN_BOT + 1, 18, 4, rgb(60, 40, 30));
  for (int j = CAN_RIM; j < CAN_BOT; j++) cv.hline(CAN_X - 14 + (j - CAN_RIM) / 14, j, 28 - 2 * ((j - CAN_RIM) / 14), mix(rgb(150, 160, 176), rgb(86, 94, 112), (j - CAN_RIM) * 256 / (CAN_BOT - CAN_RIM)));
  cv.ellipse(CAN_X, CAN_RIM, 14, 3, rgb(40, 44, 56));
  cv.frame(CAN_X - 14, CAN_RIM, 28, CAN_BOT - CAN_RIM, rgb(40, 44, 56));
  for (int k = -1; k <= 1; k++) cv.vline(CAN_X + k * 7, CAN_RIM + 6, CAN_BOT - CAN_RIM - 12, rgb(70, 78, 96));
  if (L.carry) {   // the bin glows when you're carrying something
    bool over = lumpInCan(L.sx, L.sy - 8);
    cv.ellipse(CAN_X, CAN_RIM, 15, 4, over ? rgb(140, 255, 160) : rgb(255, 230, 120));
    cv.ellipse(CAN_X, CAN_RIM, 13, 2, rgb(40, 44, 56));
  }
  // floor lumps (missed)
  for (int i = 0; i < L.floorN; i++) {
    int x = (int)L.floorX[i];
    drawFloorPoop(cv, x, FLOOR_Y, anim_, i);
  }
  // carried lump + fingertip
  if (L.carry) {
    int x = (int)L.sx, y = (int)L.sy - 8;
    cv.ellipse(x, y, 5, 3, rgb(86, 54, 30)); cv.ellipse(x - 1, y - 1, 3, 2, rgb(124, 84, 52)); cv.set(x - 2, y - 2, rgb(170, 124, 84));
  } else if (in.down && inTray) {
    cv.ring(in.x, in.y, 7, rgb(255, 255, 255));
  }
  // falling lump
  if (L.falling) {
    L.fvy += 300.0f * dt; L.fx += L.fvx * dt; L.fy += L.fvy * dt;
    if (L.fallToCan) L.fx += (CAN_X - L.fx) * clampf(dt * 10, 0, 1);   // guided into the bin
    cv.ellipse((int)L.fx, (int)L.fy, 4, 3, rgb(86, 54, 30)); cv.ellipse((int)L.fx - 1, (int)L.fy - 1, 2, 1, rgb(124, 84, 52));
    const bool inTrayNow = L.fx >= TX && L.fx < TX + TW && L.fy >= TY + 6 && L.fy < TY + TH;
    if (L.fallToCan && L.fy >= CAN_RIM + 6) {   // (also when it was let go beside the can's body: it still counts)
      L.falling = false; L.cleaned++; sound(SND_BLOOP); spawn(CAN_X, CAN_RIM, 6, rgb(255, 255, 160), 0, -20, 0.6f);
      if (L.carryIdx >= 0) { pet.scoop(1); litterHits_++; }
      L.carryIdx = -1;
      snprintf(L.msg, sizeof(L.msg), "In the bin!"); L.msgT = 1.2f;
      if (pet.d.droppings == 0 && L.floorN == 0) { toast("Squeaky clean!"); pet.petIt(2); autosave(false); }
    } else if (!L.fallToCan && L.carryIdx >= 0 && inTrayNow && L.fy > L.sy + 4) {   // dropped back in the sand
      L.falling = false; dropGone_[L.carryIdx] = false; dropX_[L.carryIdx] = L.fx; dropY_[L.carryIdx] = L.fy; L.carryIdx = -1;
      snprintf(L.msg, sizeof(L.msg), "Back in the sand..."); L.msgT = 1.2f;
    } else if (L.fy >= FLOOR_Y) {
      L.falling = false;
      if (L.carryIdx >= 0) { pet.scoop(1); }   // it's out of the box now: it's on the floor instead
      L.carryIdx = -1;
      float x = clampf(L.fx, 10, 174);
      if (fabsf(x - CAN_X) < 18) x = CAN_X - 26;
      if (L.floorN < 4) L.floorX[L.floorN++] = x;
      sound(SND_SAD); snprintf(L.msg, sizeof(L.msg), "Missed! Pick it up"); L.msgT = 1.6f;
    }
  }
  updateParticles(dt);
  drawParticles(cv);
  drawToasts(cv);
  int left = 0; for (int i = 0; i < 6; i++) if (!dropGone_[i]) left++;
  const char* info;
  if (L.msgT > 0) info = L.msg;
  else if (L.carry) info = "Drag it over the bin, let go";
  else if (left > 0) info = "Dig through the sand!";
  else if (L.floorN > 0) info = "Pick up the one on the floor";
  else info = "All clean. Good human!";
  cv.textCenter(W / 2, 178 + 3, info, L.msgT > 0 ? rgb(255, 240, 160) : TXT);
  char cnt[24]; snprintf(cnt, sizeof(cnt), "%d left", left);
  cv.textCenter(CAN_X, CAN_RIM - 22, cnt, left ? rgb(255, 214, 120) : rgb(140, 240, 160));
  if (pet.d.droppings >= 4 && sv.litterTier < 2) cv.textCenter(W / 2, 150, "Dirty! They're getting grumpy", rgb(255, 190, 170));
  else if (pet.d.droppings == 3 && sv.litterTier < 2) cv.textCenter(W / 2, 150, "Nearly full - scoop soon", rgb(255, 225, 160));
  if (button(cv, 40, 196, 104, 24, "BACK", PURPLE)) { leaveLitter(); setScreen(S_HOME); }
}

// Leaving the box (BACK, or the power button): put everything where it really is, so the next visit matches.
void Game::leaveLitter() {
  LitterGame& L = lt_;
  int mess = L.floorN + ((L.carry || L.falling) && L.carryIdx < 0 ? 1 : 0);   // anything left on the floor stays on the floor
  if (mess) { int m = pet.d.floorMess + mess; pet.d.floorMess = (uint8_t)(m > 4 ? 4 : m); }
  if ((L.carry || L.falling) && L.carryIdx >= 0) dropGone_[L.carryIdx] = false;   // still in the box: it stays there
  L.carry = L.falling = false; L.carryIdx = -1; L.floorN = 0;
  L.known = pet.d.droppings;
  autosave(true);
}

}  // namespace cat

namespace cat {

// ---------------------------------------------------------------------------------------
// the journal: milestones and an album of photos (one per life stage)
// ---------------------------------------------------------------------------------------

static const char* const MS_LABEL[MS_COUNT] = {
  "First meal", "First hairball", "A catch from a hunt", "3 stars in a game", "First Explore trip",
  "Beat a wild cat", "Beat the Alley King", "Found the hidden cave", "Explored together", "First birthday",
  "A full lap nap", "Their first furniture", "All grown up", "A wise old senior"};

void Game::recordMilestone(int m) {
  if (!sv.hasCat || m < 0 || m >= MS_COUNT) return;
  if (sv.x8.milestones & (1u << m)) return;
  sv.x8.milestones |= 1u << m;
  const float days = pet.ageDays();
  sv.x8.msDay[m] = (uint8_t)(days < 0 ? 0 : (days > 255 ? 255 : days));
  if (m != MS_GROWN && m != MS_SENIOR) toast("Journal: %s!", MS_LABEL[m]);
  dirty_ = true;
}

void Game::takePhoto(int stage) {
  if (stage < 0 || stage > 3) return;
  SaveData::Snap& p = sv.x8.photo[stage];
  if (p.taken) return;
  const CatDraw d = catLook(POSE_SIT, FACE_HAPPY);
  p.taken = 1; p.breed = (uint8_t)pet.lookBreed(); p.fat10 = (int8_t)clampf(d.fat * 10.0f, -10, 13);
  p.hat = sv.wornHat; p.gl = sv.wornGl; p.neck = sv.wornNeck; p.body = sv.wornCollar;
  const float days = pet.ageDays();
  p.day = (uint8_t)(days < 0 ? 0 : (days > 255 ? 255 : days));
  dirty_ = true;
}

// ---------------------------------------------------------------------------------------
// seasons, holidays and birthdays
// ---------------------------------------------------------------------------------------

int Game::season() const {
  if (!epoch) return SEASON_NONE;
  const DateTime t = fromEpoch(epoch);
  if (t.month == 10 && t.day >= 24) return SEASON_HALLOWEEN;          // the last week of October
  if (t.month == 12 && t.day >= 20 && t.day <= 26) return SEASON_HOLIDAYS;
  if ((t.month == 12 && t.day == 31) || (t.month == 1 && t.day == 1)) return SEASON_NEWYEAR;
  if (t.month == 12 || t.month <= 2) return SEASON_WINTER;            // snow instead of rain
  return SEASON_NONE;
}

static bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

// Is this date the cat's birthday? (A February 29th birthday is held on the 28th in years without one.)
bool Game::calendarBirthday(const DateTime& t) const {
  if (!sv.hasCat || !sv.x8.bdayMonth) return false;
  int bd = sv.x8.bdayDay;
  if (sv.x8.bdayMonth == 2 && bd == 29 && !isLeap(t.year)) bd = 28;
  return t.month == sv.x8.bdayMonth && t.day == bd;
}

// The game picks a birthday for each cat on its own (about 10 days after it came home, so they get to celebrate it);
// the player can move it in Settings.
void Game::ensureBirthday() {
  if (!sv.hasCat || sv.x8.bdayMonth || !epoch) return;
  uint32_t born = epoch > (uint32_t)pet.d.age ? epoch - (uint32_t)pet.d.age : epoch;
  uint32_t when = born + 10u * 86400u;
  if (when < epoch) when = epoch + 3u * 86400u;   // an older cat: a few days from now
  const DateTime t = fromEpoch(when);
  sv.x8.bdayMonth = t.month; sv.x8.bdayDay = t.day;
  dirty_ = true;
}

bool Game::birthdayToday() const {   // the chosen birthday, and a cat's life is about a month so every week of it is a party too
  if (!sv.hasCat) return false;
  if (epoch && calendarBirthday(fromEpoch(epoch))) return true;
  const float days = pet.ageDays();
  const int weeks = (int)(days / 7.0f);
  return weeks >= 1 && days - weeks * 7.0f < 1.0f;
}

int Game::seasonalHat() const {
  if (birthdayToday()) return HAT_PARTY;
  switch (season()) {
    case SEASON_HALLOWEEN: return HAT_PUMPKIN;
    case SEASON_HOLIDAYS: return HAT_SANTA;
    case SEASON_NEWYEAR: return HAT_PARTY;
    default: return 0;
  }
}

void Game::updateSpecialDays() {
  if (!sv.hasCat) return;
  // photos for the album, the first time we see them at each life stage
  const int st = pet.stage();
  if (!sv.x8.photo[st].taken) takePhoto(st);
  if (st >= STAGE_ADULT) recordMilestone(MS_GROWN);
  if (st >= STAGE_SENIOR) recordMilestone(MS_SENIOR);
  if (scr != S_HOME) return;   // the parties happen at home
  ensureBirthday();
  if (epoch) {   // the real birthday: once a year, on the day the player picked
    const DateTime now = fromEpoch(epoch);
    if (calendarBirthday(now) && sv.x8.bdayYear != now.year) {
      sv.x8.bdayYear = now.year;
      toast("Happy birthday, %s!", pet.d.name);
      toast("Party time! +5 hairballs");
      giveCoins(5);
      giftArrives(1);   // and a birthday present
      pet.d.fun = clampf(pet.d.fun + 20, 0, 100);
      sound(SND_WIN);
      static const Color bc[4] = {rgb(255, 120, 170), rgb(255, 220, 80), rgb(120, 220, 255), rgb(160, 255, 140)};
      for (int i = 0; i < 20; i++) spawn(a_.x + (float)((int)(pet.rnd() % 70) - 35), a_.y - 50, 7, bc[i & 3], (float)((int)(pet.rnd() % 70) - 35), -40.0f - (pet.rnd() % 30), 1.8f);
      recordMilestone(MS_BIRTHDAY);
      autosave(true);
    }
  }
  // weekly birthdays: a party hat for the day, confetti, a present
  const int weeks = (int)(pet.ageDays() / 7.0f);
  if (weeks >= 1 && weeks > sv.x8.bdayWeek && weeks < 250) {
    sv.x8.bdayWeek = (uint8_t)weeks;
    toast("Happy birthday, %s!", pet.d.name);
    toast("%d week%s old! +5 hairballs", weeks, weeks > 1 ? "s" : "");
    giveCoins(5);
    pet.d.fun = clampf(pet.d.fun + 20, 0, 100);
    sound(SND_WIN);
    static const Color cc[4] = {rgb(255, 120, 170), rgb(255, 220, 80), rgb(120, 220, 255), rgb(160, 255, 140)};
    for (int i = 0; i < 16; i++) spawn(a_.x + (float)((int)(pet.rnd() % 60) - 30), a_.y - 50, 7, cc[i & 3], (float)((int)(pet.rnd() % 60) - 30), -40.0f - (pet.rnd() % 30), 1.6f);
    recordMilestone(MS_BIRTHDAY);
    autosave(true);
  }
  // a greeting once per season
  const int s = season();
  if (s != SEASON_NONE && sv.x8.seasonSeen != s) {
    sv.x8.seasonSeen = (uint8_t)s;
    static const char* hi[5] = {"", "Happy Halloween! Boo!", "Brr, it's snowing out there!", "Happy holidays!", "Happy New Year!"};
    toast("%s", hi[s]);
    if (s == SEASON_HALLOWEEN || s == SEASON_HOLIDAYS || s == SEASON_NEWYEAR) giftArrives(2);   // a present for the occasion
    dirty_ = true;
  } else if (s == SEASON_NONE && sv.x8.seasonSeen) sv.x8.seasonSeen = 0;
  // good care: at the first visit home on a new day, a cat who is healthy, fed and content adds a day to the streak
  // (anything else starts it over); GIFT_STREAK days in a row earn a gift box
  if (epoch) {
    const uint8_t today = (uint8_t)((epoch / 86400u) % 255u + 1);
    if (sv.pad9[2] != today) {
      if (sv.pad9[2]) {
        const bool good = pet.d.health >= 70 && pet.d.resent < 40 && pet.d.hunger >= 25;
        sv.pad9[1] = good ? (uint8_t)(sv.pad9[1] + 1) : 0;
      }
      sv.pad9[2] = today;
      dirty_ = true;
    }
    if (sv.pad9[1] >= GIFT_STREAK && !sv.pad9[0]) { sv.pad9[1] = 0; giftArrives(3); }
  }
}

// a jack-o'-lantern at Halloween, a little gift box over the holidays (drawn on the floor, behind the cat)
void Game::drawSeasonalDecor(Canvas& cv) {
  const int s = season();
  const Color ol = rgb(30, 20, 36);
  if (s == SEASON_HALLOWEEN) {
    const int x = 132, y = 182;
    cv.ellipse(x, y - 6, 9, 7, ol); cv.ellipse(x, y - 6, 8, 6, rgb(236, 128, 40));
    cv.vline(x - 3, y - 11, 9, rgb(206, 100, 30)); cv.vline(x + 3, y - 11, 9, rgb(206, 100, 30));
    cv.rect(x - 1, y - 15, 3, 4, rgb(80, 120, 50));
    const bool glow = ((int)(anim_ * 3)) % 7 != 0;   // the candle flickers
    const Color f = glow ? rgb(255, 230, 110) : rgb(255, 190, 80);
    cv.tri(x - 6, y - 7, x - 3, y - 7, x - 4, y - 10, f); cv.tri(x + 3, y - 7, x + 6, y - 7, x + 4, y - 10, f);
    cv.hline(x - 4, y - 3, 9, f); cv.set(x - 2, y - 4, f); cv.set(x + 2, y - 4, f);
  } else if (s == SEASON_HOLIDAYS || s == SEASON_NEWYEAR) {
    const int x = 132, y = 182;
    cv.rect(x - 8, y - 12, 16, 12, ol); cv.rect(x - 7, y - 11, 14, 10, rgb(200, 50, 70));
    cv.vline(x, y - 11, 10, rgb(250, 220, 90)); cv.hline(x - 7, y - 7, 14, rgb(250, 220, 90));
    cv.disc(x - 2, y - 14, 2, rgb(250, 220, 90)); cv.disc(x + 2, y - 14, 2, rgb(250, 220, 90));
  }
}

// The journal (it holds the Hall of Cats too), kept until the game is reset from Settings:
//   the cats: everyone you've raised, the one you have now first (7 to a page; tap one to see their album)
//   an album page for each of them: a photo at every life stage, and how their story went
//   the milestones (the whole game's: they carry on from cat to cat)
void Game::screenJournal(Canvas& cv) {
  drawBackdrop(cv, false);
  const int cur = sv.hasCat ? 1 : 0;
  const int cats = cur + sv.albumCount;
  const int ROWS = 7;
  const int listPages = cats > 0 ? (cats + ROWS - 1) / ROWS : 1;
  const int PAGES = listPages + cats + 1;
  if (journalPage_ < 0 || journalPage_ >= PAGES) journalPage_ = 0;
  char b[48];
  auto upper = [](char* s) { for (char* p = s; *p; p++) if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32); };
  auto endColor = [](int e) { return e == END_RETIRED ? rgb(150, 255, 190) : (e == END_SUN || e == END_BLACK_HOLE ? rgb(255, 214, 120) : rgb(255, 170, 160)); };
  const int pg = journalPage_;
  if (pg < listPages) {   // ---- the cats ----
    panelTitle(cv, "JOURNAL", 6);
    if (cats == 0) {
      cv.textCenter(W / 2, 80, "No cats yet.", TXT);
      cv.textCenter(W / 2, 94, "Take good care of", DIM);
      cv.textCenter(W / 2, 105, "your first one!", DIM);
    }
    for (int r = 0; r < ROWS; r++) {
      const int i = pg * ROWS + r;
      if (i >= cats) break;
      const int y = 22 + r * 21;
      const bool now = i < cur;
      const SaveData::AlbumCat* a = now ? nullptr : &sv.album[i - cur];
      const bool hit = tapIn(4, y, 176, 19);
      cv.panel(4, y, 176, 19, now ? rgb(46, 70, 60) : PANEL, now ? rgb(120, 220, 160) : BORDER);
      CatDraw d; d.breed = now ? pet.lookBreed() : a->breed; d.pose = POSE_FRONT; d.size = 0.3f; d.face = FACE_NORMAL;
      drawCat(cv, 17, y + 17, d);
      cv.text(30, y + 3, now ? pet.d.name : a->name, GOLD);
      if (now) { snprintf(b, sizeof(b), "With you now  %dd  caught %d", (int)pet.ageDays(), pet.d.preyCount); cv.text(30, y + 11, b, rgb(150, 255, 190)); }
      else { snprintf(b, sizeof(b), "%s  %dd  caught %d", endingName(a->ending), a->ageDays, a->prey); cv.text(30, y + 11, b, endColor(a->ending)); }
      cv.text(172, y + 6, ">", DIM);
      if (hit) { journalPage_ = listPages + i; sound(SND_TAP); }
    }
    snprintf(b, sizeof(b), "Cats raised: %lu", (unsigned long)sv.lifeCats);
    cv.textCenter(W / 2, 172, b, DIM);
    if (cats > 0) cv.textCenter(W / 2, 183, "Tap a cat for their photos", rgb(150, 140, 180));
  } else if (pg < listPages + cats) {   // ---- one cat's album ----
    const int i = pg - listPages;
    const bool now = i < cur;
    const SaveData::AlbumCat* a = now ? nullptr : &sv.album[i - cur];
    const SaveData::Snap* photos = now ? sv.x8.photo : a->photo;
    snprintf(b, sizeof(b), "%.11s'S ALBUM", now ? pet.d.name : a->name); upper(b);
    panelTitle(cv, b, 6);
    static const char* stg[4] = {"Kitten", "Junior", "Adult", "Senior"};
    // (no smaller than 0.5: below that the face loses its pixels - a pale Sphynx kitten looks like it's facing away)
    static const float sz[4] = {0.50f, 0.55f, 0.60f, 0.60f}, kit[4] = {1.0f, 0.3f, 0.0f, 0.0f};
    for (int k = 0; k < 4; k++) {
      const int fx = 8 + (k % 2) * 86, fy = 22 + (k / 2) * 76, fw = 82, fh = 72;
      const SaveData::Snap& p = photos[k];
      cv.panel(fx, fy, fw, fh, rgb(250, 244, 230), rgb(200, 180, 150));      // a polaroid
      cv.rect(fx + 4, fy + 4, fw - 8, fh - 20, p.taken ? rgb(120, 100, 160) : rgb(70, 60, 90));
      if (p.taken) {
        cv.rect(fx + 4, fy + 4 + (fh - 20) * 2 / 3, fw - 8, (fh - 20) / 3, rgb(150, 110, 90));   // floor
        CatDraw d; d.breed = p.breed; d.fat = p.fat10 / 10.0f; d.size = sz[k]; d.kit = kit[k];
        d.pose = POSE_FRONT; d.face = FACE_HAPPY; d.t = 0.2f;
        d.hat = p.hat; d.glasses = p.gl; d.neck = p.neck; d.body = p.body;
        drawCat(cv, fx + fw / 2, fy + fh - 20, d);
        snprintf(b, sizeof(b), "%s, day %d", stg[k], p.day);
      } else {
        cv.textCenter(fx + fw / 2, fy + 21, now ? "?" : "-", rgb(150, 140, 180), 2);   // (yet to come / they never got there)
        snprintf(b, sizeof(b), "%s", stg[k]);
      }
      cv.textCenter(fx + fw / 2, fy + fh - 12, b, rgb(70, 50, 60));
    }
    if (now) { snprintf(b, sizeof(b), "%s  %s, day %d", BREEDS[pet.d.breed].name, stageName(pet.stage()), (int)pet.ageDays()); cv.textCenter(W / 2, 176, b, rgb(150, 255, 190)); }
    else {
      snprintf(b, sizeof(b), "%s  %dd  caught %d", endingName(a->ending), a->ageDays, a->prey); cv.textCenter(W / 2, 176, b, endColor(a->ending));
    }
    snprintf(b, sizeof(b), "cat %d of %d", i + 1, cats); cv.textCenter(W / 2, 186, b, DIM);
  } else {                   // ---- milestones ----
    panelTitle(cv, "MILESTONES", 6);
    int y = 22;
    for (int m = 0; m < MS_COUNT; m++) {
      const bool got = sv.x8.milestones & (1u << m);
      const Color c = got ? TXT : rgb(110, 100, 140);
      if (got) { cv.line(8, y + 3, 10, y + 6, rgb(120, 230, 140)); cv.line(10, y + 6, 15, y, rgb(120, 230, 140)); }   // a tick
      else cv.frame(8, y, 7, 7, rgb(110, 100, 140));
      cv.text(20, y, MS_LABEL[m], c);
      if (got) { snprintf(b, sizeof(b), "d%d", sv.x8.msDay[m]); cv.textRight(179, y, b, DIM); }
      y += 11;
    }
    int n = 0; for (int m = 0; m < MS_COUNT; m++) n += (sv.x8.milestones >> m) & 1;
    snprintf(b, sizeof(b), "%d of %d", n, (int)MS_COUNT);
    cv.textCenter(W / 2, y + 2, b, GOLD);
  }
  if (button(cv, 8, 196, 52, 22, "<", PURPLE, journalPage_ > 0)) journalPage_--;
  if (button(cv, 66, 196, 52, 22, pg >= listPages && pg < listPages + cats ? "CATS" : "BACK", GREEN)) {
    if (pg >= listPages && pg < listPages + cats) journalPage_ = (pg - listPages) / ROWS;   // an album: back to the list
    else setScreen(S_MENU);
  }
  if (button(cv, 124, 196, 52, 22, ">", PURPLE, journalPage_ < PAGES - 1)) journalPage_++;
}

}  // namespace cat
