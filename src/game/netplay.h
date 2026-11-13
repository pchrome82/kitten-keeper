// Explore Together: up to 4 Kitten Keepers nearby share the Explore world for a 5-minute match (see netplay.cpp).
#pragma once
#include <stdint.h>

namespace cat {

constexpr int NET_MAX = 4;                  // players in a match
constexpr int NET_SQ = 30;                  // squares to claim = the Explore screens (EXW x EXH = 6 x 5)
constexpr float NET_MATCH_TIME = 300.0f;    // 5 minutes
constexpr float NET_CLAIM_TIME = 4.0f;      // stay alone on a square this long to claim it
constexpr float NET_TIMEOUT = 6.0f;         // a player (or the host) not heard from this long has gone
constexpr float NET_STUN = 4.0f;            // the loser of a fight is dazed (can't claim) this long
constexpr float NET_PAIR_COOLDOWN = 12.0f;  // the same two cats won't fight again straight away
constexpr uint8_t NET_NOBODY = 0xFF;
constexpr int NET_RAID_HP = 8;              // King Raid: the King takes this many hairball hits per player
constexpr float NET_KING_PROWL = 40.0f;     // ...and moves to another square this often
constexpr float NET_RAID_DAZE = 8.0f;       // knocked out by the King: out of it this long

enum NetMode : uint8_t {
  NM_OFF,      // radio off
  NM_LOBBY,    // looking for games nearby
  NM_HOST,     // hosting: waiting for players, then START
  NM_JOINED,   // joined someone's game: waiting for them to start
  NM_MATCH,    // playing
  NM_OVER      // the results
};

struct NetPeer {          // one player in the roster (slot = index; the host is slot 0)
  uint32_t id;
  char name[12];
  uint8_t breed; int8_t fat10;
  bool gone;              // left / out of range
  float heard;            // seconds since we last heard from them
  int8_t sx, sy;          // which square they're on
  float x, y, dx, dy;     // reported position on that screen, and a smoothed one to draw
  bool left, moving;
  float stun;             // dazed after losing a fight
  bool fighting;          // host: in a fight right now
  float fightT;           // host: how long that fight has run (a stuck one is cancelled)
};

struct NetAd {            // a game we can join
  uint32_t id; uint16_t session; char name[12]; uint8_t players; bool open; float heard;
};

struct NetFight {         // my side of a fight with another player
  bool on;
  uint32_t fid, foe; uint8_t square; int foeSlot;
  int round, myWins, foeWins;
  float t;                // time into this round
  float marker, dir, speed;
  float zoneC, zoneW;     // my green zone (stronger cats get a wider one)
  float myScore;          // this round: -1 = not swung yet
  float foeScores[8];     // their swing per round (-1 = not heard yet)
  float showT; int roundWinner;   // showing who took the round (1 me, 2 them, 3 both missed: go again)
  bool done, iWon; float doneT;
  float sendT, foeHeard;
  uint32_t rng;
};

struct NetState {
  NetMode mode = NM_OFF;
  bool radio = false;
  uint32_t me = 0;
  uint16_t session = 0;
  uint32_t hostId = 0;
  bool isHost = false;
  NetPeer p[NET_MAX]; int n = 0; int mySlot = -1;
  NetAd ads[3]; int nAds = 0;
  uint8_t owner[NET_SQ];
  float timeLeft = 0; uint32_t seed = 0;
  float sendT = 0, adT = 0, boardT = 0, hostHeard = 0;
  bool stateDue = false;      // time to broadcast where I am
  int claimSq = -1; float claimT = 0, claimSendT = 0;
  float pairCd[NET_MAX][NET_MAX];
  uint32_t nextFid = 0;
  NetFight f;
  int prevSq = -1;
  char msg[32]; float msgT = 0;
  int overSent = 0;
  bool rewarded = false; int place = 0; int reward = 0;   // results: my place (1st..4th) and hairballs won
  bool hostLeft = false;
  float peerHeard = 99.0f;    // lobby: seconds since another Kitten Keeper nearby was heard
  // ---- KING RAID (co-op): everyone against the Alley King, who prowls the map ----
  uint8_t coop = 0;           // 0 = claim squares, 1 = King Raid
  int16_t bossHp = 0, bossMax = 0;
  uint8_t kingSq = NET_NOBODY;   // the square the King is on
  float kingMoveT = 0;        // host: time until the King prowls to the next square
  uint32_t kingRng = 1;
  uint16_t myHits = 0;        // my hairball hits on the King (cumulative: resending is harmless)
  uint16_t hits[NET_MAX];     // everyone's hits (host keeps the count, the board shares it)
  float hitSendT = 0;
  float raidStun = 0;         // knocked out by the King: back on your feet in a few seconds
  bool raidWon = false;
};

}  // namespace cat
