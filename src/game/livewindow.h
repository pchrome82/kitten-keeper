// The one live part of the room: what is outside the window (sky, sun/moon, stars, clouds, city, birds, rain).
#pragma once
#include <stdint.h>
namespace cat {
// room = the 368x448 hi-res buffer. hour 0..24, t = seconds (animation clock), weather 0 clear / 1 overcast / 2 rain / 3 snow.
// roomId picks the room's glass shape and view (city / sea / forest).
void drawLiveWindow(uint16_t* room, int roomId, float hour, float t, int weather);
// deterministic per-day weather
int weatherForDay(uint32_t epochDay);
}  // namespace cat
