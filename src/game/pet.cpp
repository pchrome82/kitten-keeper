#include "pet.h"
#include "chibi.h"
#include "breeds.h"
#include <string.h>
#include <math.h>

namespace cat {

static constexpr float NEGLECT_K = 0.06f;   // tuned with sim/balance (total neglect -> ~3 days)

static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

const char* traitName(int t) {
  static const char* n[TRAIT_COUNT] = {"Lazy", "Curious", "Playful", "Foodie", "Cuddly", "Brave"};
  return t >= 0 && t < TRAIT_COUNT ? n[t] : "?";
}
const char* traitBlurb(int t) {   // (fits the Stats screen: ~28 characters)
  static const char* b[TRAIT_COUNT] = {"Naps a lot, tires slowly", "Loves the window, bores fast", "Games are extra fun",
                                       "Always hungry for more", "Petting means the world", "Hits a bit harder"};
  return t >= 0 && t < TRAIT_COUNT ? b[t] : "";
}

const char* stageName(Stage s) {
  switch (s) {
    case STAGE_KITTEN: return "Kitten";
    case STAGE_JUNIOR: return "Junior";
    case STAGE_ADULT: return "Adult";
    default: return "Senior";
  }
}
const char* preyName(uint8_t p) {
  switch (p) {
    case PREY_MOUSE: return "Mouse";
    case PREY_BIRD: return "Bird";
    case PREY_LIZARD: return "Lizard";
    case PREY_FISH: return "Fish";
    case PREY_GOLDEN: return "Golden Mouse";
    default: return "Nothing";
  }
}
int preyValue(uint8_t p) {
  switch (p) {
    case PREY_MOUSE: return 2;
    case PREY_BIRD: return 3;
    case PREY_LIZARD: return 3;
    case PREY_FISH: return 5;
    case PREY_GOLDEN: return 12;
    default: return 0;
  }
}
const char* endingName(uint8_t e) {
  switch (e) {
    case END_RETIRED: return "Retired";
    case END_RAN_AWAY: return "Ran away";
    case END_BLACK_HOLE: return "Black hole";
    case END_SUN: return "Tiny sun";
    default: return "-";
  }
}

void Pet::newKitten(uint32_t seed, int breed, const char* name) {
  memset(&d, 0, sizeof(d));
  strncpy(d.name, name, sizeof(d.name) - 1);
  d.breed = (uint8_t)breed;
  d.hunger = 20; d.fun = 20; d.energy = 85; d.health = 100; d.chonk = 34; d.resent = 0;   // new kittens arrive hungry and bored: lots to do
  if (isSecretBreed(breed)) d.chonk = 49;   // (they arrive as adults: an adult's weight)
  d.healthEma = 100;
  rng = seed * 2654435761u + 0x9E3779B9u;
  if (!rng) rng = 1;
  for (int i = 0; i < 5; i++) rnd();
  d.pendingPoop = 1; d.digestTimer = rndRange(2 * 60.0f, 5 * 60.0f);   // a new kitten needs to go within 5 minutes
  d.hairTimer = rndRange(2 * HOUR, 5 * HOUR);
  d.huntTimer = rndRange(3 * HOUR, 6 * HOUR);
  sleeping = false; napping = false; ended = false;
  grump = 0; disturbCd = 0; lastWarn = 0;
  qh_ = qt_ = 0;
}

void Pet::push(EventType t, int a, int b) {
  q_[qt_] = {t, (int16_t)a, (int16_t)b};
  qt_ = (uint8_t)((qt_ + 1) % 12);
  if (qt_ == qh_) qh_ = (uint8_t)((qh_ + 1) % 12);
}
bool Pet::pop(Event& e) {
  if (qh_ == qt_) return false;
  e = q_[qh_];
  qh_ = (uint8_t)((qh_ + 1) % 12);
  return true;
}

// How grown they are, as an age. The secret breeds (the Rainbow Cat, the Hamster) arrive fully grown and stay adults
// all their lives: no kitten or junior stage, and no senior one either. Their real age still counts their days.
float Pet::growAge() const {
  if (isSecretBreed(d.breed)) return AGE_ADULT;
  return d.age;
}
Stage Pet::stage() const {
  const float a = growAge();
  if (a < AGE_JUNIOR) return STAGE_KITTEN;
  if (a < AGE_ADULT) return STAGE_JUNIOR;
  if (a < AGE_SENIOR) return STAGE_ADULT;
  return STAGE_SENIOR;
}
float Pet::visualSize() const { return 0.58f + 0.42f * clampf(growAge() / AGE_ADULT, 0, 1); }
float Pet::kitFactor() const { float g = 1.0f - clampf(growAge() / AGE_ADULT, 0, 1); return g * g; }
int Pet::lookBreed() const { return d.breed; }

int Pet::evolveChoice(float* out) const {
  float h = d.age / HOUR; if (h < 1) h = 1;
  auto cl = [](float v) { return v < 0 ? 0.0f : (v > 1 ? 1.0f : v); };
  float speed = cl(d.acc[ACC_SPEED] / 4.0f), strength = cl(d.acc[ACC_STRENGTH] / 4.0f);
  float clean = cl(d.acc[ACC_CLEAN] / (0.9f * h)) * 0.7f + cl(d.cleanActs / 5.0f) * 0.3f;
  float wavg = d.acc[ACC_WEIGHT] / h, weight = cl((wavg - 0.35f) / 0.25f);
  float neglect = cl(d.acc[ACC_NEGLECT] / h * 2.5f), love = cl(d.acc[ACC_LOVE] / (0.8f * h));
  float sc[7];
  sc[SP_SIAMESE] = speed; sc[SP_SKOGKATT] = strength; sc[SP_CALICO] = weight;
  sc[SP_SPHYNX] = (clean > 0.75f && wavg < 0.27f) ? clean * 0.95f : clean * 0.4f;
  sc[SP_TUXEDO] = (love + clean) * 0.5f * 0.85f;
  sc[SP_VOID] = neglect >= 0.5f ? 1.5f + neglect : neglect * 0.5f;
  sc[SP_TABBY] = 0.42f;
  if (out) for (int i = 0; i < 7; i++) out[i] = sc[i];
  int best = SP_TABBY;
  for (int i = 0; i < 7; i++) if (sc[i] > sc[best]) best = i;
  return best;
}

void Pet::evolve() {
  d.breed = (uint8_t)evolveChoice();
  d.evolving = 0;
  push(EV_EVOLVE, d.breed);
}

float Pet::idealWeight() const {
  const float a = growAge();
  if (a < AGE_JUNIOR) return 35.0f;
  if (a >= AGE_ADULT) return 50.0f;
  return 35.0f + 15.0f * (a - AGE_JUNIOR) / (AGE_ADULT - AGE_JUNIOR);   // growing up
}

float Pet::weightSpeed() const {
  const float over = d.chonk - idealWeight();
  if (over <= 3) return over < -20 ? 0.9f : 1.0f;              // skinny cats are a bit weak too
  return 1.0f - 0.35f * clampf((over - 3) / 32.0f, 0, 1);       // -35% once they're 35 over ideal
}

float Pet::gameFat() const { return clampf(fatLook() * 1.25f, 0.0f, 1.0f); }
float Pet::gameDrag() const { return 1.0f - 0.15f * gameFat(); }
float Pet::gameScale() const { return 1.0f + 0.25f * gameFat(); }
float Pet::fatLook() const { return clampf((d.chonk - idealWeight()) / 50.0f, -0.3f, 1.3f); }

float Pet::agility() const {
  const Breed& b = BREEDS[d.breed];
  float stageF = 1.0f;
  switch (stage()) {
    case STAGE_KITTEN: stageF = 1.0f; break;
    case STAGE_JUNIOR: stageF = 1.1f; break;
    case STAGE_ADULT: stageF = 1.0f; break;
    default: stageF = 0.8f; break;
  }
  float healthF = 0.6f + 0.4f * clampf(d.health / 100.0f, 0, 1);
  // the heavier they are (past their ideal weight), the slower they get
  const float weightF = weightSpeed();
  const float traitF = trait == TRAIT_LAZY ? 0.95f : (trait == TRAIT_CURIOUS ? 1.03f : 1.0f);
  return b.speed * stageF * healthF * weightF * boost * traitF;
}
float Pet::power() const {
  float stageF = stage() == STAGE_KITTEN ? 0.7f : (stage() == STAGE_SENIOR ? 0.85f : 1.0f);
  const float breedF = BREEDS[d.breed < NUM_BREED_SLOTS ? d.breed : 0].strength;   // the breed's Strength stat
  return (0.9f + d.chonk * 0.025f) * stageF * breedF * boost * (trait == TRAIT_BRAVE ? 1.05f : 1.0f);
}
int Pet::mood() const {
  if (sleeping) return 4;
  if (grump > 0 || d.resent >= 80) return 3;
  if (d.resent >= 55 || d.hunger < 25 || d.fun < 25 || d.droppings >= 3 || d.floorMess) return 2;
  if (d.fun > 70 && d.hunger > 45) return 0;
  return 1;
}
void Pet::clampAll() {
  d.hunger = clampf(d.hunger, 0, 100);
  d.fun = clampf(d.fun, 0, 100);
  d.energy = clampf(d.energy, 0, 100);
  d.health = clampf(d.health, 0, 100);
  d.chonk = clampf(d.chonk, 3, 130);
  d.resent = clampf(d.resent, 0, 100);
  d.healthEma = clampf(d.healthEma, 0, 100);
  if (d.droppings > 6) d.droppings = 6;
  if (d.floorMess > 4) d.floorMess = 4;
  if (d.hairballsOnFloor > 3) d.hairballsOnFloor = 3;
}

// how much of the weight a yellow-zone (trickle) meal sheds is kept: the Tiny Sun needs about as many feedings as the Black Hole does
constexpr float LEAN_SLIM = 0.85f;

float Pet::feed(Food f, bool& overfed, float amount, float spill, float lean) {
  overfed = false;
  float before = d.hunger;
  const Breed& b = BREEDS[d.breed];
  float gained = 0;
  switch (f) {
    case FOOD_KIBBLE: {
      // Weight targets: 3 bad pours change the cat's look (chonk / skinny); ~10 reach the Black Hole (+50) or the Tiny Sun (-44).
      if (lean > amount) lean = amount;
      const float green = amount - lean;
      d.hunger += 45 * green + 22.5f * lean;              // a trickle-poured (yellow) meal is less filling
      // proper meal: offsets the daily burn and gently pulls toward the ideal weight (so kittens grow into adults)
      gained = green * b.gain * (1.0f + clampf((idealWeight() - d.chonk) * 0.1f, -0.9f, 2.0f)) - 3.0f * lean;
      overfed = before >= 85 || spill >= 0.2f;
      if (overfed) gained += (4.9f + 0.8f * clampf((spill - 0.2f) / 0.6f, 0, 1)) * (b.gain < 0.9f ? 0.9f : b.gain);   // ~+5 per overfeed (a low-gain cat still bulks up when stuffed on purpose)
      else gained += spill * 5.0f;                        // a few stray bits eaten off the floor
      const bool under = !overfed && (amount < 0.5f || lean > 0.6f * amount);
      if (under) gained -= amount < 0.6f ? 4.5f : 2.0f;   // underfeed: about -4.4 per pour
      d.fun += 1;
      break;
    }
    case FOOD_TREAT:
      d.hunger += 15;
      d.fun += 20;
      gained = (3.5f + (before > 85 ? 1.5f : 0.0f)) * b.gain;
      overfed = before >= 90;
      d.resent -= 3;
      break;
    case FOOD_DIET:
      d.hunger += 24;
      gained = -0.8f;
      d.fun -= 2;
      d.health += 1;
      overfed = before >= 95;
      break;
  }
  if (overfed) { d.health -= 1.5f; d.fun -= 2; }
  if (f == FOOD_KIBBLE && gained < 0 && lean > 0.6f * amount) gained *= LEAN_SLIM;   // a trickle meal slims, but not by much: the Tiny Sun takes many of them
  d.chonk += gained;
  if (d.pendingPoop < 6) d.pendingPoop++;
  if (d.digestTimer <= 0) {
    if (stage() == STAGE_KITTEN) {   // kittens go a lot: 15-30 min after a meal (bigger meal = sooner)
      float a = clampf(amount, 0, 1);
      d.digestTimer = rndRange((26 - 11 * a) * 60.0f, (30 - 11 * a) * 60.0f);
    } else d.digestTimer = rndRange(25 * 60.0f, 45 * 60.0f);
  }
  clampAll();
  return gained;
}

void Pet::petIt(float amt) {
  if (trait == TRAIT_CUDDLY) amt *= 1.5f;   // a cuddly cat loves it even more
  d.fun += amt;
  d.resent -= amt * 0.3f;
  clampAll();
}

void Pet::exercise(float funGain, float energyCost, float chonkLoss) {
  d.fun += funGain;
  d.energy -= energyCost;
  d.chonk -= chonkLoss * (d.breed == 5 ? 1.25f : 1.0f);   // a Sphynx burns it off faster (lean = the Tiny Sun comes easier)
  d.resent -= (funGain < 40 ? funGain : 40) * 0.2f;   // a good game cheers them up (capped, now that games can fill Fun)
  clampAll();
}

void Pet::disturb() {
  if (!sleeping || disturbCd > 0) return;
  sleeping = false;
  disturbCd = 60;
  grump = 20;
  d.fun -= 2;
  push(EV_WOKE_ANGRY, 1);
  clampAll();
}

void Pet::wakeGently() {
  if (!sleeping) return;
  sleeping = false;
  push(EV_WAKE);
}

float Pet::scoop(int n) {
  int removed = 0;
  while (n-- > 0 && d.droppings > 0) { d.droppings--; removed++; if (d.cleanActs < 60000) d.cleanActs++; }
  if (!d.droppings) d.autoTimer = 0;
  return (float)removed;
}

bool Pet::wantsSleep(const SimCtx& cx) const {
  if (d.hunting) return false;
  if (napping) return true;                              // worn out: a nap, whatever the hour
  if (isDaytime(cx.hour)) return false;                  // by day they're up and about, whether you're there or not
  return !cx.playerActive;                               // at night they sleep, unless you've just woken them
}

void Pet::poop(const SimCtx& cx) {
  // A happy cat always uses the box, even a dirty one (a dirty box makes their grumpier instead).
  // Only a really grumpy cat protests on the floor.
  const bool protest = (d.resent >= 60.0f && d.droppings >= 2) || d.resent >= 85.0f;
  bool box = !protest || cx.litterTier >= 2;
  if (box) {
    d.droppings++;
    push(EV_POOP, 1);
    if (cx.litterTier == 1 && d.autoTimer <= 0) d.autoTimer = 40 * 60.0f;
    if (cx.litterTier == 2 && d.autoTimer <= 0) d.autoTimer = 60.0f;
  } else {
    if (d.floorMess < 4) d.floorMess++;
    push(EV_POOP, 2);
  }
  if (d.pendingPoop) d.pendingPoop--;
  d.digestTimer = d.pendingPoop ? (stage() == STAGE_KITTEN ? rndRange(15 * 60.0f, 30 * 60.0f) : rndRange(25 * 60.0f, 40 * 60.0f)) : 0;
}

void Pet::tick(float dt, const SimCtx& cx) {
  const bool away = cx.away;
  const float hr = dt / HOUR;
  const bool night = cx.hour >= 23 || cx.hour < 6;
  const Breed& br = BREEDS[d.breed];

  // ---- sleeping ----
  if (d.energy < 12.0f) napping = true;
  if (d.energy >= 50.0f) napping = false;
  if (grump > 0) grump -= dt;
  if (disturbCd > 0) disturbCd -= dt;
  bool want = wantsSleep(cx);
  if (!sleeping && want && grump <= 0) { sleeping = true; push(EV_SLEEP); }
  else if (sleeping && !want) { sleeping = false; push(EV_WAKE); }

  // ---- needs ----
  // Fullness and Fun run on the clock alone, whatever the player or the cat is doing: the slow night rates from
  // 8 pm to 8 am (both creep down together), the day rates below otherwise. Only Energy follows actual sleep.
  const bool day = isDaytime(cx.hour);
  float hungerRate = -4, funRate = -4;
  float energyRate = sleeping ? 120.0f * sleepMul : -8.0f;
  float mul = away ? 0.35f : 0.75f;
  // a kitten's night: Fullness and Fun last about 2 h without toys (the toys stretch them, below)
  float kH = 1.0f, kF = 1.0f;
  if (stage() == STAGE_KITTEN) { kH = 16.5f; kF = 16.5f; }
  // at night toys count double: an extra cut to fun drain, and kittens drain slower overall
  if (night && toyBonus > 0) {
    kF *= 1.0f - toyBonus;
    if (stage() == STAGE_KITTEN) kH *= 1.0f - 2.0f * toyBonus;
    if (stage() == STAGE_KITTEN && allToys) { kH *= 0.4f; kF *= 0.325f; }   // the full set: Fullness and Fun last ~12 h overnight
  }
  // by day: Fullness and Fun both run from full to empty in DAY_EMPTY_HOURS (set for a running game, mul 0.75); no boredom while eating
  if (day) {
    const float perHour = 100.0f / (stage() == STAGE_KITTEN ? DAY_EMPTY_HOURS_KITTEN : DAY_EMPTY_HOURS) / 0.75f;
    hungerRate = -perHour; funRate = eating ? 0.0f : -perHour;
    kH = kF = 1.0f;
  }
  // personality: Foodie gets hungry faster, Curious bores faster, Lazy tires (and bores) slower, Playful tires faster
  const float tH = trait == TRAIT_FOODIE ? 1.1f : 1.0f;
  const float tF = trait == TRAIT_CURIOUS ? 1.1f : (trait == TRAIT_LAZY ? 0.9f : 1.0f);
  const float tE = trait == TRAIT_LAZY ? 0.8f : (trait == TRAIT_PLAYFUL ? 1.1f : 1.0f);
  d.hunger += hungerRate * (hungerRate < 0 ? decayMul * kH * tH : 1.0f) * mul * hr;
  d.fun += funRate * (funRate < 0 ? funMul * decayMul * kF * tF : 1.0f) * mul * hr;
  d.energy += energyRate * (energyRate > 0 ? 1.0f : mul * decayMul * tE) * hr;

  // ---- weight ----
  d.chonk -= hr * 0.06f;   // (one rate round the clock: being awake all day mustn't slim them faster than before)
  if (d.hunger <= 0) d.chonk -= hr * 0.8f;

  // ---- litter / mess ----
  if (d.pendingPoop) {
    // night (10pm-7am): they hold it, so nearly everything waits for the morning
    const bool quiet = cx.hour >= 22 || cx.hour < 7;
    d.digestTimer -= quiet ? dt * 0.05f : dt;
    // a napping cat still gets up to go (and curls back up); only a hunt makes their wait
    if (d.digestTimer <= 0 && (!d.hunting || away)) poop(cx);
  }
  if (cx.litterTier >= 1 && d.droppings > 0) {
    if (d.autoTimer <= 0) d.autoTimer = cx.litterTier == 1 ? 40 * 60.0f : 60.0f;
    d.autoTimer -= dt;
    if (d.autoTimer <= 0) {
      if (cx.litterTier == 1) d.droppings = 0; else d.droppings--;
      d.autoTimer = 0;
      push(EV_AUTO_CLEAN);
    }
  } else if (cx.litterTier == 0 || d.droppings == 0) {
    d.autoTimer = 0;
  }
  if (d.floorMess) d.messAge += dt; else d.messAge = 0;

  // ---- hairballs ---- (only while you're around, as before: an awake cat left alone all day doesn't fill the floor)
  if (!sleeping && !d.hunting && cx.playerActive) {
    d.hairTimer -= dt;
    if (d.hairTimer <= 0 && br.hair <= 0) d.hairTimer = 1e9f;   // hairless (Sphynx, hair 0): never a hairball
    if (d.hairTimer <= 0) {
      if (d.hairballsOnFloor < 3) {
        d.hairballsOnFloor++;
        if (d.hairballsOnFloor == 1) d.hbAge = 0;
        push(EV_HAIRBALL);
      }
      d.hairTimer = rndRange(2 * HOUR, 5 * HOUR) / (br.hair < 0.05f ? 0.05f : br.hair);
    }
  }
  if (d.hairballsOnFloor) {
    d.hbAge += dt;
    if (d.hbAge > 1800.0f) {
      d.hairballsOnFloor--;
      if (d.floorMess < 4) d.floorMess++;
      d.hbAge = 0;
      push(EV_MESS_FROM_HAIRBALL);
    }
  }

  // ---- hunting ----
  if (!away) {
    if (d.hunting) {
      d.huntLeft -= dt;
      if (d.huntLeft <= 0) {
        d.hunting = 0;
        float luck = d.health / 100.0f;
        uint8_t type = PREY_NONE;
        if (rndf() < 0.55f + 0.4f * luck) {
          float r = rndf();
          type = r < 0.45f ? PREY_MOUSE : r < 0.70f ? PREY_BIRD : r < 0.85f ? PREY_LIZARD : r < 0.95f ? PREY_FISH : PREY_GOLDEN;
        }
        if (type) { d.preyOnFloor = type; d.preyAge = 0; }
        push(EV_PREY_BACK, 0, type);
      }
    } else if (!sleeping && cx.playerActive && d.preyOnFloor == PREY_NONE && stage() != STAGE_KITTEN &&
               d.fun >= 30 && d.hunger >= 25 && d.health >= 30) {
      d.huntTimer -= dt;
      if (d.huntTimer <= 0) {
        d.hunting = 1;
        d.huntLeft = rndRange(120, 240);
        d.huntTimer = rndRange(1.5f * HOUR, 4 * HOUR);
        push(EV_HUNT_GO);
      }
    }
  } else if (d.preyOnFloor == PREY_NONE && !sleeping && isDaytime(cx.hour) && stage() != STAGE_KITTEN) {
    // while you are away the cat may leave a gift
    d.huntTimer -= dt;
    if (d.huntTimer <= 0) {
      float r = rndf();
      d.preyOnFloor = r < 0.5f ? PREY_MOUSE : r < 0.8f ? PREY_BIRD : r < 0.95f ? PREY_LIZARD : PREY_FISH;
      d.preyAge = 0;
      d.huntTimer = rndRange(2 * HOUR, 5 * HOUR);
    }
  }
  if (d.preyOnFloor) {
    d.preyAge += dt;
    if (d.preyAge > 900.0f && !away) { d.preyOnFloor = PREY_NONE; d.preyAge = 0; push(EV_PREY_LOST); }
  }

  // ---- meows: they tell you when they need something ----
  if (!away && !sleeping && cx.playerActive) {
    int reason = d.hunger < 30 ? MR_HUNGRY : ((d.droppings >= 3 || d.floorMess) ? MR_LITTER : (d.fun < 20 ? MR_BORED : MR_NONE));
    if (reason) {
      easyMeowCd -= dt;
      if (easyMeowCd <= 0) { push(EV_MEOW, 0, reason); easyMeowCd = rndRange(60, 120); }
    }
  }

  // ---- resentment & health ----
  // Neglect builds resentment slowly: a totally ignored cat lasts about 3 days before they leave.
  const float NK = NEGLECT_K;
  if (d.hunger <= 0) { d.health -= 6 * hr; d.resent += 12 * hr * NK; }
  else if (d.hunger < 20) d.resent += 3 * hr * NK;
  if (d.fun < 10) d.resent += 6 * hr * NK * (trait == TRAIT_CUDDLY ? 1.25f : 1.0f);   // a cuddly cat takes being ignored harder
  if (d.floorMess && d.messAge > 1200.0f) {
    d.health -= 2 * hr * d.floorMess;
    d.resent += 4 * hr * (1 + 0.5f * d.floorMess) * NK;
  }
  if (d.droppings >= 4) { d.health -= 1.5f * hr; d.resent += 3 * hr * NK; }   // a day's worth (3) is tolerated
  const float ideal = idealWeight();
  if (d.chonk > ideal + 45) d.health -= 3 * hr;
  else if (d.chonk > ideal + 30) d.health -= 1 * hr;
  if (d.health < 10) d.resent += 10 * hr * NK;
  bool good = d.hunger >= 35 && d.fun >= 30 && d.floorMess == 0 && d.droppings < 3;
  if (good) {
    d.resent -= 3.5f * hr;
    if (d.chonk < ideal + 30) d.health += 5 * hr;
  }
  float k = dt / (6 * HOUR);
  d.healthEma += (d.health - d.healthEma) * (k > 1 ? 1 : k);

  // ---- ageing ----
  // they age faster when they're unwell, overweight, grumpy or neglected: only great care reaches 31 days
  float rate = 1.0f;
  if (d.health < 70) rate += (70 - d.health) / 70.0f * 0.6f;
  if (d.chonk > idealWeight() + 25) rate += (d.chonk - idealWeight() - 25) / 40.0f * 0.3f;
  rate += clampf(d.resent, 0, 100) / 100.0f * 0.8f;
  if (d.hunger < 20 || d.fun < 20) rate += 0.35f;
  Stage before = stage();
  d.age += dt * rate;
  if (stage() != before) push(EV_STAGE_UP, stage());

  clampAll();
  if (d.health <= 0.0f && !ended) collapse = true;

  // ---- warnings & endings ----
  if (d.resent < 50) lastWarn = 0;
  if (d.resent >= 85 && lastWarn < 2) { lastWarn = 2; push(EV_WARN, 2); }
  else if (d.resent >= 60 && lastWarn < 1) { lastWarn = 1; push(EV_WARN, 1); }

  if (away) {
    if (d.resent > 90) d.resent = 90;
    if (d.chonk > 99) d.chonk = 99;
    if (d.chonk < CHONK_SUN + 1) d.chonk = CHONK_SUN + 1;   // the secret endings only happen while you're there to see them
  } else if (!ended) {
    const bool grownUp = growAge() >= AGE_ADULT;   // both secret endings are for grown-up cats only
    if (grownUp && d.chonk >= CHONK_BLACK_HOLE) { ended = true; push(EV_ENDING, END_BLACK_HOLE); }
    else if (grownUp && d.chonk <= CHONK_SUN && d.health >= 70 && d.healthEma >= 70 && d.hunger >= 30) { ended = true; push(EV_ENDING, END_SUN); }
    else if (d.resent >= 100) { ended = true; push(EV_ENDING, END_RAN_AWAY); }
    else if (d.age >= AGE_RETIRE) { ended = true; push(EV_ENDING, END_RETIRED); }
  }
}

}  // namespace cat
