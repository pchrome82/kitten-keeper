// Everything the portable game core needs from the outside world.
// The ESP32 firmware (src/hal) and the PC simulator (sim/) each implement this.
#pragma once
#include <stdint.h>

namespace cat {

enum Sound : uint8_t {
  SND_TAP, SND_MEOW_SOFT, SND_MEOW, SND_MEOW_LOUD, SND_PURR, SND_EAT, SND_COIN,
  SND_SCOOP, SND_HACK, SND_POUNCE, SND_WIN, SND_LOSE, SND_SAD, SND_HOLE,
  SND_ALARM, SND_SCRATCH, SND_YAWN, SND_POOF, SND_HISS, SND_KIBBLE_BOWL, SND_KIBBLE_FLOOR, SND_BEAT, SND_MISS, SND_BLOOP, SND_SQUISH, SND_POP, SND_SPLASH, SND_BACK, SND_ALERT, SND_CHIME, SND_HIT_GOOD, SND_HIT_BAD, SND_COUNT
};

struct DateTime {
  uint16_t year;   // 2000..2099
  uint8_t month;   // 1..12
  uint8_t day;     // 1..31
  uint8_t hour;    // 0..23
  uint8_t minute;
  uint8_t second;
};

class Platform {
 public:
  virtual ~Platform() {}
  virtual uint32_t millis() = 0;
  // Battery: percent 0..100 and whether it's charging. Returns false if there's no battery info.
  virtual bool battery(int& percent, bool& charging) { (void)percent; (void)charging; return false; }
  // True once per press of the device's PWR button (screen on/off).
  virtual bool powerButton() { return false; }
  // True once per press of the device's BOOT button (sound off / back on).
  virtual bool muteButton() { return false; }
  // Short-range link to other Kitten Keepers nearby, for Explore Together (ESP-NOW broadcast on the board:
  // no router, no pairing). Packets are small (<= 200 bytes) and may be lost; the game copes with that.
  // netOpen() false = no radio here (the PC simulator and the web page).
  virtual bool netOpen() { return false; }
  virtual void netClose() {}
  virtual bool netSend(const void* data, int len) { (void)data; (void)len; return false; }   // to everyone in range
  virtual int netRecv(void* buf, int max) { (void)buf; (void)max; return 0; }              // one waiting packet, 0 = none
  virtual uint32_t netId() { return 0; }
  // Touch calibration, kept by the device itself (not in the game save): true touch = a * raw + b on each axis,
  // in physical pixels (0..367 x 0..447). touchCalGet false = this platform has no touch to calibrate.
  virtual bool touchCalGet(float& ax, float& bx, float& ay, float& by) { ax = ay = 1; bx = by = 0; return false; }
  virtual void touchCalSet(float ax, float bx, float ay, float by) { (void)ax; (void)bx; (void)ay; (void)by; }
  virtual bool touchCalDone() { return true; }   // false = never calibrated on this device: the game asks once                                                     // this device's id (from its MAC)
  // Battery-backed clock. Returns false if the clock lost power / was never set.
  virtual bool rtcGet(DateTime& dt) = 0;
  virtual void rtcSet(const DateTime& dt) = 0;
  // Accelerometer in g. Returns false if no fresh sample.
  virtual bool imuRead(float& ax, float& ay, float& az) = 0;
  // Persistent storage of one blob.
  virtual bool loadSave(void* buf, uint32_t len) = 0;   // true only if exactly len bytes were read
  virtual bool storeSave(const void* buf, uint32_t len) = 0;
  // A second copy, written now and then, in case the main one is ever lost (power cut mid-write).
  virtual bool loadSaveBackup(void* buf, uint32_t len) { (void)buf; (void)len; return false; }
  virtual bool storeSaveBackup(const void* buf, uint32_t len) { (void)buf; (void)len; return false; }
  // Screen orientation: the board draws everything upside down (and flips the touch) when asked. orientGet/Set keep what the
  // game has learned about how the device is normally held (kept by the device itself, like the touch calibration).
  virtual void setFlipped(bool flipped) { (void)flipped; }
  virtual int orientGet() { return 0; }
  virtual void orientSet(int v) { (void)v; }
  virtual void playSound(Sound s, uint8_t volume) = 0;  // volume 1..3 (0 is never sent)
  virtual void setBrightness(uint8_t level) = 0;        // 0 (off) .. 255
  virtual uint32_t randomSeed() = 0;
};

// Seconds since 2000-01-01 00:00:00 (local wall-clock time; no time zones needed).
uint32_t toEpoch(const DateTime& dt);
DateTime fromEpoch(uint32_t e);

}  // namespace cat
