// Skins == breeds. Each is a palette + coat pattern, with tiny gameplay quirks.
#pragma once
#include "canvas.h"

namespace cat {

enum Pattern : uint8_t { PAT_SOLID, PAT_TABBY, PAT_TUX, PAT_CALICO, PAT_POINTS, PAT_SPHYNX };

struct Breed {
  const char* name;
  Color coat, coat2, belly, earIn;   // (the little Explore / Maze sprite; the big cats use the baked palettes)
  Pattern pat;
  float speed;        // agility multiplier
  float strength;     // fight power multiplier
  float gain;         // how easily it gains weight
  float hair;         // hairball frequency multiplier
  uint16_t price;     // in hairballs
  const char* blurb;
};

constexpr int NUM_BREEDS = 7;   // the regular breeds 0..6 (shop, skins, adoption)
// Index 7 was the generic "let it evolve" kitten (no longer offered). The secret breeds come after it,
// so older saves keep their numbering.
constexpr int BR_KITTEN = 7, BR_RAINBOW = 8, BR_HAMSTER = 9;
constexpr int NUM_BREED_SLOTS = 10;
inline bool isSecretBreed(int b) { return b == BR_RAINBOW || b == BR_HAMSTER; }
extern const Breed BREEDS[NUM_BREED_SLOTS];

}  // namespace cat
