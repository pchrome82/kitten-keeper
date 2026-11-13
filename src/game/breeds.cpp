#include "breeds.h"

namespace cat {

// Order MUST match Species in chibi.h (TABBY, SIAMESE, TUXEDO, VOID, CALICO, SPHYNX, SKOGKATT).
const Breed BREEDS[NUM_BREED_SLOTS] = {
  // name       coat                 coat2                belly                earIn                pat         speed  str    gain   hair  price blurb
  {"Tabby",      rgb(236, 146, 62),   rgb(182, 88, 34),    rgb(252, 216, 166),  rgb(244, 160, 150),  PAT_TABBY,  1.00f, 1.00f, 1.00f, 1.00f,  10, "Classic. Loud. Orange."},
  {"Siamese",    rgb(240, 224, 190),  rgb(100, 70, 56),    rgb(250, 240, 218),  rgb(150, 100, 90),   PAT_POINTS, 1.20f, 0.90f, 0.95f, 0.80f,  10, "Fast and chatty."},
  {"Tuxedo",     rgb(46, 46, 60),     rgb(246, 246, 250),  rgb(246, 246, 250),  rgb(240, 160, 160),  PAT_TUX,    0.90f, 1.00f, 1.00f, 1.10f,  10, "Dressed for dinner."},
  {"Void Cat",   rgb(52, 52, 68),     rgb(36, 36, 50),     rgb(76, 76, 96),     rgb(150, 100, 120),  PAT_SOLID,  1.10f, 0.90f, 1.00f, 1.00f,  10, "Stealthy. Stares."},
  {"Calico",     rgb(246, 240, 232),  rgb(228, 140, 60),   rgb(250, 248, 244),  rgb(244, 170, 170),  PAT_CALICO, 1.00f, 1.00f, 1.00f, 1.10f,  10, "Patches of attitude."},
  {"Sphynx",     rgb(232, 182, 164),  rgb(206, 146, 134),  rgb(244, 204, 188),  rgb(240, 150, 150),  PAT_SPHYNX, 1.30f, 1.00f, 0.80f, 0.00f,  10, "Hairless, light & quick."},
  {"Skogkatt", rgb(156, 114, 82),   rgb(90, 62, 46),     rgb(204, 172, 132),  rgb(214, 150, 140),  PAT_TABBY,  0.80f, 1.10f, 0.90f, 1.20f,  10, "Big floof. Hits hard."},
  // 7: the old generic kitten ("let it evolve" is gone; kept so saves keep their numbering)
  {"Kitten",     rgb(216, 204, 192),  rgb(180, 166, 152),  rgb(242, 234, 224),  rgb(240, 176, 176),  PAT_SOLID,  1.00f, 1.00f, 1.00f, 1.00f,   0, "Still deciding."},
  // 8, 9: SECRET breeds, never sold: a Black Hole ending reveals the Rainbow Cat, a Sun ending the Hamster
  {"Rainbow Cat", rgb(244, 236, 255),  rgb(216, 200, 244),  rgb(255, 255, 255),  rgb(255, 184, 224),  PAT_SOLID,  1.00f, 1.30f, 1.00f, 1.00f,   0, "Born of a black hole. Mighty."},
  {"Hamster",    rgb(242, 182, 96),   rgb(208, 140, 60),   rgb(255, 246, 232),  rgb(255, 168, 184),  PAT_SOLID,  1.50f, 1.10f, 0.60f, 1.00f,   0, "Tiny sunbeam. Zoom zoom!"},
};

}  // namespace cat
