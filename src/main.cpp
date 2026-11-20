// Kitten Keeper - ESP32-S3 Touch AMOLED 1.8" firmware entry point.
#include <Arduino.h>
#include "hal/hal.h"
#include "game/game.h"
#include "game/catart.h"

static uint16_t fb[cat::W * cat::H];
static cat::Canvas canvas(fb);
static hal::EspPlatform platform;
static cat::Game game(&platform);

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n[boot] Kitten Keeper v1.4.0");
  cat::gShowTouch = false;   // true = debug rings where touches land + raw tilt numbers on the pour screen   // baked pixel-art cats (same look as the web build)
  hal::begin();
  game.begin();
  canvas.clear(0);
  game.frame(canvas, false, 0, 0);
  hal::flush(fb, game.roomPixels(), game.fi.keyed, game.fi.tintR, game.fi.tintG, game.fi.tintB);
  hal::setBrightness(game.brightness());
}

void loop() {
  const uint32_t t0 = millis();
  int tx = 0, ty = 0;
  bool down = hal::readTouch(tx, ty);
  bool refresh = game.frame(canvas, down, tx, ty);
  if (refresh) hal::flush(fb, game.roomPixels(), game.fi.keyed, game.fi.tintR, game.fi.tintG, game.fi.tintB);                 // flush first, then brightness: no flash of stale image on wake
  hal::setBrightness(game.brightness());
  const bool on = game.displayOn();
  hal::setPowerSave(!on);                                // screen off: slow CPU, slow sensor, light sleep between checks
  const uint32_t target = on ? (game.fastFrames() ? 16 : 33) : 100;   // ~60 fps in the action games, ~30 fps otherwise, 10 Hz while the screen is off
  const uint32_t el = millis() - t0;
  if (el < target) { if (on) delay(target - el); else hal::idleWait(target - el); }
}
