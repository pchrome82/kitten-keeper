// Painterly perspective room backgrounds (the empty starter house: big window + table with a glass of water).
#pragma once
#include "../src/game/canvas.h"

namespace cat {
constexpr int NUM_ROOM_STYLES = 7;
const char* roomStyleName(int s);
// Paints physical rows 52..367 of a 368x448 canvas for the given style and clock hour (0..24).
void renderStyledRoom(Canvas& hi, int style, float hour);
// Shipped-game bake: empty-window room (glass pixels = 0xF81F key), no sunbeam. glassMask (368*316 bytes) gets 1 on glass.
extern bool gNoTable;   // true: the rooms are painted without the little table and its glass of water
void renderStyledRoomBaked(Canvas& hi, int style, float hour, uint8_t* glassMask);
}  // namespace cat
