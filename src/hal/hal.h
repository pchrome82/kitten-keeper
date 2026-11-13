#pragma once
#include <stdint.h>
#include "../game/platform.h"

namespace hal {

void begin();                                   // bring up every peripheral
bool readTouch(int& x, int& y);                 // physical coords 0..367 x 0..447; false = finger up
void flush(const uint16_t* canvas184x224, const uint16_t* room368x448, bool keyed, int tintR, int tintG, int tintB);   // compose + 2x upscale to the AMOLED
void setBrightness(uint8_t level);              // 0..255, only sends on change (0 also switches the panel itself off)
void setFlipped(bool flipped);                  // draw upside down and mirror the touch (device held the other way up)
void setPowerSave(bool on);                     // screen is off: slow the CPU and the sensors right down
void idleWait(uint32_t ms);                     // wait, in light sleep when nothing needs the CPU (screen off, no radio, no sound)
bool audioBusy();                               // a sound is playing

// audio (runs on its own task)
void audioBegin();
void audioPlay(cat::Sound s, uint8_t volume);

// The cat::Platform implementation used by the firmware.
class EspPlatform : public cat::Platform {
 public:
  uint32_t millis() override;
  bool rtcGet(cat::DateTime& dt) override;
  void rtcSet(const cat::DateTime& dt) override;
  bool imuRead(float& ax, float& ay, float& az) override;
  bool loadSave(void* buf, uint32_t len) override;
  bool storeSave(const void* buf, uint32_t len) override;
  bool loadSaveBackup(void* buf, uint32_t len) override;
  bool storeSaveBackup(const void* buf, uint32_t len) override;
  void playSound(cat::Sound s, uint8_t volume) override;
  void setBrightness(uint8_t level) override;
  uint32_t randomSeed() override;
  bool battery(int& percent, bool& charging) override;
  bool powerButton() override;
  bool netOpen() override;
  void netClose() override;
  bool netSend(const void* data, int len) override;
  int netRecv(void* buf, int max) override;
  uint32_t netId() override;
  bool touchCalGet(float& ax, float& bx, float& ay, float& by) override;
  void touchCalSet(float ax, float bx, float ay, float by) override;
  bool touchCalDone() override;
  void setFlipped(bool flipped) override;
  int orientGet() override;
  void orientSet(int v) override;
};

}  // namespace hal
