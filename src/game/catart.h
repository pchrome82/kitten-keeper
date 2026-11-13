// Game-facing cat drawing API: baked pixel-art sprites (catsprites) + costume overlays.
#pragma once
#include "canvas.h"
#include "breeds.h"
#include "chibi.h"

namespace cat {

enum Pose : uint8_t {
  POSE_SIT, POSE_STAND, POSE_WALK, POSE_SLEEP, POSE_EAT, POSE_CROUCH, POSE_POUNCE,
  POSE_HACK, POSE_BACK, POSE_UPRIGHT, POSE_FRONT, POSE_COUNT
};

enum Face : uint8_t {
  FACE_NORMAL, FACE_HAPPY, FACE_SLEEP, FACE_SAD, FACE_ANGRY, FACE_SURPRISE, FACE_MEOW, FACE_EAT, FACE_BLINK
};

struct CatDraw {
  int breed = 0;
  float fat = 0.0f;       // -0.3 (skinny) .. 0 ideal .. 1.0 (chonk) .. 1.3 (about to collapse)
  float size = 1.0f;      // overall multiplier (growth stage * perspective)
  float kit = 0.0f;       // 0 adult proportions .. 1 baby proportions (big head)
  Pose pose = POSE_SIT;
  Face face = FACE_NORMAL;
  bool faceLeft = false;
  float t = 0.0f;         // animation clock, seconds
  float pawPhase = 0.0f;  // scratching / paw animation phase
  bool blink = false;
  uint8_t hat = 0, glasses = 0, neck = 0, body = 0;   // costume slots (Hat/Glasses/Neck/Body in chibi.h)
  float drop = 0;
  int8_t look = 0;        // sitting only: -1 / +1 = wide, dilated pupils staring off to the left / right
  Color silhouette = 0;   // non-zero: draw only the cat's shape in this one colour (a locked secret breed)
  int8_t backFrame = 0;   // POSE_BACK: 0 still, 1 ear twitch, 2 tail flick, 3 glancing back, 4..7 bird tracker
};

extern bool gShowTouch;               // debug: draw where touches land (+ raw tilt on the pour screen)
extern bool gShadowKey;                 // true while drawing over the keyed room (shadow = darken what is below)
constexpr Color KEY_COLOR = 0xF81F;      // transparent: shows the hi-res room
constexpr Color SHADOW_KEY = 0xF81E;     // darkens the room underneath

// (ox, oy) = ground contact point, centre of the cat.
int spriteRightExtent(const CatDraw& d);   // sprite cats only
CatMetrics drawCat(Canvas& cv, int ox, int oy, const CatDraw& d);
// How far the sprite for this look reaches left / right of its ground point, in pixels (both facings: the larger side).
int catReach(const CatDraw& d);
// How tall the sprite for this look is above its ground point, in pixels.
int catHeight(const CatDraw& d);
// Shrink d.size (never grow it) so the cat, sitting at ground y, keeps its ears at or below topMin. Sized from the
// sitting frame whatever the pose, so a pounce doesn't suddenly change the cat's size.
void fitCatHeight(CatDraw& d, int y, int topMin);

// Soft blob shadow on the floor.
void drawCatShadow(Canvas& cv, int ox, int oy, const CatDraw& d, Color floorColor);

}  // namespace cat
