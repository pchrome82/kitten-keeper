// The cat itself: needs, weight ("chonk"), growth, moods and the events that happen to it.
// Pure logic, no drawing, no hardware. Time is advanced in "sim seconds".
#pragma once
#include <stdint.h>

namespace cat {

enum Mode : uint8_t { MODE_EASY = 0, MODE_UNSET = 255 };   // (Hard mode was removed before v1.0; old Hard saves load as Easy)
enum Stage : uint8_t { STAGE_KITTEN, STAGE_JUNIOR, STAGE_ADULT, STAGE_SENIOR };
// Personality: one per cat, picked when they come home. Small nudges to their needs and habits (see pet.cpp / home.cpp).
enum Trait : uint8_t { TRAIT_LAZY, TRAIT_CURIOUS, TRAIT_PLAYFUL, TRAIT_FOODIE, TRAIT_CUDDLY, TRAIT_BRAVE, TRAIT_COUNT };
const char* traitName(int t);
const char* traitBlurb(int t);
enum Ending : uint8_t { END_NONE, END_RETIRED, END_RAN_AWAY, END_BLACK_HOLE, END_SUN };   // END_SUN: secret, super lean + healthy
enum Food : uint8_t { FOOD_KIBBLE, FOOD_TREAT, FOOD_DIET };
enum Prey : uint8_t { PREY_NONE, PREY_MOUSE, PREY_BIRD, PREY_LIZARD, PREY_FISH, PREY_GOLDEN };
enum MeowReason : uint8_t { MR_NONE, MR_HUNGRY, MR_LITTER, MR_BORED, MR_ALARM };

enum EventType : uint8_t {
  EV_NONE, EV_POOP, EV_HAIRBALL, EV_HUNT_GO, EV_PREY_BACK, EV_MEOW, EV_STAGE_UP,
  EV_ENDING, EV_WOKE_ANGRY, EV_SLEEP, EV_WAKE, EV_WARN, EV_AUTO_CLEAN, EV_PREY_LOST,
  EV_MESS_FROM_HAIRBALL, EV_EVOLVE
};
struct Event {
  EventType type;
  int16_t arg;   // poop: 1 box / 2 floor ; meow: level ; stage: new stage ; ending: Ending ; warn: 1/2
  int16_t arg2;  // meow: reason ; prey: type
};

// Tuning (seconds). Kept in one place so balance is easy to change.
constexpr float HOUR = 3600.0f;
constexpr float DAY = 86400.0f;
constexpr float AGE_JUNIOR = 0.5f * DAY;
constexpr float AGE_ADULT = 1 * DAY;
constexpr float AGE_SENIOR = 26 * DAY;
constexpr float AGE_RETIRE = 31 * DAY;   // the best-kept cats live a full month; poor care ages them faster
// Day and night. By day (DAY_START..DAY_END o'clock) the cat is up and about whether you're there or not, and a full
// Fullness or Fun meter runs down to empty in DAY_EMPTY_HOURS (before toys, traits and charms). At night they nap on
// screen and the meters keep their old, slow rates. A touch wakes a napping cat for a feed or a game;
// NIGHT_AWAKE_SECS without another touch and they curl up again.
constexpr int DAY_START = 8, DAY_END = 20;        // 8 am .. 8 pm
constexpr float DAY_EMPTY_HOURS = 12.0f;          // junior / adult / senior: one full meter lasts the day
constexpr float DAY_EMPTY_HOURS_KITTEN = 2.0f;    // a kitten needs you every couple of hours
constexpr float NIGHT_AWAKE_SECS = 15 * 60.0f;
inline bool isDaytime(int hour) { return hour >= DAY_START && hour < DAY_END; }
constexpr float CHONK_BLACK_HOLE = 100.0f;
// The opposite secret: whittled right down to weight 6 by exercise and lean (yellow-zone) meals while healthy and fed, they shrink
// into a tiny bright sun (starving their can't get there: their health drops long before they're that light).
constexpr float CHONK_SUN = 6.0f;

struct HallEntry {
  char name[12];
  uint8_t breed;
  uint8_t ending;
  uint16_t ageDays;
  uint16_t prey;
  uint16_t pad;
};

struct PetData {
  char name[12];
  uint8_t breed;
  uint8_t evolving;           // 1 = generic kitten that evolves into a breed at adulthood
  uint8_t pad0[2];
  float age;                  // cat-seconds
  float hunger, fun, energy;  // 0..100
  float health;               // 0..100
  float chonk;                // 0..100 (35 = ideal); 100 -> black hole
  float resent;               // 0..100; 100 -> runs away
  float healthEma;
  uint8_t droppings;          // in the litter box
  uint8_t floorMess;          // on the floor
  uint8_t hairballsOnFloor;   // collectable
  uint8_t preyOnFloor;        // Prey enum
  uint8_t pendingPoop;
  uint8_t hunting;
  uint8_t pad1[2];
  float digestTimer, hairTimer, huntTimer, huntLeft;
  float autoTimer, messAge, hbAge, preyAge;
  uint16_t preyCount;         // this cat's trophies
  uint16_t pad2;
  float acc[6];               // care habits while evolving: speed, clean, weight, strength, neglect, love
  uint16_t cleanActs, pad4;
};
enum EvoAcc { ACC_SPEED, ACC_CLEAN, ACC_WEIGHT, ACC_STRENGTH, ACC_NEGLECT, ACC_LOVE };

struct SimCtx {
  int hour;               // local hour 0..23
  bool playerActive;      // the player touched the screen within the last NIGHT_AWAKE_SECS (they're around)
  bool away;              // offline catch-up: gentler, never ends the game
  uint8_t litterTier;     // 0 basic, 1 sifting, 2 auto
};

class Pet {
 public:
  explicit Pet(PetData& data) : d(data) {}
  PetData& d;
  float funMul = 1.0f;
  float toyBonus = 0.0f;   // fun-drain cut from toys alone (0..0.29); doubled at night
  bool eating = false;     // at the bowl right now (set by the game): no boredom while eating
  uint8_t trait = 0;       // Trait (set by the game from the save)
  bool allToys = false;    // owns all 4 toys: a kitten's needs stretch to ~12 h overnight
  float sleepMul = 1.0f;   // > 1: naps restore energy faster (beds)
  float decayMul = 1.0f;   // hermit's charm: all needs fade slower   // < 1 slows fun decay (house set bonus)

  // runtime-only state
  bool sleeping = false;
  float grump = 0;            // seconds of post-disturbance grumpiness
  float disturbCd = 0;
  uint8_t lastWarn = 0;
  float easyMeowCd = 30;
  bool napping = false;       // asleep because of low energy (not because of schedule / idle)
  bool collapse = false;      // health hit 0: the game rushes them to the vet (runtime only)
  bool ended = false;         // an ending event has been emitted
  uint32_t rng = 12345;

  void newKitten(uint32_t seed, int breed, const char* name);
  void tick(float dt, const SimCtx& cx);
  bool pop(Event& e);

  // player actions
  bool canFeed() const { return !sleeping && !d.hunting && d.hunger <= 98.0f; }   // a completely stuffed cat refuses food
  // returns chonk gained (for feedback); sets overfed
  float feed(Food f, bool& overfed, float amount = 1.0f, float spill = 0.0f, float lean = 0.0f);   // lean = part of amount poured in the yellow zone
  void petIt(float amt);                  // affection
  void exercise(float funGain, float energyCost, float chonkLoss);
  void disturb();                         // picked up / shaken while asleep
  void wakeGently();
  void cleanFloor() { if (d.cleanActs < 60000) d.cleanActs++; if (d.floorMess) d.floorMess--; if (!d.floorMess) d.messAge = 0; }
  void collectHairball() { if (d.hairballsOnFloor) { d.hairballsOnFloor--; if (!d.hairballsOnFloor) d.hbAge = 0; } }
  void takePrey() { d.preyOnFloor = PREY_NONE; d.preyAge = 0; d.preyCount++; }
  void vetVisit() { d.health = d.health + 45 > 100 ? 100 : d.health + 45; d.resent = d.resent - 10 < 0 ? 0 : d.resent - 10; }
  float scoop(int n);                     // remove n droppings from the box, returns how many removed

  // derived values
  Stage stage() const;
  float growAge() const;                  // their age as far as growing up goes (the secret breeds are always adults)
  float gameFat() const;                  // 0 normal .. 1 very chonky: how much a game squeezes / slows / enlarges the cat
  float gameDrag() const;                 // extra game speed multiplier for chonky cats: 1.0 .. 0.85
  float gameScale() const;                // extra on-screen size in games: 1.0 .. 1.25
  float visualSize() const;               // 0.6 kitten .. 1.0 adult
  float kitFactor() const;                // 1 baby proportions .. 0 adult
  float fatLook() const;
  float idealWeight() const;
  float weightSpeed() const;              // 1.0 at/below ideal ... 0.65 when very heavy (-35%)              // 35 for kittens, grows to 50 by adulthood ("chonk" is shown as Weight)
  float boost = 1.0f;                     // the Hidden Hermit's amulet: +15% attack and speed (set by the game)
  int lookBreed() const;                  // breed to DRAW: NUM_BREEDS (generic kitten) while evolving
  int evolveChoice(float* scores = nullptr) const;   // which breed the care habits point at
  void evolve();                  // for drawing
  float agility() const;                  // speed multiplier for games
  float power() const;                    // scratching power
  int mood() const;                       // 0 happy 1 ok 2 sad 3 angry 4 sleepy
  float ageDays() const { return d.age / DAY; }
  uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
  float rndf() { return (rnd() & 0xFFFFFF) / 16777216.0f; }
  float rndRange(float a, float b) { return a + (b - a) * rndf(); }
  void clampAll();

 private:
  Event q_[12];
  uint8_t qh_ = 0, qt_ = 0;
  void push(EventType t, int a = 0, int b = 0);
  void poop(const SimCtx& cx);
  bool wantsSleep(const SimCtx& cx) const;
  int breedHairIndex() const;
};

const char* stageName(Stage s);
const char* preyName(uint8_t p);
int preyValue(uint8_t p);
const char* endingName(uint8_t e);

}  // namespace cat
