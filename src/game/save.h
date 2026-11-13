// Persistent game data. One flat POD blob with magic/version/CRC.
#pragma once
#include "pet.h"
#include <stddef.h>


namespace cat {

constexpr uint32_t SAVE_MAGIC = 0xC47F00D5u;
constexpr uint16_t SAVE_VERSION = 9;   // 7 = + Explore world progress, 8 = + personality, journal, Sphynx petting,
                                       // 9 = + the album of past cats (older saves are migrated, not lost)
constexpr int HALL_MAX = 8;     // (the old Hall of Cats list: read from older saves only)
constexpr int ALBUM_MAX = 24;   // the journal remembers this many past cats (then the oldest makes room)

struct SaveData {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t crc;            // CRC32 of everything after this field

  uint8_t mode;            // Mode
  uint8_t volume;          // 0..10 (saves from before eq[3] == 10 used 0..3)
  uint8_t brightness;      // 1..10 (saves from before eq[3] == 10 used 1..5)
  uint8_t alarmOn;
  uint8_t alarmH, alarmM;
  uint8_t clockSet;        // user confirmed the clock
  uint8_t hallCount;

  uint32_t unlocked;       // bitmask of owned skins
  uint32_t coins;          // hairballs
  uint16_t treats, diets;  // (no longer sold or used: kept so saves stay the same shape)
  uint8_t litterTier;      // 0 basic, 1 sifting, 2 auto
  uint8_t hasCat;          // 0 until first kitten adopted
  uint8_t pad[2];          // pad[0] = adopt-screen breed pick; pad[1] = litter hairball finds today ((day%32)<<3 | count, max 6)
  uint16_t exploreDay;     // epoch day of the last Explore trip
  uint8_t exploreTrips;    // trips used that day (max 2)
  uint8_t room;            // current home (0 = starter)
  uint16_t items;          // owned furniture / toys (ITEM_* bits)
  uint8_t boxType, boxFlags, slowItem, roomsOwned;   // roomsOwned: bitmask of bought homes (bit 0 = starter, always yours)   // box (BoxType), flags bit0 = adulthood shoebox given, slowItem = hermit's charm
  uint32_t boxExp;         // epoch when the box falls apart

  uint8_t own[4];          // owned house-part themes (bitmask) for wall, floor, rug, ceiling; bit 0 = plain, always owned
  uint8_t eq[4];           // (old decor slots) eq[2] = screen timeout (0..8: 1s 3s 5s 10s 15s 30s 1min 5min never);
                           // eq[3] = settings version: 10 = 0..10 volume / brightness, 11 = + screen timeout
  uint8_t ownHat, ownGl, ownNeck, ownCollar;   // owned costumes (bitmask, bit 0 = none). v7: ownCollar = BODY slot (cape, saddle)
  uint8_t wornHat, wornGl, wornNeck, wornCollar;   // v7: the bell collar is neck item 3; wornCollar = BODY slot

  uint32_t lastSeen;       // epoch seconds (local) at last save
  uint32_t fallbackClock;  // used if the RTC is unavailable

  uint32_t lifeFed, lifeGames, lifeHairballs, lifePrey, lifeCats;

  HallEntry hall[HALL_MAX];
  PetData pet;

  // ---- v7: the Explore world (appended so v6 saves can be migrated) ----
  struct ExploreSave {
    uint32_t seen;        // screens ever visited (5x5 world, bit = sy*5+sx)
    uint32_t journal;     // bit 0.. critters (mouse, bird, frog, firefly, butterfly, fish); bit 8.. places
    uint16_t chests;      // chests opened (forever)
    uint8_t flags;        // bit0 key, bit1 gate open, bit2 Alley King beaten, bit3 boulder rolled away
    uint8_t quest[3];     // neighbours: 0 not met, 1 asked, 2 done
    uint8_t fish, mice;   // counters toward the neighbours' requests
    uint8_t hbDay, hbCount;   // hairballs found lying around today (capped)
    uint32_t rocksToday, nestsToday;   // loose rocks / nests already searched today
    uint32_t trashToday;  // city trash cans searched today (2 per screen)
  } ex;

  // ---- v8: this cat's personality and journal (appended so v7 saves can be migrated) ----
  struct Snap {           // a photo for the album: how they looked when they reached a life stage
    uint8_t taken, breed, hat, gl, neck, body; int8_t fat10; uint8_t day;   // day = their age in days
  };
  struct CatExtra {
    uint8_t trait;        // Trait (personality), picked when they come home
    uint8_t bdayWeek;     // last weekly birthday celebrated (age in weeks)
    uint8_t seasonSeen;   // last seasonal greeting shown (Season)
    uint8_t pad;
    uint32_t petHbAt;     // epoch of the last petting hairball (hairless cats)
    uint32_t milestones;  // journal: Milestone bits reached
    uint8_t msDay[24];    // the age in days each milestone was reached
    Snap photo[4];        // kitten, junior, adult, senior
    uint32_t debt;        // hairballs owed to the vet (shown as a negative balance; paid off by the next hairballs found)
    uint32_t collapses;   // times this cat has hit 0 health (the third one and they run away)
    uint8_t bdayMonth, bdayDay;   // the birthday party date, picked by the game at first and changeable in Settings (0 = not picked yet)
    uint16_t bdayYear;            // the last year this cat had its birthday party
    uint32_t reserved;
  } x8;

  // ---- v9: the journal's record of every past cat (appended so v8 saves can be migrated) ----
  // This replaces the old Hall of Cats list above (`hall`, kept only so older saves can be read): one entry per cat
  // whose life has ended, with its photos, kept until the game is reset from Settings.
  struct AlbumCat {
    char name[12];
    uint8_t breed, ending;  // Ending
    uint16_t ageDays, prey; // how long they lived, gifts brought home
    uint16_t pad;
    Snap photo[4];          // kitten, junior, adult, senior (as they were; not taken = they never got there / an old save)
  };
  AlbumCat album[ALBUM_MAX];  // newest first
  uint8_t albumCount;
  uint8_t pad9[3];      // gift boxes: [0] one waiting (0 none, else why), [1] good-care days in a row, [2] the day last counted
  uint32_t worldSeed;   // Explore's world for this player: 0 not made yet, 1 the classic map, else a random layout (made once)
};

void saveDefaults(SaveData& s);
void saveSeal(SaveData& s);
bool saveValid(const SaveData& s);
// A v6 save (everything up to `ex`) loaded into `s`: check it and upgrade it in place to v7.
bool saveMigrateV6(SaveData& s);
constexpr uint32_t SAVE_V6_SIZE = (uint32_t)offsetof(SaveData, ex);
// A v7 save (everything up to `x8`): check it and upgrade it in place to v8.
bool saveMigrateV7(SaveData& s);
constexpr uint32_t SAVE_V7_SIZE = (uint32_t)offsetof(SaveData, x8);
// A v8 save (everything up to `album`): check it and upgrade it in place to v9.
bool saveMigrateV8(SaveData& s);
constexpr uint32_t SAVE_V8_SIZE = (uint32_t)offsetof(SaveData, album);
uint32_t crc32(const uint8_t* p, uint32_t n);

}  // namespace cat
