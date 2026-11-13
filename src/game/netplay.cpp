// Explore Together: up to 4 Kitten Keepers nearby share the Explore world for a 5-minute match.
//  * Claim a square (one screen of the map) by staying on it alone for 4 s. Your colour shows on everyone's map.
//  * Meet another player on the same square and a fight starts: you both swing at the same time, best of 3.
//    Stronger cats get a wider green zone, faster cats score more for swinging quickly. The winner takes the square,
//    the loser is dazed for a few seconds.
//  * Whoever holds the most squares when the time runs out wins.
// One device hosts: it keeps the board (who owns what, the clock) and starts the fights. Everyone broadcasts where they
// are a few times a second. Packets can be lost, so everything that matters is simply sent again until it's seen.
#include "game.h"
#include "catart.h"
#include "gfx.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace cat {

namespace {

inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

const Color GREEN = rgb(60, 150, 90), BLUE = rgb(70, 110, 200), PURPLE = rgb(110, 80, 160);
const Color SLOT_COL[NET_MAX] = {rgb(240, 84, 96), rgb(84, 152, 250), rgb(96, 210, 112), rgb(250, 200, 64)};
const int8_t SPAWN[NET_MAX][2] = {{0, 0}, {4, 4}, {4, 0}, {0, 4}};   // each player starts in a different corner

constexpr uint8_t MAGIC0 = 'K', MAGIC1 = 'k', NET_VER = 2;   // 2 = + King Raid (older boards just don't see these games)
enum : uint8_t { PK_AD = 1, PK_JOIN, PK_ROSTER, PK_STATE, PK_CLAIM, PK_BOARD, PK_SWING, PK_RESULT, PK_LEAVE, PK_HELLO, PK_HIT };   // append only
// PK_HELLO: "a Kitten Keeper is here and could play" (sent while in the lobby; header only)

#pragma pack(push, 1)
struct Hdr { uint8_t m0, m1, ver, type; uint32_t from; uint16_t session; };
struct PkAd { Hdr h; char name[12]; uint8_t players, open; };
struct PkJoin { Hdr h; uint32_t host; char name[12]; uint8_t breed; int8_t fat10; };
struct PkRoster { Hdr h; uint8_t started, n; uint32_t seed; uint16_t matchSec; struct { uint32_t id; char name[12]; uint8_t breed; int8_t fat10; } p[NET_MAX]; uint8_t coop; };
struct PkState { Hdr h; int8_t sx, sy; int16_t x, y; uint8_t flags; };   // flags: 1 facing left, 2 walking
struct PkClaim { Hdr h; uint8_t square; };
struct PkBoard {
  Hdr h; uint8_t owner[NET_SQ]; uint16_t timeLeft10; uint8_t stun10[NET_MAX]; uint8_t over;
  struct { uint32_t fid; uint8_t a, b, square; } fight[2];   // fights going on (fid 0 = none)
  int16_t bossHp, bossMax; uint8_t kingSq; uint16_t hits[NET_MAX];   // King Raid
};
struct PkHit { Hdr h; uint16_t hits; };   // King Raid: my hits on the King so far
struct PkSwing { Hdr h; uint32_t fid; uint8_t round; int16_t score; };   // score x 1000
struct PkResult { Hdr h; uint32_t fid; uint32_t winner; };
#pragma pack(pop)

// host bookkeeping for the fights it has started
struct HostFight { uint32_t fid; int a, b; uint8_t square; float t; };
HostFight gHF[2];

uint32_t mix32(uint32_t h) { h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16; return h; }
float rnd01(uint32_t& s) { s = s * 1664525u + 1013904223u; return (s >> 8) / 16777216.0f; }

}  // namespace

// ------------------------------------------------------------------------------------------- sending

static void hdr(Hdr& h, uint8_t type, const NetState& n) { h.m0 = MAGIC0; h.m1 = MAGIC1; h.ver = NET_VER; h.type = type; h.from = n.me; h.session = n.session; }

static int slotOf(const NetState& n, uint32_t id) {
  for (int i = 0; i < n.n; i++) if (n.p[i].id == id) return i;
  return -1;
}

// ------------------------------------------------------------------------------------------- lobby

void Game::startLink() {
  NetState& n = net_;
  if (n.mode != NM_OFF) netStop();
  n = NetState();
  n.radio = plat->netOpen();   // the radio is only ever on from here (TOGETHER) until the match / lobby ends
  n.me = plat->netId();
  if (!n.me) n.me = (plat->randomSeed() | 1u);
  n.mode = NM_LOBBY;
  memset(n.owner, NET_NOBODY, sizeof(n.owner));
  memset(gHF, 0, sizeof(gHF));
  setScreen(S_LINK);
}

void Game::netStop() {
  NetState& n = net_;
  if (n.radio) {
    PkJoin l; memset(&l, 0, sizeof(l)); hdr(l.h, PK_LEAVE, n);
    for (int i = 0; i < 3; i++) plat->netSend(&l, sizeof(Hdr));   // say goodbye (a few times: packets get lost)
    plat->netClose();
  }
  n.radio = false;
  n.mode = NM_OFF;
  n.f.on = false;
  ex_.mp = false;
}

static void netSay(NetState& n, const char* m) { snprintf(n.msg, sizeof(n.msg), "%s", m); n.msgT = 2.5f; }

void Game::netHostStart() {   // HOST pressed: a new game, with me in slot 0
  NetState& n = net_;
  n.isHost = true; n.hostId = n.me;
  n.session = (uint16_t)(mix32(n.me ^ nowMs) | 1u);
  n.n = 1; n.mySlot = 0;
  NetPeer& p = n.p[0]; memset(&p, 0, sizeof(p));
  p.id = n.me; snprintf(p.name, sizeof(p.name), "%s", pet.d.name); p.breed = (uint8_t)pet.lookBreed(); p.fat10 = (int8_t)(pet.fatLook() * 10);
  n.mode = NM_HOST;
  n.adT = 0;
}

void Game::netEnterMatch(uint32_t seed, int seconds) {
  NetState& n = net_;
  n.mode = NM_MATCH;
  n.seed = seed;
  n.timeLeft = (float)seconds;
  memset(n.owner, NET_NOBODY, sizeof(n.owner));
  memset(n.pairCd, 0, sizeof(n.pairCd));
  memset(gHF, 0, sizeof(gHF));
  n.f = NetFight(); n.f.on = false;
  n.claimSq = -1; n.claimT = 0; n.prevSq = -1; n.overSent = 0; n.rewarded = false; n.hostLeft = false;
  n.myHits = 0; n.hitSendT = 0; n.raidStun = 0; n.raidWon = false; memset(n.hits, 0, sizeof(n.hits));
  n.bossMax = n.bossHp = (int16_t)(NET_RAID_HP * (n.n < 1 ? 1 : n.n));
  n.kingSq = (uint8_t)(2 * 6 + 3); n.kingMoveT = NET_KING_PROWL; n.kingRng = seed | 1u;   // the King starts in the middle
  for (int i = 0; i < n.n; i++) {
    NetPeer& p = n.p[i];
    p.gone = false; p.heard = 0; p.stun = 0; p.fighting = false;
    p.sx = SPAWN[i][0]; p.sy = SPAWN[i][1]; p.x = p.dx = 92; p.y = p.dy = 104;
  }
  n.hostHeard = 0;
  sound(SND_WIN);
  startExploreMP(SPAWN[n.mySlot < 0 ? 0 : n.mySlot][0], SPAWN[n.mySlot < 0 ? 0 : n.mySlot][1]);
}

void Game::netToOver() {
  NetState& n = net_;
  if (n.mode == NM_OVER) return;
  n.mode = NM_OVER;
  n.f.on = false;
  if (n.coop) {   // King Raid: everyone wins or loses together
    n.raidWon = n.bossHp <= 0;
    n.place = 1;
    if (!n.rewarded) {
      n.rewarded = true;
      recordMilestone(MS_TOGETHER);
      if (n.raidWon) recordMilestone(MS_KING);
      n.reward = n.raidWon ? 3 : (n.myHits > 0 ? 1 : 0);
      giveCoins(n.reward);
      pet.exercise(gfun(40.0f), EXPLORE_ENERGY, 1.0f);
      sv.lifeGames++;
      autosave(true);
    }
    ex_.active = false; ex_.mp = false;
    sound(n.raidWon ? SND_WIN : SND_SAD);
    setScreen(S_LINK);
    return;
  }
  // results: squares per player, my place, a reward
  int cnt[NET_MAX] = {0};
  for (int s = 0; s < NET_SQ; s++) if (n.owner[s] < NET_MAX) cnt[n.owner[s]]++;
  const int mine = n.mySlot >= 0 ? cnt[n.mySlot] : 0;
  int place = 1;
  for (int i = 0; i < n.n; i++) if (i != n.mySlot && cnt[i] > mine) place++;
  n.place = place;
  if (!n.rewarded) {
    n.rewarded = true;
    recordMilestone(MS_TOGETHER);
    n.reward = place == 1 && mine > 0 ? 3 : (mine > 0 ? 1 : 0);
    giveCoins(n.reward);
    pet.exercise(gfun(40.0f), EXPLORE_ENERGY, 1.0f);   // a big run around: fun, tiring (like any Explore), a little lighter
   
    sv.lifeGames++;
    autosave(true);
  }
  ex_.active = false; ex_.mp = false;
  sound(place == 1 ? SND_WIN : SND_POP);
  setScreen(S_LINK);
}

// ------------------------------------------------------------------------------------------- the fight

static float pvpZoneW(float power) { return clampf(0.07f + 0.03f * (power - 1.5f), 0.06f, 0.16f); }

void Game::netFightStart(uint32_t fid, int foeSlot, uint8_t square) {
  NetState& n = net_;
  NetFight& f = n.f;
  f = NetFight();
  f.on = true; f.fid = fid; f.foeSlot = foeSlot; f.foe = n.p[foeSlot].id; f.square = square;
  f.round = 0; f.myWins = f.foeWins = 0;
  f.rng = mix32(fid ^ n.me);
  f.speed = 1.35f + (mix32(fid) % 6) * 0.08f;   // the same sweep speed for both cats
  for (int i = 0; i < 8; i++) f.foeScores[i] = -1;
  f.myScore = -1; f.t = 0; f.marker = 0; f.dir = 1;
  f.zoneW = pvpZoneW(pet.power()); f.zoneC = f.zoneW + rnd01(f.rng) * (1.0f - 2 * f.zoneW);
  f.foeHeard = 0;
  sound(SND_HISS);
}

static void nextRound(NetFight& f, float power) {
  f.round++; f.t = 0; f.marker = 0; f.dir = 1; f.myScore = -1; f.roundWinner = 0; f.showT = 0;
  f.zoneW = pvpZoneW(power); f.zoneC = f.zoneW + rnd01(f.rng) * (1.0f - 2 * f.zoneW);
}

void Game::netFightTick() {
  NetState& n = net_;
  NetFight& f = n.f;
  if (!f.on) return;
  f.foeHeard += dt;
  if (f.done) { f.doneT += dt; }
  else {
    if (f.foeHeard > NET_TIMEOUT) {   // they vanished: the square is ours
      f.done = true; f.iWon = true; f.doneT = 0; netSay(n, "They ran off! You win.");
    } else if (f.showT > 0) {
      f.showT -= dt;
      if (f.showT <= 0) {
        if (f.myWins >= 2 || f.foeWins >= 2 || f.round >= 6) {
          f.done = true; f.doneT = 0;
          f.iWon = f.myWins != f.foeWins ? f.myWins > f.foeWins : n.me < f.foe;
          sound(f.iWon ? SND_WIN : SND_LOSE);
        } else nextRound(f, pet.power());
      }
    } else {
      f.t += dt;
      f.marker += f.dir * dt * f.speed;
      if (f.marker > 1) { f.marker = 1; f.dir = -1; } else if (f.marker < 0) { f.marker = 0; f.dir = 1; }
      if (f.myScore < 0 && f.t > 3.5f) f.myScore = 0;   // too slow: counts as a miss
      const float foe = f.round < 8 ? f.foeScores[f.round] : 0.0f;
      if (f.myScore >= 0 && foe >= 0) {   // both swung: same rule on both devices, so both agree
        if (f.myScore <= 0 && foe <= 0) f.roundWinner = 3;
        else if (f.myScore > foe || (f.myScore == foe && n.me < f.foe)) { f.roundWinner = 1; f.myWins++; sound(SND_POUNCE); }
        else { f.roundWinner = 2; f.foeWins++; sound(SND_MISS); }
        f.showT = 1.3f;
      }
    }
  }
  // keep telling them my swing (and, once it's over, the host who won)
  f.sendT -= dt;
  if (f.sendT <= 0) {
    f.sendT = 0.2f;
    if (f.myScore >= 0 && !f.done) {
      PkSwing s; hdr(s.h, PK_SWING, n); s.fid = f.fid; s.round = (uint8_t)f.round; s.score = (int16_t)(f.myScore * 1000);
      plat->netSend(&s, sizeof(s));
    }
    if (f.done) {
      PkResult r; hdr(r.h, PK_RESULT, n); r.fid = f.fid; r.winner = f.iWon ? n.me : f.foe;
      if (n.isHost) netApplyResult(f.fid, r.winner); else plat->netSend(&r, sizeof(r));
      // the swing that decided it, for a foe that missed it
      PkSwing s; hdr(s.h, PK_SWING, n); s.fid = f.fid; s.round = (uint8_t)f.round; s.score = (int16_t)(f.myScore < 0 ? 0 : f.myScore * 1000);
      plat->netSend(&s, sizeof(s));
    }
  }
  if (f.done && f.doneT > 2.2f) f.on = false;
}

void Game::netApplyResult(uint32_t fid, uint32_t winner) {   // host
  NetState& n = net_;
  for (auto& h : gHF) {
    if (!h.fid || h.fid != fid) continue;
    const int w = slotOf(n, winner);
    const int loser = w == h.a ? h.b : h.a;
    if (w >= 0) n.owner[h.square] = (uint8_t)w;   // the winner takes the square
    if (loser >= 0 && loser < n.n) n.p[loser].stun = NET_STUN;
    if (h.a < n.n) n.p[h.a].fighting = false;
    if (h.b < n.n) n.p[h.b].fighting = false;
    n.pairCd[h.a][h.b] = n.pairCd[h.b][h.a] = NET_PAIR_COOLDOWN;
    h.fid = 0;
  }
}

// ------------------------------------------------------------------------------------------- every frame

void Game::netPoll() {
  NetState& n = net_;
  if (n.mode == NM_OFF) return;
  if (n.msgT > 0) n.msgT -= dt;
  // ---- receive ----
  uint8_t buf[200];
  int len;
  while (n.radio && (len = plat->netRecv(buf, sizeof(buf))) >= (int)sizeof(Hdr)) {
    const Hdr& h = *(const Hdr*)buf;
    if (h.m0 != MAGIC0 || h.m1 != MAGIC1 || h.ver != NET_VER || h.from == n.me) continue;
    if (h.type == PK_HELLO || h.type == PK_AD || h.type == PK_JOIN) n.peerHeard = 0;   // a nearby player responded
    switch (h.type) {
      case PK_AD: {
        if (n.mode != NM_LOBBY || len < (int)sizeof(PkAd)) break;
        const PkAd& a = *(const PkAd*)buf;
        int k = -1;
        for (int i = 0; i < n.nAds; i++) if (n.ads[i].id == h.from) k = i;
        if (k < 0) {
          if (n.nAds < 3) k = n.nAds++;
          else { k = 0; for (int i = 1; i < 3; i++) if (n.ads[i].heard > n.ads[k].heard) k = i; }
        }
        NetAd& d = n.ads[k];
        d.id = h.from; d.session = h.session; memcpy(d.name, a.name, sizeof(d.name)); d.name[11] = 0;
        d.players = a.players; d.open = a.open != 0; d.heard = 0;
        break; }
      case PK_JOIN: {
        if (n.mode != NM_HOST || len < (int)sizeof(PkJoin)) break;
        const PkJoin& j = *(const PkJoin*)buf;
        if (j.host != n.me) break;
        int s = slotOf(n, h.from);
        if (s < 0 && n.n < NET_MAX) {
          s = n.n++;
          NetPeer& p = n.p[s]; memset(&p, 0, sizeof(p));
          p.id = h.from; memcpy(p.name, j.name, sizeof(p.name)); p.name[11] = 0; p.breed = j.breed; p.fat10 = j.fat10;
          sound(SND_MEOW_SOFT);
        }
        if (s >= 0) n.p[s].heard = 0;
        break; }
      case PK_ROSTER: {
        if (len < (int)sizeof(PkRoster) || h.from != n.hostId || h.session != n.session || n.isHost) break;
        if (n.mode != NM_JOINED && n.mode != NM_MATCH) break;
        const PkRoster& r = *(const PkRoster*)buf;
        n.hostHeard = 0;
        if (n.mode == NM_JOINED) {
          n.n = r.n > NET_MAX ? NET_MAX : r.n; n.mySlot = -1;
          for (int i = 0; i < n.n; i++) {
            NetPeer& p = n.p[i]; p.id = r.p[i].id; memcpy(p.name, r.p[i].name, sizeof(p.name)); p.name[11] = 0;
            p.breed = r.p[i].breed; p.fat10 = r.p[i].fat10;
            if (p.id == n.me) n.mySlot = i;
          }
          n.coop = r.coop;
          if (r.started) {
            if (n.mySlot >= 0) netEnterMatch(r.seed, r.matchSec);
            else { netSay(n, "That game started without you"); n.mode = NM_LOBBY; n.nAds = 0; }
          }
        }
        break; }
      case PK_STATE: {
        if (n.mode != NM_MATCH || h.session != n.session || len < (int)sizeof(PkState)) break;
        const PkState& st = *(const PkState*)buf;
        const int s = slotOf(n, h.from);
        if (s < 0) break;
        NetPeer& p = n.p[s];
        if (p.sx != st.sx || p.sy != st.sy) { p.dx = st.x; p.dy = st.y; }   // new square: no sliding across the screen
        p.sx = st.sx; p.sy = st.sy; p.x = st.x; p.y = st.y; p.left = st.flags & 1; p.moving = (st.flags & 2) != 0;
        p.heard = 0; p.gone = false;
        if (n.f.on && h.from == n.f.foe) n.f.foeHeard = 0;
        break; }
      case PK_CLAIM: {
        if (n.mode != NM_MATCH || !n.isHost || h.session != n.session || len < (int)sizeof(PkClaim)) break;
        const PkClaim& c = *(const PkClaim*)buf;
        const int s = slotOf(n, h.from);
        if (s >= 0 && !n.coop && c.square < NET_SQ && n.p[s].stun <= 0 && !n.p[s].fighting && n.timeLeft > 0) n.owner[c.square] = (uint8_t)s;
        break; }
      case PK_HIT: {   // King Raid: a player's hits so far (host)
        if (n.mode != NM_MATCH || !n.isHost || !n.coop || h.session != n.session || len < (int)sizeof(PkHit)) break;
        const PkHit& k = *(const PkHit*)buf;
        const int s = slotOf(n, h.from);
        if (s >= 0 && k.hits > n.hits[s] && k.hits <= n.bossMax) n.hits[s] = k.hits;
        break; }
      case PK_BOARD: {
        if (n.mode != NM_MATCH || n.isHost || h.from != n.hostId || h.session != n.session || len < (int)sizeof(PkBoard)) break;
        const PkBoard& b = *(const PkBoard*)buf;
        n.hostHeard = 0;
        memcpy(n.owner, b.owner, NET_SQ);
        n.timeLeft = b.timeLeft10 / 10.0f;
        for (int i = 0; i < n.n; i++) n.p[i].stun = b.stun10[i] / 10.0f;
        if (n.coop) { n.bossHp = b.bossHp; n.bossMax = b.bossMax; n.kingSq = b.kingSq; for (int i = 0; i < NET_MAX; i++) n.hits[i] = b.hits[i]; }
        bool listed = false;
        for (const auto& fg : b.fight) {
          if (!fg.fid) continue;
          if (fg.fid == n.f.fid) listed = true;
          if (n.mySlot >= 0 && (fg.a == n.mySlot || fg.b == n.mySlot) && !n.f.on && fg.fid != n.f.fid) {
            const int foe = fg.a == n.mySlot ? fg.b : fg.a;
            if (foe < n.n) { netFightStart(fg.fid, foe, fg.square); listed = true; }
          }
        }
        if (n.f.on && !n.f.done && !listed) {
          NetFight& f = n.f;
          if (f.myWins >= 2 || f.foeWins >= 2) {   // already decided (the other side's result just got there first): finish it here too
            f.done = true; f.doneT = 0; f.showT = 0;
            f.iWon = f.myWins > f.foeWins;
            sound(f.iWon ? SND_WIN : SND_LOSE);
          } else f.on = false;                     // the host called it off
        }
        if (b.over) netToOver();
        break; }
      case PK_SWING: {
        if (len < (int)sizeof(PkSwing)) break;
        const PkSwing& s = *(const PkSwing*)buf;
        if (!n.f.on || s.fid != n.f.fid || h.from != n.f.foe) break;
        if (s.round < 8) n.f.foeScores[s.round] = s.score / 1000.0f;
        n.f.foeHeard = 0;
        break; }
      case PK_RESULT: {
        if (!n.isHost || len < (int)sizeof(PkResult)) break;
        const PkResult& r = *(const PkResult*)buf;
        netApplyResult(r.fid, r.winner);
        break; }
      case PK_LEAVE: {
        const int s = slotOf(n, h.from);
        if (n.mode == NM_HOST && s > 0) {   // left the waiting room: close the gap
          for (int i = s; i < n.n - 1; i++) n.p[i] = n.p[i + 1];
          n.n--;
        } else if (s >= 0) n.p[s].gone = true;
        if (h.from == n.hostId && !n.isHost && (n.mode == NM_JOINED || n.mode == NM_MATCH)) n.hostLeft = true;
        for (int i = 0; i < n.nAds; i++) if (n.ads[i].id == h.from) { n.ads[i] = n.ads[--n.nAds]; break; }
        break; }
    }
  }

  // ---- timers, sending ----
  n.peerHeard += dt;
  for (int i = 0; i < n.nAds; i++) { n.ads[i].heard += dt; if (n.ads[i].heard > 3.0f) { n.ads[i] = n.ads[--n.nAds]; i--; } }
  n.sendT -= dt;
  const bool tick = n.sendT <= 0;
  if (tick) n.sendT = n.mode == NM_MATCH ? 0.16f : 0.5f;
  n.stateDue = tick && n.mode == NM_MATCH;

  if (n.mode == NM_HOST) {
    for (int i = 1; i < n.n; i++) {   // drop anyone who walked away
      n.p[i].heard += dt;
      if (n.p[i].heard > NET_TIMEOUT) { for (int k = i; k < n.n - 1; k++) n.p[k] = n.p[k + 1]; n.n--; i--; }
    }
    if (tick) {
      PkAd a; hdr(a.h, PK_AD, n); snprintf(a.name, sizeof(a.name), "%s", pet.d.name); a.players = (uint8_t)n.n; a.open = n.n < NET_MAX;
      plat->netSend(&a, sizeof(a));
      netSendRoster(false);
    }
  } else if (n.mode == NM_LOBBY) {
    if (tick) { Hdr hh; hdr(hh, PK_HELLO, n); plat->netSend(&hh, sizeof(hh)); }   // "I'm here": lets a friend's lobby know someone responded
  } else if (n.mode == NM_JOINED) {
    n.hostHeard += dt;
    if (n.hostHeard > NET_TIMEOUT || n.hostLeft) { netSay(n, "The host went away"); n.mode = NM_LOBBY; n.nAds = 0; n.hostLeft = false; }
    else if (tick) {
      PkJoin j; hdr(j.h, PK_JOIN, n); j.host = n.hostId; snprintf(j.name, sizeof(j.name), "%s", pet.d.name);
      j.breed = (uint8_t)pet.lookBreed(); j.fat10 = (int8_t)(pet.fatLook() * 10);
      plat->netSend(&j, sizeof(j));
    }
  } else if (n.mode == NM_MATCH) {
    for (int i = 0; i < n.n; i++) {
      NetPeer& p = n.p[i];
      if (i == n.mySlot) continue;
      p.heard += dt;
      if (p.heard > NET_TIMEOUT) p.gone = true;
      const float k = clampf(dt * 10.0f, 0, 1);
      p.dx += (p.x - p.dx) * k; p.dy += (p.y - p.dy) * k;
      if (n.isHost && p.stun > 0) p.stun -= dt;
    }
    if (n.isHost) {
      if (n.mySlot >= 0 && n.p[n.mySlot].stun > 0) n.p[n.mySlot].stun -= dt;
      n.timeLeft -= dt;
      for (int a = 0; a < n.n; a++) for (int b = 0; b < n.n; b++) {   // the rematch timer only runs once they've gone separate ways
        if (n.pairCd[a][b] <= 0) continue;
        const bool together = n.p[a].sx == n.p[b].sx && n.p[a].sy == n.p[b].sy;
        if (!together) n.pairCd[a][b] -= dt;
      }
      for (auto& h : gHF) {   // a fight nobody finished (someone dropped out): call it off
        if (!h.fid) continue;
        h.t += dt;
        if (h.t > 30.0f || n.p[h.a].gone || n.p[h.b].gone) {
          if (n.p[h.a].gone != n.p[h.b].gone) netApplyResult(h.fid, n.p[h.a].gone ? n.p[h.b].id : n.p[h.a].id);
          else { n.p[h.a].fighting = n.p[h.b].fighting = false; h.fid = 0; }
        }
      }
      if (n.coop) {   // King Raid: tally the hits; the King prowls to a neighbouring square now and then
        if (n.mySlot >= 0) n.hits[n.mySlot] = n.myHits;
        int tot = 0; for (int i = 0; i < n.n; i++) tot += n.hits[i];
        n.bossHp = (int16_t)(n.bossMax - tot < 0 ? 0 : n.bossMax - tot);
        n.kingMoveT -= dt;
        bool engaged = false;   // (not while someone is fighting them)
        for (int i = 0; i < n.n; i++) if (!n.p[i].gone && n.p[i].sy * 6 + n.p[i].sx == n.kingSq) engaged = true;
        if (n.kingMoveT <= 0 && !engaged) {
          n.kingMoveT = NET_KING_PROWL;
          int kx = n.kingSq % 6, ky = n.kingSq / 6;
          static const int8_t step[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
          for (int tries = 0; tries < 8; tries++) {
            const int k = (int)(rnd01(n.kingRng) * 4) & 3, nx = kx + step[k][0], ny = ky + step[k][1];
            if (nx >= 0 && nx < 6 && ny >= 0 && ny < 5) { kx = nx; ky = ny; break; }
          }
          n.kingSq = (uint8_t)(ky * 6 + kx);
        }
      }
      // two cats on the same square: FIGHT!
      for (int a = 0; a < n.n && n.timeLeft > 0 && !n.coop; a++) for (int b = a + 1; b < n.n; b++) {
        NetPeer &A = n.p[a], &B = n.p[b];
        if (A.gone || B.gone || A.fighting || B.fighting || A.stun > 0 || B.stun > 0 || n.pairCd[a][b] > 0) continue;
        if (A.sx != B.sx || A.sy != B.sy) continue;
        HostFight* slot = gHF[0].fid ? (gHF[1].fid ? nullptr : &gHF[1]) : &gHF[0];
        if (!slot) continue;
        slot->fid = (++n.nextFid & 0xFFFF) | ((n.me & 0xFFFF) << 16); slot->a = a; slot->b = b; slot->t = 0;
        slot->square = (uint8_t)(A.sy * 6 + A.sx);
        A.fighting = B.fighting = true;
        if (a == n.mySlot || b == n.mySlot) netFightStart(slot->fid, a == n.mySlot ? b : a, slot->square);
      }
      n.adT -= dt;
      if (n.adT <= 0) { n.adT = 1.0f; netSendRoster(true); }   // anyone who missed START still gets in
      n.boardT -= dt;
      const bool over = n.timeLeft <= 0 || (n.coop && n.bossHp <= 0);
      if (n.coop && n.bossHp <= 0 && n.timeLeft > 0) n.timeLeft = 0.01f;   // (the King is down: wrap up)
      if (n.boardT <= 0) {
        n.boardT = 0.25f;
        PkBoard b; memset(&b, 0, sizeof(b)); hdr(b.h, PK_BOARD, n);
        memcpy(b.owner, n.owner, NET_SQ);
        b.timeLeft10 = (uint16_t)(n.timeLeft > 0 ? n.timeLeft * 10 : 0);
        for (int i = 0; i < n.n; i++) b.stun10[i] = (uint8_t)clampf(n.p[i].stun * 10, 0, 255);
        b.over = over;
        for (int k = 0; k < 2; k++) if (gHF[k].fid) { b.fight[k].fid = gHF[k].fid; b.fight[k].a = (uint8_t)gHF[k].a; b.fight[k].b = (uint8_t)gHF[k].b; b.fight[k].square = gHF[k].square; }
        b.bossHp = n.bossHp; b.bossMax = n.bossMax; b.kingSq = n.kingSq; for (int i = 0; i < NET_MAX; i++) b.hits[i] = n.hits[i];
        plat->netSend(&b, sizeof(b));
        if (over) n.overSent++;
      }
      if (over && n.overSent >= 6) netToOver();   // told everyone (several times), now show the results
    } else {
      if (n.coop && n.myHits > 0) {   // tell the host my hits (again and again: packets get lost)
        n.hitSendT -= dt;
        if (n.hitSendT <= 0) { n.hitSendT = 0.3f; PkHit k; hdr(k.h, PK_HIT, n); k.hits = n.myHits; plat->netSend(&k, sizeof(k)); }
      }
      n.timeLeft -= dt;   // smooth countdown between board updates
      n.hostHeard += dt;
      if (n.hostHeard > NET_TIMEOUT || n.hostLeft) { netSay(n, "The host went away"); netToOver(); }
    }
    netFightTick();
  }
}

// Called by Explore every frame of a match: where I am, claiming the square I'm on.
void Game::netMatchTick(int sx, int sy, float x, float y, bool left, bool moving) {
  NetState& n = net_;
  if (n.mode != NM_MATCH || n.mySlot < 0) return;
  NetPeer& me = n.p[n.mySlot];
  me.sx = (int8_t)sx; me.sy = (int8_t)sy; me.x = me.dx = x; me.y = me.dy = y; me.left = left; me.moving = moving; me.heard = 0;
  const int sq = sy * 6 + sx;
  if (sq != n.prevSq) { n.prevSq = sq; n.claimT = 0; }
  if (n.stateDue) {   // a few times a second: tell everyone where I am
    n.stateDue = false;
    PkState s; hdr(s.h, PK_STATE, n); s.sx = (int8_t)sx; s.sy = (int8_t)sy; s.x = (int16_t)x; s.y = (int16_t)y; s.flags = (left ? 1 : 0) | (moving ? 2 : 0);
    plat->netSend(&s, sizeof(s));
  }
  if (n.raidStun > 0) n.raidStun -= dt;
  if (n.coop) { n.claimT = 0; return; }   // King Raid: no squares to claim
  // claiming: alone on a square that isn't mine yet, not dazed, not fighting
  bool alone = true;
  for (int i = 0; i < n.n; i++) if (i != n.mySlot && !n.p[i].gone && n.p[i].sx == sx && n.p[i].sy == sy) alone = false;
  const bool can = alone && !n.f.on && me.stun <= 0 && n.owner[sq] != n.mySlot && n.timeLeft > 0;
  if (!can) { n.claimT = 0; return; }
  n.claimT += dt;
  if (n.claimT >= NET_CLAIM_TIME) {
    if (n.isHost) n.owner[sq] = (uint8_t)n.mySlot;
    else {
      n.claimSendT -= dt;
      if (n.claimSendT <= 0) { n.claimSendT = 0.4f; PkClaim c; hdr(c.h, PK_CLAIM, n); c.square = (uint8_t)sq; plat->netSend(&c, sizeof(c)); }
    }
    if (n.claimT - dt < NET_CLAIM_TIME) { sound(SND_COIN); netSay(n, "Square claimed!"); }
  }
}

void Game::netSendRoster(bool started) {
  NetState& n = net_;
  PkRoster r; memset(&r, 0, sizeof(r)); hdr(r.h, PK_ROSTER, n);
  r.started = started; r.n = (uint8_t)n.n; r.seed = n.seed; r.matchSec = (uint16_t)NET_MATCH_TIME; r.coop = n.coop;
  for (int i = 0; i < n.n; i++) { r.p[i].id = n.p[i].id; memcpy(r.p[i].name, n.p[i].name, 12); r.p[i].breed = n.p[i].breed; r.p[i].fat10 = n.p[i].fat10; }
  plat->netSend(&r, sizeof(r));
}

// ------------------------------------------------------------------------------------------- drawing (Explore)

// the other players on my square: their cat, a ring in their colour, their name
void Game::netDrawPeers(Canvas& cv, int sx, int sy, float t) {
  const NetState& n = net_;
  if (n.mode != NM_MATCH) return;
  if (n.coop && n.bossHp > 0 && sy * 6 + sx == n.kingSq) {   // the Alley King, prowling this square
    const int x = 92 + (int)(sinf(t * 0.7f) * 18), y = 96;
    cv.ellipse(x, y + 1, 9, 3, rgb(30, 20, 36));
    drawMiniCat(cv, x, y, sinf(t * 0.7f) < 0, t, 2, 1.0f, true);
    cv.rect(x - 4, y - 16, 9, 3, rgb(250, 210, 60)); cv.set(x - 4, y - 18, rgb(250, 210, 60)); cv.set(x, y - 19, rgb(250, 210, 60)); cv.set(x + 4, y - 18, rgb(250, 210, 60));
    cv.textCenter(x, y - 28, "ALLEY KING", rgb(250, 210, 60));
  }
  for (int i = 0; i < n.n; i++) {
    const NetPeer& p = n.p[i];
    if (i == n.mySlot || p.gone || p.sx != sx || p.sy != sy) continue;
    const int x = (int)p.dx, y = (int)p.dy + 4;
    cv.ellipse(x, y + 1, 6, 2, SLOT_COL[i]);
    cv.ellipse(x, y + 1, 4, 1, rgb(30, 30, 30));
    drawMiniCat(cv, x, y, p.left, t + i, p.breed, p.fat10 / 10.0f, p.moving);
    cv.textCenter(x, y - 22, p.name, SLOT_COL[i]);
    if (p.stun > 0) for (int k = 0; k < 3; k++) {   // dazed: little stars round their head
      const float a = t * 4 + k * 2.1f;
      cv.set(x + (int)(cosf(a) * 6), y - 14 + (int)(sinf(a) * 2), rgb(255, 240, 150));
    }
  }
}

// the square's owner (a frame round the play area), how the claim is going, the clock and a mini map
void Game::netDrawHud(Canvas& cv, int sx, int sy, int ox, int oy, int pw, int ph) {
  const NetState& n = net_;
  if (n.mode != NM_MATCH) return;
  const int sq = sy * 6 + sx;
  if (n.owner[sq] < NET_MAX) { cv.frame(ox, oy, pw, ph, SLOT_COL[n.owner[sq]]); cv.frame(ox + 1, oy + 1, pw - 2, ph - 2, SLOT_COL[n.owner[sq]]); }
  // top bar: time left, my squares, the map of who owns what
  cv.rect(60, 0, W - CORNER - 60, 23, rgb(40, 30, 62));   // (BACK stays on the left)
  char b[16];
  const int tl = n.timeLeft > 0 ? (int)ceilf(n.timeLeft) : 0;
  snprintf(b, sizeof(b), "%d:%02d", tl / 60, tl % 60);
  cv.text(64, 3, b, tl <= 30 ? rgb(255, 140, 140) : rgb(236, 228, 250));
  if (n.coop) {   // King Raid: the King's health, where he is, and everyone's hits
    const int mx = W - CORNER - 2 - 6 * 6, my = 2;
    cv.text(64, 13, "KING", rgb(250, 210, 60));
    const int bx = 64 + 27, bw = mx - 4 - bx;
    cv.rect(bx, 14, bw, 5, rgb(24, 18, 40));
    cv.rect(bx, 14, n.bossMax > 0 ? bw * n.bossHp / n.bossMax : 0, 5, rgb(250, 120, 90));
    for (int y = 0; y < 5; y++) for (int x = 0; x < 6; x++) {
      const bool king = y * 6 + x == n.kingSq && n.bossHp > 0;
      cv.rect(mx + x * 6, my + y * 4, 5, 3, king ? (((int)(ex_.t * 3) & 1) ? rgb(250, 210, 60) : rgb(200, 120, 40)) : rgb(70, 60, 96));
    }
    for (int i = 0; i < n.n; i++) {
      const NetPeer& p = n.p[i];
      if (p.gone) continue;
      if (i == n.mySlot && ((int)(ex_.t * 4) & 1)) continue;
      cv.set(mx + p.sx * 6 + 2, my + p.sy * 4 + 1, i == n.mySlot ? rgb(255, 255, 255) : SLOT_COL[i]);
    }
    cv.rect(0, oy + ph + 1, W, H - oy - ph - 1, rgb(40, 30, 62));
    char line[48];
    const bool here = sy * 6 + sx == n.kingSq;
    if (n.msgT > 0) snprintf(line, sizeof(line), "%s", n.msg);
    else if (ex_.msgT > 0) snprintf(line, sizeof(line), "%s", ex_.msg);
    else if (n.raidStun > 0) snprintf(line, sizeof(line), "Knocked out... %.0fs", ceilf(n.raidStun));
    else if (here) snprintf(line, sizeof(line), "The King is HERE!");
    else snprintf(line, sizeof(line), "Find the King (gold on the map)");
    cv.textCenter(W / 2, oy + ph + 3, line, (n.msgT > 0 || ex_.msgT > 0) ? rgb(255, 240, 160) : rgb(200, 190, 230));
    int x = CORNER + 4;
    for (int i = 0; i < n.n; i++) {
      const NetPeer& p = n.p[i];
      cv.rect(x, 205, 6, 6, p.gone ? rgb(80, 70, 100) : SLOT_COL[i]);
      snprintf(b, sizeof(b), "%d", i == n.mySlot && n.myHits > n.hits[i] ? n.myHits : n.hits[i]);
      cv.text(x + 8, 204, b, p.gone ? rgb(110, 100, 130) : rgb(236, 228, 250));
      x += 34;
    }
    return;
  }
  int cnt[NET_MAX] = {0};
  for (int s = 0; s < NET_SQ; s++) if (n.owner[s] < NET_MAX) cnt[n.owner[s]]++;
  if (n.mySlot >= 0) {
    cv.rect(64, 13, 6, 6, SLOT_COL[n.mySlot]);
    snprintf(b, sizeof(b), "x%d", cnt[n.mySlot]);
    cv.text(73, 13, b, rgb(236, 228, 250));
  }
  const int mx = W - CORNER - 2 - 6 * 6, my = 2;   // 6x5 squares, 5x3 px each
  for (int y = 0; y < 5; y++) for (int x = 0; x < 6; x++) {
    const uint8_t o = n.owner[y * 6 + x];
    cv.rect(mx + x * 6, my + y * 4, 5, 3, o < NET_MAX ? SLOT_COL[o] : rgb(70, 60, 96));
  }
  for (int i = 0; i < n.n; i++) {   // everyone's dot (mine blinks)
    const NetPeer& p = n.p[i];
    if (p.gone) continue;
    if (i == n.mySlot && ((int)(ex_.t * 4) & 1)) continue;
    cv.set(mx + p.sx * 6 + 2, my + p.sy * 4 + 1, i == n.mySlot ? rgb(255, 255, 255) : rgb(20, 16, 30));
  }
  // bottom: the claim, or whose square this is, and the score of everyone
  cv.rect(0, oy + ph + 1, W, H - oy - ph - 1, rgb(40, 30, 62));
  char line[48];
  const NetPeer* me = n.mySlot >= 0 ? &n.p[n.mySlot] : nullptr;
  if (n.msgT > 0) snprintf(line, sizeof(line), "%s", n.msg);
  else if (ex_.msgT > 0) snprintf(line, sizeof(line), "%s", ex_.msg);
  else if (me && me->stun > 0) snprintf(line, sizeof(line), "Dazed... %.0fs", ceilf(me->stun));
  else if (n.owner[sq] == n.mySlot) snprintf(line, sizeof(line), "Your square!");
  else if (n.claimT > 0) snprintf(line, sizeof(line), "Claiming...");
  else if (n.owner[sq] < NET_MAX) snprintf(line, sizeof(line), "%s's square: stay to claim", n.p[n.owner[sq]].name);
  else snprintf(line, sizeof(line), "Stay here to claim it");
  cv.textCenter(W / 2, oy + ph + 3, line, (n.msgT > 0 || ex_.msgT > 0) ? rgb(255, 240, 160) : rgb(200, 190, 230));
  if (n.claimT > 0 && n.owner[sq] != n.mySlot && me) {
    const int bw = 100, bx = W / 2 - bw / 2, by = oy + ph + 12;
    cv.rect(bx, by, bw, 4, rgb(24, 18, 40));
    cv.rect(bx, by, (int)(bw * clampf(n.claimT / NET_CLAIM_TIME, 0, 1)), 4, SLOT_COL[n.mySlot]);
  }
  int x = CORNER + 4;
  for (int i = 0; i < n.n; i++) {
    const NetPeer& p = n.p[i];
    cv.rect(x, 205, 6, 6, p.gone ? rgb(80, 70, 100) : SLOT_COL[i]);
    snprintf(b, sizeof(b), "%d", cnt[i]);
    cv.text(x + 8, 204, b, p.gone ? rgb(110, 100, 130) : rgb(236, 228, 250));
    x += 34;
  }
}

// The fight with another player, drawn over the world. Both of you swing at the same moment.
bool Game::netDrawFight(Canvas& cv) {
  NetState& n = net_;
  NetFight& f = n.f;
  if (!f.on || n.mode != NM_MATCH) return false;
  const NetPeer& foe = n.p[f.foeSlot];
  const Color mc = n.mySlot >= 0 ? SLOT_COL[n.mySlot] : rgb(255, 255, 255), fc = SLOT_COL[f.foeSlot];
  cv.dimRect(0, 24, W, 160, 45);
  cv.panel(6, 34, 172, 148, rgb(48, 36, 74), rgb(130, 100, 170));
  const char* title = f.done ? (f.iWon ? "YOU WIN THE SQUARE!" : "YOU LOST THIS ONE") : "CAT FIGHT!";
  cv.textCenter(W / 2, 40, title, rgb(250, 230, 150));
  // the two cats
  { CatDraw d; d.breed = pet.lookBreed(); d.fat = pet.fatLook(); d.size = 0.8f; d.kit = pet.kitFactor(); d.t = ex_.t;
    d.pose = f.roundWinner == 1 && f.showT > 0 ? POSE_POUNCE : POSE_SIT; d.face = f.done && !f.iWon ? FACE_SAD : FACE_ANGRY;
    fitCatHeight(d, 104, 50);
    drawCat(cv, 52 + (d.pose == POSE_POUNCE ? 8 : 0), 104, d); }
  { CatDraw d; d.breed = foe.breed; d.fat = foe.fat10 / 10.0f; d.size = 0.8f; d.t = ex_.t + 1; d.faceLeft = true;
    d.pose = f.roundWinner == 2 && f.showT > 0 ? POSE_POUNCE : POSE_SIT; d.face = f.done && f.iWon ? FACE_SAD : FACE_ANGRY;
    fitCatHeight(d, 104, 50);
    drawCat(cv, 132 - (d.pose == POSE_POUNCE ? 8 : 0), 104, d); }
  cv.textCenter(48, 50, pet.d.name, mc);
  cv.textCenter(136, 50, foe.name, fc);
  for (int i = 0; i < 2; i++) {   // rounds won (2 wins it)
    gdisc(cv, 38 + i * 10, 60, 3, i < f.myWins ? mc : rgb(70, 60, 90));
    gdisc(cv, 146 - i * 10, 60, 3, i < f.foeWins ? fc : rgb(70, 60, 90));
  }
  if (f.done) {
    cv.textCenter(W / 2, 118, f.iWon ? "The square is yours." : "Dazed for a few seconds...", f.iWon ? rgb(160, 255, 180) : rgb(255, 170, 170));
    return true;
  }
  // the bar: my green zone, the sweeping line
  const int bx = 22, bw = 140, by = 132;
  cv.rect(bx, by, bw, 10, rgb(50, 40, 70));
  cv.rect(bx + (int)((f.zoneC - f.zoneW) * bw), by, (int)(f.zoneW * 2 * bw), 10, rgb(70, 200, 90));
  cv.frame(bx - 1, by - 1, bw + 2, 12, rgb(20, 12, 36));
  if (f.showT <= 0) cv.rect(bx + (int)(f.marker * bw) - 1, by - 3, 3, 16, rgb(255, 255, 255));
  char b[40];
  if (f.showT > 0) {
    if (f.roundWinner == 3) snprintf(b, sizeof(b), "Both missed! Again...");
    else if (f.roundWinner == 1) snprintf(b, sizeof(b), "You take round %d!", f.round + 1);
    else snprintf(b, sizeof(b), "%s takes round %d", foe.name, f.round + 1);
    cv.textCenter(W / 2, 118, b, f.roundWinner == 1 ? rgb(160, 255, 180) : rgb(255, 200, 170));
  } else if (f.myScore >= 0) {
    snprintf(b, sizeof(b), "%s! Waiting for %s...", f.myScore > 0 ? "Hit" : "Miss", foe.name);
    cv.textCenter(W / 2, 118, b, rgb(200, 190, 230));
  } else {
    cv.textCenter(W / 2, 118, "Swing on the green, FAST!", rgb(160, 146, 190));
  }
  const bool canSwing = f.myScore < 0 && f.showT <= 0;
  if (button(cv, 22, 150, 140, 22, "SCRATCH!", rgb(200, 110, 70), canSwing) && canSwing) {
    const float off = fabsf(f.marker - f.zoneC);
    if (off < f.zoneW) {   // a hit: how close to the middle, and how quickly (fast cats get more for it)
      const float acc = 1.0f - off / f.zoneW;
      // quickness counts for less than aim, and speed only nudges it (a very fast cat used to win two fights in three)
      const float quick = clampf(1.0f - f.t / 3.0f, 0, 1) * clampf(0.8f + 0.2f * (pet.agility() - 1.0f) / 0.5f, 0.6f, 1.0f);
      f.myScore = clampf(0.75f * acc + 0.25f * quick, 0.01f, 1.0f);
      f.myScore = roundf(f.myScore * 1000.0f) / 1000.0f;   // the same precision they'll receive, so both sides agree
      sound(SND_SCRATCH);
    } else { f.myScore = 0; sound(SND_MISS); }
    f.sendT = 0;
  }
  return true;
}

// ------------------------------------------------------------------------------------------- the lobby screen

void Game::screenLink(Canvas& cv) {
  NetState& n = net_;
  drawBackdrop(cv, false);
  auto row = [&](int y, int slot, const char* name, const char* right) {
    cv.panel(14, y, 156, 16, rgb(48, 36, 74), slot >= 0 ? SLOT_COL[slot] : rgb(130, 100, 170));
    if (slot >= 0) cv.rect(19, y + 5, 6, 6, SLOT_COL[slot]);
    cv.text(30, y + 5, name, rgb(236, 228, 250));
    if (right) cv.textRight(164, y + 5, right, rgb(160, 146, 190));
  };
  if (n.mode == NM_OVER) {
    panelTitle(cv, n.coop ? "KING RAID OVER" : "MATCH OVER", 8);
    int cnt[NET_MAX] = {0};
    if (n.coop) for (int i = 0; i < n.n; i++) cnt[i] = n.hits[i];   // (raid: hairball hits on the King)
    else for (int s = 0; s < NET_SQ; s++) if (n.owner[s] < NET_MAX) cnt[n.owner[s]]++;
    int order[NET_MAX] = {0, 1, 2, 3};
    for (int a = 0; a < n.n; a++) for (int b = a + 1; b < n.n; b++) if (cnt[order[b]] > cnt[order[a]]) { int t = order[a]; order[a] = order[b]; order[b] = t; }
    for (int k = 0; k < n.n; k++) {
      const int i = order[k]; char r[16]; snprintf(r, sizeof(r), n.coop ? "%d hits" : "%d squares", cnt[i]);
      char nm[20]; snprintf(nm, sizeof(nm), "%d. %s%s", k + 1, n.p[i].name, i == n.mySlot ? " *" : "");
      row(30 + k * 20, i, nm, r);
    }
    static const char* PL[4] = {"You WON!", "2nd place!", "3rd place", "4th place"};
    if (n.coop) cv.textCenter(W / 2, 118, n.raidWon ? "KING DOWN!" : "The King got away", n.raidWon ? rgb(255, 220, 120) : rgb(255, 170, 170), n.raidWon ? 2 : 1);
    else cv.textCenter(W / 2, 118, PL[(n.place < 1 ? 1 : (n.place > 4 ? 4 : n.place)) - 1], n.place == 1 ? rgb(255, 220, 120) : rgb(200, 190, 230), n.place == 1 ? 2 : 1);
    char b[32];
    if (n.reward) { snprintf(b, sizeof(b), "+%d hairball%s", n.reward, n.reward > 1 ? "s" : ""); cv.textCenter(W / 2, 140, b, rgb(250, 230, 150)); }
    char fe[32]; snprintf(fe, sizeof(fe), "Fun +%d  Energy -%d", (int)gfun(40.0f), (int)EXPLORE_ENERGY);
    cv.textCenter(W / 2, 152, fe, rgb(160, 146, 190));
    if (n.msgT > 0) cv.textCenter(W / 2, 164, n.msg, rgb(255, 170, 170));
    if (button(cv, 52, 186, 80, 26, "OK", GREEN)) { netStop(); setScreen(S_HOME); }
    return;
  }
  // TOGETHER pressed: wait until another Kitten Keeper nearby answers (or there's a game to join)
  if (n.mode == NM_LOBBY && (!n.radio || (n.peerHeard > 3.0f && !n.nAds))) {
    const int px = 12, py = 62, pw = 160, ph = 96;
    cv.panel(px, py, pw, ph, rgb(44, 34, 70), rgb(130, 100, 170));
    cv.textCenter(W / 2, py + 8, "EXPLORE TOGETHER", rgb(250, 230, 150));
    cv.textCenter(W / 2, py + 30, "...waiting for nearby", rgb(236, 228, 250));
    cv.textCenter(W / 2, py + 41, "player(s) to respond...", rgb(236, 228, 250));
    cv.textCenter(W / 2, py + 56, n.radio ? "They tap TOGETHER too" : "(no radio on this device)", rgb(160, 146, 190));
    { const float t = anim_ * 3.0f;   // a little radar ping
      for (int k = 0; k < 3; k++) { const int r = (int)fmodf(t * 4 + k * 6, 18); if (r > 1) cv.ring(W / 2, py - 14, r, rgb(110 - r * 3, 90 - r * 2, 170 - r * 4)); }
      cv.disc(W / 2, py - 14, 2, rgb(250, 230, 150)); }
    if (button(cv, 52, py + ph - 28, 80, 22, "CANCEL", PURPLE)) { netStop(); setScreen(S_HOME); }
    return;
  }
  panelTitle(cv, "EXPLORE TOGETHER", 8);
  if (n.mode == NM_LOBBY) {
    cv.textCenter(W / 2, 24, "Up to 4 cats nearby: claim", rgb(200, 190, 230));
    cv.textCenter(W / 2, 34, "squares, fight for them!", rgb(200, 190, 230));
    if (button(cv, 30, 48, 124, 24, "HOST A GAME", GREEN)) { netHostStart(); sound(SND_BLOOP); }
    cv.textCenter(W / 2, 82, "Games nearby:", rgb(250, 230, 150));
    if (!n.nAds) {
      char dots[8] = "...."; dots[1 + ((int)(anim_ * 2) % 3)] = 0;
      char b[24]; snprintf(b, sizeof(b), "Looking%s", dots);
      cv.textCenter(W / 2, 100, b, rgb(160, 146, 190));
    }
    for (int i = 0; i < n.nAds; i++) {
      const NetAd& a = n.ads[i];
      char b[28]; snprintf(b, sizeof(b), "JOIN %s (%d/4)", a.name, a.players);
      if (button(cv, 14, 94 + i * 24, 156, 20, b, BLUE, a.open && a.players < NET_MAX)) {
        n.isHost = false; n.hostId = a.id; n.session = a.session; n.mode = NM_JOINED; n.hostHeard = 0; n.hostLeft = false;
        n.n = 0; n.mySlot = -1;
        sound(SND_BLOOP);
      }
    }
    if (n.msgT > 0) cv.textCenter(W / 2, 172, n.msg, rgb(255, 170, 170));
    if (button(cv, 52, 190, 80, 24, "BACK", PURPLE)) { netStop(); setScreen(S_HOME); }
    return;
  }
  // waiting room (host or joined)
  for (int i = 0; i < NET_MAX; i++) {
    if (i < n.n) row(26 + i * 20, i, n.p[i].name, i == 0 ? "host" : (i == n.mySlot ? "you" : nullptr));
    else row(26 + i * 20, -1, "...", nullptr);
  }
  if (n.mode == NM_HOST) {
    cv.textCenter(W / 2, 108, n.n < 2 ? "Waiting for players..." : "Ready when you are!", rgb(200, 190, 230));
    if (button(cv, 30, 118, 124, 18, n.coop ? "GAME: KING RAID" : "GAME: SQUARES", n.coop ? rgb(200, 140, 60) : BLUE)) { n.coop ^= 1; netSendRoster(false); }
    cv.textCenter(W / 2, 139, n.coop ? "Team up: beat the Alley King!" : "5 minutes. Most squares wins.", rgb(160, 146, 190));
    if (button(cv, 30, 150, 124, 26, "START!", GREEN, n.n >= 2)) {
      n.seed = mix32(n.me ^ nowMs);
      for (int k = 0; k < 4; k++) netSendRoster(true);   // started: tell everyone (a few times)
      netEnterMatch(n.seed, (int)NET_MATCH_TIME);
      return;
    }
    if (button(cv, 52, 190, 80, 24, "CANCEL", PURPLE)) { netStop(); startLink(); return; }
  } else {
    char b[32]; snprintf(b, sizeof(b), "Waiting for %s", n.n ? n.p[0].name : "the host");
    cv.textCenter(W / 2, 112, n.mySlot < 0 ? "Joining..." : b, rgb(200, 190, 230));
    cv.textCenter(W / 2, 124, "to start the match", rgb(160, 146, 190));
    cv.textCenter(W / 2, 140, n.coop ? "Game: KING RAID (co-op)" : "Game: SQUARES", n.coop ? rgb(250, 200, 120) : rgb(160, 200, 255));
    if (button(cv, 52, 190, 80, 24, "LEAVE", PURPLE)) { netStop(); startLink(); return; }
  }
}

}  // namespace cat
