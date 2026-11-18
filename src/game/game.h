// The whole game: one object, driven by Game::frame() ~30 times a second.
// Implementation is split over game.cpp (loop, sim, events), home.cpp (room + cat actor),
// screens.cpp (menus), minigames.cpp (laser + scratching post).
#pragma once
#include "platform.h"
#include "canvas.h"
#include "catart.h"
#include "pet.h"
#include "save.h"
#include "motion.h"
#include "netplay.h"

namespace cat {

// ---- layout ----
constexpr int HUD_H = 26;
constexpr float TAP_GUARD = 0.35f;   // seconds a newly opened screen ignores presses (stops a double tap carrying over)
constexpr float AMB_SECS = 9.0f;   // length of the ambulance scene
constexpr int SKIN_FEE = 10;   // switching breed mid-game (a new kitten after a game over may be any regular breed for free)
constexpr int VET_FEE = 10;   // hairballs charged when a cat collapses at 0 health
constexpr int CORNER = 22;   // the AMOLED's corners are rounded: keep text/icons in the top & bottom rows this far from the sides
constexpr int TOOL_Y = 184;
constexpr int HOME_DIM = 60;
constexpr float GAME_FUN = 1.0f / 3.0f;
constexpr float KITTEN_GAME_FUN = 0.75f;
constexpr uint32_t EXJ_CRYSTAL = 1u << 30;   // sv.ex.journal: the Cave Crystal (from the Deep Tunnels' chest): Explore screens cost 4 less energy (ex.flags is full)
constexpr uint8_t EXF_TRAIL_PAWS = 128;   // sv.ex.flags: the Trail Paws (won with the first hairball knockout): Explore screens cost 8 energy, not 10   // a kitten (under half a day) gets 25% less Fun from a game: more games to fill a new kitten's bar     // games fill the Fun bar at a third of their old rate
inline float gameFun(float f) { return (float)(int)(f * GAME_FUN + 0.5f); }   // rounded to whole points
constexpr float EXPLORE_ENERGY = 50.0f;
constexpr uint32_t PET_HB_EVERY = 3 * 3600;   // a hairless cat's petting hairball: at most one per 3 hours      // energy an Explore trip (or a match) takes   // how dark the home gets behind a popup (percent)
constexpr int TOOL_H = 40;
// room prop layout (logical px), chosen so nothing hides anything else:
//   back row:  toy mouse, litter ~6-48 (baked), cat tree 40-80 (against the wall, right of the mouse hole), ripped chair by the table, bowls (baked)
//   mid row:   crinkle tunnel 64-92 (mid screen, below the house mouse's run), luxury chair 31-57 (front left, over the litter box's corner)
//   front row: bed 83-125, box 126-166, the Hamster's wheel far right
constexpr int TREE_X = 60, TREE_Y = 138, BOX2_X = 146, CHAIR_X = 96, CHAIR_Y = 150;
constexpr int LCHAIR_X = 150, LCHAIR_Y = 134;   // the Luxury Chair: against the RIGHT wall (back to the wall, seat facing the window); x = its centre, y = its feet
constexpr int GIFT_X = 18, GIFT_Y = 182;       // a waiting gift box: front left corner of the floor, below the litter tray (its base is row 160)
constexpr int GIFT_STREAK = 5;                 // well-cared-for days in a row that earn one
constexpr int TUNNEL_X = 78, TUNNEL_Y = 147;   // its top edge (y - 12) stays below the house mouse (rows ~122-129)
enum BoxType : uint8_t { BT_NONE, BT_TISSUE, BT_SHOE, BT_CARD, BT_CRATE };
constexpr int BOX_X = 26, BED_X = 104, BOWL_X = 134, POST_X = 62, FLAP_X = 170;
constexpr int CAT_X_MIN = 22, CAT_X_MAX = 150;
constexpr int CAT_BASE_Y = 156;
// the Hamster's wheel: on the floor at the front right, in every apartment: in front of the little table (which ends
// around row 143-148; the rim starts at 140), nearer the player.
constexpr int WHEEL_X = 162, WHEEL_Y = 158, WHEEL_R = 18;

enum Screen : uint8_t {
  S_MODE, S_CLOCK, S_NAME, S_HOME, S_FEED, S_PLAY, S_LITTER, S_SHOP, S_MENU, S_STATS, S_SKINS,
  S_SETTINGS, S_ALARM_SET, S_HALL, S_LASER, S_SCRATCH, S_RESULT, S_ENDING, S_ALARM_RING,
  S_CONFIRM, S_POUR, S_LAP, S_KNOCK, S_HEIST, S_EXPLORE, S_WARDROBE, S_MAGIC, S_HELP, S_BATTERY, S_LINK, S_TOUCHCAL, S_JOURNAL, S_AMBULANCE, S_BDAY, S_NAMEKB, S_COUNT
};

// The journal: things this cat has done (bits in sv.x8.milestones; append only)
enum Milestone : uint8_t {
  MS_MEAL, MS_HAIRBALL, MS_GIFT, MS_STARS, MS_EXPLORE, MS_WILD, MS_KING, MS_CAVE, MS_TOGETHER, MS_BIRTHDAY,
  MS_LAP, MS_FURNI, MS_GROWN, MS_SENIOR, MS_COUNT
};
// Seasons and special days (from the real-time clock)
enum Season : uint8_t { SEASON_NONE, SEASON_HALLOWEEN, SEASON_WINTER, SEASON_HOLIDAYS, SEASON_NEWYEAR };

enum ConfirmId : uint8_t { CF_NONE, CF_RESET, CF_NEWCAT, CF_SKIN };

enum Act : uint8_t {
  A_IDLE, A_WALK, A_EAT, A_POOP, A_HACK, A_SLEEP, A_GROOM, A_ZOOM, A_MEOW, A_HAPPY, A_YAWN,
  A_EXIT, A_HIDDEN, A_ENTER, A_SULK, A_STARE, A_WINDOW,
  A_WHEEL, A_FLOAT, A_COUNT   // secret breeds: the Hamster runs in its wheel, the Rainbow Cat floats up to the window   // A_STARE: staring at something off screen; A_WINDOW: watching out of the window
};

struct Actor {
  float x = 92, y = CAT_BASE_Y, tx = 92;
  bool left = false;
  Act act = A_IDLE;
  float actT = 0;          // time left in timed act
  Act after = A_IDLE;      // act to start once the walk finishes
  float afterT = 0;
  float idleT = 2;
  float t = 0;
  float speed = 40;
  int8_t stareDir = 1;     // A_STARE: which edge of the screen they're staring at (-1 left, +1 right)
  float stareLen = 0;      // A_STARE: how long this stare lasts
  bool winTracker = false; // A_WINDOW: following a bird (tail up, head tilting) instead of quietly watching
  float actLen = 0;        // A_WHEEL / A_FLOAT: how long the whole act lasts
  float x0 = 0;            // A_FLOAT: where they took off from
  float winGlanceAt = -1;  // A_WINDOW: time left (actT) at which they glance back at you (-1 = no glance this time)
};

struct Particle {
  float x, y, vx, vy, life, maxLife;
  uint8_t kind;   // 0 heart 1 sparkle 2 zzz 3 coin 4 fiber 5 puff 6 note 7 star
  Color col;
};

struct Toast { char text[64]; float t; };

struct LaserGame {
  float timeLeft, cx, cy, vx, vy, stateT, ptx, pty, psx, psy, lockout, cd;
  float lx, ly; bool lv;
  float plx, ply; bool ptouch;   // where the finger was last frame (a fast swipe is tested along the whole line)
  float trail[6][2];
  int state;     // 0 chase 1 windup 2 pounce 3 recover
  int score, catches, near, miss;
  bool left;
  float flash;
  bool started;
  float idle;
  int dodges;               // pounces the player dodged
  float hbX, hbY;           // bonus hairball on the rug
  bool hbTaken;
  int streak;               // catches in a row (3 = you lose)
  bool hb2Spawned, hb2Taken; float hb2X, hb2Y;   // 2nd hairball: appears somewhere random with 8 s left
  bool hb3Spawned, hb3Taken; float hb3X, hb3Y;   // 3rd hairball: appears somewhere random with 3 s left
};

// Scratching post is a beat game: up/down arrows scroll along the top into a target ring; swipe that way on the post as they arrive.
constexpr int SCR_MAX = 32;
struct ScratchGame {
  float t;                 // song time (s)
  float endT;              // the round ends once the last arrow is past
  float sy0, swT;          // current stroke: where and when it started
  int swDir;               // direction already counted for this touch (0 none)
  bool swiping;
  float fbT; int fb;       // feedback flash: 1 PURRFECT, 2 GOOD, 3 MISS
  float pawPhase, pawTarget, lastHitT, sadT;
  int n, hits, perfects, misses, combo, bestCombo;
  float at[SCR_MAX];       // when each arrow reaches the target
  int8_t dir[SCR_MAX];     // -1 swipe up, +1 swipe down
  uint8_t res[SCR_MAX];    // 0 waiting, 1 purrfect, 2 good, 3 miss
  bool ticked[SCR_MAX];    // beat tick already played
};

// one piece of kibble: flies from the bag mouth to (tx, ty) along an arc, then rests in the bowl or bounces/rolls on the floor
struct Kibble { float x, y, vx, vy, t, dur, x0, y0, tx, ty, arc; uint8_t state, shape, col; };   // state: 0 free, 1 flying, 2 in bowl, 3 floor (moving), 4 floor (still)
constexpr int KIB_N = 140;
struct PourGame { float p, spill, t, angle, levelT, flash; bool started, done; int verdict; float bx, by, bz, calib, lean, pourT, zAmt[4];   // b* = resting gravity (how the player holds it)
                  Kibble kib[KIB_N]; float bowlAcc, floorAcc, sndBowl, sndFloor; uint32_t seed;
                  float weightChange; };   // how much heavier (+) / lighter (-) this meal made them
struct LapGame { float t, streak, steady, shown, stun, doneT, e0, gain; bool begun, done, jolting; float bx, by, bz, calib, dev; int interrupts; float cap; };   // cap: Energy stops here (a nap you chose adds at most 50; a worn-out cat naps to full)   // begun = intro dismissed, done = result showing; stun = startled after a bump; gain = energy earned; b* = resting pose, dev = degrees away from it   // b* = resting pose, dev = degrees away from it
// Knock It Off: the cat strolls along the table to the thing it is about to shove; tap that item in time to save it.
struct KnockItem { float x, xo, fall, vy, vx; uint8_t type; uint8_t st; };   // st: 0 on the table, 1 falling, 2 smashed on the floor

struct KnockGame { float timeLeft, msgT, cx, cTarget, cT, win; KnockItem it[5]; int knocked, saved, tgt, last, dir, cState, msgKind, hairballs; float hbX[2], hbT[2]; bool hbSpawned[2]; bool started; };   // knocked = items lost off the table, saved = shoves stopped; msgKind 1 saved / 2 crash   // cState: 0 pause, 1 walking over, 2 poised (tap now!), 3 shoving, 4 shrug (too slow)
constexpr int HZ_W = 15, HZ_H = 15;
enum ItemId : uint8_t { ITEM_BED, ITEM_TREE, ITEM_TREEBED, ITEM_MOUSE, ITEM_YARN, ITEM_TUNNEL, ITEM_WAND, ITEM_RCHAIR, ITEM_LCHAIR, ITEM_COUNT };   // append only (saved as bits)
struct HeistLoot { int8_t tx, ty; uint8_t type; bool taken; };
struct HeistGame {
  uint8_t maze[HZ_H][HZ_W];                // 1 = wall
  float x, y, timeLeft, stun, flash;
  bool left, started;
  float fullT;   // > 0: show the 'too full, go home' banner
  int hairballs; // bonus hairballs picked up in the maze (2 per maze, 3 for a hairless cat)
  bool quitAsk;   // BACK pressed: paused, asking to confirm (the whole screen steers, so BACK confirms)
  float carry; int carried, banked, items;
  HeistLoot loot[16]; int nloot;
  float dx, dy; int dogDirX, dogDirY; float dogT;
  float msgT; char msg[24];
};
// ---- Explore: a little Zelda-style world (see explore.cpp) ----
constexpr int EXW = 6, EXH = 5;          // world: 6x5 screens (the right-hand column is the city)
constexpr int EXC = 11, EXR = 10;        // each screen: 11x10 tiles of 16 px
struct ExMob { float x, y, vx, vy, t; uint8_t kind; bool alive; uint8_t phase; float aux, aux2; };   // phase / aux: the city car's trip (see explore.cpp)
constexpr int EX_MOBS = 6;   // up to 2 critters + 2 cars + a wild cat, with room to spare
struct ExploreGame {
  int sx, sy;                    // current screen
  int room;                      // -1 outdoors, else the place id we're inside
  uint8_t map[EXR][EXC];         // tiles of the current screen / room
  float x, y;                    // cat position (logical px)
  bool left, moving;
  int state;                     // 0 walk, 1 slide, 2 dialog, 3 fight, 5 map, 6 summary
  float t, stateT;
  int slideDx, slideDy; uint8_t prevMap[EXR][EXC]; float slide;
  ExMob mob[EX_MOBS]; int nmob;  // critters, cars and a wild cat on this screen (never more than EX_MOBS)
  uint32_t catsBeaten;           // wild cats beaten this trip (per screen)
  uint16_t bedCatsBeaten, bedsUsed;   // per place: the cat squatting on its bed is beaten / they're napped in it this trip
  bool onBed; float napT;        // standing on a bed (so it only triggers once per visit) / napping right now
  float dOpenT;                  // when the dialog opened (its buttons ignore the tap that opened it)
  int screens, caught, fights, wins, found, places;   // trip stats
  bool lost;
  // dialog
  char dTitle[24]; char dLine[4][30]; char dA[14], dB[14]; int dAct; int dArg;
  // fight
  int foeSpecies; float foeStr; int foeHp, foeMax, myHp, myMax; float marker, markerDir, roundT; int lastHit; bool boss; int prize = 0;   // prize: the King rematch drop (0 = 20 hairballs, 1 Cowboy Hat, 2 Bandana, 3 Angel Wings)
  float throwT, throwCd; bool throwHit; bool hbKO;   // hbKO: this fight was won with a hairball   // a hairball in flight (1 hairball: one-hit KO, 5% miss; the King dodges ~33%)
  int bossPhase; float bossPhaseT; bool feint, defended, defLock;   // the King's leaps: 0 waiting, 1 crouching (warning), 2 in the air (DEFEND now!)
  float zoneC, zoneW;            // the green zone: centre and half-width (0..1); it moves after every swing
  float swingT, swingMarker, swingZoneC, swingZoneW; int8_t swingRes;   // the last SCRATCH, held on the bar for a second: where they pressed, the zone it was judged against, 1 hit / 2 slam / 0 miss
  float fxT; int fxKind, fxAfter;   // fight transition: 1 into the fight, 2 out of it; fxAfter: 1 back to walking, 2 trip lost
  // fishing
  struct { float x, y, vy, t; bool alive; } fish[3]; int nfish;   // fish swimming in the river (hidden under bridges)
  int fishToday;                 // fish caught this trip
  int fireflies;                 // fireflies caught this trip: each lets their see further at night
  // map
  int mapTab;
  float msgT; char msg[40];
  int enterX, enterY;            // where we came in (to come back out)
  bool mp;                       // an Explore Together match (see netplay.cpp)
  bool coop;                     // this fight is the King Raid (the King's health is shared by everyone)
  bool active;                   // a trip is under way: leaving with BACK keeps it (until they sleep)
  float hitT;                    // stunned after a car scare
  float irisT;                   // > 0: just went through a door / cave mouth / the crack: the view irises open from the cat
  bool autoWalk, pressInWalk;    // tap to walk: heading for (goalX, goalY) on their own / this press began while they were
  float goalX, goalY;
};
struct MagicGame { int round, correct, state; float t; bool inside, tell; int guess; };
constexpr int LT_GX = 55, LT_GY = 51;   // litter sand grid (2 px cells over the 110x102 tray)
struct LitterGame {
  bool carry, falling, ok; float sx, sy, fx, fy, fvx, fvy; int floorN; float floorX[4]; int cleaned; float msgT; char msg[28]; bool init;
  int carryIdx;      // which buried lump is stuck to the finger (-1 = a floor lump)
  bool fallToCan;    // the falling lump was let go over the bin
  uint8_t sand[LT_GY][LT_GX];   // 255 = smooth sand on top, 0 = dug down to the tray
  bool hbHere, hbFound; float hbX, hbY, hbT;   // a very rare buried hairball (+1, no binning needed)
  int known;         // lumps in the box when the player last left (the box remembers its sand)
};
struct ResultInfo {
  int kind;       // 0 laser 1 scratch
  int score, stars, coins;
  int catches;    // laser: times the cat caught the dot
  bool win;
  float funGain;
};

void touchBox(int& x, int& y, int& w, int& h);   // a target's touch area (screens.cpp)
void drawFloorPoop(Canvas& cv, int x, int y, float anim, int k);   // a poop on the floor (extras.cpp)

class Game {
 public:
  explicit Game(Platform* plat);
  ~Game();
  Game(const Game&) = delete;
  Game& operator=(const Game&) = delete;

  // Output description for the compositor: where the hi-res room shows through, and the night tint.
  struct FrameInfo { bool keyed = false; int tintR = 100, tintG = 100, tintB = 100; } fi;
  const uint16_t* roomPixels() const { return roomBuf_; }

  // lifecycle
  void begin();
  // Run one frame. Returns true if the display should be refreshed.
  bool frame(Canvas& cv, bool touchDown, int physX, int physY);
  uint8_t brightness() const { return bright_; }
  bool displayOn() const { return screenOn_; }
  // The timing games want a quick frame rate (smoother motion, and the touch is read twice as often): up to ~60 fps
  // while one is on screen, ~30 fps everywhere else to spare the battery.
  bool fastFrames() const { return scr == S_SCRATCH || scr == S_LASER || scr == S_KNOCK || scr == S_HEIST; }
  void saveNow();

  // ---- state (public: handy for the simulator & tests) ----
  Platform* plat;
  SaveData sv;
  Pet pet;
  Screen scr = S_MODE;
  uint32_t nowMs = 0;
  float dt = 0.033f;
  int hour = 12, minute = 0;
  uint32_t epoch = 0;
  float idleSec = 0;
  bool clockOk = false;
  int timeScale = 1;            // debug fast-forward (1 normal)

  // simulate N seconds of game time instantly (tests / catch-up)
  void simulate(float seconds, bool away);
  void debugStart(int which);   // test / demo: jump straight into an activity
  void devGift();                // dev: a gift box turns up now (the web page's developer panel)
  void devAddHairballs(int n);   // dev: the web page's developer panel, and holding the version line in Settings for 3 s
  void setAlarmForTest(int h, int m) { alarmH_ = h; alarmM_ = m; sv.alarmOn = 1; alarmDayDone_ = -1; }

 private:
  friend struct GameTestAccess;
  // input
  struct In {
    bool down = false, pressed = false, released = false, tap = false;
    int x = 0, y = 0, px = 0, py = 0;
    uint32_t downMs = 0;
    int heldMs = 0;
    uint32_t serial = 0;   // the screen (scrSerial_) the press landed on
    bool early = false;    // pressed within TAP_GUARD of that screen opening: ignored
  } in;
  bool swallow_ = false;
  MotionDetector motion_;
  float imuT_ = 0;
  float imuAge_ = 99;   // seconds since the tilt sensor last gave a believable sample (> 1: no sensor: Pour falls back to dragging)

  // timing
  float simAccum_ = 0, clockT_ = 0, saveT_ = 0, saveCool_ = 0;
  uint32_t backupAt_ = 0;   // epoch of the last backup save
  uint32_t turboOffset_ = 0;
  bool screenOn_ = true;
  uint8_t bright_ = 200;
  float anim_ = 0;
  bool dirty_ = false;
  uint32_t snoozeEpoch_ = 0;
  int alarmDayDone_ = -1;
  float alarmRing_ = 0;

  // screen flow
  Screen clockNext_ = S_NAME;
  float scrT_ = 0;
  ConfirmId confirm_ = CF_NONE;
  Screen confirmBack_ = S_HOME;
  char nameBuf_[12] = "Mittens";
  char customName_[12] = {0};      // the last name the player typed: kept so RANDOM NAME can be undone
  void stashName();                // remember nameBuf_ if it is a typed (non-list) name
  int nameSeed_ = 0;
  int skinSel_ = 0;
  int wardTab_ = 0, wardSel_ = 0;
  int bdaySel_[2] = {1, 1};                 // the birthday picker: month, day
  void screenBday(Canvas& cv);
  void screenNameKb(Canvas& cv);            // the on-screen keyboard for typing a kitten's name
  bool kbShift_ = true;
  void ensureBirthday();
  bool calendarBirthday(const DateTime& t) const;
  int clockSel_[5] = {2026, 1, 1, 12, 0};  // y m d h min
  int alarmH_ = 7, alarmM_ = 0;
  int endingKind_ = 0;
  float endT_ = 0;
  bool buttonOff_ = false;   // the side button turned the screen off: motion must not wake it
  uint8_t mutedVol_ = 0;     // the volume before the BOOT button muted it (0 = not known: back on at 2)
  float ambT_ = 0; int ambPaid_ = 0, ambOwe_ = 0;   // the ambulance scene: time, hairballs paid, hairballs put on the tab
  char endMsg_[6][40];   // the ending story (3 lines; the ran-away one has 6)
  int shopPage_ = -1;    // -1 = the shop menu; 0 goods, 1 furniture, 2 boxes, 4 homes
  bool wardFromShop_ = false;   // the Wardrobe was opened from the shop menu: BACK returns there
  int helpPage_ = 0;
  int journalPage_ = 0;
  int homeSel_ = 0;      // shop HOMES tab: which home is previewed
  int roomShown_ = -1;   // room currently cached in roomBuf_
  bool batOk_ = false, batChg_ = false; int batPct_ = 0; float batT_ = -1;   // battery for the HUD
  // battery history for the time-to-full / time-to-empty estimate: one sample a minute, the last 30 minutes
  struct BatSample { uint32_t sec; int8_t pct; };
  static constexpr int BAT_HIST = 31;
  BatSample batHist_[BAT_HIST]; int batN_ = 0; float batSampleT_ = 0; bool batHistChg_ = false;
  int batEstimate(int& kind) const;   // minutes; kind: 0 working it out, 1 to full, 2 to empty, 3 more than (minutes), 4 full
  void screenBattery(Canvas& cv);
  // auto-rotate: which sensor axis runs along the screen and which way is "up" is learned from the first steady upright pose
  // (oriCfg_ & 7: 0 = not learned, else 1 + axis*2 + negative; oriCfg_ & 16 = auto-rotate switched off in Settings; & 32 = ...and held upside down: flipped by hand)
  int oriCfg_ = 0; bool flipped_ = false; float oriHold_ = 0, oriT_ = 0;
  void updateOrientation();
  void setFlip(bool f);
  bool foodZero_ = false, funZero_ = false;   // the meter is empty (so the alert chirps once, not every frame)
  bool wasNapping_ = false, lapDue_ = false;   // out of energy -> Lap Cat as soon as you're home
  void drawItem(Canvas& cv, int item, int x, int y, bool mirror = false);
  void updateToys();
  void giveToy(int id) { if (!hasItem(id)) { sv.items |= (uint16_t)(1u << id); updateToys(); } }   // Explore finds: the toys are no longer sold
  // boxes, magic cat, litter (extras.cpp)
  void updateBox();
  // journal, birthdays and seasons (extras.cpp)
  void recordMilestone(int m);
  void takePhoto(int stage);
  void updateSpecialDays();
  int season() const;            // Season for today's date
  int seasonalHat() const;       // a hat they wear today when they have none on (0 = none)
  bool birthdayToday() const;
  void drawSeasonalDecor(Canvas& cv);
  void screenJournal(Canvas& cv);
  void giveBox(uint8_t type);
  void drawBoxProp(Canvas& cv, int type, int x, int y, int part);   // part 0 whole, 1 back only, 2 front only
  const char* boxName(int type) const;
  void shopBoxes(Canvas& cv, int y0);
  void shopHomes(Canvas& cv, int y0);
  void startMagic();
  void screenMagic(Canvas& cv);
  bool hasItem(int i) const { return (sv.items >> i) & 1; }

  // home
  Actor a_;
  Particle parts_[32];
  Toast toasts_[3];
  int holdBox_ = 0, holdFloor_ = 0, holdHair_ = 0, holdPrey_ = 0;
  uint16_t* roomBuf_ = nullptr;
  float roomHour_ = -1;
  int roomW_ = 0;              // the day/night blend the room was baked with (0 day .. 256 night)
  bool tableHidden_ = false;   // the room's little table is left out (Knock It Off)
  void drawRoomTable(int rid, int w);   // the little table sprite, blended for the time of day (w as in ensureRoom)
  bool roomDirty_ = true;
  float winT_ = -1;
  void ensureRoom();
  float bowl_ = 0;            // kibble visible in the bowl 0..1
  float petHold_ = 0;
  float bubbleT_ = 0; char bubble_[12] = "";
  float blinkT_ = 3;
  int litterHits_ = 0;
  bool secretNew_ = false;      // the ending just revealed a secret breed for the first time
  float guardT_ = 1; uint32_t guardSerial_ = 0;   // time since the screen (or page) last changed, and a count of those changes
  void guardTaps() { guardT_ = 0; guardSerial_++; }   // a new screen or page: ignore presses for TAP_GUARD, and drop a press already under way
  uint32_t scrSerial_ = 0;   // +1 every time a screen is opened (so a screen can tell its first frame reliably)
  uint32_t litterSerial_ = 0xFFFFFFFFu;
  // ---- touch targets: every button / tap area drawn this frame. A tap goes to the ONE target nearest
  // where the finger landed (so neighbours never both fire, and small targets catch near misses).
  struct HitRect { int16_t x, y, w, h; };
  static constexpr int MAX_HITS = 64;
  mutable HitRect hits_[MAX_HITS], hitsPrev_[MAX_HITS];
  mutable int nHits_ = 0;
  int nHitsPrev_ = 0, hitsScr_ = -1, hitsPrevScr_ = -1;
  int tapMode_ = 0;        // 0: decide the old way, 1: tapWin_ is the target, 2: the tap hit no known target
  HitRect tapWin_ = {0, 0, 0, 0};
  void resolveTap();
  float dropX_[6], dropY_[6]; bool dropGone_[6];

  // minigames
  LaserGame lg_;
  ScratchGame sg_;
  ResultInfo res_;
  PourGame pg_;
  LapGame lap_;
  KnockGame kg_;
  HeistGame hz_;
  ExploreGame ex_;
  MagicGame mg_;
  LitterGame lt_;
  float tiltAx_ = 0, tiltAy_ = 0, tiltAz_ = 1;

  // ---- game.cpp ----
  void updateInput(bool down, int px, int py);
  void updateClock();
  void updateSim();
  void stepSim(float seconds, bool away);
  SimCtx ctx(bool away) const;
  void handleEvents();
  void updateMotion();
  void updatePower();
  void checkAlarm();
  void wake();                    // wake display / reset idle
  void setScreen(Screen s);
  void toast(const char* fmt, ...);
  void sound(Sound s);
  void autosave(bool force);
  void newKitten(bool inherit);
  void lastSeenReset_();
  void endCat(int ending);
  void applyOfflineCatchUp();
  const char* randomName();
  float chonkFat() const { return pet.fatLook(); }

  // ---- home.cpp ----
  void drawHome(Canvas& cv, bool interactive);
  void drawRoom(Canvas& cv);
  void drawHud(Canvas& cv);
  void drawToolbar(Canvas& cv, bool interactive);
  void drawActor(Canvas& cv);
  void updateActor();
  void actorWalkTo(float x, Act after, float dur);
  void actorEvent(const Event& e);
  void spawn(float x, float y, uint8_t kind, Color c = 0, float vx = 0, float vy = -20, float life = 1.0f);
  void updateParticles(float d);
  void drawParticles(Canvas& cv);
  void drawToasts(Canvas& cv);
  void homeInput();
  void drawBubble(Canvas& cv, int x, int y, const char* s);
  void drawIcon(Canvas& cv, int id, int cx, int cy);
  void drawIconS(Canvas& cv, int id, int cx, int cy, float s);
  void drawPrey(Canvas& cv, int type, int cx, int cy);
  void drawHairball(Canvas& cv, int cx, int cy, int r);
  void drawBox(Canvas& cv, int cx, int baseY, int dropCount);
  void drawFloorX(int i, int& x, int& y) const;
  CatDraw catLook(Pose pose, Face face) const;
  void dimHome(Canvas& cv, int pct);   // a popup over the home: dim what's behind it (see game.cpp)
  int roamX();
  float gfun(float f) const { return gameFun(f * (pet.trait == TRAIT_PLAYFUL ? 1.25f : 1.0f) * (pet.stage() == STAGE_KITTEN ? KITTEN_GAME_FUN : 1.0f)); }   // a game's Fun (a Playful cat gets more)   // a random spot to wander to, kept so the cat is never more than a little off screen
  int windowX() const;
  int windowBottomY() const;   // logical y of the bottom of this room's window glass
  void drawWheel(Canvas& cv, bool front);
  // the mouse hole in every home: now and then a mouse pops out; tap it and the cat pounces (+energy)
  struct HouseMouse { int st = 0; float t = 0, x = 0, tx = 0, next = 90.0f; bool left = false; int room = -1; };
  HouseMouse mouse_;
  int mouseHoleX() const;
  int mouseHoleY() const;
  int mouseRunY() const;   // the row the mouse's feet run along (just in front of the hole)
  void updateMouse();
  void giftArrives(int why);   // gift boxes (screens.cpp, by the wardrobe): why = 1 birthday, 2 holiday, 3 good care
  void openGift();
  void drawGiftBox(Canvas& cv);
  void bakeMouseHole();
  void bakeCeilingLights(int night);   // the Modern Apartment's three recessed lights (night: 0 day .. 256 night)
  void drawMouse(Canvas& cv);
  float wheelSpin_ = 0;   // where they sit to look out of this room's window

  // ---- screens.cpp ----
  bool button(Canvas& cv, int x, int y, int w, int h, const char* label, Color fill, bool enabled = true, Color textCol = 0xFFFF);
  bool backButton(Canvas& cv);   // top-left BACK used by every mini-game (inside the screen's rounded corner)
  bool tapIn(int x, int y, int w, int h) const;
  void panelTitle(Canvas& cv, const char* title, int y = 6);
  void drawBackdrop(Canvas& cv, bool dimHome);
  void screenMode(Canvas& cv);
  void screenClock(Canvas& cv);
  void screenName(Canvas& cv);
  void screenPlay(Canvas& cv);
  void screenLitter(Canvas& cv);
  void leaveLitter();
  void backHomeForSleep();
  void screenShop(Canvas& cv);
  void screenMenu(Canvas& cv);
  void screenHelp(Canvas& cv);
  void screenStats(Canvas& cv);
  void screenSkins(Canvas& cv);
  void screenWardrobe(Canvas& cv);
  void updateDecor();
  void screenSettings(Canvas& cv);
  void screenAlarmSet(Canvas& cv);
  void screenResult(Canvas& cv);
  void screenEnding(Canvas& cv);
  void screenAlarmRing(Canvas& cv);
  void screenConfirm(Canvas& cv);
  bool stepper(Canvas& cv, int x, int y, int w, const char* label, int& val, int lo, int hi, int step = 1, bool wrap = false);
  void startLaser();
  void startScratch();
  void giveCoins(int n);
  void archiveAlbum(int ending);   // a cat's photos go into the journal's album when its life ends
  void coinText(char* b, size_t n) const;   // the hairball balance (negative while in debt to the vet)
  Color coinColor(Color normal) const;
  void checkCollapse();                      // health 0: ambulance to the vet
  void screenAmbulance(Canvas& cv);

  // ---- activities.cpp ----
  void startPour();
  void screenPour(Canvas& cv);
  void startLap(bool chosen = false);   // chosen: the player asked for it (Play menu / the Explore pop-up): +50 Energy at most
  bool napAsk_ = false;   // Play menu: the "needs 50 energy, nap?" pop-up is up
  bool exploreAsk_ = false;   // Play menu: the "solo or together?" pop-up is up
  // A star game's Fun: 3 stars fill the bar in 3 games (34), 2 stars in 6 (17), 1 star in 9 (11.2); no stars 6.7..10 by how
  // close they got (10 to 15 games). A Playful cat gets a quarter more.
  float starFun(int stars, float perf) const { const float p = perf < 0 ? 0 : (perf > 1 ? 1 : perf); const float b = stars >= 3 ? 34.0f : (stars == 2 ? 17.0f : (stars == 1 ? 11.2f : 6.7f + 3.3f * p)); return b * (pet.trait == TRAIT_PLAYFUL ? 1.25f : 1.0f); }
  void pourLaunch(float mx, float my, bool toBowl, int zone);
  void pourUpdateKibble();
  void screenLap(Canvas& cv);
  void startKnock();
  void screenKnock(Canvas& cv);
  void startHeist();
  void screenHeist(Canvas& cv);
  void startExplore();
  void screenExplore(Canvas& cv);
  void startExploreMP(int sx, int sy);
  // touch calibration (Settings > TOUCH FIX)
  void startTouchCal();
  void screenTouchCal(Canvas& cv);
  int calStep_ = 0; int calReturn_ = S_SETTINGS; bool calAsked_ = false; float calPts_[5][2]; float calOld_[4]; float calResult_[4]; float calMsgT_ = 0; bool calFail_ = false;   // a fresh Explore trip for an Explore Together match
  // ---- Explore Together (netplay.cpp) ----
  NetState net_;
  void startLink();
  void netStop();
  void netPoll();
  void screenLink(Canvas& cv);
  void netHostStart();
  void netEnterMatch(uint32_t seed, int seconds);
  void netToOver();
  void netSendRoster(bool started);
  void netFightStart(uint32_t fid, int foeSlot, uint8_t square);
  void netFightTick();
  void netApplyResult(uint32_t fid, uint32_t winner);
  void netMatchTick(int sx, int sy, float x, float y, bool left, bool moving);
  void netDrawPeers(Canvas& cv, int sx, int sy, float t);
  void netDrawHud(Canvas& cv, int sx, int sy, int ox, int oy, int pw, int ph);
  bool netDrawFight(Canvas& cv);
  void exTeleport(int sx, int sy, int tx, int ty);   // tests
  bool exploreAllowed(const char** why) const;
  float exScreenCost() const { return 10.0f - ((sv.ex.flags & EXF_TRAIL_PAWS) ? 2.0f : 0.0f) - ((sv.ex.journal & EXJ_CRYSTAL) ? 4.0f : 0.0f); }   // energy per Explore screen: 10 / 8 Trail Paws / 6 Cave Crystal / 4 both
  void drawLittleCat(Canvas& cv, int x, int y, Pose pose, Face face, bool left, float size, int topMin = -1);

  // ---- minigames.cpp ----
  void screenLaser(Canvas& cv);
  void screenScratch(Canvas& cv);
  void finishGame(int kind, int score, int target, int stars = -1, int bonusCoins = 0, int catches = 0);
  void drawPlayFloor(Canvas& cv);
};

// tiny side-view cat (Maze Run, Explore): breed colours, no snout
void drawMiniCat(Canvas& cv, int x, int y, bool left, float t, int breed, float fat, bool moving);

}  // namespace cat
