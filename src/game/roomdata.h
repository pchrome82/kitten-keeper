// Baked rooms (GENERATED data in roomdata.cpp by sim/bakeroom.cpp). 368x316 RGB565, physical rows 52..367.
#pragma once
#include <stdint.h>
namespace cat {
constexpr int ROOM_W = 368, ROOM_H = 316, ROOM_TOP = 52;
struct GlassSpan { uint16_t y, x0, x1; };       // inclusive x range of window-glass pixels on band row y
struct RoomImg {
  const uint16_t* day;      // noon lighting
  const uint16_t* night;    // 11 pm lighting (blended with day by the clock)
  const GlassSpan* glass;   // live-window pixels
  uint16_t glassN;
  uint8_t view;             // what the window shows: 0 city, 1 sea, 2 forest, 3 river through the forest, 4 deep space
  struct Sprite {           // the little table by the window (with its glass of water): not in the pictures above,
    const uint16_t* day;    // drawn over them unless a game hides it. 0xF81F = see-through. day/night as above.
    const uint16_t* night;
    int16_t x, y, w, h;     // where it stands, in room-picture coords (w 0 = this room has no table)
  } table;
};
constexpr int NUM_ROOMS = 7;   // room 6: the secret Halo Bridge (both secret cats unlock it)   // 0 = the starter home (Grimy Cartoon); shop order = price order
extern const RoomImg ROOMS[NUM_ROOMS];
}  // namespace cat
