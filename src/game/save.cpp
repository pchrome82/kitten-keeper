#include "save.h"
#include <string.h>

namespace cat {

uint32_t crc32(const uint8_t* p, uint32_t n) {
  uint32_t c = 0xFFFFFFFFu;
  for (uint32_t i = 0; i < n; i++) {
    c ^= p[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return ~c;
}

void saveDefaults(SaveData& s) {
  memset(&s, 0, sizeof(s));
  s.magic = SAVE_MAGIC;
  s.version = SAVE_VERSION;
  s.size = sizeof(SaveData);
  s.mode = MODE_UNSET;
  s.volume = 2;
  s.brightness = KK_DEFAULT_BRIGHT;
  s.eq[2] = KK_DEFAULT_SCREEN_OFF;   // screen off after 1 minute (30 s on the LCD)
  s.eq[3] = 11;   // settings version (0..10 volume / brightness + screen timeout)
  s.alarmH = 7;
  s.alarmM = 0;
  for (int i = 0; i < 4; i++) s.own[i] = 1;
  s.ownHat = s.ownGl = s.ownNeck = 1; s.ownCollar = 1;
  s.unlocked = 0u;  // nothing owned yet: the very first kitten is a free pick of any regular breed
}

void saveSeal(SaveData& s) {
  s.magic = SAVE_MAGIC;
  s.version = SAVE_VERSION;
  s.size = sizeof(SaveData);
  s.crc = crc32(reinterpret_cast<const uint8_t*>(&s) + 12, sizeof(SaveData) - 12);
}

bool saveMigrateV8(SaveData& s) {
  if (s.magic != SAVE_MAGIC || s.version != 8 || s.size != SAVE_V8_SIZE) return false;
  if (s.crc != crc32(reinterpret_cast<const uint8_t*>(&s) + 12, SAVE_V8_SIZE - 12)) return false;
  // the old Hall of Cats becomes the journal's list of past cats (their photos weren't kept back then)
  memset(s.album, 0, sizeof(s.album)); s.albumCount = 0;
  for (int i = 0; i < s.hallCount && i < HALL_MAX; i++) {
    SaveData::AlbumCat& a = s.album[s.albumCount++];
    memcpy(a.name, s.hall[i].name, sizeof(a.name)); a.name[sizeof(a.name) - 1] = 0;
    a.breed = s.hall[i].breed; a.ending = s.hall[i].ending; a.ageDays = s.hall[i].ageDays; a.prey = s.hall[i].prey;
  }
  memset(s.pad9, 0, sizeof(s.pad9)); s.worldSeed = 0;
  saveSeal(s);
  return true;
}

bool saveMigrateV7(SaveData& s) {
  if (s.magic != SAVE_MAGIC || s.version != 7 || s.size != SAVE_V7_SIZE) return false;
  if (s.crc != crc32(reinterpret_cast<const uint8_t*>(&s) + 12, SAVE_V7_SIZE - 12)) return false;
  memset(&s.x8, 0, sizeof(s.x8));
  s.x8.trait = 0xFF;   // "not picked yet": the game gives the cat a personality when it loads
  saveSeal(s);
  return true;
}

bool saveMigrateV6(SaveData& s) {
  if (s.magic != SAVE_MAGIC || s.version != 6 || s.size != SAVE_V6_SIZE) return false;
  if (s.crc != crc32(reinterpret_cast<const uint8_t*>(&s) + 12, SAVE_V6_SIZE - 12)) return false;
  memset(&s.ex, 0, sizeof(s.ex));
  memset(&s.x8, 0, sizeof(s.x8));
  s.x8.trait = 0xFF;
  // the bell collar moved into the NECK slot (id 3); the old collar slot becomes BODY (cape, saddle)
  if (s.ownCollar & 2) s.ownNeck |= 1u << 3;
  if (s.wornCollar && !s.wornNeck) s.wornNeck = 3;
  s.ownCollar = 1; s.wornCollar = 0;
  saveSeal(s);
  return true;
}

bool saveValid(const SaveData& s) {
  if (s.magic != SAVE_MAGIC || s.version != SAVE_VERSION || s.size != sizeof(SaveData)) return false;
  return s.crc == crc32(reinterpret_cast<const uint8_t*>(&s) + 12, sizeof(SaveData) - 12);
}

}  // namespace cat
