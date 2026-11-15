// Rebuilds the Skogkatt's front-facing (sitting) frames in src/game/catsprites_data.cpp.
//
// The Skogkatt ("fluffy") set is the tabby with fur tufts added by art/fluffgen.py. For the sitting, facing-you frames
// that came out ragged: spikes all round, and the tufts on the tail read as an extra limb. This tool replaces just
// those 9 frames (3 bodies x 3 sit frames) with the tabby's own sitting frames plus a tidier coat, so the Skogkatt
// faces you with the same build, outline, paws and tail as every other breed:
//   - a ruff of short, downswept teeth on both cheeks (mirrored, so the face stays symmetrical)
//   - smaller teeth down both flanks, stopping above the paws
//   - short lynx tufts on the ear tips
//   - a few fur ticks on the chest bib
// It works on the baked data itself (4-bit role ids), so it needs no Python: every other frame is copied through
// byte for byte, and running it again gives the same file (it always starts from the tabby frames).
//
// Build + run (from the repo root; any C++17 compiler):
//   g++ -std=c++17 -O1 -I src/game -o fluffsit art/fluffsit.cpp src/game/catsprites_data.cpp
//   ./fluffsit bake src/game/catsprites_data.cpp      (or: ./fluffsit dump  -> prints the frames as text)
// NOTE: art/bake.py regenerates the whole file from the Python painters, including the old ragged sit frames:
// run this tool again after any re-bake.
#include "catsprites.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
using namespace cat;

enum { T = 0, OUT = 1, FUR = 2, FURD = 3, FURL = 4, MARK = 5, MARKD = 6, WHT = 7, WHTD = 8, EAR = 9, EYE = 10, PUP = 11, SHN = 12, NOSE = 13, MOUTH = 14, BLUSH = 15 };
static const int SET_TABBY = 0, SET_FLUFFY = 6, N_SIT = 3;

struct Img {
  int w = 0, h = 0, ax = 0, ay = 0, ex2 = 0, ec2 = 0, ew = 0;
  std::vector<uint8_t> p;
  uint8_t at(int x, int y) const { return (x < 0 || y < 0 || x >= w || y >= h) ? (uint8_t)T : p[y * w + x]; }
  void set(int x, int y, uint8_t v) { if (x >= 0 && y >= 0 && x < w && y < h) p[y * w + x] = v; }
};

static size_t frameBytes(const SpriteFrame& f) {   // length of a frame's RLE data
  size_t n = 0; int left = f.w * f.h; const uint8_t* d = CAT_DATA + f.off;
  while (left > 0) { left -= d[n] & 15; n++; }
  return n;
}
static Img decode(const SpriteFrame& f) {
  Img m; m.w = f.w; m.h = f.h; m.ax = f.ax; m.ay = f.ay; m.ex2 = f.ex2; m.ec2 = f.ec2; m.ew = f.ew;
  m.p.resize((size_t)f.w * f.h);
  const uint8_t* d = CAT_DATA + f.off; size_t i = 0;
  while (i < m.p.size()) { const uint8_t b = *d++; for (int r = b & 15; r > 0 && i < m.p.size(); r--) m.p[i++] = b >> 4; }
  return m;
}
static std::vector<uint8_t> encode(const Img& m) {   // same RLE as art/bake.py
  std::vector<uint8_t> o; size_t i = 0, n = m.p.size();
  while (i < n) { const uint8_t v = m.p[i]; int r = 1; while (i + r < n && m.p[i + r] == v && r < 15) r++; o.push_back((uint8_t)((v << 4) | r)); i += r; }
  return o;
}

static bool solid(const Img& m, int x, int y) { return m.at(x, y) != T; }
// A whisker pixel: white, one pixel thin, with nothing of the cat above or below it.
static bool whisker(const Img& m, int x, int y) {
  if (m.at(x, y) != WHT) return false;
  const uint8_t a = m.at(x, y - 1), b = m.at(x, y + 1);
  return (a == T || a == OUT) && (b == T || b == OUT);
}
static bool furry(uint8_t v) { return v == FUR || v == FURD || v == FURL || v == MARK || v == MARKD || v == WHT || v == WHTD; }

static Img fluff(const Img& src, bool scratch = false) {
  const int PAD = 4;                      // room round the frame for the new fur
  Img m; m.w = src.w + 2 * PAD; m.h = src.h + PAD; m.ax = src.ax + PAD; m.ay = src.ay + PAD;
  m.ex2 = src.ex2 ? src.ex2 + 2 * PAD : 0; m.ec2 = src.ec2 ? src.ec2 + 2 * PAD : 0; m.ew = src.ew;
  m.p.assign((size_t)m.w * m.h, T);
  for (int y = 0; y < src.h; y++) for (int x = 0; x < src.w; x++) m.set(x + PAD, y + PAD, src.p[y * src.w + x]);

  // landmarks
  int top = m.h, bot = -1, eyeT = m.h, eyeB = -1;
  for (int y = 0; y < m.h; y++) for (int x = 0; x < m.w; x++) {
    if (solid(m, x, y)) { if (y < top) top = y; if (y > bot) bot = y; }
    const uint8_t v = m.at(x, y);
    if (v == EYE || v == PUP) { if (y < eyeT) eyeT = y; if (y > eyeB) eyeB = y; }
  }
  if (bot < 0 || eyeB < 0) return m;
  const int mid2 = m.ex2 ? m.ex2 : m.w;   // twice the face's mirror line
  auto edge = [&](int y, int side) {      // outermost solid column on the left (side -1) / right (+1) of row y, or -1
    if (side < 0) { for (int x = 0; x < m.w; x++) if (solid(m, x, y)) return x; }
    else { for (int x = m.w - 1; x >= 0; x--) if (solid(m, x, y)) return x; }
    return -1;
  };
  // the neck: the narrowest row a little below the eyes (the head is wider above it, the body below)
  int neckY = eyeB + 2, best = 1 << 30;
  for (int y = eyeB + 1; y <= eyeB + (eyeB - eyeT) + 6 && y < bot; y++) {
    const int l = edge(y, -1), r = edge(y, 1); if (l < 0) continue;
    // measured on the left only: the tail can sit against the right side
    const int half = mid2 / 2 - l;
    if (half < best) { best = half; neckY = y; }
    (void)r;
  }

  std::vector<uint8_t> added((size_t)m.w * m.h, 0);
  auto put = [&](int x, int y, uint8_t v) {   // new fur goes on empty space or over the old outline, never over the cat
    const uint8_t c = m.at(x, y);
    if (x < 0 || y < 0 || x >= m.w || y >= m.h) return;
    if (c != T && c != OUT) return;
    m.set(x, y, v); added[(size_t)y * m.w + x] = 1;
  };
  auto coatAt = [&](int x, int y, int side) {   // the fur colour just inside the outline at this spot
    for (int k = 0; k < 4; k++) { const uint8_t v = m.at(x - side * k, y); if (furry(v)) return v; }
    return (uint8_t)FUR;
  };
  // A tooth of fur on the silhouette: `rows` tall, sticking out a little further on each row down (the fur lies
  // downward), so its lower edge is the point. reach[] = how far it sticks out on each row.
  auto tooth = [&](int y0, int side, const int* reach, int rows) {
    for (int r = 0; r < rows; r++) {
      const int y = y0 + r, e = edge(y, side);
      if (e < 0) continue;
      const uint8_t col = coatAt(e, y, side);
      for (int k = 0; k < reach[r]; k++) put(e + side * k, y, col);   // k = 0 is the outline pixel itself
    }
  };
  // Teeth are placed from the unchanged outline, so both sides (and all three sit frames) get the same pattern.
  const Img base = m;
  auto baseEdge = [&](int y, int side) {
    if (side < 0) { for (int x = 0; x < base.w; x++) if (solid(base, x, y)) return x; }
    else { for (int x = base.w - 1; x >= 0; x--) if (solid(base, x, y)) return x; }
    return -1;
  };
  auto toothB = [&](int y0, int side, const int* reach, int rows) {
    for (int r = 0; r < rows; r++) {
      const int y = y0 + r, e = baseEdge(y, side);
      if (e < 0) continue;
      uint8_t col = FUR;
      for (int k = 0; k < 4; k++) { const uint8_t v = base.at(e - side * k, y); if (furry(v)) { col = v; break; } }
      for (int k = 0; k < reach[r]; k++) put(e + side * k, y, col);
    }
  };
  (void)tooth; (void)coatAt;

  // Rows the whiskers cross (and the rows touching them) are left alone, so the whiskers stay clean lines.
  auto whiskerRow = [&](int y, int side) {
    for (int yy = y - 1; yy <= y + 1; yy++) for (int x = 0; x < base.w; x++)
      if (((2 * x + 1 < mid2) == (side < 0)) && whisker(base, x, yy)) return true;
    return false;
  };
  auto clear3 = [&](int y, int side) { return !whiskerRow(y, side) && !whiskerRow(y + 1, side) && !whiskerRow(y + 2, side); };
  (void)neckY;
  for (int side = -1; side <= 1; side += 2) {
    // 1. cheek ruff: a run of teeth down the side of the head, from eye level to the whiskers
    static const int cheek[3] = {1, 2, 3};
    int y = eyeT;
    for (; y + 2 <= eyeB + 1; y += 3) if (clear3(y, side)) toothB(y, side, cheek, 3);
    // 2. flanks (and the outside of the tail): smaller teeth, spaced out, stopping above the paws
    static const int flank[3] = {1, 1, 2};
    if (scratch && side > 0) continue;   // (scratching: the paws and the chest are on the right; fur only on the back and tail)
    for (y = eyeB + 2; y + 2 <= bot - 8;) { if (clear3(y, side)) { toothB(y, side, flank, 3); y += 5; } else y++; }
  }
  // 3. lynx tufts: two dark pixels up from each ear tip, leaning outward
  for (int side = -1; side <= 1; side += 2) {
    int tx = -1, ty = m.h;
    for (int y = 0; y < eyeT; y++) for (int x = 0; x < m.w; x++) {
      if (!solid(base, x, y)) continue;
      if ((side < 0) != (2 * x + 1 < mid2)) continue;
      if (y < ty) { ty = y; tx = x; }
    }
    if (tx < 0) continue;
    put(tx, ty, MARK); put(tx, ty - 1, MARK); put(tx + side, ty - 2, MARK);
  }
  // 4. fur ticks on the chest bib (inside the cat: drawn straight onto the white)
  if (!scratch) {
    const int cx = mid2 / 2;
    for (int i = 0; i < 3; i++) {
      const int y = neckY + 4 + i * 4, x = cx + (i == 1 ? 3 : -3) + (i == 2 ? 2 : 0);
      if (base.at(x, y) == WHT && base.at(x + 1, y + 1) == WHT && base.at(x - 1, y) == WHT && base.at(x + 2, y) == WHT) { m.set(x, y, WHTD); m.set(x + 1, y + 1, WHTD); }
    }
  }
  // a dark outline round the new fur (only where fur was added: the whiskers stay bare lines)
  for (int y = 0; y < m.h; y++) for (int x = 0; x < m.w; x++) {
    if (m.at(x, y) != T) continue;
    bool near = false;
    static const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (auto& d : d4) { const int nx = x + d[0], ny = y + d[1]; if (nx >= 0 && ny >= 0 && nx < m.w && ny < m.h && added[(size_t)ny * m.w + nx]) near = true; }
    if (near) m.set(x, y, OUT);
  }
  // (the outline pixels just set are not "added", so the loop above cannot chain)
  return m;
}

// Scratching frames: in two of the five the raised front paw sat right beside the face, at eye and nose height, so the
// face looked as if it were on the arm. Move that paw down so its top is just below the nose (where the other frames
// hold it), join it to the body with a short arm, and close the cheek's outline where it used to touch. A frame whose
// paw is already below the nose is returned as it is, so running this again changes nothing.
static Img lowerPaw(const Img& src, bool* changed = nullptr) {
  Img m = src;
  if (changed) *changed = false;
  int faceTop = m.h, noseBot = -1;
  for (int y = 0; y < m.h; y++) for (int x = 0; x < m.w; x++) {
    const uint8_t v = m.at(x, y);
    if ((v == EYE || v == PUP) && y < faceTop) faceTop = y;
    if (v == NOSE && y > noseBot) noseBot = y;
  }
  if (noseBot < 0 || faceTop >= m.h) return m;
  std::vector<int> R(m.h, -1);   // rightmost solid pixel in each row
  for (int y = 0; y < m.h; y++) for (int x = m.w - 1; x >= 0; x--) if (solid(m, x, y)) { R[y] = x; break; }
  // the paw: a bump of 3+ px out to the right that starts at or above the nose
  int pt = -1;
  for (int y = faceTop - 6 > 1 ? faceTop - 6 : 1; y <= noseBot; y++) if (R[y - 1] >= 0 && R[y] >= R[y - 1] + 3) { pt = y; break; }
  if (pt < 0) return m;
  const int xc = R[pt - 1];        // pixels at x >= xc in the paw's rows are the paw (the head's own outline is left of it)
  int pb = pt;
  while (pb + 1 < m.h && R[pb + 1] > xc) pb++;
  const int D = (noseBot + 1) - pt;   // move it down so its top sits just under the nose
  if (D <= 0) return m;
  // lift the paw out
  struct Px { int x, y; uint8_t v; };
  std::vector<Px> paw;
  for (int y = pt; y <= pb; y++) for (int x = xc; x < m.w; x++) if (m.at(x, y) != T) { paw.push_back({x, y, m.at(x, y)}); m.set(x, y, T); }
  // close the cheek where the paw was: an outline just right of what's left of each row
  for (int y = pt; y <= pb; y++) {
    int r = -1; for (int x = xc - 1; x >= 0; x--) if (solid(m, x, y)) { r = x; break; }
    if (r >= 0 && m.at(r, y) != OUT && r + 1 < m.w) m.set(r + 1, y, OUT);
  }
  // grow the frame downwards if the paw would hang off the bottom
  int need = 0; for (const Px& p : paw) if (p.y + D >= m.h) need = p.y + D - m.h + 1 > need ? p.y + D - m.h + 1 : need;
  if (need > 0) { Img g2 = m; g2.h += need; g2.p.assign((size_t)g2.w * g2.h, T); for (int y = 0; y < m.h; y++) for (int x = 0; x < m.w; x++) g2.set(x, y, m.at(x, y)); m = g2; }
  // put it back lower down (only over empty space or outline: never over the cat)
  for (const Px& p : paw) { const uint8_t c = m.at(p.x, p.y + D); if (c == T || c == OUT) m.set(p.x, p.y + D, p.v); }
  // the arm: fill the gap between the body and the paw on each of the paw's new rows (outline on its top and bottom rows)
  const int nt = pt + D, nb = pb + D;
  for (int y = nt; y <= nb && y < m.h; y++) {
    int pl = m.w; for (const Px& p : paw) if (p.y + D == y && p.x < pl) pl = p.x;
    int rb = -1; for (int x = pl - 1; x >= 0; x--) if (m.at(x, y) != T) { rb = x; break; }
    if (pl >= m.w || rb < 0 || pl - rb <= 1) continue;
    uint8_t fur = FUR; for (const Px& p : paw) if (p.y + D == y && p.v != OUT) { fur = p.v; break; }
    for (int x = rb + 1; x < pl; x++) m.set(x, y, (y == nt || y == nb) ? OUT : fur);
  }
  if (changed) *changed = true;
  return m;
}

static void dump(const Img& m, const char* title) {
  static const char* ch = ".#fdlmMwWeEPsnvb";
  printf("%s  %dx%d  anchor %d,%d  eyes ex2 %d ec2 %d ew %d\n", title, m.w, m.h, m.ax, m.ay, m.ex2, m.ec2, m.ew);
  for (int y = 0; y < m.h; y++) { printf("%2d ", y); for (int x = 0; x < m.w; x++) putchar(ch[m.at(x, y)]); putchar('\n'); }
}

int main(int argc, char** argv) {
  if (argc < 2) { fprintf(stderr, "usage: fluffsit dump | fluffsit bake <catsprites_data.cpp>\n"); return 2; }
  const int n = CAT_FRAMES_N;
  if (!strcmp(argv[1], "dump")) {
    for (int body = 0; body < 3; body++) {
      const int k = argc > 2 ? atoi(argv[2]) : 0;
      char t[64];
      snprintf(t, sizeof t, "tabby body %d sit %d", body, k); dump(decode(spriteFrame(SET_TABBY, body, SA_SIT, k)), t);
      snprintf(t, sizeof t, "NEW skogkatt body %d sit %d", body, k); dump(fluff(decode(spriteFrame(SET_TABBY, body, SA_SIT, k))), t);
    }
    return 0;
  }
  if (strcmp(argv[1], "bake") || argc < 3) return 2;
  // every frame's bytes: copied through, except the Skogkatt's sitting frames
  std::vector<std::vector<uint8_t>> data(n);
  std::vector<SpriteFrame> tab(CAT_FRAMES, CAT_FRAMES + n);
  for (int i = 0; i < n; i++) { const size_t len = frameBytes(CAT_FRAMES[i]); data[i].assign(CAT_DATA + CAT_FRAMES[i].off, CAT_DATA + CAT_FRAMES[i].off + len); }
  auto store = [&](int set, int body, int anim, int k, const Img& m) {
    if (m.w > 255 || m.h > 255) { fprintf(stderr, "frame too big\n"); exit(1); }
    const int i = (int)(&spriteFrame(set, body, anim, k) - CAT_FRAMES);
    data[i] = encode(m);
    tab[i].w = (uint8_t)m.w; tab[i].h = (uint8_t)m.h; tab[i].ax = (uint8_t)m.ax; tab[i].ay = (uint8_t)m.ay;
    tab[i].ex2 = (uint8_t)m.ex2; tab[i].ec2 = (uint8_t)m.ec2; tab[i].ew = (uint8_t)m.ew;
  };
  for (int body = 0; body < 3; body++) for (int k = 0; k < N_SIT; k++) store(SET_FLUFFY, body, SA_SIT, k, fluff(decode(spriteFrame(SET_TABBY, body, SA_SIT, k))));
  // scratching: every set's raised paw moved below the face; the Skogkatt's frames rebuilt from the tabby's, soft fur
  int moved = 0;
  for (int set = 0; set < SPRITE_SETS; set++) for (int body = 0; body < 3; body++) for (int k = 0; k < spriteAnimFrames(SA_SCRATCH); k++) {
    if (set == SET_FLUFFY) { store(set, body, SA_SCRATCH, k, fluff(lowerPaw(decode(spriteFrame(SET_TABBY, body, SA_SCRATCH, k))), true)); continue; }
    bool ch = false; const Img m = lowerPaw(decode(spriteFrame(set, body, SA_SCRATCH, k)), &ch);
    if (ch) { store(set, body, SA_SCRATCH, k, m); moved++; }
  }
  printf("scratching frames with the paw moved below the face: %d\n", moved);
  size_t total = 0;
  for (int i = 0; i < n; i++) { tab[i].off = (uint32_t)total; total += data[i].size(); }
  // palettes and the breed -> set table pass through unchanged
  std::string o;
  char b[256];
  o += "// GENERATED by art/bake.py - do not edit.\n// (Skogkatt sitting + scratching frames rebuilt, and every scratching paw moved below the face, by art/fluffsit.cpp: run it again after any re-bake.)\n#include \"catsprites.h\"\nnamespace cat {\n";
  snprintf(b, sizeof b, "const uint16_t CAT_FRAMES_N = %d;\nconst SpriteFrame CAT_FRAMES[] = {\n", n); o += b;
  for (int i = 0; i < n; i++) { snprintf(b, sizeof b, "  {%uu,%d,%d,%d,%d,%d,%d,%d},\n", (unsigned)tab[i].off, tab[i].w, tab[i].h, tab[i].ax, tab[i].ay, tab[i].ex2, tab[i].ec2, tab[i].ew); o += b; }
  o += "};\nconst uint16_t CAT_PALS[10][16] = {\n";
  for (int p = 0; p < 10; p++) { o += "  {"; for (int c = 0; c < 16; c++) { snprintf(b, sizeof b, "%s0x%04X", c ? "," : "", CAT_PALS[p][c]); o += b; } o += "},\n"; }
  o += "};\nconst uint8_t CAT_BREED_SET[10] = {";
  for (int p = 0; p < 10; p++) { snprintf(b, sizeof b, "%s%d", p ? "," : "", CAT_BREED_SET[p]); o += b; }
  snprintf(b, sizeof b, "};\nconst uint8_t CAT_DATA[%u] = {\n", (unsigned)total); o += b;
  { int col = 0; std::string line = "  ";
    for (int i = 0; i < n; i++) for (uint8_t v : data[i]) {
      snprintf(b, sizeof b, "%s%d", col ? "," : "", v); line += b;
      if (++col == 24) { o += line + ",\n"; line = "  "; col = 0; }
    }
    if (col) o += line + ",\n";
  }
  o += "};\n}  // namespace cat\n";
  FILE* f = fopen(argv[2], "wb"); if (!f) { perror("open"); return 1; }
  fwrite(o.data(), 1, o.size(), f); fclose(f);
  printf("frames=%d data=%u bytes (9 Skogkatt sit frames rebuilt)\n", n, (unsigned)total);
  return 0;
}
