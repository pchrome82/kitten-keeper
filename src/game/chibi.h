// Shared cat vocabulary: species ids and costume slots (the cats themselves are baked pixel-art sprites, see catsprites.h).
#pragma once
#include "canvas.h"

namespace cat {

enum Species : uint8_t { SP_TABBY, SP_SIAMESE, SP_TUXEDO, SP_VOID, SP_CALICO, SP_SPHYNX, SP_SKOGKATT, SP_KITTEN, SP_RAINBOW, SP_HAMSTER, SP_COUNT };   // = breed ids
// Costume ids are saved (as bits of the owned masks and as the worn id): append only, at most 7 per slot.
// The ones marked (gift) are never sold: they come out of gift boxes.
enum Hat : uint8_t { HAT_NONE, HAT_TOP, HAT_PARTY, HAT_CROWN, HAT_WIZARD, HAT_SANTA, HAT_FLOWER, HAT_COWBOY /* gift */, HAT_COUNT };
constexpr uint8_t HAT_PUMPKIN = 40;   // a seasonal hat (not in the wardrobe): Halloween
enum Glasses : uint8_t { GL_NONE, GL_SUN, GL_HEART, GL_STAR /* gift */, GL_MONOCLE /* gift */, GL_3D /* gift */, GL_COUNT };
enum Neck : uint8_t { NK_NONE, NK_BOW, NK_SCARF, NK_BELL, NK_BANDANA /* gift */, NK_PEARLS /* gift */, NK_TIE /* gift */, NK_COUNT };   // NK_BELL = bell collar
enum Body : uint8_t { BD_NONE, BD_CAPE, BD_SADDLE, BD_WINGS /* gift */, BD_COUNT };   // BD_SADDLE comes with a little hamster rider

struct CatMetrics { int headX, headY, topY, halfWidth; int eyeX = -1, eyeY = -1; };   // eyeX/Y: middle of the eyes (-1: none showing)
const char* speciesName(int sp);

}  // namespace cat
