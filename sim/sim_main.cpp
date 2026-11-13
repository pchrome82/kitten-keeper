// Host-side simulator: runs the real game core with a fake board.
//   ./sim shots <outdir>     scripted tour of every screen (raw frames -> raw2png.py)
//   ./sim balance            long-run balance tests with simulated players
//   ./sim motion             pick-up / shake detector tests
//   ./sim persist            save/load + offline catch-up tests
#include "../src/game/game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <math.h>

using namespace cat;

struct SimPlat : Platform {
  uint32_t ms = 1000;
  int64_t epochOffset;   // epoch = epochOffset + ms/1000
  bool rtcValid = true;
  float ax = 0, ay = 0, az = 1;
  std::vector<uint8_t> store;
  int soundCount[SND_COUNT] = {0};
  uint8_t lastBright = 0;
  SimPlat() { epochOffset = (int64_t)toEpoch({2026, 10, 30, 18, 9, 0}) - ms / 1000; }
  uint32_t millis() override { return ms; }
  bool rtcGet(DateTime& dt) override { if (!rtcValid) return false; dt = fromEpoch((uint32_t)(epochOffset + ms / 1000)); return true; }
  void rtcSet(const DateTime& dt) override { epochOffset = (int64_t)toEpoch(dt) - ms / 1000; rtcValid = true; }
  bool imuRead(float& x, float& y, float& z) override { x = ax; y = ay; z = az; return true; }
  bool loadSave(void* buf, uint32_t len) override { if (store.size() != len) return false; memcpy(buf, store.data(), len); return true; }
  bool storeSave(const void* buf, uint32_t len) override { store.assign((const uint8_t*)buf, (const uint8_t*)buf + len); return true; }
  void playSound(Sound s, uint8_t) override { soundCount[s]++; }
  void setBrightness(uint8_t b) override { lastBright = b; }
  uint32_t randomSeed() override { return 0xC0FFEE; }
  void setClock(int h, int m) { DateTime d = fromEpoch((uint32_t)(epochOffset + ms / 1000)); d.hour = h; d.minute = m; d.second = 0; rtcSet(d); }
};

static uint16_t fb[W * H];
static Canvas cv(fb);
static int shotNo = 0;
static const char* outDir = ".";

static void step(Game& g, SimPlat& p, bool down = false, int x = 0, int y = 0) {
  p.ms += 33;
  g.frame(cv, down, x * 2, y * 2);
}
static void run(Game& g, SimPlat& p, float sec) { int n = (int)(sec * 30); for (int i = 0; i < n; i++) step(g, p); }
static void tap(Game& g, SimPlat& p, int x, int y) {
  step(g, p, true, x, y); step(g, p, true, x, y); step(g, p, true, x, y);
  step(g, p);
  step(g, p);
}
static void drag(Game& g, SimPlat& p, int x0, int y0, int x1, int y1, int frames) {
  for (int i = 0; i <= frames; i++) step(g, p, true, x0 + (x1 - x0) * i / frames, y0 + (y1 - y0) * i / frames);
  step(g, p);
}
static void shot(const char* name) {
  char fn[256];
  snprintf(fn, sizeof fn, "%s/%02d_%s.raw", outDir, ++shotNo, name);
  FILE* f = fopen(fn, "wb"); fwrite(fb, 2, W * H, f); fclose(f);
}

// ------------------------------------------------------------------------------------------
static int cmdShots() {
  SimPlat p;
  Game g(&p);
  g.begin();
  run(g, p, 0.5f);  shot("mode");
  tap(g, p, 92, 196);                       // HARD
  run(g, p, 0.3f);  shot("clock");
  tap(g, p, 92, 207);                       // SAVE
  run(g, p, 0.3f);  shot("name");
  tap(g, p, 136, 199);                      // ADOPT
  run(g, p, 4);     shot("home_evening");
  // chonky, messy, gift
  g.pet.d.chonk = 70; g.pet.d.droppings = 3; g.pet.d.hairballsOnFloor = 2; g.pet.d.preyOnFloor = PREY_BIRD; g.pet.d.floorMess = 1;
  g.sv.coins = 57;
  run(g, p, 1);     shot("home_messy");
  p.setClock(23, 40); run(g, p, 2); shot("home_night");
  tap(g, p, 134, 150); run(g, p, 0.3f); shot("feed_popup");
  tap(g, p, 92, 82);  run(g, p, 3.0f); shot("eating");
  run(g, p, 4);
  p.setClock(19, 30); g.pet.d.chonk = 35; g.pet.d.energy = 90; g.pet.d.hunger = 60; g.pet.sleeping = false; g.pet.grump = 0; run(g, p, 1);
  tap(g, p, 55, 203); run(g, p, 0.3f); shot("play_popup");
  tap(g, p, 92, 94); run(g, p, 0.5f);                       // CHASE THE LASER
  step(g, p, true, 120, 120); run(g, p, 0.1f);
  for (int i = 0; i < 60; i++) step(g, p, true, 60 + (int)(50 * sin(i * 0.2)), 130 + (int)(30 * cos(i * 0.2)));
  shot("laser_chase");
  for (int i = 0; i < 40; i++) step(g, p, true, 100, 150);
  shot("laser_hold");
  for (int i = 0; i < 1500 && g.scr == S_LASER; i++) step(g, p, (i / 45) % 2 == 0, 40 + (i % 90), 100 + (i % 70));
  printf("after laser loop scr=%d\n", g.scr); run(g, p, 0.2f);  shot("laser_result");
  printf("before HOME tap scr=%d\n", g.scr);
  tap(g, p, 136, 199); run(g, p, 0.3f);                        // HOME
  printf("scr after HOME=%d sleeping=%d energy=%.0f\n", g.scr, g.pet.sleeping, g.pet.d.energy); g.pet.d.energy = 90; g.pet.sleeping = false; g.pet.grump = 0; g.pet.d.hunting = 0; run(g, p, 0.5f);
  tap(g, p, 55, 203); run(g, p, 0.2f);
  printf("scr after PLAY=%d\n", g.scr);
  tap(g, p, 92, 130); run(g, p, 0.5f);                         // SCRATCH POST
  for (int r = 0; r < 14; r++) { drag(g, p, 100, 80, 100, 130, 3); drag(g, p, 100, 130, 100, 80, 3); run(g, p, 0.12f); }
  shot("scratch");
  run(g, p, 1);
  for (int i = 0; i < 700 && g.scr == S_SCRATCH; i++) { drag(g, p, 100, 80, 100, 130, 4); drag(g, p, 100, 130, 100, 80, 4); }
  run(g, p, 0.5f);  shot("scratch_result");
  tap(g, p, 136, 199); run(g, p, 0.3f);
  tap(g, p, 92, 203);                                          // BOX (litter)
  run(g, p, 0.3f);  shot("litter");
  g.pet.d.droppings = 3; run(g, p, 0.3f);
  drag(g, p, 40, 60, 140, 120, 30); shot("litter_scooping");
  tap(g, p, 92, 208); run(g, p, 0.3f);
  tap(g, p, 129, 203); run(g, p, 0.3f); g.sv.coins = 150; run(g, p, 0.2f); shot("shop");
  tap(g, p, 155, 196); run(g, p, 0.3f);                        // skins
  tap(g, p, 120, 90); run(g, p, 0.2f);  shot("skins");
  tap(g, p, 150, 208); tap(g, p, 92, 208);
  tap(g, p, 166, 203); run(g, p, 0.3f); shot("menu");
  tap(g, p, 92, 41); run(g, p, 0.3f); shot("stats");
  tap(g, p, 92, 208); run(g, p, 0.2f);
  tap(g, p, 92, 161); run(g, p, 0.3f); shot("settings");
  tap(g, p, 92, 208); tap(g, p, 92, 208);
  // endings
  for (int k = 0; k < 3; k++) {
    g.pet.ended = false; g.sv.hasCat = 1;
    if (k == 0) g.pet.d.resent = 100; else if (k == 1) g.pet.d.chonk = 101; else g.pet.d.age = 14 * 86400.0f + 10;
    g.simulate(2, false);
    run(g, p, k == 2 ? 3.5f : (k == 0 ? 2.0f : 3.0f)); shot(k == 0 ? "end_runaway" : (k == 1 ? "end_blackhole" : "end_retire"));
    run(g, p, k == 0 ? 5.0f : 4.0f); shot(k == 0 ? "end_runaway_text" : (k == 1 ? "end_blackhole_text" : "end_retire_text"));
    tap(g, p, 92, 204); run(g, p, 0.3f);
    if (k == 0) shot("new_kitten_name");
    tap(g, p, 136, 199); run(g, p, 0.5f);
  }
  return 0;
}

// ------------------------------------------------------------------------------------------
// balance: simulated players
// ------------------------------------------------------------------------------------------
struct Bot {
  const char* name;
  int kind;
};
static void careFeed(Game& g) { bool o; if (g.pet.canFeed()) g.pet.feed(FOOD_KIBBLE, o); }
static void careClean(Game& g) {
  g.pet.scoop(6);
  while (g.pet.d.floorMess) g.pet.cleanFloor();
  while (g.pet.d.hairballsOnFloor) { g.pet.collectHairball(); g.sv.coins++; }
  if (g.pet.d.preyOnFloor) { g.sv.coins += preyValue(g.pet.d.preyOnFloor); g.pet.takePrey(); }
}
static void carePlay(Game& g) { if (!g.pet.sleeping && !g.pet.d.hunting && g.pet.d.energy >= 15) g.pet.exercise(20, 14, 1.5f); }

static void runBot(const char* name, int kind) {
  SimPlat p; Game g(&p);
  g.begin();
  g.sv.mode = MODE_EASY; g.sv.hasCat = 1; g.sv.clockSet = 1;
  g.pet.newKitten(7, 0, "Bot");
  int maxMin = 40 * 1440;
  int m = 0;
  for (; m < maxMin && g.sv.hasCat; m++) {
    int hr = g.hour;
    bool active = true;
    g.idleSec = 1000;           // idle by default (easy-mode cat sleeps when idle)
    auto wakeUp = [&]() { g.idleSec = 0; g.simulate(2, false); };
    switch (kind) {
      case 0: // attentive: checks every 20 min, 07:00-23:00
        if (m % 20 == 0 && hr >= 7 && hr < 23) {
          wakeUp();
          if (g.pet.d.hunger < 50) careFeed(g);
          careClean(g);
          if (g.pet.d.fun < 55) carePlay(g);
        }
        break;
      case 1: break; // neglect
      case 2: // glutton: feeds constantly, never plays
        if (m % 10 == 0) { wakeUp(); careFeed(g); careClean(g); }
        break;
      case 3: // feeder only
        if (m % 20 == 0 && g.pet.d.hunger < 50) { wakeUp(); careFeed(g); }
        break;
      case 4: // mild: 3 meals, 1 clean, 1 game a day
        if (m % 1440 == 8 * 60 || m % 1440 == 13 * 60 || m % 1440 == 20 * 60) { wakeUp(); careFeed(g); }
        if (m % 1440 == 20 * 60 + 5) { wakeUp(); careClean(g); carePlay(g); }
        break;
      case 5: // regular: 3 meals, 4 games and a clean-up a day, by the clock (what the day rates are tuned for)
        if (g.minute == 0) {
          if (hr == 8 || hr == 13 || hr == 18) { wakeUp(); careFeed(g); }
          if (hr == 10 || hr == 12 || hr == 15 || hr == 19) { wakeUp(); if (!g.pet.sleeping && !g.pet.d.hunting && g.pet.d.energy >= 15) g.pet.exercise(25, 14, 1.5f); }
          if (hr == 19) careClean(g);
        }
        break;
      case 6: // light: 2 meals and 2 games a day
        if (g.minute == 0) {
          if (hr == 9 || hr == 18) { wakeUp(); careFeed(g); careClean(g); }
          if (hr == 12 || hr == 19) { wakeUp(); if (!g.pet.sleeping && !g.pet.d.hunting && g.pet.d.energy >= 15) g.pet.exercise(25, 14, 1.5f); }
        }
        break;
    }
    (void)active;
    if (getenv("TRACE") && m % 60 == 0 && kind == atoi(getenv("TRACE"))) printf("h%3d %02d:%02d hun=%.0f fun=%.0f en=%.0f hp=%.0f ch=%.1f res=%.1f drop=%d mess=%d slp=%d\n", m / 60, g.hour, g.minute, g.pet.d.hunger, g.pet.d.fun, g.pet.d.energy, g.pet.d.health, g.pet.d.chonk, g.pet.d.resent, g.pet.d.droppings, g.pet.d.floorMess, g.pet.sleeping);
    g.simulate(60, false);
  }
  const PetData& d = g.pet.d;
  if (g.sv.hasCat)
    printf("%-26s  SURVIVED %d days  chonk=%.0f health=%.0f resent=%.0f age=%.1fd coins=%lu\n", name, m / 1440, d.chonk, d.health, d.resent, d.age / 86400, (unsigned long)g.sv.coins);
  else
    printf("%-26s  ENDED: %-10s after %d d %02d h  (cat age %dd, weight %.0f)\n", name, g.sv.albumCount ? endingName(g.sv.album[0].ending) : "?", m / 1440, (m % 1440) / 60, g.sv.albumCount ? g.sv.album[0].ageDays : 0, d.chonk);
}

static int cmdBalance() {
  const char* names[] = {"attentive owner", "total neglect", "glutton (no exercise)", "feeds only, never cleans", "mild care (1 chore/day)", "regular (3 meals, 4 games)", "light (2 meals, 2 games)"};
  for (int k = 0; k < 7; k++) runBot(names[k], k);
  return 0;
}

static int cmdMotion() {
  int fails = 0;
  auto check = [&](bool ok, const char* msg) { printf("%s %s\n", ok ? "PASS" : "FAIL", msg); if (!ok) fails++; };
  { // resting on a desk with noise: nothing
    MotionDetector m; uint32_t t = 0; bool any = false;
    for (int i = 0; i < 600; i++) { t += 33; m.feed(0.01f * sinf(i), 0.01f * cosf(i * 1.3f), 1.0f + 0.01f * sinf(i * 2), t); any |= m.shake() | m.pickup(); }
    check(!any, "desk at rest -> no events");
  }
  { // taps on screen (small jolts)
    MotionDetector m; uint32_t t = 0; bool any = false;
    for (int i = 0; i < 600; i++) { t += 33; float j = (i % 40 == 0) ? 0.12f : 0; m.feed(0.0f, 0.0f, 1.0f + j, t); any |= m.shake() | m.pickup(); }
    check(!any, "finger taps on a desk -> no events");
  }
  { // picked up and tilted
    MotionDetector m; uint32_t t = 0; bool pick = false;
    for (int i = 0; i < 300; i++) { t += 33; m.feed(0, 0, 1, t); m.shake(); m.pickup(); }
    for (int i = 0; i < 60; i++) { t += 33; float a = i * 0.03f; m.feed(sinf(a), 0, cosf(a), t); pick |= m.pickup(); }
    check(pick, "lift + tilt 60 deg -> pickup");
  }
  { // shake
    MotionDetector m; uint32_t t = 0; bool sh = false;
    for (int i = 0; i < 300; i++) { t += 33; m.feed(0, 0, 1, t); m.shake(); m.pickup(); }
    for (int i = 0; i < 30; i++) { t += 33; float s = (i % 3 == 0) ? 2.2f : 0.1f; m.feed(0, s, 1, t); sh |= m.shake(); }
    check(sh, "vigorous shake -> shake");
  }
  { // shaking a cat that's napping because they're worn out wakes them up, a bit grumpy
    SimPlat p; Game g(&p); g.begin();
    g.sv.mode = MODE_EASY; g.sv.hasCat = 1; g.pet.newKitten(3, 0, "T"); g.pet.d.age = 3 * 86400.0f;
    p.setClock(12, 0); run(g, p, 1.5f);
    g.pet.d.energy = 8; for (int i = 0; i < 90; i++) { g.idleSec = 100; step(g, p); }
    check(g.pet.sleeping, "worn out -> she naps");
    float fun0 = g.pet.d.fun;
    for (int i = 0; i < 30; i++) { p.ax = (i % 3 == 0) ? 2.5f : 0.f; p.ay = 0; p.az = 1; g.idleSec = 100; step(g, p); }
    p.ax = 0; step(g, p);
    check(!g.pet.sleeping && g.pet.d.fun < fun0, "shaking her awake: she wakes up a bit grumpy (fun down)");
  }
  printf("%d failure(s)\n", fails);
  return fails;
}

static int cmdPersist() {
  int fails = 0;
  auto check = [&](bool ok, const char* msg) { printf("%s %s\n", ok ? "PASS" : "FAIL", msg); if (!ok) fails++; };
  SimPlat p;
  {
    Game g(&p); g.begin();
    g.sv.mode = MODE_EASY; g.sv.hasCat = 1; g.sv.clockSet = 1; g.sv.coins = 77; g.sv.unlocked |= 6;
    g.pet.newKitten(5, 3, "Saved"); g.pet.d.chonk = 61; g.pet.d.age = 6 * 86400.0f; g.pet.d.hunger = g.pet.d.fun = 90;   // adult: kittens drain fast by design
    g.saveNow();
  }
  check(!p.store.empty(), "save written");
  { // corrupt copy is rejected
    SimPlat q; q.store = p.store; q.store[100] ^= 0xFF;
    Game g(&q); g.begin();
    check(g.scr == S_MODE && g.sv.coins == 0, "corrupted save is ignored -> fresh game");
  }
  {
    p.ms += 6 * 3600 * 1000;   // six hours pass with the device off
    Game g(&p); g.begin();
    check(g.sv.hasCat && g.sv.coins == 77 && strcmp(g.pet.d.name, "Saved") == 0 && g.pet.d.breed == 3, "reload restores cat, coins, breed");
    check(g.pet.d.hunger < 88 && g.pet.d.hunger > 5, "offline hunger decayed gently (not starved)");
    check(g.pet.d.resent < 91 && g.sv.hasCat, "offline never makes the cat leave");
    check(g.scr == S_HOME, "resumes on the home screen");
    printf("   after 6h away: hunger=%.0f fun=%.0f health=%.0f mess=%d hairballs=%d prey=%d age=%.2fh\n", g.pet.d.hunger, g.pet.d.fun, g.pet.d.health, g.pet.d.floorMess, g.pet.d.hairballsOnFloor, g.pet.d.preyOnFloor, g.pet.d.age / 3600);
  }
  {
    SimPlat q; q.rtcValid = false;
    Game g(&q); g.begin();
    run(g, q, 1);
    check(g.scr == S_MODE, "no RTC on a fresh game still boots to mode select");
  }
  { // date maths
    DateTime d = {2028, 2, 29, 23, 59, 59};
    DateTime r = fromEpoch(toEpoch(d));
    check(r.year == 2028 && r.month == 2 && r.day == 29 && r.hour == 23 && r.minute == 59 && r.second == 59, "epoch round-trips leap day");
    check(fromEpoch(toEpoch(d) + 1).month == 3 && fromEpoch(toEpoch(d) + 1).day == 1, "leap day rolls over to Mar 1");
  }
  { // alarm (easy mode)
    SimPlat q; Game g(&q); g.begin();
    g.sv.mode = MODE_EASY; g.sv.hasCat = 1; g.sv.clockSet = 1; g.pet.newKitten(1, 0, "A"); g.sv.alarmOn = 1;
    q.setClock(6, 58); run(g, q, 1);
    g.setAlarmForTest(7, 0);
    for (int i = 0; i < 30 * 130 && g.scr != S_ALARM_RING; i++) step(g, q);
    check(g.scr == S_ALARM_RING, "alarm rings at the set time (easy mode)");
    int snd = q.soundCount[SND_ALARM];
    run(g, q, 6);
    check(q.soundCount[SND_ALARM] > snd, "alarm keeps meowing until dismissed");
    tap(g, q, 136, 200);
    check(g.scr == S_HOME, "snooze returns home");
  }
  printf("%d failure(s)\n", fails);
  return fails;
}
int main(int argc, char** argv) {
  if (argc > 2) outDir = argv[2];
  if (argc < 2) { printf("usage\n"); return 1; }
  std::string c = argv[1];
  if (c == "shots") return cmdShots();
  if (c == "balance") return cmdBalance();
  if (c == "motion") return cmdMotion();
  if (c == "persist") return cmdPersist();
  return 0;
}
