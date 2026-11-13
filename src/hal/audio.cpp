// Synthesised cat sounds -> ES8311 codec over I2S (16 kHz, 16-bit stereo).
// Runs in its own FreeRTOS task so the game never waits on audio.
#include <Arduino.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <math.h>
#include "board.h"
#include "hal.h"

extern "C" {
#include "es8311/es8311.h"
}

namespace hal {

static I2SClass i2s;
static QueueHandle_t q = nullptr;
static bool ready = false;
static volatile bool busy = false;   // a sound is playing (the light sleep in main must wait)
static constexpr int SR = 16000;

struct Note {
  float f0, f1;      // start / end frequency (Hz); 0 with wave NOISE = unpitched
  uint16_t ms;       // duration; a note with f0 < 0 is a rest
  uint8_t wave;      // 0 sine, 1 square, 2 triangle, 3 noise, 4 meow (formant-ish), 5 wobble (rubbery sine)
  float amp;
  float vib;         // vibrato depth (fraction of frequency): 6 Hz, or 30 Hz for WOB
};
enum { SIN, SQR, TRI, NOI, MEW, WOB };

#define N(...) {__VA_ARGS__}
#define REST(ms) N(-1, -1, ms, SIN, 0, 0)
// The sound palette: short, soft and cute - pips, bloops, squishes and boings. WOB = a rubbery sine (fast pitch wobble).
static const Note S_TAP[] = {N(1100, 1500, 30, SIN, 0.45f, 0)};                                                       // "pip"
static const Note S_MEOW_SOFT[] = {N(700, 1000, 90, MEW, 0.35f, 0.02f), N(1000, 620, 170, MEW, 0.35f, 0.04f)};
static const Note S_MEOW[] = {N(600, 1050, 120, MEW, 0.6f, 0.02f), N(1050, 520, 300, MEW, 0.6f, 0.05f)};
static const Note S_MEOW_LOUD[] = {N(520, 1000, 170, MEW, 0.95f, 0.02f), N(1000, 420, 520, MEW, 0.95f, 0.06f), REST(120),
                                   N(500, 900, 120, MEW, 0.9f, 0.02f), N(900, 360, 380, MEW, 0.9f, 0.06f)};
static const Note S_PURR[] = {N(70, 70, 700, NOI, 0.35f, 0)};
static const Note S_EAT[] = {N(300, 480, 55, SIN, 0.55f, 0), REST(40), N(280, 460, 55, SIN, 0.55f, 0), REST(40), N(320, 520, 70, SIN, 0.55f, 0)};   // "nom nom nom"
static const Note S_COIN[] = {N(1320, 1400, 50, SIN, 0.5f, 0), N(1760, 1900, 140, SIN, 0.5f, 0)};                    // "bling!"
static const Note S_SCOOP[] = {N(480, 170, 110, WOB, 0.55f, 0.25f), N(0, 0, 25, NOI, 0.15f, 0)};                     // "squish"
static const Note S_HACK[] = {N(320, 210, 80, WOB, 0.45f, 0.2f), REST(40), N(340, 200, 90, WOB, 0.45f, 0.2f), REST(30), N(500, 950, 60, SIN, 0.45f, 0)};   // "hck, hck... pop"
static const Note S_POUNCE[] = {N(240, 780, 150, WOB, 0.55f, 0.12f)};                                                  // "boing"
static const Note S_WIN[] = {N(523, 560, 70, SIN, 0.5f, 0), N(659, 700, 70, SIN, 0.5f, 0), N(784, 830, 70, SIN, 0.5f, 0), N(1047, 1120, 220, SIN, 0.5f, 0.01f)};
static const Note S_LOSE[] = {N(440, 400, 170, TRI, 0.4f, 0.03f), N(392, 290, 300, TRI, 0.4f, 0.06f)};                // "wah wah"
static const Note S_SAD[] = {N(500, 300, 420, MEW, 0.4f, 0.06f)};
static const Note S_HOLE[] = {N(900, 50, 1300, SIN, 0.5f, 0.02f), N(0, 0, 300, NOI, 0.3f, 0)};
static const Note S_ALARM[] = {N(520, 1000, 170, MEW, 1.0f, 0.02f), N(1000, 420, 520, MEW, 1.0f, 0.06f), REST(140),
                               N(560, 1050, 170, MEW, 1.0f, 0.02f), N(1050, 400, 560, MEW, 1.0f, 0.06f), REST(140),
                               N(600, 1100, 190, MEW, 1.0f, 0.02f), N(1100, 380, 700, MEW, 1.0f, 0.07f)};
static const Note S_SCRATCH[] = {N(0, 0, 40, NOI, 0.3f, 0), N(1500, 1100, 18, SIN, 0.2f, 0)};                          // "fsst"
static const Note S_YAWN[] = {N(420, 600, 180, MEW, 0.35f, 0.02f), N(600, 300, 420, MEW, 0.35f, 0.05f)};
static const Note S_POOF[] = {N(0, 0, 70, NOI, 0.3f, 0), N(800, 300, 70, SIN, 0.35f, 0)};                              // "pfff"
static const Note S_HISS[] = {N(0, 0, 300, NOI, 0.4f, 0)};
static const Note S_KIBBLE_BOWL[] = {N(2300, 2050, 28, TRI, 0.30f, 0), N(3050, 2850, 22, SIN, 0.14f, 0)};             // a hard little "tink" off the bowl
static const Note S_KIBBLE_FLOOR[] = {N(0, 0, 20, NOI, 0.30f, 0), N(170, 115, 32, SIN, 0.28f, 0)};                    // a dull "tick" on the floorboards
static const Note S_BEAT[] = {N(950, 900, 25, SIN, 0.35f, 0)};                                                         // scratch game: an arrow arrives ("tock")
static const Note S_MISS[] = {N(320, 170, 100, WOB, 0.45f, 0.18f)};                                                    // "bonk"
static const Note S_BLOOP[] = {N(380, 880, 80, SIN, 0.55f, 0)};                                                        // "bloop": something opens / appears
static const Note S_SQUISH[] = {N(560, 200, 130, WOB, 0.55f, 0.3f)};                                                   // "squish"
static const Note S_POP[] = {N(700, 1500, 40, SIN, 0.5f, 0)};                                                          // "pop": a star, a sparkle, a new place
static const Note S_SPLASH[] = {N(0, 0, 60, NOI, 0.35f, 0), N(1000, 420, 90, SIN, 0.4f, 0), N(500, 800, 40, SIN, 0.3f, 0)};   // fish out of the water
static const Note S_ALERT[] = {N(3000, 3500, 90, SIN, 0.5f, 0), REST(70), N(3000, 3500, 90, SIN, 0.5f, 0)};          // two high chirps: a meter ran dry
static const Note S_CHIME[] = {N(1050, 1050, 70, SIN, 0.45f, 0), N(1320, 1320, 70, SIN, 0.45f, 0), N(1760, 1760, 130, SIN, 0.5f, 0)};   // magic cat: a bright rising "ta-da" for a right guess
static const Note S_BACK[] = {N(820, 400, 70, SIN, 0.5f, 0)};                                                          // "bloop" downwards: back / close

struct SoundDef { const Note* notes; uint8_t n; };
#define SD(a) {a, (uint8_t)(sizeof(a) / sizeof(a[0]))}
static const SoundDef kSounds[cat::SND_COUNT] = {
  SD(S_TAP), SD(S_MEOW_SOFT), SD(S_MEOW), SD(S_MEOW_LOUD), SD(S_PURR), SD(S_EAT), SD(S_COIN), SD(S_SCOOP), SD(S_HACK),
  SD(S_POUNCE), SD(S_WIN), SD(S_LOSE), SD(S_SAD), SD(S_HOLE), SD(S_ALARM), SD(S_SCRATCH), SD(S_YAWN), SD(S_POOF), SD(S_HISS),
  SD(S_KIBBLE_BOWL), SD(S_KIBBLE_FLOOR), SD(S_BEAT), SD(S_MISS), SD(S_BLOOP), SD(S_SQUISH), SD(S_POP), SD(S_SPLASH), SD(S_BACK), SD(S_ALERT), SD(S_CHIME)};

struct Msg { uint8_t snd; uint8_t vol; };

static uint32_t rngState = 0x12345;
static inline float noise() {
  rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
  return ((int32_t)(rngState & 0xFFFF) - 32768) / 32768.0f;
}

// Renders one sound sample-by-sample.
struct Voice {
  const SoundDef* def = nullptr;
  int note = 0;
  uint32_t pos = 0, len = 0;
  float phase = 0, gain = 0, lp = 0;
  bool active() const { return def != nullptr; }
  uint32_t age = 0;   // samples since it started (the oldest voice is the one to steal)
  void start(uint8_t snd, uint8_t vol) {
    def = &kSounds[snd];
    note = 0; pos = 0; phase = 0; lp = 0; age = 0;
    len = (uint32_t)def->notes[0].ms * SR / 1000;
    // volume 0..10 on a gentle curve (ears hear loudness roughly logarithmically); 10 is about 2/3 of the old maximum
    static const float g[11] = {0.0f, 0.02f, 0.035f, 0.055f, 0.08f, 0.11f, 0.15f, 0.2f, 0.27f, 0.36f, 0.5f};
    gain = g[vol > 10 ? 10 : vol];
  }
  float next() {
    if (!def) return 0;
    age++;
    while (pos >= len) {
      note++;
      if (note >= def->n) { def = nullptr; return 0; }
      pos = 0;
      len = (uint32_t)def->notes[note].ms * SR / 1000;
    }
    const Note& n = def->notes[note];
    float u = (float)pos / (float)len;
    pos++;
    if (n.f0 < 0) return 0;
    float env = fminf(1.0f, u / 0.08f) * fminf(1.0f, (1.0f - u) / 0.25f);
    float f = n.f0 + (n.f1 - n.f0) * u;
    if (n.vib > 0) f *= 1.0f + n.vib * sinf(2.0f * 3.14159f * (n.wave == WOB ? 30.0f : 6.0f) * (float)pos / SR);
    phase += f / SR;
    if (phase >= 1.0f) phase -= 1.0f;
    float s;
    switch (n.wave) {
      case SIN: case WOB: s = sinf(2 * 3.14159f * phase); break;
      case SQR: s = phase < 0.5f ? 1.0f : -1.0f; break;
      case TRI: s = 4.0f * fabsf(phase - 0.5f) - 1.0f; break;
      case NOI: {
        float w = noise();
        if (n.f0 > 0) {                         // "purr": low rumble chopped at ~25 Hz
          lp += (w - lp) * 0.08f;
          float chop = (fmodf((float)pos * 25.0f / SR, 1.0f) < 0.5f) ? 1.0f : 0.25f;
          s = lp * 3.0f * chop;
        } else {
          lp += (w - lp) * 0.6f;
          s = lp;
        }
        break;
      }
      default: {                                // MEW: fundamental + two formant-ish harmonics
        float p = 2 * 3.14159f * phase;
        s = 0.6f * sinf(p) + 0.35f * sinf(2 * p) + 0.25f * sinf(3 * p + 0.5f);
        break;
      }
    }
    return s * n.amp * env * gain;
  }
};

// Three voices mix together, so a catch "pop" is never cut off by the next swat or crash.
static constexpr int NV = 3;
struct Mixer {
  Voice v[NV];
  void start(uint8_t snd, uint8_t vol) {
    int pick = 0;
    for (int i = 0; i < NV; i++) { if (!v[i].active()) { pick = i; break; } if (v[i].age > v[pick].age) pick = i; }
    v[pick].start(snd, vol);
  }
  bool active() const { for (int i = 0; i < NV; i++) if (v[i].active()) return true; return false; }
  float next() { float s = 0; for (int i = 0; i < NV; i++) s += v[i].next(); return s * 0.8f; }
};

static void audioTask(void*) {
  Mixer v;
  static int16_t buf[256 * 2];
  uint32_t quietBlocks = 0;
  bool paOn = false;
  for (;;) {
    Msg m;
    TickType_t wait = (v.active() || quietBlocks < 20) ? 0 : pdMS_TO_TICKS(25);
    if (xQueueReceive(q, &m, wait) == pdTRUE) {
      if (m.snd < cat::SND_COUNT) v.start(m.snd, m.vol);
      if (!paOn) { digitalWrite(PA_PIN, HIGH); paOn = true; delay(5); }
      quietBlocks = 0;
    }
    if (!v.active() && quietBlocks >= 20) {
      if (paOn) { digitalWrite(PA_PIN, LOW); paOn = false; }
      continue;                                   // idle: block on the queue (above)
    }
    for (int i = 0; i < 256; i++) {
      float s = v.next();
      if (s > 1) s = 1;
      if (s < -1) s = -1;
      int16_t o = (int16_t)(s * 30000);
      buf[2 * i] = o;
      buf[2 * i + 1] = o;
    }
    busy = v.active();
    if (!v.active()) quietBlocks++;
    i2s.write((uint8_t*)buf, sizeof(buf));
  }
}

void audioBegin() {
  pinMode(PA_PIN, OUTPUT);
  digitalWrite(PA_PIN, LOW);
  i2s.setPins(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO, I2S_DI_IO, I2S_MCK_IO);
  if (!i2s.begin(I2S_MODE_STD, SR, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    Serial.println("[audio] i2s begin failed");
    return;
  }
  es8311_handle_t h = es8311_create(0, ES8311_ADDRRES_0);
  if (!h) { Serial.println("[audio] es8311 create failed"); return; }
  const es8311_clock_config_t clk = {
    .mclk_inverted = false,
    .sclk_inverted = false,
    .mclk_from_mclk_pin = true,
    .mclk_frequency = SR * 256,
    .sample_frequency = SR};
  if (es8311_init(h, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK ||
      es8311_sample_frequency_config(h, clk.mclk_frequency, clk.sample_frequency) != ESP_OK ||
      es8311_voice_volume_set(h, 85, NULL) != ESP_OK) {
    Serial.println("[audio] es8311 config failed");
    return;
  }
  q = xQueueCreate(8, sizeof(Msg));
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 2, nullptr, 0);
  ready = true;
  Serial.println("[audio] ok");
}

bool audioBusy() { return busy || (q && uxQueueMessagesWaiting(q) > 0); }

void audioPlay(cat::Sound s, uint8_t volume) {
  if (!ready || volume == 0) return;
  Msg m = {(uint8_t)s, volume};
  xQueueSend(q, &m, 0);   // never block the game; drop if the queue is full
}

}  // namespace hal
