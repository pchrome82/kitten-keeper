#include "game.h"
#include "roomdata.h"
#include "livewindow.h"
#include "compose.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace cat {

static const char* const kNames[] = {
  "Mittens", "Pickles", "Biscuit", "Noodle", "Waffles", "Pumpkin", "Muffin", "Tofu",
  "Ziggy", "Pepper", "Olive", "Bean", "Mochi", "Nugget", "Sprout", "Clover",
  "Peanut", "Misty", "Boots", "Gizmo", "Tigger", "Luna", "Simba", "Cleo",
  "Squeegee", "Wunkus", "Fluffy", "IceCream", "Snowy", "Vroom", "Hellman", "Angel", "Scratchy", "Furryman", "Gerald", "Mr. Cheeks", "Paws"};
constexpr int kNameCount = sizeof(kNames) / sizeof(kNames[0]);


Game::Game(Platform* p) : plat(p), pet(sv.pet) {
  saveDefaults(sv);
  memset(endMsg_, 0, sizeof(endMsg_));
  memset(parts_, 0, sizeof(parts_));
  memset(toasts_, 0, sizeof(toasts_));
  memset(&lg_, 0, sizeof(lg_));
  memset(&sg_, 0, sizeof(sg_));
  memset(&res_, 0, sizeof(res_));
  memset(&pg_, 0, sizeof(pg_)); memset(&lap_, 0, sizeof(lap_)); memset(&kg_, 0, sizeof(kg_)); memset(&hz_, 0, sizeof(hz_));
  memset(&ex_, 0, sizeof(ex_)); memset(&mg_, 0, sizeof(mg_)); memset(&lt_, 0, sizeof(lt_));
  memset(dropX_, 0, sizeof(dropX_)); memset(dropY_, 0, sizeof(dropY_)); memset(dropGone_, 0, sizeof(dropGone_));
}

Game::~Game() { delete[] roomBuf_; }

// A popup over the home (FEED, PLAY, confirm, battery): the room, cat and props behind it dim to pct, keeping
// their night look, while the popup itself (drawn after this) stays at full brightness, like the other screens.
void Game::dimHome(Canvas& cv, int pct) {
  const int tr = pct * fi.tintR / 100, tg = pct * fi.tintG / 100, tb = pct * fi.tintB / 100;
  for (int y = HUD_H; y < TOOL_Y; y++) {
    uint16_t* p = cv.px + y * cv.cw;
    for (int x = 0; x < cv.cw; x++, p++) {
      const uint16_t c = *p;
      if (c == KEY_COLOR || c == SHADOW_KEY) continue;   // the room itself is dimmed as it is composed
      int r = ((c >> 11) & 31) * tr / 100, g = ((c >> 5) & 63) * tg / 100, b = (c & 31) * tb / 100;
      *p = (uint16_t)((r << 11) | (g << 5) | b);
    }
  }
  gRoomDim = pct;
  fi.tintR = fi.tintG = fi.tintB = 100;   // the night tint is baked in above: don't tint the popup too
}

void Game::ensureRoom() {
  if (!roomBuf_) { roomBuf_ = new uint16_t[HW * HH]; memset(roomBuf_, 0, sizeof(uint16_t) * HW * HH); roomDirty_ = true; }
  float h = hour + minute / 60.0f;
  // static lighting: blend the two baked rooms by a quantized night factor (16 steps)
  float n = 1.0f - ((h >= 7 && h < 18) ? 1.0f : (h >= 18 && h < 21) ? 1.0f - (h - 18) / 3.0f : (h >= 5 && h < 7) ? (h - 5) / 2.0f : 0.0f);
  float q = floorf(n * 16.0f + 0.5f) / 16.0f;
  const int rid = sv.room < NUM_ROOMS ? sv.room : 0;
  if (rid != roomShown_) { roomShown_ = rid; roomDirty_ = true; }
  const bool hideTable = scr == S_KNOCK;   // Knock It Off "moves in" on the little table by the window: it becomes the big one
  if (hideTable != tableHidden_) { tableHidden_ = hideTable; roomDirty_ = true; }
  if (roomDirty_ || q != roomHour_) {
    int w = (int)(q * 256.0f + 0.5f);
    uint16_t* dst = roomBuf_ + ROOM_TOP * ROOM_W;
    const uint16_t* a = ROOMS[rid].day; const uint16_t* b = ROOMS[rid].night;
    for (int i = 0; i < ROOM_W * ROOM_H; i++) {
      uint16_t pa = a[i], pb = b[i];
      if (pa == 0xF81F || pb == 0xF81F || w == 0) { dst[i] = pa; continue; }
      if (w == 256) { dst[i] = pb; continue; }
      int r = (((pa >> 11) & 31) * (256 - w) + ((pb >> 11) & 31) * w) >> 8;
      int g = (((pa >> 5) & 63) * (256 - w) + ((pb >> 5) & 63) * w) >> 8;
      int bl = ((pa & 31) * (256 - w) + (pb & 31) * w) >> 8;
      dst[i] = (uint16_t)((r << 11) | (g << 5) | bl);
    }
    roomW_ = w;
    if (!tableHidden_) drawRoomTable(rid, w);
    bakeMouseHole();
    if (rid == 3) bakeCeilingLights(w);
    roomHour_ = q; roomDirty_ = false; winT_ = -1;
  }
  // only the window is live
  if (anim_ - winT_ > 0.16f || winT_ < 0) {
    int wx = weatherForDay(epoch / 86400);
    if (epoch) {   // winter (December to February): the rainy days snow, and so do half the grey ones
      const DateTime t = fromEpoch(epoch);
      if ((t.month == 12 || t.month <= 2) && (wx == 2 || (wx == 1 && ((epoch / 86400) & 1)))) wx = 3;
    }
    drawLiveWindow(roomBuf_, rid, h, anim_, wx);
    if (!tableHidden_) drawRoomTable(rid, roomW_);   // (the table stands in front of the window: the view goes behind it)
    winT_ = anim_;
  }
}

// The little table by the window (and its glass of water) is a sprite on top of the room picture, blended for the time
// of day like the room. Knock It Off leaves it out: that game "moves in" on the table, so it becomes the big one.
void Game::drawRoomTable(int rid, int w) {
  const RoomImg::Sprite& t = ROOMS[rid].table;
  if (!t.w || !t.day) return;
  uint16_t* room = roomBuf_ + ROOM_TOP * ROOM_W;
  for (int y = 0; y < t.h; y++) for (int x = 0; x < t.w; x++) {
    const int j = y * t.w + x;
    const uint16_t pa = t.day[j], pb = t.night[j];
    if (pa == 0xF81F) continue;   // see-through
    uint16_t& d = room[(t.y + y) * ROOM_W + t.x + x];
    if (w == 0) { d = pa; continue; }
    if (w == 256) { d = pb; continue; }
    const int r = (((pa >> 11) & 31) * (256 - w) + ((pb >> 11) & 31) * w) >> 8;
    const int g = (((pa >> 5) & 63) * (256 - w) + ((pb >> 5) & 63) * w) >> 8;
    const int bl = ((pa & 31) * (256 - w) + (pb & 31) * w) >> 8;
    d = (uint16_t)((r << 11) | (g << 5) | bl);
  }
}

void Game::stashName() {
  if (!nameBuf_[0]) return;
  for (int i = 0; i < kNameCount; i++) if (!strcmp(nameBuf_, kNames[i])) return;
  strncpy(customName_, nameBuf_, sizeof(customName_) - 1); customName_[sizeof(customName_) - 1] = 0;
}

const char* Game::randomName() {   // never a name the last cats had (or the one already showing)
  for (int tries = 0; tries < kNameCount; tries++) {
    nameSeed_ = (nameSeed_ + 1 + (int)(pet.rnd() % 7)) % kNameCount;
    const char* c = kNames[nameSeed_];
    bool used = !strncmp(c, pet.d.name, sizeof(pet.d.name)) || !strncmp(c, nameBuf_, sizeof(nameBuf_) - 1);
    for (int i = 0; i < sv.albumCount && i < ALBUM_MAX; i++) if (!strncmp(c, sv.album[i].name, sizeof(sv.album[i].name) - 1)) used = true;
    if (!used) return c;
  }
  return kNames[nameSeed_];
}

// ---------------------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------------------

void Game::begin() {
  // the main save, else the backup copy; each may also be from older firmware (then it's upgraded, keeping the cat)
  static SaveData tmp;
  bool ok = false;
  for (int src = 0; src < 2 && !ok; src++) {
    auto load = [&](uint32_t len) { memset(&tmp, 0, sizeof(tmp)); return src == 0 ? plat->loadSave(&tmp, len) : plat->loadSaveBackup(&tmp, len); };
    if (load(sizeof(tmp)) && saveValid(tmp)) ok = true;
    else if (load(SAVE_V8_SIZE) && saveMigrateV8(tmp)) ok = true;
    else if (load(SAVE_V7_SIZE) && saveMigrateV7(tmp)) ok = true;
    else if (load(SAVE_V6_SIZE) && saveMigrateV6(tmp)) ok = true;
    if (ok) { sv = tmp; if (src == 1) toast("Save restored from backup"); }
  }
  if (!ok) saveDefaults(sv);
  oriCfg_ = plat->orientGet();   // how this device is normally held (and whether auto-rotate is on)
  if (sv.hasCat && sv.x8.trait >= TRAIT_COUNT) sv.x8.trait = (uint8_t)(plat->randomSeed() % TRAIT_COUNT);   // (a cat from older firmware)
  pet.rng = (plat->randomSeed() ^ 0xA5A5A5A5u) | 1u;
  nowMs = plat->millis();
  alarmH_ = sv.alarmH;
  alarmM_ = sv.alarmM;
  if (sv.eq[3] != 10 && sv.eq[3] != 11) {   // a save from before the finer volume / brightness steps: convert it once
    static const uint8_t vol[4] = {0, 2, 4, 6}, br[6] = {8, 2, 4, 6, 8, 10};
    sv.volume = vol[sv.volume > 3 ? 2 : sv.volume];
    sv.brightness = br[sv.brightness > 5 ? 0 : sv.brightness];
    sv.eq[3] = 10;
  }
  if (sv.eq[3] != 11) { sv.eq[2] = 6; sv.eq[3] = 11; }   // before the screen-timeout setting: 1 minute
  if (sv.eq[2] > 8) sv.eq[2] = 6;
  if (sv.volume > 10) sv.volume = 2;
  if (sv.brightness < 1 || sv.brightness > 10) sv.brightness = 8;
  if (!sv.fallbackClock) sv.fallbackClock = toEpoch({2026, 1, 1, 12, 0, 0});

  updateClock();
  nameSeed_ = (int)(pet.rnd() % kNameCount);
  strncpy(nameBuf_, kNames[nameSeed_], sizeof(nameBuf_) - 1);
  customName_[0] = 0;

  if (sv.mode != MODE_UNSET) sv.mode = MODE_EASY;   // Hard mode is gone: a Hard save carries on as Easy
  if (sv.hasCat && pet.d.evolving) pet.evolve();   // "let it evolve" is gone: an old surprise kitten becomes the breed it was leaning to
  if (sv.mode == MODE_UNSET) {
    scr = S_MODE;
  } else if (!sv.hasCat) {
    scr = S_NAME;
  } else {
    applyOfflineCatchUp();
    scr = S_HOME;
    if (!clockOk) { clockNext_ = S_HOME; scr = S_CLOCK; }
    a_.x = pet.sleeping ? BED_X : 92;
    a_.tx = a_.x;
    if (pet.d.hunting) a_.act = A_HIDDEN;
    holdBox_ = holdFloor_ = holdHair_ = holdPrey_ = 0;
  }
  if (!clockOk) {  // pre-fill the clock editor with the fallback time
    DateTime d = fromEpoch(epoch);
    clockSel_[0] = d.year; clockSel_[1] = d.month; clockSel_[2] = d.day; clockSel_[3] = d.hour; clockSel_[4] = d.minute;
  }
  wake();
  updatePower();
}

void Game::applyOfflineCatchUp() {
  if (!sv.hasCat || !sv.lastSeen || !clockOk || epoch <= sv.lastSeen) return;
  uint32_t el = epoch - sv.lastSeen;
  if (el < 120) return;
  if (el > 400u * 86400u) el = 400u * 86400u;
  const uint32_t cap = 10u * 3600u;
  uint32_t simSecs = el < cap ? el : cap;
  const int hb0 = pet.d.hairballsOnFloor, mess0 = pet.d.floorMess, prey0 = pet.d.preyOnFloor;
  Stage st0 = pet.stage();
  for (uint32_t t = 0; t < simSecs; t += 60) {
    SimCtx c;
    c.hour = fromEpoch(sv.lastSeen + t).hour;
    c.playerActive = false;
    c.away = true;
    c.litterTier = sv.litterTier;
    pet.tick(60.0f, c);
    Event e;
    while (pet.pop(e)) {}
  }
  if (el > simSecs) pet.d.age += (float)(el - simSecs);
  pet.d.hunting = 0;
  pet.sleeping = false;
  int h = (int)(el / 3600), m = (int)((el % 3600) / 60);
  toast("Away %dh %02dm", h, m);
  if (pet.d.hairballsOnFloor > hb0) toast("Left %d hairball%s!", pet.d.hairballsOnFloor - hb0, pet.d.hairballsOnFloor - hb0 > 1 ? "s" : "");
  if (pet.d.preyOnFloor && !prey0) toast("%s left you a %s!", pet.d.name, preyName(pet.d.preyOnFloor));
  if (pet.d.floorMess > mess0) toast("Oops... %d mess%s", pet.d.floorMess - mess0, pet.d.floorMess - mess0 > 1 ? "es" : "");
  if (pet.stage() != st0) toast("%s grew up a bit!", pet.d.name);
}

void Game::saveNow() { autosave(true); }

void Game::autosave(bool force) {
  if (!force && saveCool_ > 0) return;
  sv.lastSeen = epoch;
  sv.alarmH = (uint8_t)alarmH_;
  sv.alarmM = (uint8_t)alarmM_;
  saveSeal(sv);
  const bool stored = plat->storeSave(&sv, sizeof(sv));
  if (stored && (backupAt_ == 0 || epoch - backupAt_ >= 600 || epoch < backupAt_)) {   // and the backup, every 10 minutes or so
    plat->storeSaveBackup(&sv, sizeof(sv));
    backupAt_ = epoch ? epoch : 1;
  }
  saveCool_ = 15;
  saveT_ = 0;
}

// ---------------------------------------------------------------------------------------
// input / clock / power
// ---------------------------------------------------------------------------------------

void Game::updateInput(bool down, int x, int y) {
  in.pressed = in.released = in.tap = false;
  if (down && !in.down) { in.pressed = true; in.px = x; in.py = y; in.downMs = nowMs; }
  if (!down && in.down) {
    in.released = true;
    in.heldMs = (int)(nowMs - in.downMs);
    in.tap = (abs(in.x - in.px) <= 14 && abs(in.y - in.py) <= 14 && in.heldMs < 1000);   // forgiving: firm presses and edge jitter still count
  }
  in.down = down;
  if (down) { in.x = x; in.y = y; in.heldMs = (int)(nowMs - in.downMs); }
}

void Game::wake() { idleSec = 0; screenOn_ = true; buttonOff_ = false; }

// The touch targets drawn last frame decide who gets a tap: the nearest one to where the finger came down,
// as long as it's within a little slack (and the finger didn't slide far away before lifting).
void Game::resolveTap() {
  memcpy(hitsPrev_, hits_, sizeof(HitRect) * nHits_); nHitsPrev_ = nHits_; hitsPrevScr_ = hitsScr_;
  nHits_ = 0; hitsScr_ = scr;
  tapMode_ = 0;
  if (!in.tap || hitsPrevScr_ != scr || nHitsPrev_ == 0) return;   // a brand-new screen: the old per-button test
  const int SLACK = 8;
  int best = -1; long bd = (long)SLACK * SLACK + 1, bc = 1L << 30;
  auto dist2 = [&](int x, int y, int w, int h) -> long {   // squared distance from where the finger landed to a box
    const int dx = in.px < x ? x - in.px : (in.px >= x + w ? in.px - (x + w - 1) : 0);
    const int dy = in.py < y ? y - in.py : (in.py >= y + h ? in.py - (y + h - 1) : 0);
    return (long)dx * dx + (long)dy * dy;
  };
  bd = 1L << 30;
  for (int i = 0; i < nHitsPrev_; i++) {
    const int rx = hitsPrev_[i].x, ry = hitsPrev_[i].y, rw = hitsPrev_[i].w, rh = hitsPrev_[i].h;
    int x = rx, y = ry, w = rw, h = rh;
    touchBox(x, y, w, h);                                                    // its (bigger) touch area
    if (dist2(x, y, w, h) > (long)SLACK * SLACK) continue;                   // too far from this one
    if (in.x < x - 12 || in.x >= x + w + 12 || in.y < y - 12 || in.y >= y + h + 12) continue;   // slid off before lifting
    const long d = dist2(rx, ry, rw, rh);                                    // ranked by the button as drawn
    const long cx = (long)(in.px - (rx + rw / 2)), cy = (long)(in.py - (ry + rh / 2)), c = cx * cx + cy * cy;
    if (d < bd || (d == bd && c < bc)) { bd = d; bc = c; best = i; }
  }
  if (best >= 0) { tapMode_ = 1; tapWin_ = hitsPrev_[best]; } else tapMode_ = 2;
}

void Game::updateClock() {
  DateTime d;
  if (plat->rtcGet(d)) {
    clockOk = true;
    epoch = toEpoch(d) + turboOffset_;
  } else {
    clockOk = false;
    epoch = sv.fallbackClock + (nowMs / 1000) + turboOffset_;
  }
  DateTime n = fromEpoch(epoch);
  hour = n.hour;
  minute = n.minute;
}

void Game::setFlip(bool f) {
  if (f == flipped_) return;
  flipped_ = f;
  plat->setFlipped(f);
}

// Upside down -> the picture turns round too. Which sensor axis runs along the screen (and which end is "up") is learned
// from the first steady upright pose, so it works whichever way the chip is mounted. Landscape holds change nothing.
void Game::updateOrientation() {
  if (oriCfg_ & 16) { setFlip((oriCfg_ & 32) != 0); return; }   // auto-rotate off: the way up chosen in Settings
  if (!screenOn_ || scr == S_POUR || scr == S_LAP) { oriT_ = 0; return; }
  const float v[3] = {tiltAx_, tiltAy_, tiltAz_};
  const float step = 0.03f;                                   // (this runs once per IMU sample)
  if ((oriCfg_ & 7) == 0) {
    const int ax = fabsf(v[0]) > fabsf(v[1]) ? 0 : 1;
    const bool upright = fabsf(v[ax]) > 0.85f && fabsf(v[1 - ax]) < 0.4f && fabsf(v[2]) < 0.45f;
    if (!upright) { oriHold_ = 0; return; }
    oriHold_ += step;
    if (oriHold_ > 1.5f) { oriCfg_ = (oriCfg_ & ~7) | (1 + ax * 2 + (v[ax] < 0 ? 1 : 0)); plat->orientSet(oriCfg_); oriHold_ = 0; }
    return;
  }
  const int la = ((oriCfg_ & 7) - 1) >> 1, neg = ((oriCfg_ & 7) - 1) & 1;
  if (fabsf(v[la]) < 0.75f || fabsf(v[1 - la]) > 0.55f || fabsf(v[2]) > 0.6f) { oriT_ = 0; return; }   // not clearly upright: stay as it is
  const bool want = (v[la] < 0) != (neg == 1);
  if (want == flipped_) { oriT_ = 0; return; }
  oriT_ += step;
  if (oriT_ > 0.6f) { setFlip(want); oriT_ = 0; }
}

void Game::updateMotion() {
  imuT_ += dt;
  if (imuAge_ < 99) imuAge_ += dt;
  if (imuT_ < 0.03f) return;
  imuT_ = 0;
  float ax, ay, az;
  if (!plat->imuRead(ax, ay, az)) { if (oriCfg_ & 16) setFlip((oriCfg_ & 32) != 0); return; }   // (no sensor: a manual flip still applies)
  {
    float gm = sqrtf(ax * ax + ay * ay + az * az);
    if (gm > 0.3f && gm < 3.0f) {   // ignore glitchy samples for tilt (wide band: works even if the g-scale is off); smooth the rest
      tiltAx_ += (ax - tiltAx_) * 0.5f; tiltAy_ += (ay - tiltAy_) * 0.5f; tiltAz_ += (az - tiltAz_) * 0.5f;
      imuAge_ = 0;
    }
  }
  updateOrientation();
  motion_.feed(ax, ay, az, nowMs);
  bool sh = motion_.shake();
  bool pk = motion_.pickup();
  if (!(sh || pk) || !sv.hasCat) return;
  bool usingScreen = screenOn_ && idleSec < 2.0f;
  if (!screenOn_ && pk && !buttonOff_) { wake(); }   // picking it up lights the screen, unless the side button turned it off
  if (pet.sleeping && !usingScreen) {
    if (pet.napping) {
      bool was = pet.sleeping;
      pet.disturb();
      if (was && !pet.sleeping && !buttonOff_) { wake(); }
    }
  }
}

// The screen is going dark: whatever menu or game was open, it wakes up on the home screen.
// Setup screens (no cat yet), a cat's ending story and a ringing alarm stay where they are.
void Game::backHomeForSleep() {
  if (!sv.hasCat || scr == S_HOME || scr == S_MODE || scr == S_CLOCK || scr == S_NAME || scr == S_NAMEKB || scr == S_ENDING || scr == S_AMBULANCE || scr == S_ALARM_RING) return;
  if (net_.mode != NM_OFF && (scr == S_EXPLORE || scr == S_LINK)) return;   // Explore Together: the match goes on (tap to come back)
  if (scr == S_LITTER) leaveLitter();
  setScreen(S_HOME);
}

// How long until the battery is full / empty, from how fast the level moved over the last half hour.
int Game::batEstimate(int& kind) const {
  kind = 0;
  if (batChg_ && batPct_ >= 100) { kind = 4; return 0; }
  if (batN_ < 2) return 0;
  const BatSample& a = batHist_[0]; const BatSample& b = batHist_[batN_ - 1];
  const float mins = (b.sec - a.sec) / 60.0f;
  if (mins < 4.0f) return 0;                                   // need a few minutes to see the trend
  const float dp = (float)(b.pct - a.pct);
  if (batChg_) {
    if (dp <= 0) return 0;                                     // charging, but not moved yet
    kind = 1; return (int)((100 - b.pct) / (dp / mins) + 0.5f);
  }
  if (dp >= 0) { kind = 3; return (int)(b.pct * mins); }      // hasn't dropped 1% yet: at least this long
  kind = 2; return (int)(b.pct / (-dp / mins) + 0.5f);
}

void Game::updatePower() {
  static const uint8_t kLevels[10] = {12, 25, 40, 60, 85, 110, 140, 175, 215, 255};   // finer steps at the dim end (AMOLED)
  uint8_t base = kLevels[(sv.brightness < 1 ? 1 : (sv.brightness > 10 ? 10 : sv.brightness)) - 1];
  bool keepOn = (scr == S_ALARM_RING) || (scr == S_LASER) || (scr == S_SCRATCH) || (scr == S_ENDING) || (scr == S_AMBULANCE) ||
                (scr == S_POUR) || (scr == S_LAP) || (scr == S_KNOCK) || (scr == S_HEIST) || (scr == S_EXPLORE) || (scr == S_MAGIC) || (scr == S_LITTER) ||
                (scr == S_CLOCK) || (scr == S_MODE) || (scr == S_NAME) || (scr == S_NAMEKB) || (scr == S_LINK) || (scr == S_TOUCHCAL);
  // the "Screen off" setting: 1 s .. 5 min, or never. The screen dims a little before it goes off
  // (for the longer timeouts), and after a minute when it's set to never turn off.
  static const uint16_t kTimeout[9] = {1, 3, 5, 10, 15, 30, 60, 300, 0};   // seconds; 0 = never
  const float timeout = kTimeout[sv.eq[2] < 9 ? sv.eq[2] : 6];
  const bool never = timeout <= 0;
  const float dimAt = never ? 60.0f : (timeout >= 10 ? timeout * 0.6f : 1e9f);
  // a game left untouched for 5 minutes (put down mid-game) gives up too, so the screen can't burn the battery
  const bool stays = scr == S_ALARM_RING || scr == S_ENDING || scr == S_AMBULANCE || scr == S_CLOCK || scr == S_MODE || scr == S_NAME || scr == S_NAMEKB ||
                     (net_.mode == NM_MATCH && scr == S_EXPLORE);   // a 5-minute match with friends: don't give up on it
  if (keepOn && !stays && !never && idleSec > 300.0f) keepOn = false;
  if (!keepOn && !never && idleSec > timeout) {
    if (screenOn_) backHomeForSleep();
    screenOn_ = false;
    bright_ = 0;
  } else if (!keepOn && idleSec > dimAt) {
    bright_ = (uint8_t)(base * 15 / 100 < 8 ? 8 : base * 15 / 100);
  } else {
    bright_ = base;
  }
  if (!screenOn_) bright_ = 0;
}

// ---------------------------------------------------------------------------------------
// simulation
// ---------------------------------------------------------------------------------------

SimCtx Game::ctx(bool away) const {
  SimCtx c;
  c.hour = hour;
  c.playerActive = idleSec < NIGHT_AWAKE_SECS;   // touched within the last 15 minutes
  c.away = away;
  c.litterTier = sv.litterTier;
  return c;
}

void Game::stepSim(float seconds, bool away) {
  pet.eating = !away && a_.act == A_EAT;
  pet.tick(seconds, ctx(away));
}

void Game::simulate(float seconds, bool away) {
  // test helper: advance the world by N seconds in 1 s steps
  int n = (int)seconds;
  for (int i = 0; i < n; i++) {
    turboOffset_++;
    epoch++;
    DateTime d = fromEpoch(epoch);
    hour = d.hour; minute = d.minute;
    stepSim(1.0f, away);
    handleEvents();
  }
}

void Game::updateSim() {
  simAccum_ += dt;
  clockT_ += dt;
  if (clockT_ >= 1.0f) { clockT_ = 0; updateClock(); }
  int guard = 0;
  while (simAccum_ >= 1.0f && guard++ < 8) {
    simAccum_ -= 1.0f;
    if (!sv.hasCat) continue;
    for (int k = 0; k < timeScale; k++) {
      if (k > 0) {
        turboOffset_++;
        epoch++;
        DateTime d = fromEpoch(epoch);
        hour = d.hour; minute = d.minute;
      }
      stepSim(1.0f, false);
      if (timeScale > 1) handleEvents();
      if (pet.ended) break;
    }
  }
  saveT_ += dt;
  if (saveCool_ > 0) saveCool_ -= dt;
  if (saveT_ > 180.0f) autosave(false);
}

void Game::sound(Sound s) {
  if (sv.volume > 0 && timeScale == 1) plat->playSound(s, sv.volume);
}

void Game::toast(const char* fmt, ...) {
  char buf[64];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  int slot = -1;
  for (int i = 0; i < 3; i++) if (toasts_[i].t <= 0) { slot = i; break; }
  if (slot < 0) {                       // all busy: drop the oldest
    for (int i = 0; i < 2; i++) toasts_[i] = toasts_[i + 1];
    slot = 2;
  }
  strncpy(toasts_[slot].text, buf, sizeof(toasts_[slot].text) - 1);
  toasts_[slot].text[sizeof(toasts_[slot].text) - 1] = 0;
  toasts_[slot].t = 4.5f;   // (3.2 s + 40%)
}

void Game::setScreen(Screen s) {
  scr = s;
  scrT_ = 0;
  scrSerial_++;
  if (s != S_HOME && s != S_FEED && s != S_PLAY && s != S_CONFIRM) memset(toasts_, 0, sizeof(toasts_));
  in.tap = false;
  in.pressed = false;
}

void Game::handleEvents() {
  Event e;
  while (pet.pop(e)) {
    switch (e.type) {
      case EV_ENDING:
        endCat(e.arg);
        break;
      case EV_STAGE_UP: {
        toast("%s is now %s %s!", pet.d.name, e.arg == STAGE_ADULT ? "an" : "a", stageName((Stage)e.arg));
        sound(SND_WIN);
        for (int i = 0; i < 8; i++) spawn(a_.x + (i - 4) * 5, a_.y - 30, 7, rgb(255, 230, 90), (i - 4) * 8, -35, 1.4f);
        break;
      }
      case EV_EVOLVE:
        toast("%s evolved into a %s!", pet.d.name, speciesName(e.arg));
        sound(SND_WIN);
        for (int i = 0; i < 12; i++) spawn(a_.x + (i - 6) * 4, a_.y - 30, 7, rgb(255, 230, 90), (i - 6) * 7, -30 - (i % 3) * 8, 1.6f);
        break;
      case EV_WARN:
        if (e.arg == 1) toast("%s is giving you side-eye...", pet.d.name);
        else toast("%s is packing a tiny bag!", pet.d.name);
        sound(SND_SAD);
        break;
      case EV_WOKE_ANGRY:
        toast("%s yawns grumpily.", pet.d.name);
        sound(SND_HISS);
        break;
      case EV_MEOW: {
        static const char* why[] = {"", "HUNGRY!", "CLEAN MY BOX!", "PLAY WITH ME!"};
        int r = e.arg2 >= 1 && e.arg2 <= 3 ? e.arg2 : 0;
        if (e.arg == 0) { sound(SND_MEOW_SOFT); }
        else {
          wake();
          sound(e.arg == 1 ? SND_MEOW : SND_MEOW_LOUD);
          toast("%s: %s", pet.d.name, why[r]);
        }
        actorEvent(e);
        break;
      }
      case EV_AUTO_CLEAN:
        sound(SND_POOF);
        spawn(BOX_X, 140, 1, rgb(160, 240, 255), 0, -20, 0.8f);
        break;
      case EV_PREY_LOST:
        toast("%s ate their catch.", pet.d.name);
        break;
      case EV_MESS_FROM_HAIRBALL:
        toast("The hairball got squished...");
        break;
      default:
        actorEvent(e);
        break;
    }
  }
}

void Game::checkAlarm() {
  if (!sv.alarmOn || !sv.hasCat || scr == S_ALARM_RING) return;
  uint32_t dayId = epoch / 86400u;
  bool due = false;
  if (hour == alarmH_ && minute == alarmM_ && alarmDayDone_ != (int)dayId) { due = true; alarmDayDone_ = (int)dayId; }
  if (snoozeEpoch_ && epoch >= snoozeEpoch_) { due = true; snoozeEpoch_ = 0; }
  if (due) {
    wake();
    alarmRing_ = 0;
    pet.wakeGently();
    setScreen(S_ALARM_RING);
  }
}

void Game::newKitten(bool inherit) {
  (void)inherit;
  const int pick = sv.pad[0];   // 0..6 a breed, 8/9 a secret breed (once unlocked)
  int breed = (pick >= NUM_BREED_SLOTS || pick == BR_KITTEN) ? 0 : pick;
  if (isSecretBreed(breed) && !(sv.unlocked & (1u << breed))) breed = 0;   // (a secret breed must be found first; every regular breed is free)
  pet.newKitten(plat->randomSeed() ^ nowMs ^ epoch, breed, nameBuf_);
  pet.d.evolving = 0;
  // a fresh page in the journal, and a personality of their own
  const uint32_t keepDebt = sv.x8.debt;   // the vet's tab follows you to the next kitten
  const uint32_t keepMs = sv.x8.milestones;   // and the journal's milestones: they're the whole game's, not one cat's
  uint8_t keepMsDay[sizeof(sv.x8.msDay)]; memcpy(keepMsDay, sv.x8.msDay, sizeof(keepMsDay));
  memset(&sv.x8, 0, sizeof(sv.x8));          // (a fresh set of photos: the last cat's went into the album)
  sv.x8.debt = keepDebt;
  sv.x8.milestones = keepMs; memcpy(sv.x8.msDay, keepMsDay, sizeof(keepMsDay));
  sv.x8.trait = (uint8_t)((plat->randomSeed() ^ epoch) % TRAIT_COUNT);
  pet.trait = sv.x8.trait;
  sv.hasCat = 1;
  sv.lifeCats++;
  sv.boxFlags &= (uint8_t)~1u;
  // no free starter box any more (boxes are bought in the shop)
  a_ = Actor();
  a_.x = 92;
  a_.tx = 92;
  holdBox_ = holdFloor_ = holdHair_ = holdPrey_ = 0;
  bowl_ = 0;
  memset(toasts_, 0, sizeof(toasts_));
  lastSeenReset_();
  autosave(true);
}

void Game::lastSeenReset_() { sv.lastSeen = epoch; }

void Game::endCat(int ending) {
  if (net_.mode != NM_OFF) netStop();   // (it happened while Explore Together was open: the radio goes off)
  archiveAlbum(ending);   // into the journal for good: who they were, how it ended, and their photos
  sv.lifePrey += pet.d.preyCount;
  endingKind_ = ending;
  endT_ = 0;
  const char* n = pet.d.name;
  memset(endMsg_, 0, sizeof(endMsg_));
  if (ending == END_RAN_AWAY) {
    const int days = (int)(pet.d.age / DAY), hrs = (int)(fmodf(pet.d.age, DAY) / 3600.0f);
    snprintf(endMsg_[0], 40, "Due to neglect,");
    snprintf(endMsg_[1], 40, "%.14s had to leave", n);
    snprintf(endMsg_[2], 40, "and take their chances");
    snprintf(endMsg_[3], 40, "in the wild.");
    snprintf(endMsg_[4], 40, "Please try better next time!");
    snprintf(endMsg_[5], 40, "Time together: %dd %dh", days, hrs);
    sv.coins = 0; sv.treats = sv.diets = 0; sv.litterTier = 0;
    sv.x8.debt = 0;   // leaving wipes the vet's tab
  } else if (ending == END_BLACK_HOLE) {
    snprintf(endMsg_[0], 40, "%s got SO chonky that", n);
    snprintf(endMsg_[1], 40, "they collapsed into a");
    snprintf(endMsg_[2], 40, "black hole... a RAINBOW CAT!");
    // (the hairballs are kept: over-feeding is punished enough by losing the cat)
    sv.x8.debt = 0;   // the black hole swallows the vet's tab, though
    secretNew_ = !(sv.unlocked & (1u << BR_RAINBOW));
    sv.unlocked |= 1u << BR_RAINBOW; sv.pad[0] = BR_RAINBOW;   // the adoption picker opens on them
  } else if (ending == END_SUN) {
    snprintf(endMsg_[0], 40, "%s got so lean & zippy", n);
    snprintf(endMsg_[1], 40, "they shrank into a tiny sun!");
    snprintf(endMsg_[2], 40, "A HAMSTER! +50 hairballs");
    giveCoins(50);   // the tiny sun arrives with a treasure: 50 hairballs
    secretNew_ = !(sv.unlocked & (1u << BR_HAMSTER));
    sv.unlocked |= 1u << BR_HAMSTER; sv.pad[0] = BR_HAMSTER;
  } else {
    snprintf(endMsg_[0], 40, "%s retired to Grandma's", n);
    snprintf(endMsg_[1], 40, "sunny garden. What a life!");
    snprintf(endMsg_[2], 40, "A new kitten arrives...");
    giveCoins(10 + pet.d.preyCount * 2);  // legacy bonus
  }
  sv.hasCat = 0;
  pet.d.hunting = 0;
  sound(ending == END_BLACK_HOLE ? SND_HOLE : (ending == END_RAN_AWAY ? SND_SAD : (ending == END_SUN ? SND_POP : SND_WIN)));
  wake();
  setScreen(S_ENDING);
  autosave(true);
}

// When a cat's life ends it goes into the journal (newest first): name, breed, how it ended, how long it lived, its
// gifts and its four photos. Kept until the game is reset from Settings (the last ALBUM_MAX cats).
void Game::archiveAlbum(int ending) {
  for (int i = ALBUM_MAX - 1; i > 0; i--) sv.album[i] = sv.album[i - 1];
  SaveData::AlbumCat& a = sv.album[0];
  memset(&a, 0, sizeof(a));
  strncpy(a.name, pet.d.name, sizeof(a.name) - 1);
  a.breed = pet.d.breed; a.ending = (uint8_t)ending;
  a.ageDays = (uint16_t)(pet.d.age / DAY); a.prey = pet.d.preyCount;
  memcpy(a.photo, sv.x8.photo, sizeof(a.photo));
  if (sv.albumCount < ALBUM_MAX) sv.albumCount++;
}

void Game::giveCoins(int n) {
  uint32_t add = (uint32_t)n;
  if (sv.x8.debt > 0) {   // anything found goes to the vet's tab first
    const uint32_t pay = add < sv.x8.debt ? add : sv.x8.debt;
    sv.x8.debt -= pay; add -= pay;
    if (sv.x8.debt == 0) toast("Vet bill paid off!");
  }
  sv.coins += add;
  if (sv.coins > 99999) sv.coins = 99999;
}

// Developer: +n hairballs. On the device: hold the version line in Settings for 3 seconds. On the web page: the developer panel.
void Game::devAddHairballs(int n) {
  giveCoins(n);
  toast("+%d hairballs (dev)", n);
  autosave(true);
}

void Game::coinText(char* b, size_t n) const {
  if (sv.x8.debt > 0) snprintf(b, n, "-%lu", (unsigned long)sv.x8.debt);
  else snprintf(b, n, "%lu", (unsigned long)sv.coins);
}
Color Game::coinColor(Color normal) const { return sv.x8.debt > 0 ? rgb(255, 96, 96) : normal; }

// Health hit 0: straight to the vet. It costs 10 hairballs (the rest goes on a tab if there aren't enough) and they come back at 45 health.
void Game::checkCollapse() {
  if (!pet.collapse || !sv.hasCat || pet.ended || !screenOn_) return;
  if (pet.d.health > 0.0f) { pet.collapse = false; return; }
  switch (scr) {   // not in the middle of a game, the litter box or a setup screen: it waits for the next calm moment
    case S_HOME: case S_FEED: case S_PLAY: case S_MENU: case S_SHOP: case S_STATS: case S_SKINS: case S_SETTINGS: case S_BDAY:
    case S_HALL: case S_JOURNAL: case S_WARDROBE: case S_HELP: case S_BATTERY: break;
    default: return;
  }
  pet.collapse = false;
  if (++sv.x8.collapses >= 3) {   // third time at 0 health: neglect, so they run away
    pet.ended = true;
    endCat(END_RAN_AWAY);
    return;
  }
  const uint32_t fee = VET_FEE;
  const uint32_t pay = sv.coins >= fee ? fee : sv.coins;
  sv.coins -= pay;
  ambPaid_ = (int)pay; ambOwe_ = (int)(fee - pay);
  sv.x8.debt += fee - pay;
  if (sv.x8.debt > 9999) sv.x8.debt = 9999;
  pet.vetVisit();
  pet.d.health = 45.0f;
  wake();
  ambT_ = 0;
  sound(SND_SAD);
  setScreen(S_AMBULANCE);
  autosave(true);
}

// ---------------------------------------------------------------------------------------
// main frame
// ---------------------------------------------------------------------------------------

bool Game::frame(Canvas& cv, bool touchDown, int physX, int physY) {
  uint32_t now = plat->millis();
  float d = nowMs ? (now - nowMs) / 1000.0f : 0.033f;
  if (d < 0.001f) d = 0.001f;
  if (d > 0.1f) d = 0.1f;
  dt = d;
  nowMs = now;
  fi.keyed = false; gShadowKey = false; gRoomDim = 100;
  pet.trait = sv.x8.trait < TRAIT_COUNT ? sv.x8.trait : 0;
  anim_ += dt;
  scrT_ += dt;
  idleSec += dt;

  // PWR / BOOT button: screen off (the clock and their needs keep going) / back on
  bool justOff = false;
  if (plat->powerButton()) {
    if (screenOn_ && scr != S_ALARM_RING) {
      backHomeForSleep();   // a mini-game in progress is dropped (an Explore trip waits)
      screenOn_ = false; idleSec = 200.0f;   // counts as "you put it down"
      buttonOff_ = true;                     // ...but moving the device won't light it again: only a button or a touch does
      in.down = false; swallow_ = touchDown;  // a finger lifting right now mustn't switch it back on
      justOff = true;
      autosave(true);
    } else if (!screenOn_) { wake(); swallow_ = true; }
  }
  if (!justOff && !screenOn_ && touchDown && !in.down) { wake(); swallow_ = true; }
  if (swallow_) { if (!touchDown) swallow_ = false; touchDown = false; }
  updateInput(touchDown, physX / 2, physY / 2);
  resolveTap();
  if (in.down || in.released) wake();

  updateDecor();
  updateBox();
  updateSpecialDays();
  updateSim();
  if (pet.sleeping) ex_.active = false;   // an Explore trip lasts while they're awake
  {   // fullness or fun just hit zero: two high chirps (once per time it runs dry)
    const bool fz = sv.hasCat && !pet.ended && pet.d.hunger <= 0.05f, uz = sv.hasCat && !pet.ended && pet.d.fun <= 0.05f;
    if ((fz && !foodZero_) || (uz && !funZero_)) sound(SND_ALERT);
    foodZero_ = fz; funZero_ = uz;
  }
  // they ran out of energy: Lap Cat (the only way to fully rest them). Mid-game it waits until you're back home;
  // with the screen off it waits until you turn it back on.
  if (pet.napping && !wasNapping_) lapDue_ = true;
  wasNapping_ = pet.napping;
  if (!pet.napping || pet.d.hunting || !sv.hasCat) lapDue_ = false;
  const bool calm = scr == S_HOME || scr == S_PLAY || scr == S_MENU || scr == S_STATS || scr == S_HELP;
  if (lapDue_ && screenOn_ && calm) {
    lapDue_ = false;
    toast("%s is out of energy... lap time!", pet.d.name);
    startLap();
  }
  // battery for the HUD (polled every 10 s)
  batT_ -= dt;
  if (batT_ <= 0) { batT_ = 10.0f; int p = 0; bool c = false; batOk_ = plat->battery(p, c); batPct_ = p; batChg_ = c; }
  // once a minute, remember the battery level (start over when the charger goes in or out)
  batSampleT_ -= dt;
  if (batOk_ && (batSampleT_ <= 0 || batChg_ != batHistChg_)) {
    if (batChg_ != batHistChg_) { batN_ = 0; batHistChg_ = batChg_; }
    batSampleT_ = 60.0f;
    if (batN_ == BAT_HIST) { memmove(batHist_, batHist_ + 1, sizeof(BatSample) * (BAT_HIST - 1)); batN_--; }
    batHist_[batN_++] = {nowMs / 1000u, (int8_t)(batPct_ < 0 ? 0 : (batPct_ > 100 ? 100 : batPct_))};
  }
  updateMotion();
  checkAlarm();
  handleEvents();
  checkCollapse();
  netPoll();          // Explore Together keeps talking while the screen is off
  updatePower();

  if (!screenOn_ && scr != S_ALARM_RING) {
    if (dirty_) { autosave(true); dirty_ = false; }
    return false;
  }
  dirty_ = true;

  if (!calAsked_) {   // a device whose touch was never calibrated: run TOUCH FIX once, before anything else
    calAsked_ = true;
    float a_, b_, c_, d_;
    if (plat->touchCalGet(a_, b_, c_, d_) && !plat->touchCalDone() && scr != S_ALARM_RING) startTouchCal();
  }
  switch (scr) {
    case S_MODE: screenMode(cv); break;
    case S_CLOCK: screenClock(cv); break;
    case S_NAME: screenName(cv); break;
    case S_HOME: drawHome(cv, true); break;
    case S_FEED: if (pet.canFeed()) startPour(); else setScreen(S_HOME); break;   // (the old feed menu: straight to pouring)
    case S_PLAY: drawHome(cv, false); screenPlay(cv); break;
    case S_BATTERY: drawHome(cv, false); screenBattery(cv); break;
    case S_LINK: screenLink(cv); break;
    case S_TOUCHCAL: screenTouchCal(cv); break;
    case S_LITTER: screenLitter(cv); break;
    case S_SHOP: screenShop(cv); break;
    case S_MENU: screenMenu(cv); break;
    case S_STATS: screenStats(cv); break;
    case S_SKINS: screenSkins(cv); break;
    case S_SETTINGS: screenSettings(cv); break;
    case S_BDAY: screenBday(cv); break;
    case S_NAMEKB: screenNameKb(cv); break;
    case S_ALARM_SET: screenAlarmSet(cv); break;
    case S_HALL: journalPage_ = 0; setScreen(S_JOURNAL); break;   // (the Hall of Cats lives in the journal now)
    case S_JOURNAL: screenJournal(cv); break;
    case S_LASER: screenLaser(cv); break;
    case S_SCRATCH: screenScratch(cv); break;
    case S_RESULT: screenResult(cv); break;
    case S_ENDING: screenEnding(cv); break;
    case S_AMBULANCE: screenAmbulance(cv); break;
    case S_ALARM_RING: screenAlarmRing(cv); break;
    case S_CONFIRM: drawHome(cv, false); screenConfirm(cv); break;
    case S_POUR: screenPour(cv); break;
    case S_LAP: screenLap(cv); break;
    case S_KNOCK: screenKnock(cv); break;
    case S_HEIST: screenHeist(cv); break;
    case S_MAGIC: screenMagic(cv); break;
    case S_EXPLORE: screenExplore(cv); break;
    case S_WARDROBE: screenWardrobe(cv); break;
    case S_HELP: screenHelp(cv); break;
    default: scr = S_HOME; break;
  }
  if (gShowTouch && in.down) {   // touch-position check: a ring where the controller says the finger is
    cv.ring(in.x, in.y, 5, rgb(255, 255, 255));
    cv.ring(in.x, in.y, 6, rgb(0, 0, 0));
    cv.set(in.x, in.y, rgb(255, 60, 60));
  }
  return true;
}

}  // namespace cat
