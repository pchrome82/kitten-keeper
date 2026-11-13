// Final frame assembly: 184x224 logical layer (pixel art, UI) over the 368x448 hi-res room.
#pragma once
#include "canvas.h"
namespace cat {
// out = 368x448 RGB565. Where `keyed`, logical pixels equal to KEY_COLOR show the room; SHADOW_KEY
// darkens it. Non-key pixels in the room band get the night tint (percent per channel).
// Same, for logical rows y0..y1 only; out receives (y1-y0)*2 physical rows (used for strip-wise DMA flushes).
void composeRows(const uint16_t* logical, const uint16_t* room, bool keyed, int tintR, int tintG, int tintB, uint16_t* out, int y0, int y1);
// While a popup sits over the home (FEED, PLAY, ...), the room behind it is dimmed to this percent (100 = as is).
extern int gRoomDim;
void composeFrame(const uint16_t* logical, const uint16_t* room, bool keyed, int tintR, int tintG, int tintB, uint16_t* out);
}
