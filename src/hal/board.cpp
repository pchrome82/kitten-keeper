// Display, touch, IMU, RTC, storage. Talks to the chips directly over I2C (no extra libraries
// besides Arduino_GFX for the QSPI display).
#include <Arduino.h>
#include <string.h>
#include <math.h>
#include <Wire.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <esp_sleep.h>
#include <Arduino_GFX_Library.h>
#include "board.h"
#include "hal.h"
#include "../game/compose.h"

namespace hal {

// ---------------------------------------------------------------------------------------
// I2C helpers
// ---------------------------------------------------------------------------------------
static bool wr(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}
static bool rd(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)addr, (int)n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

// ---------------------------------------------------------------------------------------
// display
// ---------------------------------------------------------------------------------------
static Arduino_DataBus* bus = nullptr;
static Arduino_CO5300* gfx = nullptr;   // V2 board display driver (the original board used SH8601)
static uint16_t* chunk = nullptr;        // DMA-capable scratch for the 2x upscaler
static constexpr int LW = 184, LH = 224;
static constexpr int ROWS = 16;          // logical rows per chunk -> 32 physical rows
#ifndef KK_TOUCH_LOG
#define KK_TOUCH_LOG 1   // print where each touch lands (Serial Monitor)
#endif

#ifndef KK_AUDIO
#define KK_AUDIO 1   // ES8311 speaker (set to 0 to build without sound)
#endif

static int lastBright = -1;   // -1 = never set, so the first request always goes through
static bool haveImu = false;
static uint8_t imuAddr = ADDR_QMI8658;
static bool haveRtc = false;
static bool haveAxp = false;

#if defined(KK_BOARD_AMOLED216)
// CST9220 touch: 16-bit command 0xD000 returns the touch report; it wants 0xD000 0xAB back as an acknowledgement.
static bool cst92Read(uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(ADDR_CST92XX);
  Wire.write(0xD0); Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((int)ADDR_CST92XX, (int)n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  Wire.beginTransmission(ADDR_CST92XX);
  Wire.write(0xD0); Wire.write(0x00); Wire.write(0xAB);
  Wire.endTransmission();
  return true;
}
#endif

// IMU: QMI8658 (A or C), accel only, +-4 g @ ~125 Hz. Looks at both possible addresses, resets the chip so it starts
// from a known state whatever the bootloader / a warm restart left behind, then checks that samples really arrive.
static bool imuInit(bool verbose) {
  static const uint8_t ADDRS[2] = {ADDR_QMI8658, 0x6A};
  haveImu = false;
  for (uint8_t a : ADDRS) {
    uint8_t id = 0, rev = 0;
    if (!rd(a, 0x00, &id, 1)) continue;
    rd(a, 0x01, &rev, 1);
    if (verbose) Serial.printf("[hal] imu at 0x%02X: who_am_i 0x%02X rev 0x%02X\n", a, id, rev);
    if (id != 0x05) continue;
    imuAddr = a;
    wr(a, 0x60, 0xB0);        // soft reset
    delay(30);
    wr(a, 0x02, 0x40);        // CTRL1: auto-increment register address, little-endian data (the 8658A defaults to big-endian)
    wr(a, 0x03, 0x16);        // CTRL2: accel +-4g, ODR code 6 (~125 Hz)
    wr(a, 0x08, 0x01);        // CTRL7: enable accelerometer
    delay(30);
    uint8_t r[6] = {0};
    const bool data = rd(a, 0x35, r, 6) && (r[0] | r[1] | r[2] | r[3] | r[4] | r[5]);
    if (verbose) Serial.printf("[hal] imu first sample: %d %d %d%s\n", (int16_t)(r[0] | (r[1] << 8)), (int16_t)(r[2] | (r[3] << 8)), (int16_t)(r[4] | (r[5] << 8)), data ? "" : "  (nothing yet)");
    haveImu = true;
    return true;
  }
  return false;
}

static void expanderReset() {
  // TCA9554: EXIO0..2 are reset lines for display / touch / peripherals. Pulse them low, then high.
  uint8_t cfg = 0xFF, out = 0xFF;
  if (!rd(ADDR_TCA9554, 0x03, &cfg, 1)) return;
  rd(ADDR_TCA9554, 0x01, &out, 1);
  wr(ADDR_TCA9554, 0x03, cfg & ~0x07);   // pins 0..2 -> outputs
  wr(ADDR_TCA9554, 0x01, out & ~0x07);   // low
  delay(20);
  wr(ADDR_TCA9554, 0x01, out | 0x07);    // high
  delay(50);
}

// Clock out 9 pulses on SCL and send a STOP so a slave left mid-transfer releases SDA.
static void i2cRecover() {
  pinMode(IIC_SDA, INPUT_PULLUP);
  pinMode(IIC_SCL, OUTPUT);
  for (int i = 0; i < 9; i++) {
    digitalWrite(IIC_SCL, LOW); delayMicroseconds(10);
    digitalWrite(IIC_SCL, HIGH); delayMicroseconds(10);
  }
  pinMode(IIC_SDA, OUTPUT);
  digitalWrite(IIC_SDA, LOW); delayMicroseconds(10);
  digitalWrite(IIC_SDA, HIGH); delayMicroseconds(10);
}

void begin() {
  i2cRecover();
  Wire.begin(IIC_SDA, IIC_SCL);
  Wire.setClock(400000);
  Wire.setTimeOut(20);
#if defined(KK_BOARD_AMOLED216)
  // touch (CST9220): its reset line is a GPIO on this board
  pinMode(TP_INT, INPUT);
  pinMode(TP_RESET, OUTPUT);
  digitalWrite(TP_RESET, LOW); delay(20);
  digitalWrite(TP_RESET, HIGH); delay(120);
  { uint8_t t[12]; Serial.println(cst92Read(t, sizeof(t)) ? "[hal] touch CST9220 ok" : "[hal] touch CST9220 not answering"); }
#else
  expanderReset();

  // touch (CST816): keep it awake so the first tap after a pause isn't swallowed, report while touched
  delay(50);
  if (wr(ADDR_CST816, 0xFE, 0x01)) Serial.println("[hal] touch CST816 ok");   // DisAutoSleep
  else Serial.println("[hal] touch CST816 not answering");
  wr(ADDR_CST816, 0xFA, 0x40);   // IrqCtl: periodic while touched
#endif

  // display
  bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
  gfx = new Arduino_CO5300(bus, LCD_RESET < 0 ? GFX_NOT_DEFINED : LCD_RESET /* 1.8": reset handled by the expander */, 0, LCD_WIDTH, LCD_HEIGHT, LCD_COL_OFFSET, 0, 0, 0);
  if (!gfx->begin()) Serial.println("[hal] gfx->begin() failed");
#if defined(KK_BOARD_AMOLED216)
  if (KK_LCD_MADCTL >= 0) { bus->beginWrite(); bus->writeC8D8(0x36, (uint8_t)KK_LCD_MADCTL); bus->endWrite(); }   // scan direction (see board.h)
#endif
  gfx->fillScreen(0x0000);
  gfx->setBrightness(0);
  chunk = (uint16_t*)heap_caps_malloc((size_t)LCD_WIDTH * ROWS * 2 * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!chunk) Serial.println("[hal] out of DMA memory for display chunk!");

  // what is on the I2C bus (handy when bringing up a new board)
  Serial.print("[hal] i2c devices:");
  for (uint8_t a = 0x08; a < 0x78; a++) { Wire.beginTransmission(a); if (Wire.endTransmission() == 0) Serial.printf(" 0x%02X", a); }
  Serial.println();

  imuInit(true);
  Serial.printf("[hal] imu %s\n", haveImu ? "ok" : "missing (will keep trying)");

  // Power manager AXP2101: turn on battery detection + voltage measuring so the fuel gauge works
  {
    uint8_t v = 0;
    if (rd(ADDR_AXP2101, 0x68, &v, 1)) { wr(ADDR_AXP2101, 0x68, v | 0x01); haveAxp = true; }
    if (haveAxp && rd(ADDR_AXP2101, 0x30, &v, 1)) wr(ADDR_AXP2101, 0x30, v | 0x01);
    if (haveAxp && rd(ADDR_AXP2101, 0x41, &v, 1)) wr(ADDR_AXP2101, 0x41, v | 0x08);   // PWR key short-press flag
    if (haveAxp) wr(ADDR_AXP2101, 0x49, 0x08);                                          // clear any stale press
    Serial.printf("[hal] power chip %s\n", haveAxp ? "ok" : "missing");
  }

  // RTC: PCF85063 - make sure it is running in 24h mode
  uint8_t c1 = 0;
  if (rd(ADDR_PCF85063, 0x00, &c1, 1)) {
    c1 &= ~((1 << 5) | (1 << 1));   // STOP = 0, 24h mode
    wr(ADDR_PCF85063, 0x00, c1);
    haveRtc = true;
  }
  Serial.printf("[hal] rtc %s\n", haveRtc ? "ok" : "missing");
#if KK_AUDIO
  audioBegin();
#else
  Serial.println("[hal] audio disabled (KK_AUDIO=0)");
#endif
}

static bool netUpFlag();         // (defined with the radio code below)
static bool panelOff = false;    // the panel itself is switched off (brightness 0 for a while): nothing lights, nothing is refreshed
static bool flipped = false;     // device held the other way up: draw rotated 180 degrees, mirror the touch
static bool saveMode = false;    // screen off: low-power mode
static uint32_t cpuNormalMhz = 0;

// What each strip of the panel last showed (a hash of its pixels), so a strip that hasn't changed isn't sent again.
// Sending a whole frame over the 40 MHz QSPI link takes ~17 ms or more; in most screens only a few strips change.
static constexpr int NSTRIPS = 224 / 16;
static uint32_t stripHash[NSTRIPS];
static bool stripKnown = false;   // false: send every strip on the next flush (first frame, panel back on, flipped)

static void panelOn() {
  if (!panelOff || !gfx) return;
  gfx->displayOn();
  panelOff = false;
  stripKnown = false;
}

void setBrightness(uint8_t level) {
  if (!gfx || level == lastBright) return;
  lastBright = level;
  if (level > 0) panelOn();
  gfx->setBrightness(level);
  if (level == 0 && !panelOff) { gfx->displayOff(); panelOff = true; }   // an AMOLED draws nothing when black, but its driver still burns a little
}

void setFlipped(bool f) { if (f != flipped) stripKnown = false; flipped = f; }

// IMU output rate: ~125 Hz awake, ~28 Hz with the screen off (still plenty to notice it being picked up)
static void imuRate(bool slow) { if (haveImu) wr(imuAddr, 0x03, slow ? 0x18 : 0x16); }

void setPowerSave(bool on) {
  if (on == saveMode) return;
  saveMode = on;
  if (!cpuNormalMhz) cpuNormalMhz = getCpuFrequencyMhz();
  imuRate(on);
  setCpuFrequencyMhz(on ? (netUpFlag() ? 160 : 80) : cpuNormalMhz);
}

void flush(const uint16_t* src, const uint16_t* room, bool keyed, int tr, int tg, int tb) {
  if (!gfx || !chunk) return;
  panelOn();               // (still black: the brightness comes after the picture)
  const int PW = LW * 2;   // 368
  for (int ly = 0; ly < LH; ly += ROWS) {
    // composite the pixel-art layer over the hi-res room (PSRAM) straight into the DMA strip
    cat::composeRows(src, room, keyed, tr, tg, tb, chunk, ly, ly + ROWS);
    {   // the same pixels as last time? Then the panel already shows them: skip the transfer
      const uint32_t* w = (const uint32_t*)chunk;
      const int nw = PW * ROWS * 2 / 2;
      uint32_t h = 2166136261u;
      for (int i = 0; i < nw; i++) h = (h ^ w[i]) * 16777619u;
      const int s = ly / ROWS;
      if (stripKnown && stripHash[s] == h) continue;
      stripHash[s] = h;
    }
    if (flipped) {   // 180 degrees: reverse this strip's pixels and put it in the mirrored place
      const int n = PW * ROWS * 2;
      for (int i = 0; i < n / 2; i++) { const uint16_t t = chunk[i]; chunk[i] = chunk[n - 1 - i]; chunk[n - 1 - i] = t; }
      gfx->draw16bitRGBBitmap(GAME_X, GAME_Y + (LH - ly - ROWS) * 2, chunk, PW, ROWS * 2);
    } else gfx->draw16bitRGBBitmap(GAME_X, GAME_Y + ly * 2, chunk, PW, ROWS * 2);
  }
  stripKnown = true;
}

// ---------------------------------------------------------------------------------------
// touch (FT3168): regs 0x02.. = finger count, X1H, X1L, Y1H, Y1L
// ---------------------------------------------------------------------------------------
// touch calibration (Settings > TOUCH FIX), stored in its own NVS namespace so a game reset keeps it
static float calAx = 1, calBx = 0, calAy = 1, calBy = 0;
static bool calLoaded = false;
static void loadTouchCal() {
  if (calLoaded) return;
  calLoaded = true;
  Preferences p;
  if (p.begin("kk_touch", true)) {
    calAx = p.getFloat("ax", 1); calBx = p.getFloat("bx", 0); calAy = p.getFloat("ay", 1); calBy = p.getFloat("by", 0);
    p.end();
  }
  if (!(calAx > 0.6f && calAx < 1.6f && calAy > 0.6f && calAy < 1.6f)) { calAx = calAy = 1; calBx = calBy = 0; }   // never trust junk
}

// Touch controllers read short of the true edge (and worst in the rounded corners): the outer 24 px are stretched
// 1.6x so a finger on the very rim lands on the rim.
static int edgeStretch(int v, int maxv) {
  const int Z = 24;
  if (v < Z) { v = Z - (Z - v) * 8 / 5; if (v < 0) v = 0; }
  else if (v > maxv - Z) { v = maxv - Z + (v - (maxv - Z)) * 8 / 5; if (v > maxv) v = maxv; }
  return v;
}

bool readTouch(int& x, int& y) {
  static int missed = 100;
  loadTouchCal();
  static int lx = 0, ly = 0;
#if defined(KK_BOARD_AMOLED216)
  // CST9220: report = [id<<4 | event, X bits 11..4, Y bits 11..4, X low<<4 | Y low, _, finger count, 0xAB ack, ...]
  uint8_t b[12] = {0};
  bool got = cst92Read(b, sizeof(b)) && b[6] == 0xAB && b[0] != 0xAB && (b[5] & 0x7F) > 0 && (b[0] & 0x0F) == 0x06;
  if (got) {
    int rx = (b[1] << 4) | (b[3] >> 4), ry = (b[2] << 4) | (b[3] & 0x0F);   // panel coords, 0..479
    if (KK_TOUCH_SWAP_XY) { const int t = rx; rx = ry; ry = t; }
    if (KK_TOUCH_MIRROR_X) rx = LCD_WIDTH - 1 - rx;
    if (KK_TOUCH_MIRROR_Y) ry = LCD_HEIGHT - 1 - ry;
#if KK_TOUCH_LOG
    if (missed > 2) Serial.printf("[touch] panel %d,%d\n", rx, ry);
#endif
    lx = rx - GAME_X; ly = ry - GAME_Y;   // into the game picture (it sits in the middle of the panel)
#else
  uint8_t b[5];
  bool got = rd(ADDR_CST816, 0x02, b, 5);   // CST816: finger count, then X/Y high+low (same layout as FT3168)
  if (got && (b[0] & 0x0F) > 0) {
    lx = ((b[1] & 0x0F) << 8) | b[2];
    ly = ((b[3] & 0x0F) << 8) | b[4];
#endif
#if KK_TOUCH_LOG
    if (missed > 2) Serial.printf("[touch] down at %d,%d\n", lx, ly);   // raw controller coords, for calibration
#endif
    { const int cx = (int)(lx * calAx + calBx + 0.5f), cy = (int)(ly * calAy + calBy + 0.5f);   // apply the calibration
      lx = cx < 0 ? 0 : cx; ly = cy < 0 ? 0 : cy; }
    if (lx > 367) lx = 367;
    if (ly > 447) ly = 447;
#if !defined(KK_BOARD_AMOLED216)   // (on the 2.16" the game picture stops short of the glass edge, so nothing to stretch)
    lx = edgeStretch(lx, 367); ly = edgeStretch(ly, 447);   // the controller under-reads in the corners and along the rim: reach the edge
#endif
    missed = 0;
  } else {
    if (missed < 100) missed++;
    if (missed > 2) return false;   // 2-frame release debounce: the controller sometimes drops a sample
  }
  x = flipped ? 367 - lx : lx;
  y = flipped ? 447 - ly : ly;
  return true;
}

// ---------------------------------------------------------------------------------------
// EspPlatform
// ---------------------------------------------------------------------------------------
static uint8_t bcd2bin(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }
static uint8_t bin2bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

uint32_t EspPlatform::millis() { return ::millis(); }

bool EspPlatform::rtcGet(cat::DateTime& dt) {
  if (!haveRtc) return false;
  uint8_t r[7];
  if (!rd(ADDR_PCF85063, 0x04, r, 7)) return false;
  if (r[0] & 0x80) return false;   // oscillator-stop flag: time is not trustworthy
  dt.second = bcd2bin(r[0] & 0x7F);
  dt.minute = bcd2bin(r[1] & 0x7F);
  dt.hour = bcd2bin(r[2] & 0x3F);
  dt.day = bcd2bin(r[3] & 0x3F);
  dt.month = bcd2bin(r[5] & 0x1F);
  dt.year = (uint16_t)(2000 + bcd2bin(r[6]));
  if (dt.year < 2025 || dt.month < 1 || dt.month > 12 || dt.day < 1 || dt.day > 31 || dt.hour > 23 || dt.minute > 59) return false;
  return true;
}

void EspPlatform::rtcSet(const cat::DateTime& dt) {
  if (!haveRtc) return;
  uint32_t days = cat::toEpoch(dt) / 86400u;       // 2000-01-01 was a Saturday (=6)
  uint8_t wd = (uint8_t)((days + 6) % 7);
  Wire.beginTransmission(ADDR_PCF85063);
  Wire.write(0x04);
  Wire.write(bin2bcd(dt.second) & 0x7F);           // also clears the oscillator-stop flag
  Wire.write(bin2bcd(dt.minute));
  Wire.write(bin2bcd(dt.hour));
  Wire.write(bin2bcd(dt.day));
  Wire.write(wd);
  Wire.write(bin2bcd(dt.month));
  Wire.write(bin2bcd((uint8_t)(dt.year - 2000)));
  Wire.endTransmission();
}

// The BOOT button (GPIO0, active low): one event per press.
static bool bootPressed() {
  static bool pinReady = false, bootWas = false;
  if (!pinReady) { pinMode(0, INPUT_PULLUP); pinReady = true; }
  const bool boot = digitalRead(0) == LOW;
  const bool pressed = boot && !bootWas;
  bootWas = boot;
  return pressed;
}

// BOOT mutes / unmutes. (Without a power chip there's no PWR key: BOOT stays the screen button, see below.)
bool EspPlatform::muteButton() { return haveAxp && bootPressed(); }

// PWR key (via the AXP2101's short-press flag): one event per press. No power chip: the BOOT button instead.
bool EspPlatform::powerButton() {
  static uint32_t lastPoll = 0;
  bool pressed = !haveAxp && bootPressed();
  if (haveAxp && ::millis() - lastPoll > 80) {   // don't hammer the I2C bus
    lastPoll = ::millis();
    uint8_t st = 0;
    if (rd(ADDR_AXP2101, 0x49, &st, 1) && (st & 0x08)) { wr(ADDR_AXP2101, 0x49, 0x08); pressed = true; }
  }
  return pressed;
}

bool EspPlatform::battery(int& percent, bool& charging) {
  if (!haveAxp) return false;
  uint8_t st1 = 0, st2 = 0, pct = 0;
  if (!rd(ADDR_AXP2101, 0x00, &st1, 1) || !(st1 & 0x08)) return false;   // no battery connected
  if (!rd(ADDR_AXP2101, 0xA4, &pct, 1)) return false;
  rd(ADDR_AXP2101, 0x01, &st2, 1);
  percent = pct > 100 ? 100 : pct;
  charging = (st2 >> 5) == 0x01;
  return true;
}

bool EspPlatform::imuRead(float& ax, float& ay, float& az) {
  // Not found at boot, or it has stopped giving samples (all zeros / not answering): set it up again every 2 s.
  static uint32_t lastTry = 0;
  static int dead = 0;
  if (!haveImu || dead > 30) {
    if (::millis() - lastTry < 2000) return false;
    lastTry = ::millis();
    const bool was = haveImu;
    if (!imuInit(false)) return false;
    if (!was) Serial.println("[hal] imu found");
    dead = 0;
  }
  uint8_t r[6];
  if (!rd(imuAddr, 0x35, r, 6)) { dead++; return false; }
  int16_t x = (int16_t)(r[0] | (r[1] << 8));
  int16_t y = (int16_t)(r[2] | (r[3] << 8));
  int16_t z = (int16_t)(r[4] | (r[5] << 8));
  if (!x && !y && !z) { dead++; return false; }   // the accelerometer isn't running (gravity always shows on some axis)
  dead = 0;
  const float k = 4.0f / 32768.0f;
  ax = x * k; ay = y * k; az = z * k;
  return true;
}

static Preferences prefs;

bool EspPlatform::loadSave(void* buf, uint32_t len) {
  if (!prefs.begin("kitten", true)) return false;
  size_t have = prefs.getBytesLength("save");
  bool ok = false;
  if (have == len) ok = prefs.getBytes("save", buf, len) == len;
  prefs.end();
  return ok;
}

bool EspPlatform::storeSave(const void* buf, uint32_t len) {
  if (!prefs.begin("kitten", false)) return false;
  bool ok = prefs.putBytes("save", buf, len) == len;
  prefs.end();
  return ok;
}

// the backup copy lives under its own key (NVS writes each key separately, so a cut write can only hurt one)
bool EspPlatform::loadSaveBackup(void* buf, uint32_t len) {
  if (!prefs.begin("kitten", true)) return false;
  size_t have = prefs.getBytesLength("backup");
  bool ok = false;
  if (have == len) ok = prefs.getBytes("backup", buf, len) == len;
  prefs.end();
  return ok;
}

bool EspPlatform::storeSaveBackup(const void* buf, uint32_t len) {
  if (!prefs.begin("kitten", false)) return false;
  bool ok = prefs.putBytes("backup", buf, len) == len;
  prefs.end();
  return ok;
}

void EspPlatform::playSound(cat::Sound s, uint8_t volume) { audioPlay(s, volume); }
void EspPlatform::setBrightness(uint8_t level) { hal::setBrightness(level); }
uint32_t EspPlatform::randomSeed() { return esp_random(); }

// ---- Explore Together: ESP-NOW broadcast on Wi-Fi channel 1 (no access point, no pairing) ----
// The radio is only switched on while the lobby or a match is open, then off again to save the battery.
namespace {
constexpr int NET_Q = 12, NET_PKT = 200;
struct NetSlot { uint8_t len; uint8_t data[NET_PKT]; };
NetSlot netQ[NET_Q];
volatile int netHead = 0, netTail = 0;               // written by the Wi-Fi task (head) and the game loop (tail)
portMUX_TYPE netMux = portMUX_INITIALIZER_UNLOCKED;
bool netUp = false;
const uint8_t kBroadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void onNetRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  (void)info;
  if (len <= 0 || len > NET_PKT) return;
  portENTER_CRITICAL(&netMux);
  const int next = (netHead + 1) % NET_Q;
  if (next != netTail) {                              // queue full: drop the packet (the game resends what matters)
    netQ[netHead].len = (uint8_t)len;
    memcpy(netQ[netHead].data, data, len);
    netHead = next;
  }
  portEXIT_CRITICAL(&netMux);
}
}  // namespace

static bool netUpFlag() { return netUp; }

bool EspPlatform::touchCalDone() {
  Preferences p;
  bool done = false;
  if (p.begin("kk_touch", true)) { done = p.isKey("ax"); p.end(); }
  return done;
}

bool EspPlatform::touchCalGet(float& ax, float& bx, float& ay, float& by) {
  loadTouchCal();
  ax = calAx; bx = calBx; ay = calAy; by = calBy;
  return true;
}

void EspPlatform::touchCalSet(float ax, float bx, float ay, float by) {
  calAx = ax; calBx = bx; calAy = ay; calBy = by; calLoaded = true;
  Preferences p;
  if (p.begin("kk_touch", false)) { p.putFloat("ax", ax); p.putFloat("bx", bx); p.putFloat("ay", ay); p.putFloat("by", by); p.end(); }
}

void EspPlatform::setFlipped(bool f) { hal::setFlipped(f); }

int EspPlatform::orientGet() {
  Preferences p;
  int v = 0;
  if (p.begin("kk_touch", true)) { v = p.getInt("ori", 0); p.end(); }
  return v;
}
void EspPlatform::orientSet(int v) {
  Preferences p;
  if (p.begin("kk_touch", false)) { p.putInt("ori", v); p.end(); }
}

#ifndef KK_LIGHT_SLEEP
#define KK_LIGHT_SLEEP 1   // screen off, on battery: sleep between the 10 Hz checks (the USB link drops while asleep, so it is skipped on USB power)
#endif
bool audioBusy();

// USB / charger present? (AXP2101 status 0: bit 5 = VBUS good). Checked about once a second.
static bool vbusPresent() {
  static uint32_t last = 0;
  static bool v = true;   // unknown -> assume yes (don't sleep)
  if (::millis() - last > 1000 || last == 0) {
    last = ::millis();
    uint8_t st = 0;
    if (haveAxp && rd(ADDR_AXP2101, 0x00, &st, 1)) v = (st & 0x20) != 0; else v = true;
  }
  return v;
}

void idleWait(uint32_t ms) {
#if KK_LIGHT_SLEEP
  if (saveMode && !netUp && !audioBusy() && !vbusPresent() && ms >= 20) {
    esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
    esp_light_sleep_start();
    return;
  }
#endif
  delay(ms);
}

bool EspPlatform::netOpen() {
  if (netUp) return true;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) { WiFi.mode(WIFI_OFF); return false; }
  esp_now_register_recv_cb(onNetRecv);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, kBroadcast, 6);
  peer.channel = 1;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  if (!esp_now_is_peer_exist(kBroadcast) && esp_now_add_peer(&peer) != ESP_OK) { esp_now_deinit(); WiFi.mode(WIFI_OFF); return false; }
  netHead = 0;
  netTail = 0;
  netUp = true;
  return true;
}

void EspPlatform::netClose() {
  if (!netUp) return;
  esp_now_unregister_recv_cb();
  esp_now_deinit();
  WiFi.mode(WIFI_OFF);
  netUp = false;
}

bool EspPlatform::netSend(const void* data, int len) {
  if (!netUp || len <= 0 || len > NET_PKT) return false;
  return esp_now_send(kBroadcast, (const uint8_t*)data, len) == ESP_OK;
}

int EspPlatform::netRecv(void* buf, int max) {
  if (!netUp) return 0;
  int len = 0;
  portENTER_CRITICAL(&netMux);
  if (netTail != netHead) {
    len = netQ[netTail].len;
    if (len > max) len = max;
    memcpy(buf, netQ[netTail].data, len);
    netTail = (netTail + 1) % NET_Q;
  }
  portEXIT_CRITICAL(&netMux);
  return len;
}

uint32_t EspPlatform::netId() {
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  return ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16) | ((uint32_t)mac[4] << 8) | mac[5];
}

}  // namespace hal
