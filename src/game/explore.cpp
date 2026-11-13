// EXPLORE - the end-game: a little Zelda-style world the cat walks around in.
//
//  * 6x5 screens of 11x10 tiles (16 px); the right-hand column is the city (watch out for cars!). Drag anywhere: they walk there, finding their way around
//    water, trees and fences. Walk off an edge to slide to the next screen.
//  * A river with bridges (fish swim along it and under them), a moat around the Alley King's lair.
//  * Doors and caves you walk into: three neighbours' houses (with requests), a treasure cave
//    (key!), the hermit's cave and the Alley King (boss) behind a locked gate.
//  * Weight routes: slim cats squeeze through a gap in a fence, chonks shove a boulder.
//  * Hairballs hide in tall grass, under loose rocks, in nests and chests (lying-around finds are capped per day).
//  * Critters change with the real clock and weather (fireflies at night, frogs in the rain).
//  * World map with fog + a journal of critters and places. Each screen you walk into costs 10 energy (8 with the
//    Trail Paws, won in the first fight you finish with a hairball).
//  * Every player gets their own world, made the first time they explore and kept from then on: the neighbours'
//    houses, the caves, the garden and the nook land on different screens, and the woods grow differently. Home, the
//    river, downtown, the Alley King's moat and every inside (houses, caves) stay the same. One neighbour with a bed
//    is always within 2 screens of home, so a tired cat can reach a nap. Explore Together uses the classic map.
#include "game.h"
#include "gfx.h"
#include "catart.h"
#include "chibi.h"
#include "livewindow.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace cat {

namespace {

inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

constexpr int TS = 16, OX = 4, OY = 24;                  // tile size, play-area origin
constexpr int PW = EXC * TS, PH = EXR * TS;              // 176 x 160
constexpr int HOME_SX = 0, HOME_SY = 4;
constexpr int HB_DAILY_CAP = 20;                         // hairballs lying around: max finds per day

const Color C_PANEL = rgb(48, 36, 74), C_BORDER = rgb(130, 100, 170), C_TXT = rgb(236, 228, 250), C_DIM = rgb(160, 146, 190);
const Color C_GOLD = rgb(250, 230, 150), C_GREEN = rgb(60, 150, 90), C_PURP = rgb(110, 80, 160), C_RED = rgb(190, 70, 80);

enum Tile : uint8_t {
  T_GRASS, T_TALL, T_PATH, T_FLOWER, T_SAND, T_BRIDGE,                          // walkable outdoors
  T_TREE, T_NEST, T_ROCK, T_LOOSE, T_BOULDER, T_FENCE, T_GAP, T_WATER, T_LILY,  // obstacles (GAP: slim cats only)
  T_HOUSE, T_DOOR, T_CAVE, T_CLIFF, T_SIGN, T_GATE,
  T_WALL, T_FLOOR, T_CHEST, T_EXIT, T_NPC,                                      // interiors
  T_ROAD, T_WALK, T_ZEBRA, T_BLDG, T_TRASH,                                     // the city
  T_BED,                                                                         // a bed to nap in (houses, the hermit's cave)
  T_BUSH,                                                                        // a thick bush (one of them hides a secret cave: tap it)
  T_HOLE,                                                                        // a crack in the Old Cave's floor: step in to drop into the Deep Tunnels
  T_CRACK,                                                                       // an odd, cracked bit of wall (the Hermit's cave, behind him): tap it for a secret
  T_DECOR                                                                        // furniture in a house (see HOUSE_DECOR): floor underneath, not walkable
};

// places (journal bits 8+id) and which screen they're on
enum Place { P_HOME, P_GRANDMA, P_TOM, P_WHISKERS, P_TCAVE, P_HERMIT, P_KING, P_RIVER, P_MOAT, P_DOCK, P_PEN, P_NOOK, P_CITY, P_SECRET, P_DEEP, P_COUNT };   // P_DEEP: the Deep Tunnels, a dark maze under the Old Cave (not on the map or in the journal)   // P_SECRET: the Hidden Hermit (behind a bush)
const char* PLACE_NAME[P_COUNT] = {"Home", "Grandma's", "Old Tom's", "Whiskers'", "Old Cave", "Hermit's Cave",
                                   "King's Lair", "The River", "The Moat", "Fishing Spot", "Fenced Garden", "Boulder Nook", "Downtown", "Hidden Cave", "Deep Tunnels"};
enum Critter { CR_MOUSE, CR_BIRD, CR_FROG, CR_FIREFLY, CR_BUTTERFLY, CR_FISH, CR_PIGEON, CR_COUNT };
const char* CRITTER_NAME[CR_COUNT] = {"Mouse", "Bird", "Frog", "Firefly", "Butterfly", "Fish", "Pigeon"};
enum { MOB_WILDCAT = 10, MOB_CAR = 11 };
constexpr int CITY_SX = EXW - 1;
enum Chest { CH_KEY, CH_CAVEGOLD, CH_PEN, CH_NOOK, CH_CRYSTAL };

uint32_t hsh(int a, int b, int c = 0) {
  uint32_t h = (uint32_t)a * 73856093u ^ (uint32_t)b * 19349663u ^ (uint32_t)c * 83492791u;
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  return h;
}

bool walkable(uint8_t t) {
  return t == T_GRASS || t == T_TALL || t == T_PATH || t == T_FLOWER || t == T_SAND || t == T_BRIDGE ||
         t == T_DOOR || t == T_CAVE || t == T_FLOOR || t == T_EXIT || t == T_GAP || t == T_GATE ||
         t == T_ROAD || t == T_WALK || t == T_ZEBRA || t == T_BED || t == T_HOLE;
}

void carve(uint8_t m[EXR][EXC], int x0, int y0, int x1, int y1) {   // L-shaped path (vertical first)
  int x = x0, y = y0;
  while (y != y1) { if (m[y][x] != T_DOOR && m[y][x] != T_CAVE) m[y][x] = T_PATH; y += y1 > y ? 1 : -1; }
  while (x != x1) { if (m[y][x] != T_DOOR && m[y][x] != T_CAVE) m[y][x] = T_PATH; x += x1 > x ? 1 : -1; }
  if (m[y][x] != T_DOOR && m[y][x] != T_CAVE) m[y][x] = T_PATH;
}

void house(uint8_t m[EXR][EXC], int c, int r) {   // 3x2 building, door at bottom centre
  for (int y = r; y < r + 2; y++) for (int x = c; x < c + 3; x++) m[y][x] = T_HOUSE;
  m[r + 1][c + 1] = T_DOOR;
}

void cave(uint8_t m[EXR][EXC], int c, int r) {    // cave mouth in a little cliff
  for (int x = c - 1; x <= c + 1; x++) { m[r - 1][x] = T_CLIFF; }
  m[r][c - 1] = T_CLIFF; m[r][c + 1] = T_CLIFF; m[r][c] = T_CAVE;
}

// ---- the world layout: which screen each movable place is on ----
struct Layout { int8_t at[P_COUNT][2]; uint32_t salt; };   // at = {sx, sy} or {-1, -1}; salt varies the woods
Layout gLay; uint32_t gLaySeed = 0xFFFFFFFFu;
void setLayout(uint32_t seed) {   // 0 / 1: the classic map; anything else: that player's own world
  if (seed == gLaySeed) return;
  gLaySeed = seed;
  Layout& L = gLay;
  for (int p = 0; p < P_COUNT; p++) L.at[p][0] = L.at[p][1] = -1;
  auto put = [&](int p, int sx, int sy) { L.at[p][0] = (int8_t)sx; L.at[p][1] = (int8_t)sy; };
  put(P_HOME, HOME_SX, HOME_SY); put(P_KING, 4, 0);   // (these never move)
  if (seed <= 1) {
    L.salt = 0;
    put(P_GRANDMA, 1, 3); put(P_TOM, 3, 2); put(P_WHISKERS, 0, 1); put(P_TCAVE, 1, 1);
    put(P_HERMIT, 3, 4); put(P_SECRET, 4, 2); put(P_PEN, 0, 2); put(P_NOOK, 3, 3);
    return;
  }
  uint32_t r = seed;
  auto rnd = [&](int n) { r = r * 1664525u + 1013904223u; return (int)((r >> 8) % (uint32_t)n); };
  L.salt = seed % 997u + 1;
  // the screens they can go on: not home, the river (column 2), downtown (column 5) or the Alley King's moat
  int8_t free[EXW * EXH][2]; int nf = 0;
  for (int sy = 0; sy < EXH; sy++) for (int sx = 0; sx < EXW; sx++) {
    if (sx == 2 || sx == CITY_SX || (sx == HOME_SX && sy == HOME_SY) || (sx == 4 && sy == 0)) continue;
    free[nf][0] = (int8_t)sx; free[nf][1] = (int8_t)sy; nf++;
  }
  auto take = [&](int i, int p) { put(p, free[i][0], free[i][1]); free[i][0] = free[nf - 1][0]; free[i][1] = free[nf - 1][1]; nf--; };
  // first: one neighbour with a bed, within 2 screens of home
  static const int HOUSES[3] = {P_GRANDMA, P_TOM, P_WHISKERS};
  int near[8], nn = 0;
  for (int i = 0; i < nf; i++) { const int d = abs(free[i][0] - HOME_SX) + abs(free[i][1] - HOME_SY); if (d <= 2) near[nn++] = i; }
  const int first = HOUSES[rnd(3)];
  take(near[rnd(nn)], first);
  static const int REST[7] = {P_GRANDMA, P_TOM, P_WHISKERS, P_TCAVE, P_HERMIT, P_SECRET, P_PEN};
  for (int k = 0; k < 7; k++) if (REST[k] != first) take(rnd(nf), REST[k]);
  take(rnd(nf), P_NOOK);
}
bool isAt(int p, int sx, int sy) { return gLay.at[p][0] == sx && gLay.at[p][1] == sy; }

// which place a door / cave on this screen leads to (-1 none)
int placeAt(int sx, int sy) {
  static const int DOORS[8] = {P_HOME, P_GRANDMA, P_TOM, P_WHISKERS, P_TCAVE, P_HERMIT, P_KING, P_SECRET};   // (P_SECRET: only once the bush is moved, ex.flags bit 5)
  for (int i = 0; i < 8; i++) if (isAt(DOORS[i], sx, sy)) return DOORS[i];
  return -1;
}

void buildScreen(uint8_t m[EXR][EXC], int sx, int sy, uint8_t flags) {
  for (int y = 0; y < EXR; y++) for (int x = 0; x < EXC; x++) {
    uint32_t h = hsh(sx * 16 + x, sy * 16 + y, 5 + (int)gLay.salt * 7);
    uint32_t clump = hsh((sx * 16 + x) / 2, (sy * 16 + y) / 2, 9 + (int)gLay.salt * 7);
    uint8_t t = T_GRASS;
    int r = (int)(h % 100);
    if (clump % 100 < 22) t = T_TALL;
    else if (r < 9) t = T_TREE;
    else if (r < 13) t = T_ROCK;
    else if (r < 20) t = T_FLOWER;
    m[y][x] = t;
  }
  // world edge: a wall of trees
  for (int y = 0; y < EXR; y++) { if (sx == 0) m[y][0] = T_TREE; if (sx == EXW - 1) m[y][EXC - 1] = T_TREE; }
  for (int x = 0; x < EXC; x++) { if (sy == 0) m[0][x] = T_TREE; if (sy == EXH - 1) m[EXR - 1][x] = T_TREE; }
  // lattice of paths joins every screen to its neighbours (row 5 across, column 5 down)
  const bool moat = sx == 4 && sy == 0;
  if (!moat) {
    // (at the edge of the world the path stops at the crossroads instead of running into the wall of trees)
    for (int x = (sx == 0 ? 5 : 0); x <= (sx == EXW - 1 ? 5 : EXC - 1); x++) m[5][x] = T_PATH;
    for (int y = (sy == 0 ? 5 : 0); y <= (sy == EXH - 1 ? 5 : EXR - 1); y++) m[y][5] = T_PATH;
  }
  // the river runs down the middle column of screens, with a bridge where the path crosses
  if (sx == 2) {
    for (int y = 0; y < EXR; y++) {
      for (int x = 7; x <= 8; x++) m[y][x] = (hsh(x, y + sy * 10, 31) % 100 < 12) ? T_LILY : T_WATER;
      if (m[y][6] != T_PATH) m[y][6] = T_SAND;
      if (m[y][9] != T_PATH) m[y][9] = T_SAND;
    }
    m[5][7] = m[5][8] = T_BRIDGE;
    if (sy == 2) { m[2][4] = T_SIGN; carve(m, 5, 2, 5, 2); m[2][5] = T_PATH; }
  }
  // the moat around the Alley King's lair (top-right screen)
  if (moat) {
    for (int y = 1; y < EXR - 1; y++) for (int x = 0; x < EXC - 1; x++) if (m[y][x] == T_PATH) m[y][x] = T_GRASS;
    for (int x = 0; x <= 1; x++) m[5][x] = T_PATH;
    for (int y = 5; y <= 8; y++) m[y][1] = T_PATH;
    for (int x = 1; x <= 5; x++) m[8][x] = T_PATH;
    m[9][5] = T_PATH; m[7][5] = T_PATH;
    for (int y = 1; y <= 6; y++) for (int x = 2; x <= 8; x++) {
      bool ring = y == 1 || y == 6 || x == 2 || x == 8;
      m[y][x] = ring ? T_WATER : T_GRASS;
    }
    cave(m, 5, 3);
    m[6][5] = T_BRIDGE;
    m[5][5] = (flags & 2) ? T_PATH : T_GATE;
    m[4][5] = T_PATH;
    m[7][3] = T_SIGN;
    for (int x = 5; x <= 9; x++) m[8][x] = T_PATH;   // east to the city
    for (int y = 5; y <= 8; y++) m[y][9] = T_PATH;
    m[5][10] = T_PATH;
  }
  // the city: buildings, sidewalks, a two-lane road with a crosswalk, an alley between screens.
  // On the forest side there's a grassy verge and a sidewalk first, so you never walk straight into traffic.
  if (sx == CITY_SX) {
    for (int y = 0; y < EXR; y++) for (int x = 0; x < EXC; x++) {
      uint8_t t = T_BLDG;
      if (y == 3 || y == 6) t = T_WALK;
      if (y == 4 || y == 5) t = x == 5 ? T_ZEBRA : T_ROAD;
      if (x == 5 && (y < 3 || y > 6)) t = T_WALK;
      if (x == 0) t = (hsh(x, y + sy * 10, 57) % 100 < 20) ? T_FLOWER : T_GRASS;   // the outskirts
      if (x == 1) t = T_WALK;                                                       // a sidewalk along them
      if (x == EXC - 1 && y != 4 && y != 5) t = T_BLDG;   // (the road carries on east, off the screen)
      if ((sy == 0 && y == 0) || (sy == EXH - 1 && y == EXR - 1)) t = T_BLDG;
      m[y][x] = t;
    }
    m[3][2] = T_TRASH; m[6][8] = T_TRASH;
    if (sy == 2) m[3][1] = T_SIGN;
    return;
  }
  // houses, caves and the home
  if (sx == HOME_SX && sy == HOME_SY) { house(m, 1, 1); carve(m, 2, 3, 5, 5); m[3][7] = T_SIGN; }
  if (isAt(P_GRANDMA, sx, sy)) { house(m, 7, 1); carve(m, 8, 3, 5, 5); }
  if (isAt(P_TOM, sx, sy)) { house(m, 1, 6); carve(m, 2, 8, 5, 8); carve(m, 5, 8, 5, 5); }   // (the door faces down: a path from it round to the main path)
  if (isAt(P_WHISKERS, sx, sy)) { house(m, 1, 1); carve(m, 2, 3, 5, 5); }
  if (isAt(P_TCAVE, sx, sy)) { cave(m, 3, 2); carve(m, 3, 3, 5, 5); }
  if (isAt(P_HERMIT, sx, sy)) { cave(m, 8, 2); carve(m, 8, 3, 5, 5); }
  // the SECRET: no path leads here. A thick bush tucked between trees hides a cave; tap it to find the Hidden Hermit.
  if (isAt(P_SECRET, sx, sy)) {
    if (flags & 32) cave(m, 8, 2);
    else { m[1][7] = m[1][8] = m[1][9] = T_TREE; m[2][7] = m[2][9] = T_TREE; m[2][8] = T_BUSH; }
    m[3][7] = m[3][8] = m[3][9] = m[4][7] = m[4][8] = T_GRASS;   // room to stand in front of it
  }
  // slim-cats-only garden: a fence with one narrow gap, a chest inside
  if (isAt(P_PEN, sx, sy)) {
    for (int y = 6; y <= 9; y++) for (int x = 7; x <= 10; x++) m[y][x] = (y == 6 || y == 9 || x == 7 || x == 10) ? T_FENCE : T_FLOWER;
    m[7][7] = T_GAP; m[8][9] = T_CHEST; m[7][6] = T_GRASS; m[6][6] = T_SIGN;
  }
  // chonk-only nook: a boulder blocks the way to a chest
  if (isAt(P_NOOK, sx, sy)) {
    for (int y = 6; y <= 8; y++) for (int x = 7; x <= 9; x++) m[y][x] = T_ROCK;
    m[7][8] = T_CHEST;
    m[7][7] = (flags & 8) ? T_GRASS : T_BOULDER;
    m[7][6] = T_GRASS; m[8][6] = T_SIGN;
  }
  // one loose rock and one nest per screen (searched once a day)
  bool rock = false, nest = false;
  for (int i = 0; i < EXR * EXC; i++) {
    int k = (int)((hsh(sx, sy, 3) + (uint32_t)i * 37u) % (uint32_t)(EXR * EXC));
    int y = k / EXC, x = k % EXC;
    if (x == 0 || y == 0 || x == EXC - 1 || y == EXR - 1) continue;
    if (!rock && m[y][x] == T_ROCK) { m[y][x] = T_LOOSE; rock = true; }
    if (!nest && m[y][x] == T_TREE) { m[y][x] = T_NEST; nest = true; }
  }
}

// ---- the neighbours' houses: furniture and things on the walls ----
// Row 0 is the back wall (windows, pictures, a clock); everything else stands on the floor and blocks the way.
// The middle column (the way in) and the tiles round the neighbour and the bed are kept clear.
enum DecorKind : uint8_t { DK_WINDOW, DK_PICTURE, DK_CLOCK, DK_COUNTER, DK_SINK, DK_STOVE, DK_PLANT, DK_TABLE, DK_CHAIR,
                           DK_SHELF, DK_LAMP, DK_SOFA, DK_FLOWERS, DK_END };
struct Decor { uint8_t kind, x, y; };
const Decor DECOR_GRANDMA[] = {   // a cosy kitchen
  {DK_WINDOW, 2, 0}, {DK_PICTURE, 5, 0}, {DK_WINDOW, 8, 0},
  {DK_COUNTER, 1, 1}, {DK_SINK, 2, 1}, {DK_STOVE, 3, 1},
  {DK_CHAIR, 1, 6}, {DK_TABLE, 2, 6}, {DK_TABLE, 3, 6}, {DK_PLANT, 1, 8}, {DK_PLANT, 9, 8}, {DK_FLOWERS, 9, 5}, {DK_END, 0, 0}};
const Decor DECOR_TOM[] = {       // a cluttered old den
  {DK_WINDOW, 2, 0}, {DK_CLOCK, 5, 0}, {DK_WINDOW, 8, 0},
  {DK_SHELF, 1, 1}, {DK_SHELF, 2, 1}, {DK_LAMP, 3, 1},
  {DK_SOFA, 1, 6}, {DK_SOFA, 2, 6}, {DK_TABLE, 8, 6}, {DK_CHAIR, 9, 6}, {DK_PLANT, 9, 8}, {DK_END, 0, 0}};
const Decor DECOR_WHISKERS[] = {  // bright and pretty
  {DK_WINDOW, 2, 0}, {DK_PICTURE, 5, 0}, {DK_WINDOW, 8, 0},
  {DK_COUNTER, 1, 1}, {DK_SINK, 2, 1}, {DK_FLOWERS, 3, 1},
  {DK_SOFA, 1, 6}, {DK_SOFA, 2, 6}, {DK_PLANT, 1, 8}, {DK_PLANT, 9, 8}, {DK_PLANT, 9, 5}, {DK_LAMP, 9, 1}, {DK_END, 0, 0}};
const Decor* houseDecor(int place) {
  return place == P_GRANDMA ? DECOR_GRANDMA : (place == P_TOM ? DECOR_TOM : (place == P_WHISKERS ? DECOR_WHISKERS : nullptr));
}
// One piece of furniture on its tile (x, y = the tile's top-left corner).
int decorAt(const Decor* L, int x, int y) { for (; L && L->kind != DK_END; L++) if (L->x == x && L->y == y) return L->kind; return -1; }
void drawDecor(Canvas& cv, uint8_t kind, int x, int y, const Decor* L, int tx, int ty) {
  const Color ol = rgb(60, 36, 28), wood = rgb(150, 98, 58), woodD = rgb(110, 70, 40), woodL = rgb(186, 132, 84);
  switch (kind) {
    case DK_WINDOW: {   // a window onto the garden, with a sill
      cv.rect(x + 1, y + 2, 14, 11, ol); cv.rect(x + 2, y + 3, 12, 9, rgb(150, 206, 240));
      cv.rect(x + 2, y + 9, 12, 3, rgb(120, 190, 110));                              // the garden outside
      cv.vline(x + 8, y + 3, 9, ol); cv.hline(x + 2, y + 7, 12, ol);                  // the frame
      cv.set(x + 4, y + 4, rgb(255, 255, 255)); cv.set(x + 5, y + 5, rgb(230, 245, 255));
      cv.rect(x, y + 13, 16, 2, woodL); cv.hline(x, y + 15, 16, woodD);              // sill
      break; }
    case DK_PICTURE: {  // a little framed landscape
      cv.rect(x + 3, y + 3, 10, 9, rgb(200, 160, 60)); cv.rect(x + 4, y + 4, 8, 7, rgb(170, 210, 245));
      cv.rect(x + 4, y + 8, 8, 3, rgb(110, 170, 90)); cv.disc(x + 10, y + 6, 1, rgb(255, 230, 120));
      cv.tri(x + 4, y + 8, x + 7, y + 5, x + 9, y + 8, rgb(130, 140, 160));
      break; }
    case DK_CLOCK: {    // a round wall clock
      cv.disc(x + 8, y + 7, 5, ol); cv.disc(x + 8, y + 7, 4, rgb(250, 246, 232));
      cv.vline(x + 8, y + 4, 4, ol); cv.hline(x + 8, y + 7, 3, ol);
      break; }
    case DK_COUNTER: case DK_SINK: case DK_STOVE: {   // kitchen units against the back wall
      const bool stove = kind == DK_STOVE;
      cv.rect(x, y + 3, 16, 13, stove ? rgb(220, 220, 228) : rgb(232, 222, 200));
      cv.rect(x, y + 3, 16, 3, stove ? rgb(70, 70, 80) : rgb(176, 168, 160));        // the worktop
      cv.hline(x, y + 15, 16, woodD);
      cv.frame(x + 2, y + 7, 5, 7, rgb(170, 160, 150)); cv.frame(x + 9, y + 7, 5, 7, rgb(170, 160, 150));   // cupboard doors
      cv.set(x + 6, y + 10, rgb(120, 110, 100)); cv.set(x + 9, y + 10, rgb(120, 110, 100));
      if (kind == DK_SINK) {
        cv.ellipse(x + 8, y + 4, 5, 1, rgb(120, 140, 160));                            // the basin
        cv.vline(x + 8, y + 0, 3, rgb(180, 190, 200)); cv.hline(x + 8, y + 0, 3, rgb(180, 190, 200));   // the tap
      } else if (stove) {
        cv.ring(x + 4, y + 4, 2, rgb(150, 60, 50)); cv.ring(x + 11, y + 4, 2, rgb(150, 60, 50));       // burners
        cv.frame(x + 3, y + 8, 10, 5, ol);                                                              // the oven door
      } else {
        cv.rect(x + 3, y + 1, 4, 3, rgb(240, 240, 250)); cv.frame(x + 3, y + 1, 4, 3, rgb(150, 150, 170));   // a mug
        cv.rect(x + 9, y + 0, 4, 4, rgb(214, 120, 60));                                                   // a jar
      }
      break; }
    case DK_PLANT: {    // a big potted plant
      cv.tri(x + 4, y + 10, x + 12, y + 10, x + 10, y + 15, rgb(190, 100, 60)); cv.tri(x + 4, y + 10, x + 10, y + 15, x + 6, y + 15, rgb(190, 100, 60));
      cv.hline(x + 4, y + 10, 9, rgb(150, 76, 44));
      cv.ellipse(x + 8, y + 6, 5, 4, rgb(60, 140, 70)); cv.ellipse(x + 5, y + 4, 3, 3, rgb(80, 170, 80)); cv.ellipse(x + 11, y + 4, 3, 3, rgb(70, 160, 76));
      cv.ellipse(x + 8, y + 2, 2, 2, rgb(100, 190, 96));
      break; }
    case DK_FLOWERS: {  // a vase of flowers on a little stand
      cv.rect(x + 5, y + 9, 6, 7, woodD); cv.rect(x + 4, y + 8, 8, 2, wood);
      cv.rect(x + 6, y + 4, 4, 4, rgb(120, 170, 220));
      cv.disc(x + 6, y + 2, 1, rgb(255, 120, 170)); cv.disc(x + 9, y + 2, 1, rgb(255, 220, 90)); cv.disc(x + 8, y + 1, 1, rgb(190, 140, 255));
      break; }
    case DK_TABLE: {    // a wooden table (two tiles side by side make one long table)
      const bool joinL = decorAt(L, tx - 1, ty) == kind, joinR = decorAt(L, tx + 1, ty) == kind;
      cv.rect(x + (joinL ? 0 : 1), y + 5, 16 - (joinL ? 0 : 1) - (joinR ? 0 : 1), 4, woodL);
      cv.hline(x + (joinL ? 0 : 1), y + 9, 16 - (joinL ? 0 : 1) - (joinR ? 0 : 1), woodD);
      if (!joinL) cv.rect(x + 2, y + 10, 2, 6, woodD);
      if (!joinR) cv.rect(x + 12, y + 10, 2, 6, woodD);
      cv.rect(x + 6, y + 3, 4, 2, rgb(240, 240, 250));                                  // a plate
      break; }
    case DK_CHAIR: {
      cv.rect(x + 4, y + 2, 8, 7, wood); cv.frame(x + 4, y + 2, 8, 7, woodD);           // the back
      cv.rect(x + 3, y + 9, 10, 3, woodL); cv.rect(x + 4, y + 12, 2, 4, woodD); cv.rect(x + 10, y + 12, 2, 4, woodD);
      break; }
    case DK_SHELF: {    // a bookcase full of books
      cv.rect(x + 1, y + 1, 14, 15, woodD); cv.rect(x + 2, y + 2, 12, 13, rgb(80, 52, 34));
      static const Color bk[5] = {rgb(200, 70, 70), rgb(70, 120, 200), rgb(230, 190, 80), rgb(90, 160, 90), rgb(160, 90, 170)};
      for (int s = 0; s < 3; s++) { const int sy = y + 2 + s * 4; cv.hline(x + 2, sy + 4, 12, woodL);
        for (int b = 0; b < 5; b++) cv.rect(x + 3 + b * 2 + (s & 1), sy + 1, 2, 3, bk[(b + s * 2 + tx) % 5]); }
      break; }
    case DK_LAMP: {     // a floor lamp with a warm shade
      cv.rect(x + 5, y + 15, 6, 1, ol); cv.vline(x + 8, y + 5, 10, rgb(90, 80, 70));
      cv.tri(x + 4, y + 6, x + 12, y + 6, x + 8, y + 0, rgb(250, 210, 120)); cv.hline(x + 4, y + 6, 9, rgb(220, 170, 80));
      cv.set(x + 8, y + 7, rgb(255, 250, 200));
      break; }
    case DK_SOFA: {     // a sofa (two tiles side by side make one couch)
      const bool joinL = decorAt(L, tx - 1, ty) == kind, joinR = decorAt(L, tx + 1, ty) == kind;
      const Color c = rgb(90, 130, 190), cD = rgb(60, 92, 150), cL = rgb(130, 170, 220);
      cv.rect(x + (joinL ? 0 : 1), y + 3, 16 - (joinL ? 0 : 1) - (joinR ? 0 : 1), 6, cD);     // the back
      cv.rect(x + (joinL ? 0 : 1), y + 8, 16 - (joinL ? 0 : 1) - (joinR ? 0 : 1), 6, c);      // the seat
      cv.hline(x + (joinL ? 0 : 2), y + 8, 14 - (joinL ? 0 : 1) - (joinR ? 0 : 1), cL);
      if (!joinL) cv.rect(x, y + 6, 3, 8, cD);
      if (!joinR) cv.rect(x + 13, y + 6, 3, 8, cD);
      cv.hline(x, y + 14, 16, rgb(40, 30, 30));
      if (!joinR) cv.rect(x + 9, y + 5, 3, 3, rgb(250, 200, 120));                      // a cushion
      break; }
  }
}

void buildRoom(uint8_t m[EXR][EXC], int place) {
  const bool inHouse = place == P_HOME || place == P_GRANDMA || place == P_TOM || place == P_WHISKERS;
  for (int y = 0; y < EXR; y++) for (int x = 0; x < EXC; x++)
    m[y][x] = (x == 0 || y == 0 || x == EXC - 1 || y == EXR - 1) ? T_WALL : T_FLOOR;
  m[EXR - 1][5] = T_EXIT;
  if (inHouse) {
    m[3][5] = T_NPC; if (place != P_HOME) m[2][8] = T_BED;
    for (const Decor* d = houseDecor(place); d && d->kind != DK_END; d++) if (d->y > 0) m[d->y][d->x] = T_DECOR;   // (row 0 is the wall: windows and pictures hang on it)
    return;
  }
  if (place == P_DEEP) {   // a fixed maze: rooms on the odd tiles (5 x 4), carved by a seeded "growing tree" walk (salt picked for dead ends)
    for (int y = 0; y < EXR - 1; y++) for (int x = 0; x < EXC; x++) m[y][x] = T_WALL;
    constexpr int CW = 5, CH = 4, SALT = 97;
    static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    bool vis[CH][CW] = {};
    int st[CW * CH][2], n = 0, step = 0;
    st[n][0] = 2; st[n][1] = 3; n++; vis[3][2] = true; m[7][5] = T_FLOOR;   // start: just above the way out
    while (n > 0) {
      const int i = (hsh(n, step, SALT) % 3 == 0) ? (int)(hsh(step, n, SALT + 1) % n) : n - 1;   // mostly the newest room, sometimes an old one (side passages)
      const int cx = st[i][0], cy = st[i][1];
      int opt[4], no = 0;
      for (int d = 0; d < 4; d++) {
        const int nx = cx + D[d][0], ny = cy + D[d][1];
        if (nx >= 0 && ny >= 0 && nx < CW && ny < CH && !vis[ny][nx]) opt[no++] = d;
      }
      if (!no) { for (int k = i; k < n - 1; k++) { st[k][0] = st[k + 1][0]; st[k][1] = st[k + 1][1]; } n--; continue; }
      const int d = opt[hsh(cx * 7 + cy, step++, SALT) % no];
      const int nx = cx + D[d][0], ny = cy + D[d][1];
      m[1 + cy * 2 + D[d][1]][1 + cx * 2 + D[d][0]] = T_FLOOR;   // knock down the wall between
      m[1 + ny * 2][1 + nx * 2] = T_FLOOR;
      vis[ny][nx] = true; st[n][0] = nx; st[n][1] = ny; n++;
    }
    m[8][5] = T_FLOOR;
    for (int x = 0; x < EXC; x++) if (x != 5) m[8][x] = T_WALL;
    // the crystal waits in the room farthest from the way in
    int dist[EXR][EXC]; for (auto& r : dist) for (int& v : r) v = -1;
    int q[EXR * EXC][2], qh = 0, qt = 0, bx = 5, by = 7;
    dist[8][5] = 0; q[qt][0] = 5; q[qt][1] = 8; qt++;
    while (qh < qt) {
      const int x = q[qh][0], y = q[qh][1]; qh++;
      if (dist[y][x] > dist[by][bx]) { bx = x; by = y; }
      static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
      for (auto& d : D) {
        const int nx = x + d[0], ny = y + d[1];
        if (nx < 0 || ny < 0 || nx >= EXC || ny >= EXR - 1 || m[ny][nx] != T_FLOOR || dist[ny][nx] >= 0) continue;
        dist[ny][nx] = dist[y][x] + 1; q[qt][0] = nx; q[qt][1] = ny; qt++;
      }
    }
    m[by][bx] = T_CHEST;
    return;
  }
  for (int i = 0; i < 7; i++) {   // rocky pillars
    uint32_t h = hsh(place, i, 77);
    int x = 2 + (int)(h % 7), y = 2 + (int)((h >> 8) % 5);
    if (x == 5 || y == 7) continue;
    m[y][x] = T_WALL;
  }
  if (place == P_TCAVE) { m[2][2] = T_CHEST; m[2][8] = T_CHEST; m[2][3] = m[3][2] = m[2][7] = m[3][8] = T_FLOOR; m[8][1] = T_HOLE; }   // (the hole: tucked in the corner)
  if (place == P_HERMIT) { m[3][5] = T_NPC; m[3][4] = m[3][6] = m[4][5] = T_FLOOR; m[7][8] = T_BED; m[7][7] = m[6][8] = T_FLOOR;
    m[1][5] = T_CRACK; m[1][4] = m[1][6] = m[2][4] = m[2][5] = m[2][6] = T_FLOOR; }   // the secret: a cracked lump on the back wall right behind the hermit, above his hat (never by the bed)
  if (place == P_SECRET) { m[3][5] = T_NPC; m[3][4] = m[3][6] = m[4][5] = T_FLOOR; }
  if (place == P_KING) { m[2][5] = T_NPC; m[2][4] = m[2][6] = m[3][5] = m[3][4] = m[3][6] = T_FLOOR; }
  for (int y = 4; y <= 8; y++) m[y][5] = T_FLOOR;   // a clear way in
}

int chestId(int place, int sx, int sy, int tx) {
  if (place == P_TCAVE) return tx < 5 ? CH_KEY : CH_CAVEGOLD;
  if (place == P_DEEP) return CH_CRYSTAL;
  if (place < 0 && isAt(P_PEN, sx, sy)) return CH_PEN;
  if (place < 0 && isAt(P_NOOK, sx, sy)) return CH_NOOK;
  return -1;
}

// --------------------------------------------------------------------------------- drawing
float gNight = 0;                                       // 0 day .. 1 full night (outdoors only)
struct ExLight { int16_t x, y; uint8_t r, k; };          // centre (screen px), radius (px), strength 0..255
constexpr int MAX_LIGHTS = 48, LW = PW / 2, LH = PH / 2;
ExLight gLights[MAX_LIGHTS]; int gNLights = 0;
uint8_t gLightMap[LH][LW];
void addLight(int x, int y, int r, int k) {
  if (gNight <= 0.05f || gNLights >= MAX_LIGHTS) return;
  if (x < OX - r || x > OX + PW + r || y < OY - r || y > OY + PH + r) return;
  gLights[gNLights++] = {(int16_t)x, (int16_t)y, (uint8_t)(r > 255 ? 255 : r), (uint8_t)k};
}
void drawTree(Canvas& cv, int x, int y, bool nest, bool nestFull, float t) {
  cv.ellipse(x + 8, y + 14, 6, 2, rgb(30, 70, 40));
  cv.rect(x + 7, y + 8, 3, 7, rgb(110, 70, 44));
  gdisc(cv, x + 8, y + 6, 6, rgb(46, 140, 70));
  gdisc(cv, x + 5, y + 8, 4, rgb(60, 164, 80));
  gdisc(cv, x + 11, y + 8, 4, rgb(40, 124, 64));
  if (nest) {
    cv.ellipse(x + 11, y + 4, 3, 2, rgb(120, 84, 44)); cv.hline(x + 9, y + 3, 5, rgb(160, 120, 70));
    if (nestFull && ((int)(t * 3) & 1)) cv.set(x + 13, y + 1, rgb(255, 255, 200));
  }
}

void drawTile(Canvas& cv, uint8_t t, int x, int y, int tx, int ty, int salt, float tm, bool cave, bool sparkle, bool chestOpen,
              uint8_t up, uint8_t dn, uint8_t lf, uint8_t rt) {
  const Color g1 = rgb(92, 170, 80), g2 = rgb(84, 158, 74);
  uint32_t h = hsh(tx + salt * 11, ty, 1);
  auto grass = [&]() {
    cv.rect(x, y, TS, TS, ((tx + ty) & 1) ? g1 : g2);
    cv.set(x + (int)(h % 14) + 1, y + (int)((h >> 4) % 14) + 1, rgb(70, 140, 64));
    cv.set(x + (int)((h >> 8) % 14) + 1, y + (int)((h >> 12) % 14) + 1, rgb(120, 196, 100));
  };
  switch (t) {
    case T_GRASS: grass(); break;
    case T_TALL: {
      cv.rect(x, y, TS, TS, rgb(66, 140, 64));
      for (int k = 0; k < 5; k++) {
        int bx = x + 2 + k * 3, sway = (int)(sinf(tm * 2.2f + tx + k) * 1.2f);
        cv.line(bx, y + 14, bx + sway, y + 6 + (k & 1) * 2, rgb(40, 110, 50));
        cv.line(bx + 1, y + 14, bx + 1 + sway, y + 8, rgb(104, 184, 90));
      }
      break; }
    case T_PATH: {
      cv.rect(x, y, TS, TS, rgb(206, 186, 140));
      cv.set(x + (int)(h % 14) + 1, y + (int)((h >> 4) % 14) + 1, rgb(170, 150, 110));
      cv.set(x + (int)((h >> 8) % 14) + 1, y + (int)((h >> 12) % 14) + 1, rgb(230, 214, 176));
      break; }
    case T_FLOWER: {
      grass();
      static const Color pet[3] = {rgb(255, 130, 170), rgb(255, 220, 90), rgb(190, 150, 255)};
      for (int k = 0; k < 2; k++) {
        int fx = x + 3 + (int)((h >> (k * 6)) % 10), fy = y + 3 + (int)((h >> (k * 6 + 3)) % 10);
        Color c = pet[(h >> (k + 20)) % 3];
        cv.set(fx - 1, fy, c); cv.set(fx + 1, fy, c); cv.set(fx, fy - 1, c); cv.set(fx, fy + 1, c); cv.set(fx, fy, rgb(255, 250, 220));
      }
      break; }
    case T_SAND: cv.rect(x, y, TS, TS, rgb(232, 214, 160)); cv.set(x + (int)(h % 15), y + (int)((h >> 4) % 15), rgb(206, 186, 130)); break;
    case T_WATER: case T_LILY: {
      for (int r = 0; r < TS; r++) {
        float w = sinf(tm * 2.0f + (y + r) * 0.45f + tx * 0.9f);
        cv.hline(x, y + r, TS, w > 0.82f ? rgb(150, 210, 250) : (r & 2 ? rgb(60, 130, 210) : rgb(52, 120, 200)));
      }
      // banks
      if (up != T_WATER && up != T_LILY && up != T_BRIDGE) cv.hline(x, y, TS, rgb(40, 90, 150));
      if (lf != T_WATER && lf != T_LILY && lf != T_BRIDGE) cv.vline(x, y, TS, rgb(40, 90, 150));
      if (rt != T_WATER && rt != T_LILY && rt != T_BRIDGE) cv.vline(x + TS - 1, y, TS, rgb(40, 90, 150));
      if (t == T_LILY) { cv.ellipse(x + 8, y + 9, 5, 3, rgb(60, 150, 70)); cv.set(x + 9, y + 9, rgb(52, 120, 200)); cv.disc(x + 7, y + 7, 1, rgb(255, 170, 200)); }
      break; }
    case T_BRIDGE: {
      bool acrossH = (lf == T_WATER || lf == T_LILY || lf == T_BRIDGE) && t == T_BRIDGE;   // river under us: walk left-right
      Color plank = rgb(170, 116, 70), dark = rgb(120, 78, 44);
      if (!acrossH) {   // planks run left-right, rails on the sides (crossing up/down)
        cv.rect(x, y, TS, TS, plank); for (int k = 0; k < TS; k += 4) cv.hline(x, y + k, TS, dark);
        cv.vline(x + 1, y, TS, rgb(96, 60, 34)); cv.vline(x + TS - 2, y, TS, rgb(96, 60, 34));
      } else {
        cv.rect(x, y, TS, TS, plank); for (int k = 0; k < TS; k += 4) cv.vline(x + k, y, TS, dark);
        cv.hline(x, y + 1, TS, rgb(96, 60, 34)); cv.hline(x, y + TS - 2, TS, rgb(96, 60, 34));
      }
      break; }
    case T_TREE: case T_NEST: grass(); drawTree(cv, x, y, t == T_NEST, sparkle, tm); break;
    case T_ROCK: case T_LOOSE: {
      if (cave) cv.rect(x, y, TS, TS, rgb(54, 46, 58)); else grass();
      cv.ellipse(x + 8, y + 12, 7, 3, rgb(40, 50, 40));
      gellipse(cv, x + 8, y + 9, 6, 5, rgb(140, 140, 150));
      if (t == T_LOOSE && sparkle && ((int)(tm * 3) & 1)) { cv.set(x + 12, y + 4, rgb(255, 255, 200)); cv.set(x + 13, y + 4, rgb(255, 255, 200)); }
      break; }
    case T_BOULDER: grass(); cv.ellipse(x + 8, y + 14, 8, 2, rgb(30, 40, 30)); gellipse(cv, x + 8, y + 8, 8, 7, rgb(150, 136, 120)); cv.line(x + 5, y + 5, x + 9, y + 9, rgb(100, 90, 80)); break;
    case T_FENCE: case T_GAP: {
      grass();
      Color wood = rgb(214, 190, 150), dk = rgb(150, 120, 84);
      cv.hline(x, y + 6, TS, wood); cv.hline(x, y + 11, TS, wood); cv.hline(x, y + 12, TS, dk);
      if (t == T_FENCE) { for (int k = 1; k < TS; k += 5) { cv.rect(x + k, y + 2, 2, 12, wood); cv.vline(x + k + 1, y + 2, 12, dk); } }
      else { cv.rect(x + 1, y + 2, 2, 12, wood); cv.rect(x + 13, y + 2, 2, 12, wood); cv.rect(x + 5, y + 6, 6, 7, ((tx + ty) & 1) ? g1 : g2); }   // a slat's missing
      break; }
    case T_CLIFF: cv.rect(x, y, TS, TS, rgb(120, 104, 92)); cv.hline(x, y + 5, TS, rgb(96, 82, 74)); cv.hline(x, y + 11, TS, rgb(96, 82, 74)); cv.set(x + (int)(h % 14), y + 2, rgb(150, 136, 120)); break;
    case T_CAVE: cv.rect(x, y, TS, TS, rgb(120, 104, 92)); cv.ellipse(x + 8, y + 10, 6, 8, rgb(14, 10, 20)); cv.rect(x + 2, y + 10, 12, 6, rgb(14, 10, 20)); break;
    case T_SIGN: grass(); cv.rect(x + 7, y + 8, 2, 7, rgb(110, 70, 44)); cv.rect(x + 2, y + 2, 12, 7, rgb(200, 150, 90)); cv.frame(x + 2, y + 2, 12, 7, rgb(110, 70, 44)); cv.hline(x + 4, y + 4, 8, rgb(110, 70, 44)); cv.hline(x + 4, y + 6, 6, rgb(110, 70, 44)); break;
    case T_GATE: {
      cv.rect(x, y, TS, TS, rgb(206, 186, 140));
      for (int k = 1; k < TS; k += 3) cv.vline(x + k, y, TS, rgb(70, 70, 84));
      cv.hline(x, y + 3, TS, rgb(70, 70, 84)); cv.hline(x, y + 12, TS, rgb(70, 70, 84));
      cv.disc(x + 8, y + 8, 2, rgb(230, 190, 70));   // keyhole plate
      break; }
    case T_BUSH: {   // a big round shrub with berries; the secret one gives a little rustle now and then
      grass();
      const int rs = sinf(tm * 1.3f + tx) > 0.93f ? (((int)(tm * 20)) & 1) : 0;
      cv.ellipse(x + 8, y + 14, 8, 2, rgb(30, 60, 36));
      gdisc(cv, x + 4 + rs, y + 9, 5, rgb(44, 124, 60)); gdisc(cv, x + 12 - rs, y + 9, 5, rgb(40, 116, 56));
      gdisc(cv, x + 8, y + 5, 6, rgb(56, 146, 70));
      cv.set(x + 5, y + 6, rgb(230, 70, 90)); cv.set(x + 11, y + 8, rgb(230, 70, 90)); cv.set(x + 8, y + 11, rgb(230, 70, 90));
      break; }
    case T_HOUSE: case T_DOOR: break;   // drawn as whole buildings
    case T_WALL: {
      if (cave) { cv.rect(x, y, TS, TS, rgb(70, 60, 74)); cv.set(x + (int)(h % 14), y + (int)((h >> 4) % 14), rgb(96, 86, 100)); cv.hline(x, y + TS - 1, TS, rgb(40, 34, 44)); }
      else { cv.rect(x, y, TS, TS, rgb(214, 170, 150)); for (int k = 2; k < TS; k += 6) cv.vline(x + k, y, TS, rgb(200, 150, 132)); cv.hline(x, y + TS - 3, TS, rgb(150, 100, 80)); }
      break; }
    case T_CRACK: {   // a lumpy, cracked bit of wall that bulges out; once opened, a dark hollow
      cv.rect(x, y, TS, TS, rgb(54, 46, 58));                                                   // the floor around it
      cv.ellipse(x + 8, y + 8, 7, 8, rgb(40, 34, 44)); cv.ellipse(x + 8, y + 7, 6, 7, rgb(86, 74, 90));   // the bulge
      cv.ellipse(x + 6, y + 5, 2, 2, rgb(104, 92, 108));
      if (chestOpen) { cv.ellipse(x + 8, y + 9, 4, 4, rgb(16, 12, 20)); cv.set(x + 5, y + 13, rgb(86, 74, 90)); cv.set(x + 11, y + 14, rgb(86, 74, 90)); }   // broken open
      else {
        const Color cr = rgb(30, 24, 34);   // zig-zag cracks
        cv.line(x + 8, y + 1, x + 6, y + 5, cr); cv.line(x + 6, y + 5, x + 9, y + 8, cr); cv.line(x + 9, y + 8, x + 7, y + 12, cr);
        cv.line(x + 9, y + 8, x + 13, y + 9, cr); cv.line(x + 6, y + 5, x + 3, y + 6, cr);
        if (((int)(tm * 1.5f)) % 5 == 0) cv.set(x + 10, y + 7, rgb(255, 230, 160));   // a faint glint inside, now and then
      }
      break; }
    case T_BED:
      if (cave) {   // a nest of straw
        cv.rect(x, y, TS, TS, rgb(54, 46, 58));
        cv.ellipse(x + 8, y + 9, 7, 5, rgb(190, 160, 80)); cv.ellipse(x + 8, y + 9, 5, 3, rgb(150, 120, 60));
        for (int k = 0; k < 5; k++) cv.line(x + 2 + k * 3, y + 5 + (k & 1), x + 4 + k * 3, y + 3, rgb(220, 190, 110));
      } else {      // a little bed: frame, pillow, blanket
        cv.rect(x, y, TS, TS, rgb(176, 124, 80)); for (int k = 0; k < TS; k += 5) cv.hline(x, y + k, TS, rgb(150, 104, 66));
        cv.rect(x + 1, y + 2, 14, 13, rgb(110, 70, 44));
        cv.rect(x + 2, y + 3, 12, 11, rgb(240, 236, 228));
        cv.rect(x + 3, y + 3, 10, 3, rgb(255, 255, 255)); cv.frame(x + 3, y + 3, 10, 3, rgb(200, 196, 190));
        cv.rect(x + 2, y + 7, 12, 7, rgb(120, 150, 220)); cv.hline(x + 2, y + 7, 12, rgb(160, 190, 240));
        for (int k = 0; k < 3; k++) cv.set(x + 4 + k * 4, y + 10, rgb(250, 230, 150));
      }
      break;
    case T_FLOOR: case T_NPC: case T_DECOR:   // (furniture is drawn on top: drawDecor)
      if (cave) { cv.rect(x, y, TS, TS, rgb(54, 46, 58)); cv.set(x + (int)(h % 15), y + (int)((h >> 4) % 15), rgb(70, 62, 74)); }
      else { cv.rect(x, y, TS, TS, rgb(176, 124, 80)); for (int k = 0; k < TS; k += 5) cv.hline(x, y + k, TS, rgb(150, 104, 66)); }
      break;
    case T_CHEST: {
      if (cave) cv.rect(x, y, TS, TS, rgb(54, 46, 58)); else grass();
      cv.rect(x + 2, y + 6, 12, 9, rgb(150, 96, 50)); cv.frame(x + 2, y + 6, 12, 9, rgb(80, 50, 30));
      if (chestOpen) { cv.rect(x + 2, y + 3, 12, 3, rgb(110, 70, 40)); cv.rect(x + 3, y + 7, 10, 3, rgb(30, 20, 20)); }
      else { cv.rect(x + 2, y + 4, 12, 4, rgb(176, 116, 64)); cv.rect(x + 7, y + 8, 2, 3, rgb(240, 200, 80)); if (((int)(tm * 2) & 1)) cv.set(x + 13, y + 3, rgb(255, 255, 220)); }
      break; }
    case T_ROAD: case T_ZEBRA: {
      cv.rect(x, y, TS, TS, rgb(62, 62, 72));
      cv.set(x + (int)(h % 15), y + (int)((h >> 4) % 15), rgb(80, 80, 90));
      if (up == T_WALK) cv.hline(x, y, TS, rgb(170, 170, 160));
      if (dn == T_WALK) cv.hline(x, y + TS - 1, TS, rgb(170, 170, 160));
      if (t == T_ZEBRA) for (int k = 1; k < TS; k += 4) cv.rect(x + k, y + 1, 2, TS - 2, rgb(236, 236, 236));
      else if (dn == T_ROAD || dn == T_ZEBRA) for (int k = 0; k < TS; k += 8) cv.hline(x + k, y + TS - 1, 4, rgb(250, 210, 70));   // lane line
      break; }
    case T_WALK: case T_TRASH:
      cv.rect(x, y, TS, TS, rgb(176, 176, 170)); cv.hline(x, y + TS - 1, TS, rgb(150, 150, 146)); cv.vline(x + TS - 1, y, TS, rgb(150, 150, 146));
      if (t == T_WALK && (dn == T_ROAD || up == T_ROAD) && (tx % 4) == 2) {   // a street lamp every few slabs along the kerb
        const int lx = x + 8, ly = dn == T_ROAD ? y + 12 : y + 3;
        cv.ellipse(lx, ly + 1, 2, 1, rgb(110, 110, 106));
        cv.vline(lx, ly - 9, 10, rgb(60, 64, 76));
        cv.hline(lx - 2, ly - 10, 5, rgb(60, 64, 76));
        const bool on = gNight > 0.3f;
        cv.rect(lx - 1, ly - 12, 3, 2, on ? rgb(255, 236, 160) : rgb(200, 210, 220));
        if (on) { cv.set(lx, ly - 13, rgb(255, 250, 220)); addLight(lx, ly - 4, 30, 230); }
      }
      if (t == T_TRASH) {
        cv.rect(x + 3, y + 4, 10, 11, rgb(40, 110, 70)); cv.frame(x + 3, y + 4, 10, 11, rgb(20, 60, 40));
        cv.rect(x + 2, y + 2, 12, 3, rgb(60, 140, 90)); cv.vline(x + 6, y + 6, 8, rgb(30, 90, 56)); cv.vline(x + 9, y + 6, 8, rgb(30, 90, 56));
        if (sparkle && ((int)(tm * 3) & 1)) cv.set(x + 13, y + 1, rgb(255, 255, 200));
      }
      break;
    case T_BLDG: {
      static const Color fac[4] = {rgb(150, 70, 60), rgb(120, 120, 136), rgb(200, 180, 140), rgb(90, 100, 130)};
      uint32_t bh = hsh(tx / 3, ty / 3, 21);
      Color c = fac[bh % 4];
      cv.rect(x, y, TS, TS, c);
      const bool lit = cave;   // (reused flag: true at night for the city)
      for (int wy = 2; wy < TS - 2; wy += 7) for (int wx = 2; wx < TS - 2; wx += 7) {
        bool on = lit && (hsh(tx * 3 + wx, ty * 3 + wy, 22) % 100) < 55;
        cv.rect(x + wx, y + wy, 4, 4, on ? rgb(255, 220, 120) : rgb(60, 70, 90));
        if (on && gNight > 0.3f) addLight(x + wx + 2, y + wy + 2, 9, 90);
      }
      if (dn == T_WALK) { cv.rect(x + 5, y + 8, 6, 8, rgb(70, 46, 30)); cv.hline(x, y + TS - 1, TS, shade(c, 60)); }   // doorways at street level
      break; }
    case T_HOLE: {   // a dark crack in the cave floor, a cold draft (a faint glint) rising from it
      cv.rect(x, y, TS, TS, rgb(54, 46, 58));
      cv.ellipse(x + 8, y + 9, 6, 4, rgb(36, 30, 40));
      cv.ellipse(x + 8, y + 9, 4, 3, rgb(8, 6, 12));
      cv.hline(x + 3, y + 6, 3, rgb(84, 74, 88)); cv.hline(x + 10, y + 12, 3, rgb(84, 74, 88));
      if (sinf(tm * 2.1f) > 0.85f) cv.set(x + 8, y + 8, rgb(150, 220, 255));
      break; }
    case T_EXIT:
      if (cave) { cv.rect(x, y, TS, TS, rgb(54, 46, 58)); for (int r = 0; r < TS; r++) cv.hline(x + 2, y + r, 12, mix(rgb(54, 46, 58), rgb(200, 220, 255), r * 256 / TS)); }
      else { cv.rect(x, y, TS, TS, rgb(176, 124, 80)); cv.rect(x + 2, y + 4, 12, 10, rgb(170, 60, 70)); cv.frame(x + 2, y + 4, 12, 10, rgb(120, 40, 50)); }
      break;
  }
}

void drawHouse(Canvas& cv, int x, int y, int place) {   // x,y = top-left of the 3x2 block
  static const Color walls[4] = {rgb(236, 210, 170), rgb(214, 170, 190), rgb(190, 214, 236), rgb(220, 236, 190)};
  static const Color roofs[4] = {rgb(190, 70, 60), rgb(150, 80, 160), rgb(70, 100, 160), rgb(80, 150, 90)};
  Color wall = walls[place & 3], roof = roofs[place & 3];
  const int w = 3 * TS, h = 2 * TS;
  cv.rect(x + 2, y + 10, w - 4, h - 10, wall);
  cv.frame(x + 2, y + 10, w - 4, h - 10, shade(wall, 60));
  cv.tri(x - 2, y + 12, x + w + 1, y + 12, x + w / 2, y - 6, rgb(40, 24, 30));
  cv.tri(x, y + 11, x + w - 1, y + 11, x + w / 2, y - 4, roof);
  cv.rect(x + w / 2 - 4, y + 18, 8, 14, rgb(110, 70, 44)); cv.set(x + w / 2 + 2, y + 25, rgb(240, 200, 80));   // door
  const bool lit = gNight > 0.3f;   // at night the windows glow warm and spill light onto the grass
  const Color glass = lit ? rgb(255, 214, 120) : rgb(170, 220, 250);
  cv.rect(x + 7, y + 17, 7, 6, glass); cv.frame(x + 7, y + 17, 7, 6, rgb(90, 60, 40));
  cv.rect(x + w - 14, y + 17, 7, 6, glass); cv.frame(x + w - 14, y + 17, 7, 6, rgb(90, 60, 40));
  if (lit) {
    cv.vline(x + 10, y + 17, 6, rgb(90, 60, 40)); cv.vline(x + w - 11, y + 17, 6, rgb(90, 60, 40));   // window cross
    addLight(x + 10, y + 24, 22, 200); addLight(x + w - 11, y + 24, 22, 200); addLight(x + w / 2, y + 34, 14, 150);
  }
  if (place == P_HOME) { cv.rect(x + w / 2 - 9, y + 2, 18, 7, rgb(255, 240, 200)); cv.textCenter(x + w / 2, y + 2, "HOME", rgb(150, 60, 50)); }
}

void drawHermit(Canvas& cv, int x, int y, float t, bool hidden = false) {
  const Color robe = hidden ? rgb(40, 130, 140) : rgb(110, 70, 150);
  cv.ellipse(x, y + 1, 9, 2, rgb(10, 8, 16));
  cv.tri(x - 8, y, x + 8, y, x, y - 18, robe);
  gdisc(cv, x, y - 18, 5, rgb(238, 200, 160));
  cv.tri(x - 6, y - 20, x + 6, y - 20, x, y - 31, robe);
  if (hidden) { cv.set(x - 3, y - 8, rgb(255, 240, 160)); cv.set(x + 2, y - 4, rgb(255, 240, 160)); cv.set(x, y - 26, rgb(255, 240, 160)); }   // stars on the robe
  cv.ellipse(x, y - 14, 4, 4, rgb(236, 236, 240));
  cv.set(x - 2, y - 19, rgb(20, 14, 30)); cv.set(x + 2, y - 19, rgb(20, 14, 30));
  cv.rect(x + 7, y - 9, 4, 5, ((int)(t * 6) & 1) ? rgb(255, 230, 120) : rgb(255, 200, 80));
}

void drawCritter(Canvas& cv, const ExMob& m, float t) {
  int x = (int)m.x, y = (int)m.y; bool l = m.vx < 0; int s = l ? -1 : 1;
  switch (m.kind) {
    case CR_MOUSE:
      cv.ellipse(x, y, 4, 3, rgb(170, 170, 186)); cv.disc(x + s * 4, y - 1, 2, rgb(180, 180, 196)); cv.set(x + s * 6, y - 1, rgb(20, 20, 30));
      cv.set(x + s * 3, y - 3, rgb(230, 160, 180)); cv.line(x - s * 4, y + 1, x - s * 8, y - 1, rgb(230, 160, 180)); break;
    case CR_BIRD: {
      cv.ellipse(x, y, 4, 3, rgb(90, 150, 240)); cv.disc(x + s * 3, y - 2, 2, rgb(110, 170, 250)); cv.set(x + s * 5, y - 2, rgb(250, 190, 60));
      int w = (int)(sinf(t * 18) * 3); cv.tri(x - 2, y - 1, x - s * 6, y - 4 - w, x + 1, y - 2, rgb(60, 110, 210)); break; }
    case CR_FROG:
      cv.ellipse(x, y, 4, 3, rgb(80, 180, 80)); cv.disc(x - 2, y - 3, 1, rgb(80, 180, 80)); cv.disc(x + 2, y - 3, 1, rgb(80, 180, 80));
      cv.set(x - 2, y - 3, rgb(20, 20, 20)); cv.set(x + 2, y - 3, rgb(20, 20, 20)); break;
    case CR_PIGEON: {
      cv.ellipse(x, y, 4, 3, rgb(150, 150, 166)); cv.disc(x + s * 3, y - 3, 2, rgb(120, 120, 140));
      cv.set(x + s * 3, y - 1, rgb(120, 190, 150)); cv.set(x + s * 4, y - 3, rgb(240, 140, 60)); cv.set(x + s * 5, y - 3, rgb(220, 200, 180));
      cv.line(x - s * 3, y, x - s * 6, y - 1, rgb(100, 100, 120)); break; }
    case CR_FIREFLY: {
      bool on = sinf(t * 5 + m.t * 3) > -0.2f;
      if (on) { cv.disc(x, y, 2, rgb(200, 255, 120)); cv.set(x, y, rgb(255, 255, 230)); } else cv.set(x, y, rgb(90, 110, 60));
      break; }
    case CR_BUTTERFLY: {
      int f = (int)(fabsf(sinf(t * 14 + m.t)) * 3);
      cv.tri(x, y, x - 4, y - 2 - f, x - 3, y + 2, rgb(255, 170, 60)); cv.tri(x, y, x + 4, y - 2 - f, x + 3, y + 2, rgb(255, 200, 80));
      cv.vline(x, y - 2, 4, rgb(60, 40, 30)); break; }
  }
}

// ---- night: moonlit colours with warm pools of light (windows, street lamps, fireflies, headlights, the cat) ----
// Lights are gathered while the map is drawn, then the whole play area is graded in one pass. The light map is
// kept at half resolution (88 x 80 = 7 KB), which keeps it cheap and makes the pools look soft.
void nightGrade(Canvas& cv, float night) {
  memset(gLightMap, 0, sizeof(gLightMap));
  for (int i = 0; i < gNLights; i++) {
    const ExLight& L = gLights[i];
    const int cx = (L.x - OX) / 2, cy = (L.y - OY) / 2, r = L.r / 2 + 1, r2 = r * r;
    for (int y = cy - r; y <= cy + r; y++) {
      if (y < 0 || y >= LH) continue;
      for (int x = cx - r; x <= cx + r; x++) {
        if (x < 0 || x >= LW) continue;
        const int d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
        if (d2 >= r2) continue;
        const int f = 256 - d2 * 256 / r2;                       // 1 at the centre .. 0 at the rim
        const int v = L.k * f * f / 65536;                       // squared: a soft falloff
        const int s = gLightMap[y][x] + v;                       // overlapping pools add up
        gLightMap[y][x] = (uint8_t)(s > 255 ? 255 : s);
      }
    }
  }
  // moonlight: darker, cooler, a little blue; light: warm lamp-glow over the true colours.
  // Per-channel lookup tables (rebuilt each call, 128 entries) keep the per-pixel work to a few table reads.
  const int n = (int)(night * 256);
  const int dk = 256 - n * 77 / 256;   // 30% darker again at full night (outside the pools of light)
  const int mr = (256 - n * 168 / 256) * dk >> 8, mg = (256 - n * 150 / 256) * dk >> 8, mb = (256 - n * 96 / 256) * dk >> 8, lift = n * 3 / 256;
  uint8_t MR[32], MG[64], MB[32], WR[32], WG[64], WB[32];
  for (int i = 0; i < 32; i++) { MR[i] = (uint8_t)(i * mr >> 8); MB[i] = (uint8_t)((i * mb >> 8) + lift); WR[i] = (uint8_t)(i + 2 > 31 ? 31 : i + 2); WB[i] = (uint8_t)(i * 3 / 4); }
  for (int i = 0; i < 64; i++) { MG[i] = (uint8_t)(i * mg >> 8); WG[i] = (uint8_t)(i * 15 / 16); }
  Color* fb = cv.px;   // (the play area always lies inside the canvas)
  for (int y = 0; y < PH; y++) {
    const uint8_t* lrow = gLightMap[y >> 1];
    Color* row = fb + (OY + y) * cv.cw + OX;
    bool any = false;
    for (int x = 0; x < LW; x++) if (lrow[x]) { any = true; break; }
    for (int x = 0; x < PW; x++) {
      const Color c = row[x];
      const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
      int nr = MR[r], ng = MG[g], nb = MB[b];
      const int L = any ? lrow[x >> 1] : 0;
      if (L) { nr += (WR[r] - nr) * L >> 8; ng += (WG[g] - ng) * L >> 8; nb += (WB[b] - nb) * L >> 8; }
      row[x] = (Color)(((nr > 31 ? 31 : nr) << 11) | ((ng > 63 ? 63 : ng) << 5) | (nb > 31 ? 31 : nb));
    }
  }
}

// darkness outside a circle of light around the cat (caves, night)
void darkness(Canvas& cv, int cx, int cy, int r, int pct) {
  const int r2 = r * r, soft = (r + 14) * (r + 14);
  for (int y = OY; y < OY + PH; y++) for (int x = OX; x < OX + PW; x++) {
    int d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
    if (d2 <= r2) continue;
    int k = d2 >= soft ? pct : 100 - (100 - pct) * (d2 - r2) / (soft - r2);
    Color c = cv.get(x, y);
    int rr = ((c >> 11) & 31) * k / 100, gg = ((c >> 5) & 63) * k / 100, bb = (c & 31) * k / 100;
    cv.set(x, y, (Color)((rr << 11) | (gg << 5) | (bb + (k < 70 ? 1 : 0) > 31 ? 31 : bb + (k < 70 ? 1 : 0))));
  }
}

}  // namespace

// ------------------------------------------------------------------------------------- game

bool Game::exploreAllowed(const char** why) const {
  if (pet.stage() < STAGE_ADULT) { if (why) *why = "Explore unlocks at adulthood"; return false; }
  if (pet.d.energy < EXPLORE_ENERGY && !(ex_.active && ex_.state != 6)) { if (why) *why = "Needs 50 energy to explore"; return false; }   // (a trip under way can always be resumed)
  if (why) *why = nullptr;
  return true;
}

void Game::startExplore() {
  const char* why = nullptr;
  if (!exploreAllowed(&why)) { if (why) toast("%s", why); return; }
  ExploreGame& g = ex_;
  if (g.active && g.state != 6) {   // pick up where they left off (trips last while they're awake)
    snprintf(g.msg, sizeof(g.msg), "Back where you left off!"); g.msgT = 2.0f;
    setScreen(S_EXPLORE);
    return;
  }
  if (sv.worldSeed == 0) {   // the first trip ever: this player's own world (kept from now on, however the trip goes)
    const uint32_t homeBit = 1u << (HOME_SY * EXW + HOME_SX);
    if (sv.ex.seen & ~homeBit) sv.worldSeed = 1;   // (they explored on older firmware: keep the map they know)
    else { uint32_t s = plat->randomSeed() ^ epoch ^ (nowMs * 2654435761u); sv.worldSeed = s < 2 ? s + 2 : s; }
    autosave(true);
  }
  setLayout(sv.worldSeed);
  memset(&g, 0, sizeof(g));
  g.active = true;
  g.sx = HOME_SX; g.sy = HOME_SY; g.room = -1;
  buildScreen(g.map, g.sx, g.sy, sv.ex.flags);
  g.x = OX + 2 * TS + TS / 2; g.y = OY + 3 * TS + TS / 2;
  sv.ex.seen |= 1u << (g.sy * EXW + g.sx);
  sv.ex.journal |= 1u << (8 + P_HOME);
  const uint8_t day = (uint8_t)((epoch / 86400u) & 0xFF);
  if (sv.ex.hbDay != day) { sv.ex.hbDay = day; sv.ex.hbCount = 0; sv.ex.rocksToday = 0; sv.ex.nestsToday = 0; sv.ex.trashToday = 0; }
  snprintf(g.msg, sizeof(g.msg), "Drag to walk. Tap things!"); g.msgT = 2.5f;
  autosave(true);
  setScreen(S_EXPLORE);
}

// test/debug helper: put the explorer on a given outdoor tile
void Game::exTeleport(int sx, int sy, int tx, int ty) {
  ExploreGame& g = ex_;
  g.sx = sx; g.sy = sy; g.room = -1; g.state = 0; g.nmob = 0;
  setLayout(g.mp ? 1 : sv.worldSeed);
  buildScreen(g.map, sx, sy, sv.ex.flags);
  g.x = OX + tx * TS + 8.0f; g.y = OY + ty * TS + 8.0f;
}

// Explore Together: a fresh trip on the given square (each player starts in a different corner)
void Game::startExploreMP(int sx, int sy) {
  ExploreGame& g = ex_;
  memset(&g, 0, sizeof(g));
  g.active = true; g.mp = true;
  g.sx = sx; g.sy = sy; g.room = -1;
  setLayout(1);   // Explore Together: everyone on the classic map, so the squares match
  buildScreen(g.map, sx, sy, sv.ex.flags);
  // start on the walkable tile nearest the middle (not in the road)
  int best = -1, bestD = 1 << 30;
  for (int ty = 1; ty < EXR - 1; ty++) for (int tx = 1; tx < EXC - 1; tx++) {
    const uint8_t t = g.map[ty][tx];
    if (!walkable(t) || t == T_ROAD || t == T_ZEBRA || t == T_DOOR || t == T_CAVE || t == T_GAP || t == T_GATE) continue;
    const int d = (tx - 5) * (tx - 5) + (ty - 5) * (ty - 5);
    if (d < bestD) { bestD = d; best = ty * EXC + tx; }
  }
  if (best < 0) best = 5 * EXC + 5;
  g.x = OX + (best % EXC) * TS + 8.0f; g.y = OY + (best / EXC) * TS + 8.0f;
  sv.ex.seen |= 1u << (sy * EXW + sx);
  snprintf(g.msg, sizeof(g.msg), "GO! Claim squares!"); g.msgT = 2.5f;
  setScreen(S_EXPLORE);
}

void Game::screenExplore(Canvas& cv) {
  ExploreGame& g = ex_;
  if (g.mp && net_.mode != NM_MATCH) { g.mp = false; g.active = false; setScreen(S_HOME); return; }   // the match is over / gone
  setLayout(g.mp ? 1 : sv.worldSeed);   // (whose world: this player's own, or the classic map in a match)
  g.t += dt;
  if (g.state != 0) g.autoWalk = false;   // a dialog, a fight, the next screen, the map: a tap-to-walk stops there
  if (g.msgT > 0) g.msgT -= dt;
  const bool inCave = g.room == P_TCAVE || g.room == P_HERMIT || g.room == P_KING || g.room == P_SECRET || g.room == P_DEEP;
  const float hh = hour + minute / 60.0f;
  const float night = hh >= 20.5f || hh < 5.5f ? 1.0f : (hh >= 18.5f ? (hh - 18.5f) / 2.0f : (hh < 7.0f ? (7.0f - hh) / 1.5f : 0.0f));
  const int weather = weatherForDay(epoch / 86400u);
  const float idealW = pet.idealWeight();
  const bool slim = pet.d.chonk <= idealW + 5.0f;
  const bool chonk = pet.d.chonk >= idealW + 15.0f;
  auto say = [&](const char* m) { snprintf(g.msg, sizeof(g.msg), "%s", m); g.msgT = 2.2f; };
  auto findHairball = [&](const char* where) -> bool {   // capped "lying around" finds
    if (sv.ex.hbCount >= HB_DAILY_CAP) { say("Nothing here (come back tomorrow)"); return false; }
    sv.ex.hbCount++; giveCoins(1); g.found++; sound(SND_COIN);
    char b[30]; snprintf(b, sizeof(b), "%s +1 hairball!", where); say(b);
    for (int i = 0; i < 6; i++) spawn(g.x + (i - 3) * 3, g.y - 10, 1, rgb(255, 240, 120), (float)((i - 3) * 14), -30, 0.7f);
    return true;
  };
  auto dialog = [&](const char* title, const char* l0, const char* l1, const char* l2, const char* a, const char* b, int act, int arg) {
    snprintf(g.dTitle, sizeof(g.dTitle), "%s", title);
    snprintf(g.dLine[0], 30, "%s", l0 ? l0 : ""); snprintf(g.dLine[1], 30, "%s", l1 ? l1 : "");
    snprintf(g.dLine[2], 30, "%s", l2 ? l2 : ""); g.dLine[3][0] = 0;
    snprintf(g.dA, sizeof(g.dA), "%s", a ? a : "OK"); snprintf(g.dB, sizeof(g.dB), "%s", b ? b : "");
    g.dAct = act; g.dArg = arg; g.state = 2; g.dOpenT = g.t;
    sound(SND_BLOOP);
  };
  // green zone: a heavier/stronger cat gets a wider one; it jumps somewhere new (and changes size a bit) after every swing
  auto rollZone = [&]() {
    const float base = clampf(0.10f + 0.025f * (pet.power() - 0.9f), 0.06f, 0.17f);   // half-width (was twice this)
    g.zoneW = clampf(base * (0.75f + pet.rndf() * 0.5f), 0.04f, 0.17f);
    g.zoneC = g.zoneW + pet.rndf() * (1.0f - 2.0f * g.zoneW);
  };
  auto startFight = [&](int species, float str, int hp, bool boss) {
    g.state = 3; g.foeSpecies = species; g.foeStr = str; g.foeHp = g.foeMax = hp; g.myHp = g.myMax = boss ? 4 : 3;
    g.marker = 0; g.markerDir = 1; g.roundT = 0; g.lastHit = 0; g.boss = boss; g.fights++;
    g.zoneW = clampf(0.10f + 0.025f * (pet.power() - 0.9f), 0.06f, 0.17f); g.zoneC = 0.5f;   // the first swing: centred
    g.fxKind = 1; g.fxT = 0; g.fxAfter = 0;   // flash, then the screen wipes into the fight
    g.throwT = 0; g.throwCd = 0; g.throwHit = false; g.hbKO = false; g.bossPhase = 0; g.bossPhaseT = 1.6f; g.feint = g.defended = g.defLock = false;
    pet.d.energy = clampf(pet.d.energy - 3, 0, 100);
    sound(SND_HISS);
  };
  auto endTrip = [&](bool lost) {
    g.lost = lost; g.state = 6; g.stateT = 0; g.active = false;
    float fun = lost ? -6.0f : gfun(6.0f + g.caught * 2 + g.wins * 4 + g.places * 3 + g.screens * 0.5f);
    pet.exercise(fun, EXPLORE_ENERGY, 0.3f + g.screens * 0.08f);
    recordMilestone(MS_EXPLORE);   // a trip takes 50 energy (paid on the way home, so it can't cut the trip short)
    if (lost) pet.d.resent = clampf(pet.d.resent + 2, 0, 100);
    sv.lifeGames++;
    pet.clampAll();
    autosave(true);
  };
  auto spawnMobs = [&]() {
    g.nmob = 0; g.nfish = 0;
    if (g.room >= 0) return;
    if (g.sx == 2) {   // the river: 2-3 fish swimming up and down it
      g.nfish = 2 + (int)(pet.rnd() % 2);
      for (int i = 0; i < g.nfish; i++) {
        auto& f = g.fish[i];
        f.alive = true; f.t = pet.rndf() * 6;
        f.x = OX + (7 + (int)(pet.rnd() % 2)) * TS + 8.0f;
        f.y = OY + (float)(pet.rnd() % PH);
        f.vy = (pet.rnd() % 2 ? 1.0f : -1.0f) * (14.0f + pet.rndf() * 12.0f);
      }
    }
    const bool rain = weather == 2;
    int n = (int)(pet.rnd() % 3);   // 0, 1 or 2 critters (about 1 per screen)
    if (night > 0.5f && g.sx != CITY_SX) n = 2;   // after dark: two fireflies on every screen
    for (int i = 0; i < n && g.nmob < 2; i++) {
      int tries = 20;
      while (tries--) {
        int tx = 1 + (int)(pet.rnd() % (EXC - 2)), ty = 1 + (int)(pet.rnd() % (EXR - 2));
        if (!walkable(g.map[ty][tx]) || g.map[ty][tx] == T_DOOR || g.map[ty][tx] == T_CAVE) continue;
        ExMob& m = g.mob[g.nmob++];
        memset(&m, 0, sizeof(m));
        m.x = OX + tx * TS + 8.0f; m.y = OY + ty * TS + 10.0f; m.alive = true; m.t = pet.rndf() * 6;
        if (g.sx == CITY_SX) m.kind = CR_PIGEON;
        else if (night > 0.5f) { m.kind = CR_FIREFLY; if (pet.rnd() % 2) m.t += 100.0f; }   // (t >= 100: a dozy one that doesn't flee)
        else if (rain && g.sx == 2) m.kind = CR_FROG;
        else { uint32_t r = pet.rnd() % 100; m.kind = r < 45 ? CR_MOUSE : (r < 75 ? CR_BIRD : CR_BUTTERFLY); }
        if (rain && m.kind == CR_BUTTERFLY) m.kind = CR_FROG;
        break;
      }
    }
    if (g.sx == CITY_SX && g.nmob < EX_MOBS) {   // traffic: one car at a time, coming in from off-screen (see carTrip)
      ExMob& m = g.mob[g.nmob++];
      memset(&m, 0, sizeof(m));
      m.kind = MOB_CAR; m.alive = true;
      m.phase = 3; m.aux = 0.5f + pet.rndf() * 1.5f;   // on its way: here in a moment
    }
    // a wild cat on some screens (not near home or on screens with houses)
    const int bit = g.sy * EXW + g.sx;
    if (!g.mp && g.nmob < EX_MOBS && placeAt(g.sx, g.sy) < 0 && !(g.catsBeaten & (1u << bit)) && (hsh(g.sx, g.sy, (int)(epoch / 86400u)) % 100) < 35) {
      ExMob& m = g.mob[g.nmob++];
      memset(&m, 0, sizeof(m));
      m.kind = MOB_WILDCAT; m.alive = true; m.x = OX + 8 * TS + 8.0f; m.y = OY + 2 * TS + 12.0f;
      if (!walkable(g.map[2][8])) { m.x = OX + 5 * TS + 8.0f; m.y = OY + 2 * TS + 12.0f; }
      m.t = (float)(hsh(g.sx, g.sy, 4) % 7);   // breed
    }
  };
  auto tileAt = [&](float px, float py) -> int {
    int tx = (int)((px - OX) / TS), ty = (int)((py - OY) / TS);
    if (tx < 0 || ty < 0 || tx >= EXC || ty >= EXR) return -1;
    return g.map[ty][tx];
  };
  auto canWalk = [&](int tx, int ty) -> bool {
    if (tx < 0 || ty < 0 || tx >= EXC || ty >= EXR) return false;
    uint8_t t = g.map[ty][tx];
    if (t == T_GAP) return slim;
    if (t == T_GATE) return (sv.ex.flags & 2) != 0;
    return walkable(t);
  };
  auto enterRoom = [&](int place, int tx, int ty) {
    g.enterX = tx; g.enterY = ty;
    g.room = place; g.nmob = 0; g.irisT = 0.4f; g.autoWalk = false;
    buildRoom(g.map, place);
    g.x = OX + 5 * TS + 8.0f; g.y = OY + 8 * TS + 8.0f;
    if (!(sv.ex.journal & (1u << (8 + place)))) { sv.ex.journal |= 1u << (8 + place); g.places++; }
    sound(SND_BLOOP);
    if (place == P_HOME) { endTrip(false); return; }
    const char* where = PLACE_NAME[place];
    say(where);
    g.onBed = false; g.napT = 0;
    // now and then a stray has curled up on the bed: beat it to use the bed (once a day per house, ~40%)
    for (int y = 0; y < EXR; y++) for (int x = 0; x < EXC; x++) {
      if (g.map[y][x] != T_BED || g.nmob >= EX_MOBS || (g.bedCatsBeaten & (1u << place)) || (g.bedsUsed & (1u << place))) continue;
      if ((hsh(place, 31, (int)(epoch / 86400u)) % 100) >= 40) continue;
      ExMob& m = g.mob[g.nmob++];
      memset(&m, 0, sizeof(m));
      m.kind = MOB_WILDCAT; m.alive = true; m.x = OX + x * TS + 8.0f; m.y = OY + y * TS + 12.0f;
      m.t = (float)(hsh(place, 5, (int)(epoch / 86400u)) % 7);   // breed
      say("Someone's on the bed!"); sound(SND_MEOW);
    }
  };
  auto leaveRoom = [&]() {
    g.room = -1; g.irisT = 0.4f; g.autoWalk = false;
    sound(SND_BACK);
    buildScreen(g.map, g.sx, g.sy, sv.ex.flags);
    g.x = OX + g.enterX * TS + 8.0f; g.y = OY + (g.enterY + 1) * TS + 8.0f;
    spawnMobs();
  };
  auto placeSeen = [&](int p) { if (!(sv.ex.journal & (1u << (8 + p)))) { sv.ex.journal |= 1u << (8 + p); g.places++; } };

  // ===================================================================== update
  if (g.state == 0 && !(g.mp && net_.f.on)) {   // (a fight with another player freezes the world)
    // ---- walking: drag anywhere, they pathfind there (BFS over tiles) ----
    const int ctx = (int)((g.x - OX) / TS), cty = (int)((g.y - OY) / TS);
    bool tapped = false;
    // fish: tap one while they stand on the bank (not the bridge) right next to it
    if (in.tap && in.y >= OY && in.y < OY + PH && g.room < 0) {
      for (int i = 0; i < g.nfish && !tapped; i++) {
        auto& f = g.fish[i];
        if (!f.alive || tileAt(f.x, f.y) == T_BRIDGE) continue;   // under a bridge: out of reach
        if (fabsf(in.x - f.x) > 15 || fabsf(in.y - f.y) > 15) continue;   // (a roomy target: they keep swimming)
        tapped = true;
        const int fx = (int)((f.x - OX) / TS), fy = (int)((f.y - OY) / TS);
        const int here = tileAt(g.x, g.y);
        const bool bank = here != T_BRIDGE && here != T_WATER && here != T_LILY && abs(fx - ctx) <= 1 && abs(fy - cty) <= 2;   // (beside them, or a tile up or down the river: they swim past fast)
        if (!bank) { (sound(SND_TAP), say("Get on the bank next to it!")); continue; }
        if (g.fishToday >= 5) { (sound(SND_TAP), say("Full of fish! Let them be.")); continue; }
        f.alive = false; g.fishToday++; sound(SND_SPLASH);
        sv.ex.journal |= 1u << CR_FISH; placeSeen(P_DOCK);
        pet.d.hunger = clampf(pet.d.hunger + 12, 0, 100);
        if (sv.ex.quest[0] == 1 && sv.ex.fish < 9) sv.ex.fish++;
        g.left = f.x < g.x;
        say(sv.ex.quest[0] == 1 ? "Got a fish for Grandma!" : "Got a fish! Yum");
        for (int k = 0; k < 6; k++) spawn(f.x, f.y, 0, rgb(150, 210, 250), (float)((k - 3) * 12), -30, 0.6f);   // splash
      }
    }
    if (in.tap && !tapped && in.y >= OY && in.y < OY + PH) {
      // tapping something right next to them = interact
      // Which tile? The one under the finger, unless it lands just beside something you can tap (a nest in a tree's
      // leaves, a sign, a chest...) that's next to them: then that thing, the nearest one within ~15 px of the finger.
      int tx = (in.x - OX) / TS, ty = (in.y - OY) / TS;
      {
        auto tappable = [](uint8_t t) { return t == T_SIGN || t == T_BUSH || t == T_CRACK || t == T_LOOSE || t == T_TRASH || t == T_NEST || t == T_CHEST || t == T_NPC; };
        int bx = -1, by = -1; float bd = 1e9f;
        for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) {
          const int x = tx + ox, y = ty + oy;
          if (x < 0 || y < 0 || x >= EXC || y >= EXR || abs(x - ctx) > 1 || abs(y - cty) > 1 || !tappable(g.map[y][x])) continue;
          const float ddx = in.x - (OX + x * TS + 8.0f), ddy = in.y - (OY + y * TS + 8.0f), d = sqrtf(ddx * ddx + ddy * ddy);
          if ((ox || oy) && d > 15.0f) continue;   // a neighbouring tile only counts if the finger is close to it
          if (d < bd) { bd = d; bx = x; by = y; }
        }
        if (bx >= 0) { tx = bx; ty = by; }
      }
      if (tx >= 0 && ty >= 0 && tx < EXC && ty < EXR && abs(tx - ctx) <= 1 && abs(ty - cty) <= 1) {
        uint8_t t = g.map[ty][tx];
        const int bit = g.sy * EXW + g.sx;
        // something to tap: interact. Plain ground: not used up here, so it becomes a tap to walk (below)
        tapped = t == T_SIGN || t == T_BUSH || t == T_CRACK || t == T_LOOSE || t == T_TRASH || t == T_NEST || t == T_CHEST || t == T_NPC;
        if (t == T_SIGN) {
          if (g.room < 0 && g.sx == HOME_SX && g.sy == HOME_SY) { char e[28]; snprintf(e, sizeof(e), "Each screen: -%d energy", (int)exScreenCost()); dialog("SIGN", "Drag to walk anywhere.", "Tap things next to you.", e, "OK", nullptr, 0, 0); }
          else if (g.sx == 4 && g.sy == 0) dialog("SIGN", "ALLEY KING'S LAIR", "The gate is locked.", "The key? Try the old cave.", "OK", nullptr, 0, 0);
          else if (g.sx == 2 && g.sy == 2) dialog("SIGN", "GOOD FISHING", "Stand on the bank, then", "tap a fish next to you.", "OK", nullptr, 0, 0);
          else if (isAt(P_PEN, g.sx, g.sy)) dialog("SIGN", "Private garden.", "Only a SLIM cat could", "squeeze through that gap.", "OK", nullptr, 0, 0);
          else if (isAt(P_NOOK, g.sx, g.sy)) dialog("SIGN", "A huge boulder...", "Only a real CHONK could", "shove it out of the way.", "OK", nullptr, 0, 0);
          else if (g.sx == CITY_SX) dialog("SIGN", "DOWNTOWN", "LOOK BOTH WAYS!", "Cars don't stop for cats.", "OK", nullptr, 0, 0);
          else dialog("SIGN", "Nothing written here.", nullptr, nullptr, "OK", nullptr, 0, 0);
        } else if (t == T_BUSH) {   // the bush rustles aside... a cave!
          sv.ex.flags |= 32;
          buildScreen(g.map, g.sx, g.sy, sv.ex.flags);
          for (int i = 0; i < 10; i++) spawn(OX + tx * TS + 8.0f, OY + ty * TS + 6.0f, 4, rgb(70, 160, 80), (float)((i - 5) * 10), -30, 0.8f);
          sound(SND_POOF); say("A hidden cave behind the bush!"); recordMilestone(MS_CAVE);
          autosave(true);
        } else if (t == T_CRACK) {   // the Hermit's secret
          if ((sv.ownGl >> GL_GOGGLES) & 1) (sound(SND_TAP), say("Just an empty hollow."));
          else {
            sv.ownGl |= (uint8_t)(1u << GL_GOGGLES);
            for (int i = 0; i < 10; i++) spawn(OX + tx * TS + 8.0f, OY + ty * TS + 8.0f, 4, rgb(120, 104, 124), (float)((i - 5) * 10), -25, 0.7f);
            sound(SND_WIN);
            dialog("SECRET!", "The cracked wall crumbles...", "CAVE GOGGLES!", "Find them in the Wardrobe.", "WOW", nullptr, 0, 0);
            autosave(true);
          }
        } else if (t == T_LOOSE) {
          if (sv.ex.rocksToday & (1u << bit)) (sound(SND_TAP), say("Just dirt under there."));
          else { sv.ex.rocksToday |= 1u << bit; if ((pet.rnd() % 100) < 60) findHairball("Under the rock:"); else (sound(SND_TAP), say("Only a beetle. Eww.")); }
        } else if (t == T_TRASH) {
          const uint32_t tb = 1u << (g.sy * 2 + (tx < 5 ? 0 : 1));
          if (sv.ex.trashToday & tb) (sound(SND_TAP), say("Already rummaged today."));
          else { sv.ex.trashToday |= tb; if ((pet.rnd() % 100) < 70) findHairball("In the trash:"); else (sound(SND_TAP), say("Just old fish bones. Ew.")); }
        } else if (t == T_NEST) {
          if (sv.ex.nestsToday & (1u << bit)) (sound(SND_TAP), say("An empty nest."));
          else { sv.ex.nestsToday |= 1u << bit; if ((pet.rnd() % 100) < 50) findHairball("In the nest:"); else (sound(SND_TAP), say("Just twigs and a feather.")); }
        } else if (t == T_CHEST) {
          int id = chestId(g.room, g.sx, g.sy, tx);
          if (id < 0 || (sv.ex.chests & (1u << id))) (sound(SND_TAP), say("It's empty."));
          else {
            sv.ex.chests |= (uint16_t)(1u << id);
            sound(SND_WIN);
            if (id == CH_KEY) { sv.ex.flags |= 1; dialog("TREASURE!", "An old brass KEY!", "It must open a gate", "somewhere up north-east.", "NICE", nullptr, 0, 0); }
            else if (id == CH_CAVEGOLD) { giveCoins(3); g.found += 3; dialog("TREASURE!", "3 hairballs!", nullptr, nullptr, "NICE", nullptr, 0, 0); }
            else if (id == CH_PEN) { giveCoins(2); g.found += 2; sv.ex.flags |= 16; dialog("TREASURE!", "2 hairballs and...", "a pink YARN BALL!", "Someone must miss this.", "NICE", nullptr, 0, 0); }
            else if (id == CH_NOOK) { giveCoins(4); g.found += 4; dialog("TREASURE!", "4 hairballs!", "Chonk power pays off.", nullptr, "NICE", nullptr, 0, 0); }
            else if (id == CH_CRYSTAL) {
              sv.ex.journal |= EXJ_CRYSTAL;
              char e[30]; snprintf(e, sizeof(e), "Each screen: -%d energy.", (int)exScreenCost());
              dialog("SECRET TREASURE!", "A glowing CAVE CRYSTAL!", "It makes your paws light.", e, "WOW", nullptr, 0, 0);
            }
            autosave(true);
          }
        } else if (t == T_NPC) {
          char b[30];
          if (g.room == P_GRANDMA) {
            uint8_t& q = sv.ex.quest[0];
            if (q == 0) { q = 1; sv.ex.fish = 0; dialog("GRANDMA TABBY", "Oh, a visitor! Dearie,", "could you bring me", "3 fish from the river?", "SURE!", nullptr, 0, 0); }
            else if (q == 1 && sv.ex.fish >= 3) {
              q = 2;
              if (!hasItem(ITEM_LCHAIR)) { sv.items |= (uint16_t)(1u << ITEM_LCHAIR); updateToys(); dialog("GRANDMA TABBY", "Delicious! Take my old", "LUXURY CHAIR. It's", "too comfy for me now.", "THANK YOU", nullptr, 0, 0); }
              else { giveCoins(5); dialog("GRANDMA TABBY", "Delicious! Here,", "5 hairballs for you.", nullptr, "THANK YOU", nullptr, 0, 0); }
              sound(SND_WIN);
            } else if (q == 1) { snprintf(b, sizeof(b), "Fish so far: %d of 3", sv.ex.fish); dialog("GRANDMA TABBY", "Still hungry, dearie.", b, "Catch them in the river.", "OK", nullptr, 0, 0); }
            else dialog("GRANDMA TABBY", "Come visit any time!", nullptr, nullptr, "BYE", nullptr, 0, 0);
          } else if (g.room == P_TOM) {
            uint8_t& q = sv.ex.quest[1];
            if (q == 0) { q = 1; sv.ex.mice = 0; dialog("OLD TOM", "Hrmph. Mice everywhere", "and I'm too old to chase.", "Catch 3 for me, kid.", "DEAL", nullptr, 0, 0); }
            else if (q == 1 && sv.ex.mice >= 3) {
              q = 2;
              if (!((sv.ownHat >> HAT_WIZARD) & 1)) { sv.ownHat |= (uint8_t)(1u << HAT_WIZARD); dialog("OLD TOM", "Not bad! Take my old", "WIZARD HAT (Wardrobe).", nullptr, "COOL", nullptr, 0, 0); }
              else { giveCoins(5); dialog("OLD TOM", "Not bad! 5 hairballs.", nullptr, nullptr, "COOL", nullptr, 0, 0); }
              sound(SND_WIN);
            } else if (q == 1) { snprintf(b, sizeof(b), "Mice so far: %d of 3", sv.ex.mice); dialog("OLD TOM", "Well? Go catch 'em.", b, nullptr, "OK", nullptr, 0, 0); }
            else dialog("OLD TOM", "Quiet around here now.", "Heh. Good kid.", nullptr, "BYE", nullptr, 0, 0);
          } else if (g.room == P_WHISKERS) {
            uint8_t& q = sv.ex.quest[2];
            if (q == 0) { q = 1; dialog("MISS WHISKERS", "I lost my favourite", "pink yarn ball! I think", "it rolled into a garden.", "I'LL LOOK", nullptr, 0, 0); }
            else if (q == 1 && (sv.ex.flags & 16)) {
              q = 2;
              sv.ownHat |= (uint8_t)(1u << HAT_FLOWER);
              if (!hasItem(ITEM_YARN)) { sv.items |= (uint16_t)(1u << ITEM_YARN); updateToys(); }
              dialog("MISS WHISKERS", "My yarn! Keep it, and", "take this FLOWER CROWN", "too (Wardrobe)!", "YAY", nullptr, 0, 0);
              sound(SND_WIN);
            } else if (q == 1) dialog("MISS WHISKERS", "Any luck? It's pink!", "A garden with a fence...", nullptr, "OK", nullptr, 0, 0);
            else dialog("MISS WHISKERS", "Thank you again, sweetie!", nullptr, nullptr, "BYE", nullptr, 0, 0);
          } else if (g.room == P_HERMIT) {
            if (sv.slowItem) dialog("THE HERMIT", "The charm protects you.", "Go well, little one.", nullptr, "BYE", nullptr, 0, 0);
            else dialog("THE HERMIT", "A COZY CHARM, for 10", "hairballs. Your needs", "drop 15% slower.", "BUY (10)", "NO", 1, 0);
          } else if (g.room == P_SECRET) {
            if (sv.ex.flags & 64) dialog("THE HIDDEN HERMIT", "The amulet suits you.", "Swift and strong. Go!", nullptr, "BYE", nullptr, 0, 0);
            else dialog("THE HIDDEN HERMIT", "You found me! My WILD", "AMULET: +15% attack and", "+15% speed. 40 hairballs.", "BUY (40)", "NO", 3, 0);
          } else if (g.room == P_KING) {
            if (sv.ex.flags & 4) dialog("ALLEY KING", "You again? Fine, fine.", "You're the boss now.", nullptr, "HEH", nullptr, 0, 0);
            else dialog("ALLEY KING", "Who dares enter MY lair?", "Only 10 HAIRBALL hits can", "beat me! DEFEND my leaps!", "FIGHT!", "RUN", 2, 0);
          }
        } else tapped = false;
      }
    }
    // napping in a bed: they stay put, then wakes up with +40% energy
    if (g.napT > 0) {
      g.napT -= dt;
      if (g.napT <= 0) { pet.d.energy = clampf(pet.d.energy + 40.0f, 0, 100); sound(SND_PURR); say("Rested! +40% energy"); }
    }
    // movement
    g.moving = false;
    if (g.hitT > 0) g.hitT -= dt;
    // (a finger up in the top bar counts too, so heading north is as easy as heading south; not on the BACK button)
    const bool onBack = in.x < CORNER + 40 && in.y < 20;
    // Tap to walk: a quick tap picks a spot and they walk there on their own (round obstacles, and off the screen
    // if it's by an edge that leads somewhere). A quick tap while they're walking stops them. Holding the finger
    // down still steers them directly, as before (and takes over from a walk in progress).
    constexpr int TAP_MS = 300;
    if (in.pressed) g.pressInWalk = g.autoWalk;
    if (in.tap && !tapped && in.heldMs < TAP_MS && !onBack && in.y < OY + PH) {
      if (g.pressInWalk) g.autoWalk = false;
      else { g.autoWalk = true; g.goalX = (float)in.x; g.goalY = (float)in.y; }
      g.pressInWalk = false;
    }
    if (g.autoWalk && in.down && in.heldMs >= TAP_MS && !onBack) g.autoWalk = false;   // held: the finger takes over
    const bool byFinger = in.down && !onBack && !(g.pressInWalk && in.heldMs < TAP_MS);
    const float pxF = byFinger ? (float)in.x : g.goalX, pyF = byFinger ? (float)in.y : g.goalY;
    if ((byFinger || g.autoWalk) && !tapped && g.hitT <= 0 && g.napT <= 0) {
      float fx = clampf(pxF, (float)OX, (float)(OX + PW)), fy = clampf(pyF, (float)OY, (float)(OY + PH));
      int gx = (int)clampf((fx - OX) / TS, 0, EXC - 1), gy = (int)clampf((fy - OY) / TS, 0, EXR - 1);
      // finger near an edge that leads somewhere: head for the nearest way out on that side and walk off it
      if (g.room < 0) {
        const float B = 14.0f, BU = 22.0f;   // the top gets a deeper zone: the top bar sits over it and the glass edge reads short
        const float dl = (OX + B) - pxF, dr = pxF - (OX + PW - B), du = (OY + BU) - pyF, dd = pyF - (OY + PH - B);
        int side = 0; float deep = 0;
        if (dl > deep && g.sx > 0) { deep = dl; side = 1; }
        if (dr > deep && g.sx < EXW - 1) { deep = dr; side = 2; }
        if (du > deep && g.sy > 0) { deep = du; side = 3; }
        if (dd > deep && g.sy < EXH - 1) { deep = dd; side = 4; }
        if (side) {
          const bool vert = side <= 2;
          int bestI = -1, bestD = 1 << 30;
          for (int i = 0; i < (vert ? EXR : EXC); i++) {
            const int tx = side == 1 ? 0 : (side == 2 ? EXC - 1 : i), ty = vert ? i : (side == 3 ? 0 : EXR - 1);
            if (!canWalk(tx, ty)) continue;
            const int d = abs(i - (vert ? gy : gx));
            if (d < bestD) { bestD = d; bestI = i; }
          }
          if (bestI >= 0) {
            gx = side == 1 ? 0 : (side == 2 ? EXC - 1 : bestI); gy = vert ? bestI : (side == 3 ? 0 : EXR - 1);
            fx = side == 1 ? (float)OX : (side == 2 ? (float)(OX + PW) : OX + gx * TS + 8.0f);
            fy = side == 3 ? (float)OY : (side == 4 ? (float)(OY + PH) : OY + gy * TS + 8.0f);
          }
        }
      }
      // BFS from their tile; aim at the next tile on the way to the reachable tile nearest the finger
      int par[EXR][EXC]; for (int y = 0; y < EXR; y++) for (int x = 0; x < EXC; x++) par[y][x] = -2;
      int qx[EXR * EXC], qy[EXR * EXC], qh = 0, qt = 0;
      qx[qt] = ctx; qy[qt] = cty; qt++; par[cty][ctx] = -1;
      int bx = ctx, by = cty, bd = 1 << 30;
      while (qh < qt) {
        int x = qx[qh], y = qy[qh]; qh++;
        int d = (x - gx) * (x - gx) + (y - gy) * (y - gy);
        if (d < bd) { bd = d; bx = x; by = y; }
        static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (auto& dd : D) {
          int nx = x + dd[0], ny = y + dd[1];
          if (nx < 0 || ny < 0 || nx >= EXC || ny >= EXR || par[ny][nx] != -2 || !canWalk(nx, ny)) continue;
          par[ny][nx] = y * EXC + x; qx[qt] = nx; qy[qt] = ny; qt++;
        }
      }
      float ax = fx, ay = fy;
      if (!(bx == ctx && by == cty)) {
        int cx2 = bx, cy2 = by;
        while (par[cy2][cx2] >= 0 && par[cy2][cx2] != cty * EXC + ctx) { int p = par[cy2][cx2]; cx2 = p % EXC; cy2 = p / EXC; }
        ax = OX + cx2 * TS + 8.0f; ay = OY + cy2 * TS + 8.0f;
        float ccx = OX + ctx * TS + 8.0f, ccy = OY + cty * TS + 8.0f;
        if (cx2 != ctx && fabsf(g.y - ccy) > 1.5f) { ax = g.x; ay = ccy; }
        else if (cy2 != cty && fabsf(g.x - ccx) > 1.5f) { ax = ccx; ay = g.y; }
      } else {   // finger is in their own tile (or the closest they can get): move within it, up to the screen edge
        ax = clampf(fx, OX + ctx * TS + 2.0f, OX + ctx * TS + TS - 2.0f);
        ay = clampf(fy, OY + cty * TS + 2.0f, OY + cty * TS + TS - 2.0f);
        if (ctx == 0) ax = clampf(fx, OX + 0.0f, OX + TS - 2.0f);
        if (ctx == EXC - 1) ax = clampf(fx, OX + ctx * TS + 2.0f, OX + PW + 0.0f);
        if (cty == 0) ay = clampf(fy, OY + 0.0f, OY + TS - 2.0f);
        if (cty == EXR - 1) ay = clampf(fy, OY + cty * TS + 2.0f, OY + PH + 0.0f);
      }
      float dx = ax - g.x, dy = ay - g.y, dist = sqrtf(dx * dx + dy * dy);
      if (dist > 0.8f) {
        float sp = (38.0f + 22.0f * pet.agility()) * pet.weightSpeed() * pet.gameDrag();
        float st = fminf(sp * dt, dist);
        g.x += dx / dist * st; g.y += dy / dist * st;
        if (fabsf(dx) > 0.5f) g.left = dx < 0;
        g.moving = true;
      } else if (!byFinger) g.autoWalk = false;   // tap to walk: there (or as close as they can get)
    }
    // boulder: a chonk walking into it shoves it away
    if (g.room < 0 && isAt(P_NOOK, g.sx, g.sy) && !(sv.ex.flags & 8) && in.down) {
      int tx = (int)((g.x - OX) / TS), ty = (int)((g.y - OY) / TS);
      if (ty == 7 && tx == 6 && in.x > OX + 7 * TS - 4 && in.y >= OY + 7 * TS && in.y < OY + 8 * TS) {
        if (chonk) { sv.ex.flags |= 8; g.map[7][7] = T_GRASS; placeSeen(P_NOOK); sound(SND_POOF); say("HEAVE! Chonk power!"); autosave(false); }
        else if (g.msgT <= 0) say("Too heavy. Needs a chonk!");
      }
    }
    // fence gap feedback for a cat that's too big
    if (g.room < 0 && isAt(P_PEN, g.sx, g.sy) && !slim && in.down && g.msgT <= 0) {
      int tx = (int)((g.x - OX) / TS), ty = (int)((g.y - OY) / TS);
      if (tx == 6 && ty == 7 && in.x > OX + 7 * TS) say("Too chubby to squeeze in!");
    }
    if (g.room < 0 && isAt(P_PEN, g.sx, g.sy) && tileAt(g.x, g.y) == T_GAP) placeSeen(P_PEN);
    // gate: the key opens it
    if (g.room < 0 && g.sx == 4 && g.sy == 0 && !(sv.ex.flags & 2)) {
      int tx = (int)((g.x - OX) / TS), ty = (int)((g.y - OY) / TS);
      if (tx == 5 && ty == 6 && in.down && in.y < OY + 6 * TS) {
        if (sv.ex.flags & 1) { sv.ex.flags |= 2; g.map[5][5] = T_PATH; sound(SND_WIN); say("The key fits! Gate open."); autosave(false); }
        else if (g.msgT <= 0) say("Locked. Need a key.");
      }
    }
    // a bed: step on it to nap - +40% energy (once per bed per trip)
    {
      const bool on = g.room >= 0 && tileAt(g.x, g.y) == T_BED;
      if (on && !g.onBed && g.napT <= 0) {
        bool squatter = false;
        for (int i = 0; i < g.nmob; i++) if (g.mob[i].alive && g.mob[i].kind == MOB_WILDCAT) squatter = true;
        if (squatter) say("Get that cat off the bed first!");
        else if (g.bedsUsed & (1u << g.room)) say("Already napped here this trip.");
        else if (pet.d.energy >= 95) say("Not sleepy right now.");
        else { g.napT = 1.8f; g.bedsUsed |= (uint16_t)(1u << g.room); sound(SND_YAWN); say("Zzz... a cozy nap."); }
      }
      g.onBed = on;
    }
    // places you notice just by being there
    if (g.room < 0) {
      if (g.sx == 2) placeSeen(P_RIVER);
      if (g.sx == 4 && g.sy == 0) placeSeen(P_MOAT);
      if (g.sx == CITY_SX) placeSeen(P_CITY);
    }
    // tall grass: sometimes something's hiding in it
    {
      int tx = (int)((g.x - OX) / TS), ty = (int)((g.y - OY) / TS);
      static int lastTall = -1;
      int key = (g.sy * EXW + g.sx) * 200 + ty * EXC + tx;
      if (g.room < 0 && g.moving && tileAt(g.x, g.y) == T_TALL && key != lastTall) {
        lastTall = key;
        if ((pet.rnd() % 200) < 3 && sv.ex.hbCount < HB_DAILY_CAP) findHairball("In the tall grass:");   // 1.5% per new grass tile
        else if (((int)(g.t * 8)) % 2 == 0) spawn(g.x, g.y - 4, 4, rgb(120, 200, 100), (float)((int)(pet.rnd() % 30) - 15), -15, 0.3f);
      }
    }
    // doors, caves, exits
    {
      int t = tileAt(g.x, g.y);
      int tx = (int)((g.x - OX) / TS), ty = (int)((g.y - OY) / TS);
      float cyIn = g.y - (OY + ty * TS);
      if (g.room < 0 && (t == T_DOOR || t == T_CAVE) && cyIn < 9 && g.mp) { if (g.msgT <= 0) say("No going inside in a match!"); }
      else if (g.room < 0 && (t == T_DOOR || t == T_CAVE) && cyIn < 9) { int p = placeAt(g.sx, g.sy); if (p >= 0) enterRoom(p, tx, ty); }
      else if (g.room == P_TCAVE && t == T_HOLE) {   // down the crack: the Deep Tunnels (enterX/Y keep the way out of the Old Cave)
        g.room = P_DEEP; buildRoom(g.map, P_DEEP);
        g.x = OX + 5 * TS + 8.0f; g.y = OY + 8 * TS + 6.0f;
        placeSeen(P_DEEP); sound(SND_HOLE); g.irisT = 0.5f;
        say((sv.ex.journal & EXJ_CRYSTAL) ? "The Deep Tunnels" : "Whoa! Down into the dark...");
      }
      else if (g.room == P_DEEP && t == T_EXIT && cyIn > 6) {   // climb back up into the Old Cave, next to the crack
        g.room = P_TCAVE; buildRoom(g.map, P_TCAVE);
        g.x = OX + 2 * TS + 8.0f; g.y = OY + 8 * TS + 8.0f;
        sound(SND_BACK); say("Back up in the Old Cave"); g.irisT = 0.4f;
      }
      else if (g.room >= 0 && t == T_EXIT && cyIn > 6) leaveRoom();
    }
    // screen edges -> slide to the neighbour
    if (g.room < 0 && g.state == 0) {
      int dx = 0, dy = 0;
      const float E = 4.0f;   // close enough to the edge counts
      if (g.x <= OX + E && g.sx > 0) dx = -1;
      else if (g.x >= OX + PW - E && g.sx < EXW - 1) dx = 1;
      else if (g.y <= OY + E && g.sy > 0) dy = -1;
      else if (g.y >= OY + PH - E && g.sy < EXH - 1) dy = 1;
      if (dx || dy) {
        memcpy(g.prevMap, g.map, sizeof(g.map));
        g.sx += dx; g.sy += dy;
        buildScreen(g.map, g.sx, g.sy, sv.ex.flags);
        g.state = 1; g.slide = 0; g.slideDx = dx; g.slideDy = dy;
        g.screens++;
        if (!g.mp) pet.d.energy = clampf(pet.d.energy - exScreenCost(), 0, 100);   // (a match costs its energy at the end)
        const uint32_t bit = 1u << (g.sy * EXW + g.sx);
        if (!(sv.ex.seen & bit)) { sv.ex.seen |= bit; say("A new place!"); sound(SND_POP); }
        if (dx) g.x = dx > 0 ? OX + 9.0f : OX + PW - 9.0f;   // well inside the new screen, so they don't bounce straight back
        if (dy) g.y = dy > 0 ? OY + 9.0f : OY + PH - 9.0f;
        {   // arriving in a tree, a building or the road? slide along the edge to the nearest safe tile
          const int tx = (int)clampf((g.x - OX) / TS, 0, EXC - 1), ty = (int)clampf((g.y - OY) / TS, 0, EXR - 1);
          auto safe = [&](int x, int y) { if (!canWalk(x, y)) return false; const uint8_t t = g.map[y][x]; return t != T_ROAD && t != T_ZEBRA; };
          if (!safe(tx, ty)) {
            for (int r = 1; r < (dx ? EXR : EXC); r++) {
              bool done = false;
              for (int s = -1; s <= 1 && !done; s += 2) {
                const int nx = dx ? tx : tx + s * r, ny = dx ? ty + s * r : ty;
                if (nx < 0 || ny < 0 || nx >= EXC || ny >= EXR || !safe(nx, ny)) continue;
                if (dx) g.y = OY + ny * TS + 8.0f; else g.x = OX + nx * TS + 8.0f;
                done = true;
              }
              if (done) break;
            }
          }
        }
      }
    }
    // fish: swim up and down the river, now and then turning; they slip under bridges and come out the other side
    for (int i = 0; i < g.nfish && g.room < 0; i++) {
      auto& f = g.fish[i];
      if (!f.alive) continue;
      f.t += dt;
      if ((int)(f.t * 0.7f) != (int)((f.t - dt) * 0.7f) && pet.rnd() % 100 < 20) f.vy = -f.vy;
      f.y += f.vy * dt;
      if (f.y < OY - 6) f.y = OY + PH + 5.0f;   // swims off one end of the screen, another comes in at the other
      if (f.y > OY + PH + 6) f.y = OY - 5.0f;
    }
    // critters & the wild cat
    for (int i = 0; i < g.nmob; i++) {
      ExMob& m = g.mob[i];
      if (!m.alive) continue;
      float dx = m.x - g.x, dy = m.y - g.y, d = sqrtf(dx * dx + dy * dy) + 0.01f;
      if (m.kind == MOB_CAR) {
        // One car's trip: it comes in from off-screen to the east in the top lane (driving on the right), makes a
        // U-turn somewhere west of the crosswalk, and drives back out east in the bottom lane. Then, after a pause,
        // the next one. phase 0 in, 1 turning (aux = where, aux2 = how far round), 2 out, 3 gone (aux = the pause).
        const float TOP = OY + 4 * TS + 8.0f, MID = OY + 5 * TS, R = 8.0f, OFF = OX + PW + 16.0f;
        switch (m.phase) {
          case 0: m.x += m.vx * dt; if (m.x <= m.aux) { m.x = m.aux; m.phase = 1; m.aux2 = 0; } break;
          case 1: {
            m.aux2 += dt * fabsf(m.vx) * 0.6f / (3.14159f * R);   // (slower round the bend)
            if (m.aux2 >= 1) { m.aux2 = 1; m.phase = 2; m.vx = fabsf(m.vx); }
            const float a = 3.14159f * m.aux2;
            m.x = m.aux - R * sinf(a); m.y = MID - R * cosf(a);
            break; }
          case 2: m.x += m.vx * dt; if (m.x > OFF) { m.phase = 3; m.aux = 2.0f + pet.rndf() * 3.0f; } break;
          default:
            m.aux -= dt;
            if (m.aux <= 0) {
              m.phase = 0; m.x = OFF; m.y = TOP;
              m.vx = -(44.0f + pet.rndf() * 24.0f);
              m.aux = OX + (2 + (int)(pet.rnd() % 3)) * TS + 8.0f;   // where it turns: west of the crosswalk
              m.t = (float)(pet.rnd() % 4);                             // its colour
            }
            break;
        }
        const bool onRoad = m.phase != 3 && m.x < OX + PW;
        if (onRoad && g.hitT <= 0 && fabsf(dx) < 12 && fabsf(dy) < 8) {   // SCREECH!
          g.hitT = 1.2f; sound(SND_HISS);
          pet.d.energy = clampf(pet.d.energy - 8, 0, 100);
          pet.d.resent = clampf(pet.d.resent + 4, 0, 100);
          if (sv.coins > 0) { sv.coins--; say("SCREECH! Dropped a hairball!"); } else say("SCREECH! Look both ways!");
          g.y = (g.y < OY + 5 * TS) ? OY + 3 * TS + 8.0f : OY + 6 * TS + 8.0f;   // jumps back onto the sidewalk
        }
        continue;
      }
      if (m.kind == MOB_WILDCAT) {
        if (d < 10) { m.alive = false; startFight((int)m.t, 0.8f + pet.rndf() * 0.5f + (g.sx + (EXH - 1 - g.sy)) * 0.05f, 3, false); break; }
        continue;
      }
      m.t += dt;
      float flee = d < 34 ? 1.0f : 0.0f;
      if (m.kind == CR_FIREFLY && m.t >= 100.0f) flee = 0;   // about half the fireflies are dozy: they just drift about
      float sp = m.kind == CR_BIRD ? 46.0f : (m.kind == CR_FIREFLY ? 22.0f : (m.kind == CR_BUTTERFLY ? 26.0f : 34.0f));
      if (flee > 0) { m.vx = dx / d * sp; m.vy = dy / d * sp; }
      else if ((int)(m.t * 2) != (int)((m.t - dt) * 2)) { m.vx = (pet.rndf() - 0.5f) * sp * 0.6f; m.vy = (pet.rndf() - 0.5f) * sp * 0.6f; }
      float nx = m.x + m.vx * dt, ny = m.y + m.vy * dt;
      bool flyer = m.kind == CR_BIRD || m.kind == CR_BUTTERFLY;   // (fireflies keep to where a cat can walk, plus open water: never over trees, rocks or fences)
      int t = tileAt(nx, ny);
      const bool ffWater = m.kind == CR_FIREFLY && (t == T_WATER || t == T_LILY);   // (...but they do drift out over the water)
      if (t >= 0 && (flyer || ffWater || walkable((uint8_t)t)) && !(m.kind == CR_FIREFLY && (t == T_GAP || t == T_GATE)) && t != T_DOOR && t != T_CAVE) { m.x = nx; m.y = ny; }
      else { m.vx = -m.vx; m.vy = -m.vy; }
      m.x = clampf(m.x, OX + 4.0f, OX + PW - 4.0f); m.y = clampf(m.y, OY + 6.0f, OY + PH - 4.0f);
      if (d < 7) {   // caught!
        m.alive = false; g.caught++; sv.lifePrey++; pet.d.preyCount++;
        sv.ex.journal |= 1u << m.kind;
        pet.d.hunger = clampf(pet.d.hunger + (m.kind == CR_FIREFLY || m.kind == CR_BUTTERFLY ? 2.0f : 6.0f), 0, 100);
        if (m.kind == CR_MOUSE && sv.ex.quest[1] == 1 && sv.ex.mice < 9) sv.ex.mice++;
        char b[30]; snprintf(b, sizeof(b), "Caught a %s!", CRITTER_NAME[m.kind]); say(b);
        if (m.kind == CR_FIREFLY) { g.fireflies++; if (g.fireflies <= 5) say("Firefly! You can see further."); }
        sound(SND_POUNCE);
        spawn(m.x, m.y - 8, 0, 0, 0, -20, 1.0f);
      }
    }
    // worn out: head home
    if (pet.d.energy < 8.0f && g.state == 0 && !g.mp) { say("Too tired... heading home"); endTrip(false); }
  } else if (g.state == 1) {
    g.slide += dt / 0.35f;
    if (g.slide >= 1.0f) { g.state = 0; spawnMobs(); }
  }
  if (g.mp) netMatchTick(g.sx, g.sy, g.x, g.y, g.left, g.moving);
  // King Raid: walk onto the King's square and the fight is on (everyone's hits count against the same health)
  if (g.mp && net_.coop && net_.mode == NM_MATCH && g.state == 0 && !g.fxKind && net_.raidStun <= 0 && net_.bossHp > 0 &&
      g.sy * EXW + g.sx == net_.kingSq) {
    startFight(2, 1.9f, net_.bossHp, true);   // (the same King as in the lair)
    g.coop = true; g.foeMax = net_.bossMax;
    say("Team up: throw hairballs!");
  }

  // ===================================================================== draw
  cv.clear(rgb(20, 16, 30));
  gNight = (g.room < 0 && g.state != 5) ? (night > 0.3f ? night : 0.0f) : 0.0f;   // outdoors only: caves keep their own dark
  gNLights = 0;
  auto drawMap = [&](uint8_t m[EXR][EXC], int ox, int oy, int sx, int sy, int room) {
    const bool cave = room == P_TCAVE || room == P_HERMIT || room == P_KING || room == P_SECRET || room == P_DEEP || (room < 0 && sx == CITY_SX && night > 0.5f);   // city: lit windows at night
    const int bit = sy * EXW + sx;
    for (int ty = 0; ty < EXR; ty++) for (int tx = 0; tx < EXC; tx++) {
      int x = OX + tx * TS + ox, y = OY + ty * TS + oy;
      if (x <= -TS || x >= W || y <= OY - TS || y >= OY + PH) continue;
      uint8_t t = m[ty][tx];
      uint8_t up = ty > 0 ? m[ty - 1][tx] : t, dn = ty < EXR - 1 ? m[ty + 1][tx] : t, lf = tx > 0 ? m[ty][tx - 1] : t, rt = tx < EXC - 1 ? m[ty][tx + 1] : t;
      bool sparkle = (t == T_LOOSE && !(sv.ex.rocksToday & (1u << bit))) || (t == T_NEST && !(sv.ex.nestsToday & (1u << bit))) ||
                     (t == T_TRASH && !(sv.ex.trashToday & (1u << (sy * 2 + (tx < 5 ? 0 : 1)))));
      int id = t == T_CHEST ? chestId(room, sx, sy, tx) : -1;
      const bool opened = t == T_CRACK ? ((sv.ownGl >> GL_GOGGLES) & 1) != 0 : (id >= 0 && (sv.ex.chests & (1u << id)));
      drawTile(cv, t, x, y, tx + sx * EXC, ty + sy * EXR, room + 2, g.t, cave, sparkle, opened, up, dn, lf, rt);
    }
    // whole buildings on top of their tiles
    if (room < 0) for (int ty = 0; ty < EXR; ty++) for (int tx = 0; tx < EXC; tx++) {
      if (m[ty][tx] != T_HOUSE) continue;
      if ((tx > 0 && (m[ty][tx - 1] == T_HOUSE || m[ty][tx - 1] == T_DOOR)) || (ty > 0 && (m[ty - 1][tx] == T_HOUSE || m[ty - 1][tx] == T_DOOR))) continue;
      drawHouse(cv, OX + tx * TS + ox, OY + ty * TS + oy, placeAt(sx, sy));
    }
  };
  if (g.state == 1) {
    float k = clampf(g.slide, 0, 1);
    int ox = (int)(-g.slideDx * PW * k), oy = (int)(-g.slideDy * PH * k);
    drawMap(g.prevMap, ox, oy, g.sx - g.slideDx, g.sy - g.slideDy, -1);
    drawMap(g.map, ox + g.slideDx * PW, oy + g.slideDy * PH, g.sx, g.sy, -1);
  } else {
    drawMap(g.map, 0, 0, g.sx, g.sy, g.room);
  }
  // room furnishings / NPCs
  if (g.room >= 0 && g.state != 1) {
    const int nx = OX + 5 * TS + 8, ny = OY + 3 * TS + 14;
    if (const Decor* L = houseDecor(g.room)) {   // the neighbours' furniture, windows and pictures
      cv.hline(OX + TS, OY + TS, PW - 2 * TS, rgb(150, 100, 80));                              // a skirting board along the back wall
      for (const Decor* d = L; d->kind != DK_END; d++) drawDecor(cv, d->kind, OX + d->x * TS, OY + d->y * TS, L, d->x, d->y);
    }
    if (g.room == P_GRANDMA) { cv.ellipse(OX + 5 * TS + 8, OY + 5 * TS + 8, 30, 10, rgb(170, 70, 90)); drawMiniCat(cv, nx, ny, false, g.t, 0, 0.6f, false); cv.hline(nx + 1, ny - 12, 6, rgb(230, 230, 240)); }
    if (g.room == P_TOM) { cv.ellipse(OX + 5 * TS + 8, OY + 5 * TS + 8, 28, 9, rgb(110, 84, 60)); drawMiniCat(cv, nx, ny, true, g.t, 3, 0.4f, false); }
    if (g.room == P_WHISKERS) { cv.ellipse(OX + 5 * TS + 8, OY + 5 * TS + 8, 30, 10, rgb(200, 150, 190)); drawMiniCat(cv, nx, ny, false, g.t, 1, 0.0f, false); }
    if (g.room == P_HERMIT) drawHermit(cv, nx, ny + 2, g.t);
    if (g.room == P_SECRET) drawHermit(cv, nx, ny + 2, g.t, true);
    if (g.room == P_KING && !(sv.ex.flags & 4)) {
      const int kx = OX + 5 * TS + 8, ky = OY + 2 * TS + 14;
      drawMiniCat(cv, kx - 3, ky, false, g.t, 2, 1.0f, false); drawMiniCat(cv, kx + 3, ky, true, g.t, 2, 1.0f, false);   // a BIG cat
      cv.rect(kx - 3, ky - 15, 7, 3, rgb(250, 210, 60)); cv.set(kx - 3, ky - 16, rgb(250, 210, 60)); cv.set(kx, ky - 17, rgb(250, 210, 60)); cv.set(kx + 3, ky - 16, rgb(250, 210, 60));
    }
    if (g.room == P_HOME) {}
  }
  // critters (and, indoors, a stray cat curled up on the bed)
  if (g.state != 1)
    for (int i = 0; i < g.nmob; i++) {
      const ExMob& m = g.mob[i];
      if (!m.alive) continue;
      if (g.room >= 0 && m.kind != MOB_WILDCAT) continue;
      if (m.kind == MOB_CAR) {
        static const Color cc[4] = {rgb(220, 50, 50), rgb(60, 120, 220), rgb(250, 200, 50), rgb(240, 240, 240)};
        if (m.phase == 3) continue;   // between trips
        int x = (int)m.x, y = (int)m.y; bool l = m.phase == 0 || (m.phase == 1 && m.aux2 < 0.3f);
        if (m.phase == 1 && m.aux2 >= 0.3f && m.aux2 <= 0.7f) {   // halfway round the U-turn: nose towards us
          const Color body = cc[(int)m.t & 3];
          cv.rect(x - 7, y - 5, 14, 9, rgb(20, 20, 24)); cv.rect(x - 6, y - 4, 12, 7, body);
          cv.rect(x - 4, y - 8, 8, 4, body); cv.rect(x - 3, y - 7, 6, 2, rgb(160, 210, 240));   // windscreen
          cv.rect(x - 6, y + 3, 3, 2, rgb(20, 20, 24)); cv.rect(x + 3, y + 3, 3, 2, rgb(20, 20, 24));
          cv.set(x - 5, y, rgb(255, 250, 200)); cv.set(x + 4, y, rgb(255, 250, 200));   // both headlights
          addLight(x, y + 8, 16, 170);
          continue;
        }
        cv.rect(x - 11, y - 5, 22, 9, rgb(20, 20, 24));
        cv.rect(x - 10, y - 4, 20, 7, cc[(int)m.t & 3]);
        cv.rect(x - 4, y - 8, 9, 4, cc[(int)m.t & 3]); cv.rect(x - 3, y - 7, 7, 2, rgb(160, 210, 240));
        cv.disc(x - 6, y + 4, 2, rgb(20, 20, 24)); cv.disc(x + 6, y + 4, 2, rgb(20, 20, 24));
        cv.set(l ? x - 10 : x + 9, y - 2, rgb(255, 250, 200));
        addLight(l ? x - 18 : x + 17, y - 1, 16, 170);   // headlights
        continue;
      }
      if (m.kind == MOB_WILDCAT) { drawMiniCat(cv, (int)m.x, (int)m.y, true, g.t, (int)m.t, 0.3f, false); if (((int)(g.t * 2)) & 1) cv.text((int)m.x + 4, (int)m.y - 20, "!", rgb(255, 120, 120)); }
      else drawCritter(cv, m, g.t);
    }
  // fish in the river (not drawn while under a bridge)
  if (g.state != 1 && g.room < 0)
    for (int i = 0; i < g.nfish; i++) {
      const auto& f = g.fish[i];
      if (!f.alive || f.y < OY || f.y >= OY + PH || tileAt(f.x, f.y) == T_BRIDGE) continue;
      const int x = (int)(f.x + sinf(f.t * 7) * 1.5f), y = (int)f.y, d = f.vy > 0 ? 1 : -1;
      cv.ellipse(x, y, 2, 4, rgb(240, 150, 60));                  // body, head first in the swim direction
      cv.set(x, y + d * 3, rgb(40, 30, 30));                       // eye
      cv.tri(x - 3, y - d * 7, x + 3, y - d * 7, x, y - d * 4, rgb(250, 190, 90));   // tail
      cv.set(x - 1, y - d, rgb(255, 220, 160));
    }
  // little pulsing arrows where you can walk off to the next screen
  if (g.room < 0 && g.state == 0) {
    const Color ac = (((int)(g.t * 3)) & 1) ? rgb(255, 240, 170) : rgb(220, 190, 110);
    for (int side = 1; side <= 4; side++) {
      if ((side == 1 && g.sx == 0) || (side == 2 && g.sx == EXW - 1) || (side == 3 && g.sy == 0) || (side == 4 && g.sy == EXH - 1)) continue;
      const bool vert = side <= 2;
      const int n = vert ? EXR : EXC;
      for (int i = 0; i < n; ) {   // one arrow in the middle of each open stretch of edge
        auto open = [&](int k) { return canWalk(side == 1 ? 0 : (side == 2 ? EXC - 1 : k), vert ? k : (side == 3 ? 0 : EXR - 1)); };
        if (!open(i)) { i++; continue; }
        int j = i; while (j + 1 < n && open(j + 1)) j++;
        const float mid = (i + j + 1) * 0.5f * TS;
        if (side == 1) { int y = OY + (int)mid; cv.tri(OX + 1, y, OX + 5, y - 4, OX + 5, y + 4, ac); }
        else if (side == 2) { int y = OY + (int)mid; cv.tri(OX + PW - 2, y, OX + PW - 6, y - 4, OX + PW - 6, y + 4, ac); }
        else if (side == 3) { int x = OX + (int)mid; cv.tri(x, OY + 1, x - 4, OY + 5, x + 4, OY + 5, ac); }
        else { int x = OX + (int)mid; cv.tri(x, OY + PH - 2, x - 4, OY + PH - 6, x + 4, OY + PH - 6, ac); }
        i = j + 1;
      }
    }
  }
  if (g.mp && g.state != 1) netDrawPeers(cv, g.sx, g.sy, g.t);   // the other players on this square
  // where a tap-to-walk is heading: a small pulsing ring
  if (g.autoWalk && g.state == 0) {
    const int gx = (int)clampf(g.goalX, OX + 4.0f, OX + PW - 4.0f), gy = (int)clampf(g.goalY, OY + 4.0f, OY + PH - 4.0f);
    const int rr = 3 + (((int)(g.t * 4)) & 1);
    cv.ring(gx, gy, rr, rgb(255, 240, 170)); cv.set(gx, gy, rgb(255, 240, 170));
  }
  // the explorer
  {
    int cx = (int)g.x, cy = (int)g.y + 4;
    if (g.state == 1) {   // during the slide they ride along with the new screen's edge
      float k = clampf(g.slide, 0, 1);
      cx = (int)(g.x + (-g.slideDx * PW * k) + g.slideDx * PW);
      cy = (int)(g.y + (-g.slideDy * PH * k) + g.slideDy * PH) + 4;
    }
    cv.ellipse(cx, cy + 1, 5, 2, rgb(30, 30, 30));
    drawMiniCat(cv, cx, cy, g.left, g.t, pet.lookBreed(), pet.fatLook(), g.moving || g.state == 1);
    if (g.napT > 0) {   // z z z
      for (int k = 0; k < 3; k++) {
        const float p = fmodf(g.t * 0.8f + k / 3.0f, 1.0f);
        cv.text(cx + 3 + (int)(p * 8), cy - 14 - (int)(p * 14), "z", rgb(220, 230, 255));
      }
    }
  }
  updateParticles(dt);
  drawParticles(cv);
  // light: caves are dark, nights are dim
  if (g.state != 1 && g.state != 5) {
    if (inCave) {
      // fireflies caught this trip light the way underground too, but only half as well as outside
      // (4.5 px each instead of 9), and at most doubling what they can see
      const int ffc = g.fireflies > 5 ? 5 : g.fireflies;
      const int base = g.room == P_DEEP ? 20 : 34;   // the Deep Tunnels: pitch dark, they see only a step or two
      int r = base + ffc * 9 / 2; if (r > base * 2) r = base * 2;
      darkness(cv, (int)g.x, (int)g.y, r, g.room == P_DEEP ? 4 : 18);
      for (int i = 0; i < g.nmob; i++) {   // a stray on the bed: its eyes glint in the dark, so you know it's there
        const ExMob& m = g.mob[i];
        if (!m.alive || m.kind != MOB_WILDCAT) continue;
        const int mx = (int)m.x, my = (int)m.y;
        if (fabsf(m.x - g.x) < r * 0.8f && fabsf(m.y - g.y) < r * 0.8f) continue;   // (close by, it's in the light anyway)
        if (fmodf(g.t, 3.0f) > 0.15f) { cv.set(mx - 5, my - 10, rgb(255, 230, 120)); cv.set(mx - 2, my - 10, rgb(255, 230, 120)); }   // (a blink now and then)
        if (((int)(g.t * 2)) & 1) cv.text(mx + 4, my - 20, "!", rgb(255, 120, 120));
      }
      for (int k = 0; k < ffc; k++) {
        const float a = g.t * 2.2f + k * 1.2566f;
        cv.set((int)(g.x + cosf(a) * 9), (int)(g.y - 4 + sinf(a) * 5), (((int)(g.t * 6) + k) & 1) ? rgb(230, 255, 150) : rgb(160, 200, 90));
      }
    }
    else if (g.room < 0 && night > 0.3f) {
      // night: moonlit, with warm pools of light. They carry a soft glow of their own; every firefly they
      // catches this trip makes it bigger, and the wild fireflies light little pools you can chase.
      const int ff = g.fireflies > 5 ? 5 : g.fireflies;
      const int r = (int)(48 - 22 * night) + ff * 9;
      addLight((int)g.x, (int)g.y, r, 210);
      for (int i = 0; i < g.nmob; i++) {
        const ExMob& m = g.mob[i];
        if (m.alive && m.kind == CR_FIREFLY && sinf(g.t * 5 + m.t * 3) > -0.2f) addLight((int)m.x, (int)m.y, 14, 120);
      }
      nightGrade(cv, night);
      // fireflies glow through the dark, so you can go after them
      for (int i = 0; i < g.nmob; i++) {
        const ExMob& m = g.mob[i];
        if (!m.alive || m.kind != CR_FIREFLY) continue;
        if (sinf(g.t * 5 + m.t * 3) > -0.2f) { cv.disc((int)m.x, (int)m.y, 2, rgb(200, 255, 120)); cv.set((int)m.x, (int)m.y, rgb(255, 255, 230)); }
      }
      // the ones they're caught buzz around them
      for (int k = 0; k < ff; k++) {
        const float a = g.t * 2.2f + k * 1.2566f;
        cv.set((int)(g.x + cosf(a) * 9), (int)(g.y - 4 + sinf(a) * 5), (((int)(g.t * 6) + k) & 1) ? rgb(230, 255, 150) : rgb(160, 200, 90));
      }
    }
  }
  if (g.irisT > 0) {   // going in or out of somewhere: the view opens up from the cat (an iris wipe), not a hard cut
    const float full = g.room == P_DEEP ? 0.5f : 0.4f;
    if (g.state == 0) darkness(cv, (int)g.x, (int)g.y - 4, (int)((1.0f - g.irisT / full) * 150.0f), 0);
    g.irisT -= dt;
  }
  if (g.state == 1 && gNight > 0) {   // sliding to the next screen at night: keep it night (their glow rides along)
    const float k = clampf(g.slide, 0, 1);
    addLight((int)(g.x + (-g.slideDx * PW * k) + g.slideDx * PW), (int)(g.y + (-g.slideDy * PH * k) + g.slideDy * PH), (int)(48 - 22 * night), 210);
    nightGrade(cv, night);
  }
  if (g.room < 0 && weather == 2 && g.state != 5) {   // rain
    for (int n = 0; n < 40; n++) {
      int rx = OX + (int)(hsh(n, 1, 9) % PW), ry = OY + (int)fmodf(hsh(n, 2, 9) % PH + g.t * (140 + hsh(n, 3, 9) % 60), (float)PH);
      cv.line(rx, ry, rx - 1, ry + 3, rgb(170, 196, 230));
    }
  }

  // dialog overlay
  if (g.state == 2) {
    cv.panel(10, 70, 164, 96, C_PANEL, C_BORDER);
    cv.textCenter(W / 2, 76, g.dTitle, C_GOLD);
    for (int i = 0; i < 3; i++) cv.textCenter(W / 2, 92 + i * 11, g.dLine[i], C_TXT);
    const bool two = g.dB[0] != 0;
    // the finger that opened this (e.g. on a sign right under the OK button) mustn't close it straight away:
    // buttons only listen after a short moment, and only to a fresh press
    const bool ready = g.t - g.dOpenT > 0.35f;
    if (button(cv, two ? 18 : 52, 140, two ? 70 : 80, 20, g.dA, C_GREEN, ready) && ready) {
      g.state = 0;
      if (g.dAct == 1) {   // hermit's charm
        if (sv.coins >= 10) { sv.coins -= 10; sv.slowItem = 1; updateToys(); sound(SND_WIN); say("The charm glows warmly."); autosave(true); }
        else { sound(SND_LOSE); say("Not enough hairballs!"); }
      } else if (g.dAct == 2) startFight(2, 1.9f, 10, true);   // the Alley King: 10 hairball hits
      else if (g.dAct == 3) {   // the Hidden Hermit's amulet
        if (sv.coins >= 40) { sv.coins -= 40; sv.ex.flags |= 64; updateToys(); sound(SND_WIN); say("You feel strong and swift!"); autosave(true); }
        else { sound(SND_LOSE); say("Not enough hairballs!"); }
      }
    }
    if (two && button(cv, 96, 140, 70, 20, g.dB, C_PURP, ready) && ready) g.state = 0;
  }

  // fight overlay (press SCRATCH when the line is in the green; heavy cats hit hardest).
  // While wiping in, the world stays visible until the bars have closed.
  const bool fxBusy = g.fxKind != 0;
  if (g.state == 3 && !(g.fxKind == 1 && g.fxT < 0.55f)) {
    g.stateT += dt;
    cv.dimRect(0, OY, W, PH, 45);
    cv.panel(6, 40, 172, 140, C_PANEL, C_BORDER);
    cv.textCenter(W / 2, 46, g.myHp <= 0 || g.foeHp <= 0 ? (g.foeHp <= 0 ? "YOU WIN!" : "YOU LOSE") : (g.boss ? "THE ALLEY KING!" : "A WILD CAT!"), C_GOLD);
    { CatDraw d; d.breed = pet.lookBreed(); d.fat = pet.fatLook(); d.size = 0.85f; d.kit = pet.kitFactor(); d.pose = (g.lastHit == 1 ? POSE_POUNCE : POSE_SIT); d.face = g.myHp <= 0 ? FACE_SAD : FACE_ANGRY; d.t = g.t; d.faceLeft = false;
      fitCatHeight(d, 112, 58);   // a big (chonky / fluffy) cat stays under the title
      gShadowKey = false; drawCat(cv, 52 + (g.lastHit == 1 ? 8 : 0), 112, d); }
    { CatDraw d; d.breed = g.foeSpecies; d.fat = g.boss ? 1.0f : 0.3f; d.size = g.boss ? 0.92f : 0.85f; d.pose = (g.lastHit == 2 ? POSE_POUNCE : POSE_SIT); d.face = g.foeHp <= 0 ? FACE_SAD : FACE_ANGRY; d.t = g.t; d.faceLeft = true;
      fitCatHeight(d, 112, g.boss ? 60 : 58);   // (the boss's crown sits just above its ears)
      int fx = 132 - (g.lastHit == 2 ? 8 : 0), fy = 112;
      if (g.boss && g.foeHp > 0 && g.myHp > 0) {
        if (g.bossPhase == 1) d.pose = POSE_CROUCH;                       // winding up...
        else if (g.bossPhase == 2) {                                       // ...LEAP across at you
          const float k = clampf(1.0f - g.bossPhaseT / 0.55f, 0, 1);
          d.pose = POSE_POUNCE; fx = 132 - (int)(k * 64); fy = 112 - (int)(sinf(k * 3.14159f) * 24);
        }
      }
      CatMetrics fm = drawCat(cv, fx, fy, d);
      if (g.boss && g.bossPhase == 1 && ((int)(g.t * 10) & 1)) cv.textCenter(fm.headX, fm.topY - 12, "!", rgb(255, 110, 90), 2);
      if (g.boss) { int kx = fm.headX, ky = fm.topY;
        if (fm.eyeY >= 0) { kx = fm.eyeX; const int ty = fm.eyeY - (int)(15 * d.size); if (ty > ky) ky = ty; }   // on the head, not up on a raised tail mid-leap
        cv.rect(kx - 6, ky - 3, 12, 4, rgb(250, 210, 60)); cv.set(kx - 6, ky - 5, rgb(250, 210, 60)); cv.set(kx, ky - 6, rgb(250, 210, 60)); cv.set(kx + 5, ky - 5, rgb(250, 210, 60)); } }
    for (int i = 0; i < g.myMax; i++) gdisc(cv, 24 + i * 10, 58, 3, i < g.myHp ? rgb(240, 80, 110) : rgb(70, 60, 90));
    if (g.coop) {   // King Raid: everyone's hits count, and his health is a bar (it can be 30+ hits)
      if (net_.bossHp < g.foeHp) g.foeHp = net_.bossHp;
      cv.rect(96, 55, 70, 6, rgb(24, 18, 40));
      cv.rect(96, 55, g.foeMax > 0 ? 70 * (g.foeHp < 0 ? 0 : g.foeHp) / g.foeMax : 0, 6, rgb(240, 80, 110));
      cv.frame(95, 54, 72, 8, rgb(20, 12, 36));
    } else
    for (int i = 0; i < g.foeMax; i++) gdisc(cv, 160 - i * (g.foeMax > 6 ? 9 : 10), 58, 3, i < g.foeHp ? rgb(240, 80, 110) : rgb(70, 60, 90));
    bool done = g.myHp <= 0 || g.foeHp <= 0;
    if (!done) {
      // a hairball in flight: it lands (or misses) when it gets there
      if (g.throwT > 0) {
        g.throwT -= dt;
        const float k = clampf(1.0f - g.throwT / 0.4f, 0, 1);
        const int hx = 64 + (int)(k * 66), hy = 98 - (int)(sinf(k * 3.14159f) * 26) + (g.throwHit ? 0 : (int)(k * k * 22));
        drawHairball(cv, g.throwHit || k < 0.7f ? hx : hx + (int)((k - 0.7f) * 60), hy, 4);
        if (g.throwT <= 0) {
          if (g.throwHit) {
            g.foeHp = g.boss ? g.foeHp - 1 : 0;   // one hit knocks a wild cat out; the King needs 10
            if (!g.boss && !g.coop) g.hbKO = true;
            if (g.coop) net_.myHits++;            // (the raid: the host adds up everyone's)
            g.lastHit = 1; sound(SND_SQUISH);
            say(g.boss ? (g.foeHp > 0 ? "SPLAT! Keep them coming!" : "SPLAT! The King is down!") : "SPLAT! Knocked out!");
          } else {
            g.lastHit = 0; sound(SND_MISS); say(g.boss ? "The King dodged it!" : "Missed!");
            if (!g.boss) {   // a wild cat gets a swipe in when you miss
              const float pf = clampf(0.38f * g.foeStr / clampf(pet.agility(), 0.6f, 1.3f), 0.05f, 0.8f);
              if (pet.rndf() < pf) { g.myHp--; g.lastHit = 2; }
            }
          }
          g.roundT = 0.001f;
        }
      }
      if (g.throwCd > 0) g.throwCd -= dt;
      const bool canThrow = (sv.coins > 0 || g.coop) && g.throwT <= 0 && g.throwCd <= 0 && !fxBusy;
      auto throwHairball = [&]() {
        if (!g.coop) sv.coins--;   // (in the raid the team's hairballs are on the house)
        g.throwT = 0.4f; g.throwCd = g.boss ? 1.0f : 0.4f;   // winding up the next one takes a moment
        g.throwHit = pet.rndf() >= (g.boss ? 0.33f : 0.05f);   // the King dodges about a third; wild cats almost never
        sound(SND_POP);
      };
      if (g.boss) {
        // the King doesn't care for scratches: only hairballs hurt them. They leap at you on an uneven rhythm:
        // a crouch (the warning, sometimes a fake-out), then a leap; tap DEFEND while they're in the air.
        if (!fxBusy) {
          g.bossPhaseT -= dt;
          if (g.bossPhase == 0 && g.bossPhaseT <= 0) {
            g.bossPhase = 1; g.bossPhaseT = 0.25f + pet.rndf() * 0.7f; g.feint = pet.rndf() < 0.22f; g.defended = false; g.defLock = false;
          } else if (g.bossPhase == 1 && g.bossPhaseT <= 0) {
            if (g.feint) { g.bossPhase = 0; g.bossPhaseT = 0.3f + pet.rndf() * 0.9f; }   // ha! not this time
            else { g.bossPhase = 2; g.bossPhaseT = 0.55f; sound(SND_POUNCE); }
          } else if (g.bossPhase == 2 && g.bossPhaseT <= 0) {
            if (g.defended) { sound(SND_SQUISH); say("Blocked!"); }
            else { g.myHp--; g.lastHit = 2; g.roundT = 0.001f; sound(SND_HISS); say("OUCH! Defend the leaps!"); }
            g.bossPhase = 0;
            g.bossPhaseT = pet.rndf() < 0.25f ? 0.2f + pet.rndf() * 0.3f : 0.6f + pet.rndf() * 1.6f;   // sometimes straight back at you
          }
        }
        char hb[30]; snprintf(hb, sizeof(hb), "Hairball hits %d/%d", g.foeMax - g.foeHp, g.foeMax);
        cv.textCenter(W / 2, 120, hb, rgb(255, 214, 120));
        cv.textCenter(W / 2, 132, g.bossPhase == 2 ? "DEFEND NOW!" : (g.bossPhase == 1 ? "Watch out..." : "Throw hairballs!"), g.bossPhase == 2 ? rgb(255, 120, 100) : C_DIM);
        if (g.lastHit) g.roundT += dt;
        if (g.roundT > 0.45f) { g.lastHit = 0; g.roundT = 0; }
        if (button(cv, 22, 148, 68, 20, g.coop ? "THROW" : (sv.coins ? "HAIRBALL" : "NO HB!"), rgb(200, 150, 70), canThrow) && canThrow) throwHairball();
        if (button(cv, 94, 148, 68, 20, "DEFEND", g.bossPhase == 2 ? rgb(220, 80, 80) : rgb(80, 110, 190), !fxBusy) && !fxBusy) {
          if (g.bossPhase == 2 && !g.defLock) g.defended = true;
          else if (g.bossPhase != 2) { g.defLock = true; say("Too early!"); }   // mashing doesn't work: wait for the leap
        }
      } else {
        if (!fxBusy) g.marker += g.markerDir * dt * (1.3f + 0.4f * g.foeStr);
        if (g.marker > 1) { g.marker = 1; g.markerDir = -1; } else if (g.marker < 0) { g.marker = 0; g.markerDir = 1; }
        int bx = 22, bw = 140, by = 130;
        cv.rect(bx, by, bw, 10, rgb(50, 40, 70));
        cv.rect(bx + (int)((g.zoneC - g.zoneW) * bw), by, (int)(g.zoneW * 2 * bw), 10, rgb(70, 200, 90));
        cv.frame(bx - 1, by - 1, bw + 2, 12, rgb(20, 12, 36));
        cv.rect(bx + (int)(g.marker * bw) - 1, by - 3, 3, 16, rgb(255, 255, 255));
        cv.textCenter(W / 2, 118, "SCRATCH on the green!", C_DIM);
        if (g.lastHit) g.roundT += dt;
        if (g.roundT > 0.45f) { g.lastHit = 0; g.roundT = 0; }
        const bool canSwing = g.roundT == 0 && g.throwT <= 0 && !fxBusy;
        if (button(cv, 22, 148, 68, 20, "SCRATCH!", rgb(200, 110, 70), canSwing) && canSwing) {
          bool hit = fabsf(g.marker - g.zoneC) < g.zoneW;
          if (hit) {
            const bool slam = pet.d.chonk >= idealW + 20;
            g.foeHp -= slam ? 2 : 1; g.lastHit = 1; sound(SND_POUNCE);
            if (slam) say("BODY SLAM! x2");
          } else { g.lastHit = 2; sound(SND_MISS); }
          float pf = clampf((hit ? 0.15f : 0.38f) * g.foeStr / clampf(pet.agility(), 0.6f, 1.3f), 0.05f, 0.8f);
          if (pet.rndf() < pf) { g.myHp--; g.lastHit = 2; }
          g.roundT = 0.001f;
          rollZone();   // hit or miss, the green zone moves for the next swing
        }
        if (button(cv, 94, 148, 68, 20, sv.coins ? "HAIRBALL" : "NO HB!", rgb(200, 150, 70), canThrow && g.roundT == 0) && canThrow && g.roundT == 0) throwHairball();
      }
      cv.textCenter(W / 2, 170, g.coop ? "Raid hairballs are free!" : "HAIRBALL costs 1", rgb(150, 136, 180));
    } else if (g.coop) {   // King Raid: down, or knocked out (back on your feet in a few seconds)
      if (g.foeHp <= 0) cv.textCenter(W / 2, 122, "The King is DOWN!", rgb(255, 240, 160));
      else { cv.textCenter(W / 2, 118, "Knocked out!", rgb(255, 170, 170)); cv.textCenter(W / 2, 128, "Up again in a few seconds", C_DIM); }
      if (button(cv, 62, 142, 60, 22, "OK", g.foeHp <= 0 ? C_GREEN : C_RED) && !fxBusy) {
        if (g.foeHp > 0) net_.raidStun = NET_RAID_DAZE;
        g.coop = false; g.boss = false;
        g.fxKind = 2; g.fxT = 0; g.fxAfter = 1;
      }
    } else {
      if (g.foeHp <= 0) {
        if (g.boss) {
          cv.textCenter(W / 2, 118, "The crown is yours!", rgb(255, 240, 160));
          cv.textCenter(W / 2, 128, "+ the Classy Log Cabin!", rgb(255, 240, 160));
          cv.textCenter(W / 2, 138, "Exhausting: -15 energy", rgb(150, 200, 255));
          if (button(cv, 62, 148, 60, 22, "YES!", C_GREEN) && !fxBusy) {
            pet.d.energy = clampf(pet.d.energy - 15, 0, 100);
            sv.ex.flags |= 4; sv.ownHat |= (uint8_t)(1u << HAT_CROWN); sv.roomsOwned |= (uint8_t)(1u << 5);
            g.wins++; sound(SND_WIN); toast("Crown + Log Cabin unlocked!"); recordMilestone(MS_KING); autosave(true);
            g.fxKind = 2; g.fxT = 0; g.fxAfter = 1;
          }
        } else {
          cv.textCenter(W / 2, 122, "Prize: 1 hairball!", rgb(255, 240, 160));
          cv.textCenter(W / 2, 132, "Tiring: -8 energy", rgb(150, 200, 255));
          if (button(cv, 62, 142, 60, 22, "NICE", C_GREEN) && !fxBusy) {
            giveCoins(1); g.found++; g.wins++; recordMilestone(MS_WILD);
            pet.d.energy = clampf(pet.d.energy - 8, 0, 100);
            if (g.hbKO && !(sv.ex.flags & EXF_TRAIL_PAWS)) {   // the first win with a hairball: the loser drops their Trail Paws
              sv.ex.flags |= EXF_TRAIL_PAWS;
              toast("New item: TRAIL PAWS!"); toast("Each screen now costs 8 energy");
              autosave(true);
            }
            if (g.room >= 0) g.bedCatsBeaten |= (uint16_t)(1u << g.room);   // the bed is free now
            else g.catsBeaten |= 1u << (g.sy * EXW + g.sx);
            say("Won the fight!"); sound(SND_WIN);
            g.fxKind = 2; g.fxT = 0; g.fxAfter = 1;
          }
        }
      } else {
        cv.textCenter(W / 2, 126, "Ouch. Time to go home.", rgb(255, 170, 170));
        if (button(cv, 62, 142, 60, 22, "OK", C_RED) && !fxBusy) { sound(SND_LOSE); g.fxKind = 2; g.fxT = 0; g.fxAfter = 2; }
      }
    }
  }

  // fight transitions: IN = the screen flickers, stripes slam shut from both sides, "FIGHT!", then they slide open on
  // the fight. OUT = stripes close over the fight, and open again on the world (or the trip summary if they lost).
  if (g.fxKind) {
    g.fxT += dt;
    float c = 0; bool flick = false; const char* label = nullptr;
    if (g.fxKind == 1) {
      if (g.fxT - dt < 0.55f && g.fxT >= 0.55f) sound(SND_POUNCE);   // "boing!" as the bars slam shut
      if (g.fxT < 0.3f) flick = ((int)(g.fxT / 0.075f)) & 1;
      else if (g.fxT < 0.55f) c = (g.fxT - 0.3f) / 0.25f;
      else if (g.fxT < 0.8f) { c = 1; label = g.boss ? "BOSS FIGHT!" : "FIGHT!"; }
      else if (g.fxT < 1.1f) c = 1 - (g.fxT - 0.8f) / 0.3f;
      else g.fxKind = 0;
    } else {
      if (g.fxT < 0.3f) c = g.fxT / 0.3f;
      else if (g.fxT < 0.42f) {
        c = 1;
        if (g.fxAfter) { const int a = g.fxAfter; g.fxAfter = 0; if (a == 1) g.state = 0; else endTrip(true); }
      } else if (g.fxT < 0.72f) c = 1 - (g.fxT - 0.42f) / 0.3f;
      else g.fxKind = 0;
    }
    if (flick) cv.dimRect(0, 0, W, H, 35);
    if (c > 0) {
      c = c * c * (3 - 2 * c);   // ease
      const int bands = 8, bh = (H + bands - 1) / bands, len = (int)(c * W);
      for (int b = 0; b < bands; b++) {
        const int y = b * bh;
        const Color bc = (b & 1) ? rgb(30, 18, 44) : rgb(44, 26, 60);
        if (b & 1) { cv.rect(W - len, y, len, bh, bc); if (len < W) cv.vline(W - len, y, bh, rgb(250, 210, 120)); }
        else { cv.rect(0, y, len, bh, bc); if (len < W) cv.vline(len - 1, y, bh, rgb(250, 210, 120)); }
      }
    }
    if (label) cv.textCenter(W / 2, H / 2 - 8, label, rgb(255, 120, 90), 2);
  }

  // map / journal overlay
  if (g.state == 5) {
    cv.panel(4, 26, 176, 158, C_PANEL, C_BORDER);
    if (button(cv, 10, 30, 56, 14, "MAP", g.mapTab == 0 ? rgb(120, 90, 190) : rgb(60, 48, 92))) g.mapTab = 0;
    if (button(cv, 70, 30, 66, 14, "JOURNAL", g.mapTab == 1 ? rgb(120, 90, 190) : rgb(60, 48, 92))) g.mapTab = 1;
    if (button(cv, 140, 30, 34, 14, "X", C_RED)) g.state = 0;
    char b[32];
    if (g.mapTab == 0) {
      int seen = 0;
      for (int y = 0; y < EXH; y++) for (int x = 0; x < EXW; x++) {
        const int cx = 14 + x * 26, cy = 50 + y * 22;
        const bool s = (sv.ex.seen >> (y * EXW + x)) & 1;
        seen += s;
        cv.rect(cx, cy, 24, 20, s ? (x == CITY_SX ? rgb(120, 120, 136) : (x == 2 ? rgb(70, 130, 190) : rgb(80, 150, 80))) : rgb(30, 24, 40));
        cv.frame(cx, cy, 24, 20, rgb(20, 14, 30));
        if (!s) { cv.textCenter(cx + 12, cy + 7, "?", rgb(80, 70, 100)); continue; }
        int p = placeAt(x, y);
        const char* ic = p == P_HOME ? "H" : (p == P_TCAVE || p == P_HERMIT || p == P_KING ? "C" : (p >= 0 ? "h" : nullptr));
        if (p == P_SECRET) ic = (sv.ex.flags & 32) ? "C" : nullptr;
        if (x == 2 && y == 2) ic = "F";
        if (ic) cv.textCenter(cx + 12, cy + 7, ic, rgb(255, 250, 220));
        if (x == g.sx && y == g.sy && ((int)(g.t * 3) & 1)) cv.frame(cx + 1, cy + 1, 22, 18, rgb(255, 240, 120));
      }
      cv.textCenter(W / 2, 163, "H home  h house  C cave", C_DIM);
      snprintf(b, sizeof(b), "Explored %d%%", seen * 100 / (EXW * EXH));
      cv.textCenter(W / 2, 173, b, C_TXT);
    } else {
      int got = 0;
      for (int i = 0; i < CR_COUNT; i++) {
        bool has = (sv.ex.journal >> i) & 1; got += has;
        cv.text(12 + (i % 2) * 84, 50 + (i / 2) * 10, has ? CRITTER_NAME[i] : "???", has ? C_TXT : rgb(90, 80, 110));
      }
      int pl = 0;
      const bool secretFound = (sv.ex.journal >> (8 + P_SECRET)) & 1;   // the Hidden Cave isn't listed (or counted) until it's found
      for (int i = 0; i < P_DEEP; i++) {   // (the Deep Tunnels stay a secret: never listed)
        bool has = (sv.ex.journal >> (8 + i)) & 1; pl += has;
        if (i == P_SECRET && !has) continue;
        cv.text(12 + (i % 2) * 84, 92 + (i / 2) * 10, has ? PLACE_NAME[i] : "???", has ? rgb(200, 230, 255) : rgb(90, 80, 110));
      }
      snprintf(b, sizeof(b), "Critters %d/%d  Places %d/%d", got, CR_COUNT, pl, P_DEEP - (secretFound ? 0 : 1));
      cv.textCenter(W / 2, 162, b, C_GOLD);
      int q = (sv.ex.quest[0] == 2) + (sv.ex.quest[1] == 2) + (sv.ex.quest[2] == 2) + ((sv.ex.flags & 4) ? 1 : 0);
      snprintf(b, sizeof(b), "Quests done %d/4", q);
      cv.textCenter(W / 2, 172, b, C_DIM);
    }
  }

  // summary
  if (g.state == 6) {
    cv.clear(rgb(30, 40, 54));
    cv.textCenter(W / 2, 30, g.lost ? "BACK HOME TO REST" : "HOME SWEET HOME", C_GOLD, 1);
    drawLittleCat(cv, 92, 110, g.lost ? POSE_SLEEP : POSE_SIT, g.lost ? FACE_SLEEP : FACE_HAPPY, false, 1.1f, 42);   // ears stay under the title
    char b[40];
    snprintf(b, sizeof(b), "Screens walked: %d", g.screens); cv.textCenter(W / 2, 126, b, C_TXT);
    snprintf(b, sizeof(b), "Critters %d   Fights won %d/%d", g.caught, g.wins, g.fights); cv.textCenter(W / 2, 138, b, C_TXT);
    snprintf(b, sizeof(b), "Hairballs found: %d", g.found); cv.textCenter(W / 2, 150, b, C_GOLD);
    if (g.places) { snprintf(b, sizeof(b), "New places: %d", g.places); cv.textCenter(W / 2, 162, b, rgb(200, 230, 255)); }
    snprintf(b, sizeof(b), "Energy -%d", (int)EXPLORE_ENERGY); cv.textCenter(W / 2, 172, b, rgb(150, 200, 255));
    if (button(cv, 56, 182, 72, 24, "HOME", C_GREEN)) { setScreen(S_HOME); return; }
    return;
  }

  // ---- HUD (top) and status (bottom), kept inside the rounded corners ----
  cv.rect(0, 0, W, OY, rgb(40, 30, 62));
  cv.hline(0, OY - 1, W, rgb(96, 70, 130));
  if (g.mp) {   // Explore Together: the match's own HUD, the fight on top
    if (g.state == 0 && !net_.f.on && backButton(cv)) { netStop(); g.active = false; toast("You left the match"); setScreen(S_HOME); return; }
    netDrawHud(cv, g.sx, g.sy, OX, OY, PW, PH);
    netDrawFight(cv);
    drawToasts(cv);
    return;
  }
  if (g.state == 0 && backButton(cv)) { setScreen(S_HOME); return; }   // the trip waits for their (until they sleep)
  drawIconS(cv, 9, 70, 10, 0.5f);
  gbar(cv, 78, 6, 50, 7, (int)pet.d.energy, pet.d.energy < 25 ? rgb(240, 70, 70) : rgb(100, 180, 255));
  { char b[16]; coinText(b, sizeof(b));
    int tw = Canvas::textWidth(b); drawIconS(cv, 6, W - CORNER - tw - 6, 10, 0.5f); cv.text(W - CORNER - tw, 6, b, coinColor(C_GOLD)); }
  cv.rect(0, OY + PH, W, H - OY - PH, rgb(40, 30, 62));
  cv.hline(0, OY + PH, W, rgb(96, 70, 130));
  if (g.msgT > 0) cv.textCenter(W / 2, OY + PH + 3, g.msg, rgb(255, 240, 160));
  else {
    const char* where = g.room >= 0 ? PLACE_NAME[g.room] : (night > 0.5f ? "Dark! Catch fireflies" : (weather == 2 ? "Rainy: frogs by the river" : "Drag to walk"));
    cv.textCenter(W / 2, OY + PH + 3, where, C_DIM);
  }
  if (g.state == 0 && button(cv, CORNER, 199, 54, 18, "MAP", C_PURP)) { g.state = 5; g.mapTab = 0; }
  drawToasts(cv);
}

}  // namespace cat
