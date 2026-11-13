// Baked pixel-art cat sprites (see art/bake.py). Frames are RLE 4-bit role ids resolved through a per-breed palette.
#pragma once
#include "canvas.h"

namespace cat {

struct SpriteFrame { uint32_t off; uint8_t w, h, ax, ay, ex2, ec2, ew; };   // ax, ay = ground contact point inside the frame;
// ex2 = twice the x of the face's mirror line (0 = no eyes), ec2 = twice the x of the right eye's centre, ew = eye width

enum SpriteBody : uint8_t { SB_SKINNY, SB_NORMAL, SB_CHONK };
enum SpriteAnim : uint8_t { SA_SIT, SA_WALK, SA_SLEEP, SA_EAT, SA_SCRATCH, SA_BACK };   // SA_BACK: sitting, facing away

constexpr int SPRITE_SETS = 7, SPRITE_BODIES = 3, SPRITE_NF = 24;   // frames per (set, body): sit3 walk4 sleep2 eat2 scratch5 back8
constexpr int SPRITE_ANIMS = 6;

extern const uint16_t CAT_FRAMES_N;
extern const SpriteFrame CAT_FRAMES[];
extern const uint16_t CAT_PALS[][16];
extern const uint8_t CAT_BREED_SET[];
extern const uint8_t CAT_DATA[];

const SpriteFrame& spriteFrame(int set, int body, int anim, int idx);
int spriteAnimFrames(int anim);
// Draw a frame with its ground point at (ox, oy). scale < 1 shrinks (nearest). Frames face right; flip mirrors.
// Returns the drawn bounds in `b` = {left, top, right, bottom}.
// rainbowT >= 0: the Rainbow Cat's fur is recoloured in soft rainbow bands that drift with time rainbowT.
// flat != 0: every pixel is drawn in that one colour (for glows / silhouettes).
void drawSprite(Canvas& cv, const SpriteFrame& f, const uint16_t* pal, int ox, int oy, bool flip, float scale, int* b,
                float rainbowT = -1.0f, Color flat = 0);
// The eye pixels (iris = role 10, pupil = role 11) the last drawSprite call with bounds `b` put down, so costumes and
// the stare can find the eyes without mistaking a same-coloured thing behind the cat (a crown, a toy) for one.
struct SpriteEyePts { int n; int16_t x[640], y[640]; uint8_t role[640]; };   // (room for a chonk drawn at 1.7x on the alarm)
extern SpriteEyePts gSpriteEyes;

}  // namespace cat
