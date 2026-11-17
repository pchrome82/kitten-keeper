#include "catart.h"
#include "catsprites.h"
#include <math.h>

namespace cat {
bool gShadowKey = false;
bool gShowTouch = false;
namespace {
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
}  // namespace

static void spriteSel(const CatDraw& d, int& set, int& body, int& anim, int& idx, const uint16_t*& pal) {
  int breed = isSecretBreed(d.breed) ? d.breed : (d.breed >= NUM_BREEDS ? BR_KITTEN : (d.breed < 0 ? 0 : d.breed));   // 7 = generic kitten palette
  set = CAT_BREED_SET[breed]; pal = CAT_PALS[breed];
  body = d.fat < -0.22f ? SB_SKINNY : (d.fat > 0.26f ? SB_CHONK : SB_NORMAL);
  switch (d.pose) {
    case POSE_WALK: anim = SA_WALK; idx = (int)(d.t * 8.0f) & 3; break;
    case POSE_SLEEP: anim = SA_SLEEP; idx = (int)(d.t * 1.3f) & 1; break;
    case POSE_EAT: case POSE_CROUCH: case POSE_HACK: anim = SA_EAT; idx = (int)(d.t * 5.0f) & 1; break;
    case POSE_POUNCE: anim = SA_WALK; idx = 2; break;
    case POSE_BACK: anim = SA_BACK; idx = d.backFrame; break;   // facing away (window watching, sulking)
    case POSE_UPRIGHT: anim = SA_SCRATCH; idx = d.face == FACE_SAD ? 4 : ((int)floorf(d.pawPhase / 1.5708f) & 3); break;   // rearing up on the post, facing it
    default:
      anim = SA_SIT;
      idx = (d.blink || d.face == FACE_SLEEP || d.face == FACE_BLINK) ? 1 : ((((int)(d.t * 1.4f)) & 3) == 3 ? 2 : 0);
      if (d.look) idx = 0;   // staring: frozen, eyes wide open
  }
}

// Costumes on the pixel-art sprites, placed from the drawn sprite's bounding box.
// Where the eyes are, as fractions of the sprite's bounds (learned from frames with open eyes,
// reused while they blink). Lets glasses / hats / collars fit kittens and chonks alike.
struct EyeGeo { bool ok; float lx, rx, y, d; };
static EyeGeo gEyes[SPRITE_SETS][SPRITE_BODIES][SPRITE_ANIMS];

// phase 0: only what sits BEHIND a cat that faces the player (the saddle and its rider, on their back: the cat is then
// drawn again over it); phase 1: everything else.
static void drawSpriteCostume(Canvas& cv, const CatDraw& d, const int b[4], bool side, bool sleeping, const uint16_t* pal, int set, int body, int anim, int phase = 1) {
  if (!(d.hat || d.glasses || d.neck || d.body) || b[0] > b[2]) return;
  const Color ol = rgb(30, 20, 36);
  const int w = b[2] - b[0], h = b[3] - b[1];
  const bool L = d.faceLeft;
  // ---- find the eyes (iris / pupil colours) inside the sprite we just drew ----
  EyeGeo& eg = gEyes[set % SPRITE_SETS][body % SPRITE_BODIES][anim % SPRITE_ANIMS];
  const bool back = anim == SA_BACK;   // facing away: no eyes to find; hat, cape and saddle only
  if (!sleeping && !back && w > 4 && h > 4) {
    // the eye pixels the sprite itself just drew (not whatever is behind the cat in the same colour)
    const SpriteEyePts& E = gSpriteEyes;
    long sx = 0, sy = 0; int n = 0, minx = 9999, maxx = -1;
    const int yEnd = side ? b[3] : b[1] + h * 2 / 3;   // side poses: the head can be low (eating), so look everywhere
    for (int i = 0; i < E.n; i++) {
      const int x = E.x[i], y = E.y[i];
      if (y > yEnd) continue;
      sx += x; sy += y; n++; if (x < minx) minx = x; if (x > maxx) maxx = x;
    }
    if (n >= 2) {
      float cx = (float)sx / n, cy = (float)sy / n;
      long lx = 0, ln = 0, rx = 0, rn = 0;
      for (int i = 0; i < E.n; i++) {
        const int x = E.x[i];
        if (E.y[i] > yEnd) continue;
        if (x < cx) { lx += x; ln++; } else { rx += x; rn++; }
      }
      float l = ln ? (float)lx / ln : cx, r = rn ? (float)rx / rn : cx;
      if (side) l = r = cx;
      // store in "facing right" coordinates so a mirrored frame still matches
      float fl = (l - b[0]) / (float)w, fr = (r - b[0]) / (float)w;
      if (L) { float t = 1 - fr; fr = 1 - fl; fl = t; }
      eg.ok = true; eg.lx = fl; eg.rx = fr; eg.y = (cy - b[1]) / (float)h; eg.d = (maxx - minx + 1) / (float)w;
    }
  }
  // head box: from the eyes when we know them, else rough guesses from the bounds
  int hw, hx, top = b[1], eyeY, neckY, neckX, eL = 0, eR = 0;
  if (!side) { hw = (int)(w * 0.62f); hx = (b[0] + b[2]) / 2; eyeY = top + (int)(h * 0.33f); neckY = top + (int)(h * 0.585f); neckX = hx; }
  else { hw = (int)(w * 0.34f); hx = L ? b[0] + (int)(w * 0.2f) : b[2] - (int)(w * 0.2f); eyeY = top + (int)(h * 0.36f); neckY = top + (int)(h * 0.6f); neckX = hx + (L ? 1 : -1) * hw / 3; }
  if (eg.ok) {
    float fl = L ? 1 - eg.rx : eg.lx, fr = L ? 1 - eg.lx : eg.rx;
    eL = b[0] + (int)(fl * w + 0.5f); eR = b[0] + (int)(fr * w + 0.5f);
    eyeY = b[1] + (int)(eg.y * h + 0.5f);
    if (!side) {
      const int sep = eR - eL > 4 ? eR - eL : 4;          // distance between the eyes
      hx = (eL + eR) / 2; hw = (int)(sep * 2.3f);
      neckX = hx; neckY = eyeY + (int)(sep * 1.15f);
      top = eyeY - (int)(sep * 0.95f);                    // crown of the head (between the ears)
    } else {
      // side view: one big eye towards the front of a round head. Size everything from the eye.
      int ew = (int)(w * eg.d + 0.5f); if (ew < 3) ew = 3;
      hx = eL + (L ? 1 : -1) * (ew * 95 / 100);            // the middle of the head is just behind the eye
      hw = ew * 48 / 10;
      top = eyeY - ew * 21 / 10;                           // top of the head, between the ears
      neckY = eyeY + ew * 2; neckX = hx + (L ? 1 : -1) * (ew / 2);
    }
  }
  if (sleeping) { eyeY = top + h / 2; neckY = top + (int)(h * 0.7f); }
  if (hw < 6) hw = 6;
  const int k = hw;                    // everything scales with the head
  const int hatY = eg.ok ? top : top + k / 6;   // sits between the ears
  auto u = [&](float f) { return (int)(k * f + 0.5f); };
  // ---- body (behind/over the body) ----
  const bool riderBehind = (d.body == BD_SADDLE || d.body == BD_WINGS || d.body == BD_BFLY) && !side && anim != SA_BACK;   // facing the player: the rider (or the wings) is on their back, behind them
  auto wing = [&](int wx, int wy, int dir) {   // one big angel wing: a fan of long, rounded feathers (tall at the front, fanning out and down)
    const Color wh = rgb(252, 252, 255), sh = rgb(178, 204, 246), gl = rgb(255, 238, 170);
    static const float ANG[6] = {98, 82, 65, 48, 31, 14}, LEN[6] = {0.8f, 1.0f, 1.14f, 1.16f, 1.06f, 0.86f};   // degrees above level; length (x the head's half width)
    const float r0 = (u(0.17f) + 0.5f) * 0.92f;   // (0.92: 85% of the old area)
    float tx[6], ty[6];
    for (int i = 0; i < 6; i++) { const float a = ANG[i] * 0.0174533f, L = LEN[i] * (float)k * 1.05f * 0.92f; tx[i] = wx + dir * cosf(a) * L; ty[i] = wy - sinf(a) * L; }
    auto capsule = [&](int i, float grow, Color c) {   // a feather: a row of discs from the shoulder out to the tip, tapering a little
      const float dx = tx[i] - wx, dy = ty[i] - wy; const int n = (int)(sqrtf(dx * dx + dy * dy)) + 1;
      for (int s = 0; s <= n; s++) { const float f = (float)s / n; cv.disc((int)(wx + dx * f + 0.5f), (int)(wy + dy * f + 0.5f), (int)(r0 * (1.0f - 0.22f * f) + grow + 0.5f), c); }
    };
    for (int i = 5; i >= 0; i--) capsule(i, 1.0f, ol);   // one outline round the whole fan,
    for (int i = 5; i >= 0; i--) capsule(i, 0.0f, wh);   // then the white feathers over it
    for (int i = 0; i < 6; i++)                          // a soft shadow line down each one
      cv.line(wx + (int)((tx[i] - wx) * 0.3f), wy + (int)((ty[i] - wy) * 0.3f), (int)(tx[i] - (tx[i] - wx) * 0.14f), (int)(ty[i] - (ty[i] - wy) * 0.14f), sh);
    cv.set((int)(tx[2] - (tx[2] - wx) * 0.1f), (int)(ty[2] - (ty[2] - wy) * 0.1f), gl);   // a glint on a long feather
  };
  auto bwing = [&](int wx, int wy, int dir) {   // one butterfly wing: a big upper lobe and a smaller lower lobe, with spots
    const Color pk = rgb(250, 130, 190), pkD = rgb(214, 80, 160), vi = rgb(150, 100, 230), wh = rgb(255, 240, 250);
    auto s = [&](float f) { return (int)(k * f * 0.92f + 0.5f); };   // (0.92: 85% of the old area)
    const int ux = wx + dir * s(0.5f), uy = wy - s(0.42f), lx = wx + dir * s(0.36f), ly = wy + s(0.2f);
    const int urx = s(0.5f) + 1, ury = s(0.4f) + 1, lrx = s(0.34f) + 1, lry = s(0.28f) + 1;
    cv.ellipse(ux, uy, urx + 1, ury + 1, ol); cv.ellipse(lx, ly, lrx + 1, lry + 1, ol);
    cv.ellipse(ux, uy, urx, ury, pk); cv.ellipse(lx, ly, lrx, lry, vi);
    cv.ellipse(ux + dir * urx / 4, uy - ury / 5, urx * 2 / 3, ury * 2 / 3, pkD);
    cv.ellipse(ux + dir * urx / 4, uy - ury / 5, urx / 2, ury / 2, pk);
    cv.disc(ux + dir * urx / 3, uy - ury / 4, u(0.07f) + 1, wh); cv.disc(lx + dir * lrx / 3, ly + lry / 5, u(0.05f) + 1, wh);   // the spots
  };
  if (phase == 0 && !riderBehind) return;
  if (phase == 1 && riderBehind) {}   // (already drawn, behind them)
  else if (d.body == BD_CAPE) {
    Color cape = rgb(200, 40, 60), capeD = rgb(140, 24, 44), gold = rgb(250, 210, 70);
    if (back) {   // from behind: the whole cape hangs down them back
      int sy = neckY, by = b[3] - 3, sw = u(0.42f), bw = u(0.8f);
      cv.tri(hx - sw, sy, hx + sw, sy, hx + bw, by, cape);
      cv.tri(hx - sw, sy, hx + bw, by, hx - bw, by, cape);
      cv.tri(hx - bw, by, hx - sw, sy, hx - sw + u(0.15f), by, capeD);
      cv.line(hx - sw, sy, hx - bw, by, ol); cv.line(hx + sw, sy, hx + bw, by, ol); cv.hline(hx - bw, by, bw * 2, ol);
      cv.hline(hx - sw, sy, sw * 2, gold);
    } else if (!side) {
      int sy = neckY, by = b[3] - 2, sw = u(0.55f), bw = u(0.85f);
      cv.tri(hx - sw, sy, hx - bw, by, hx - sw + u(0.2f), by, capeD);
      cv.tri(hx + sw, sy, hx + bw, by, hx + sw - u(0.2f), by, capeD);
      cv.line(hx - sw, sy, hx - bw, by, ol); cv.line(hx + sw, sy, hx + bw, by, ol);
      cv.hline(hx - sw, sy, sw * 2, cape); cv.hline(hx - sw, sy + 1, sw * 2, cape);
      cv.disc(hx, sy + 1, u(0.08f) + 1, gold);
    } else {
      int sx0 = neckX, sy = neckY - u(0.1f), tailX = L ? b[2] - u(0.15f) : b[0] + u(0.15f), by = b[1] + (int)(h * 0.75f);
      int wave = (int)(sinf(d.t * 6) * u(0.08f));
      cv.tri(sx0, sy, tailX, sy + u(0.1f) + wave, tailX, by + wave, cape);
      cv.tri(sx0, sy, tailX, by + wave, sx0 + (L ? 1 : -1) * u(0.1f), by - u(0.15f), capeD);
      cv.line(sx0, sy, tailX, sy + u(0.1f) + wave, ol); cv.line(tailX, sy + u(0.1f) + wave, tailX, by + wave, ol);
      cv.disc(sx0, sy, u(0.07f) + 1, gold);
    }
  } else if (d.body == BD_WINGS || d.body == BD_BFLY) {   // big angel wings (or butterfly wings), with a gentle flap
    const int flap = (int)(sinf(d.t * 5) * u(0.05f));
    auto wg = [&](int x, int y, int dr) { if (d.body == BD_WINGS) wing(x, y, dr); else bwing(x, y, dr); };
    if (back) { const int by = b[1] + (int)(h * 0.62f); wg((b[0] + b[2]) / 2 - u(0.22f), by + flap, -1); wg((b[0] + b[2]) / 2 + u(0.22f), by + flap, 1); }   // on the middle of the back, below the head
    else if (!side) { wg(hx - u(0.42f), neckY - u(0.1f) + flap, -1); wg(hx + u(0.42f), neckY - u(0.1f) + flap, 1); }   // spread out behind the shoulders
    else wg((b[0] + b[2]) / 2 + (L ? 1 : -1) * (int)(w * 0.06f), b[3] - (int)(h * 0.5f) + flap, L ? 1 : -1);   // side on: one wing rooted on the back, behind the shoulders
  } else if (d.body == BD_SHIRT) {   // a sky-blue tee with a white collar and a little yellow star
    const Color tee = rgb(80, 150, 230), teeD = rgb(46, 100, 180), wh = rgb(250, 248, 255), star = rgb(255, 220, 80);
    int sx, sy, rx, ry;
    if (side) { rx = (int)(w * 0.24f) + 1; ry = rx * 9 / 20 + 1; sx = (b[0] + b[2]) / 2 + (L ? -1 : 1) * (int)(w * 0.06f); sy = b[3] - ry - (int)(h * 0.22f); }
    else if (back) { sx = (b[0] + b[2]) / 2; sy = b[3] - (int)(h * 0.3f); rx = u(0.42f); ry = u(0.26f) + 1; }
    else { sx = hx; sy = neckY + u(0.16f); rx = u(0.4f); ry = u(0.2f) + 1; }
    const int sl = u(0.12f) + 1;   // sleeves: both from the front and back, the front leg's one from the side
    if (!side) { cv.disc(sx - rx, sy - ry / 2, sl + 1, ol); cv.disc(sx + rx, sy - ry / 2, sl + 1, ol); cv.disc(sx - rx, sy - ry / 2, sl, teeD); cv.disc(sx + rx, sy - ry / 2, sl, teeD); }
    else { const int fx = sx + (L ? -1 : 1) * (rx - 1); cv.disc(fx, sy, sl + 1, ol); cv.disc(fx, sy, sl, teeD); }
    cv.ellipse(sx, sy, rx + 1, ry + 1, ol); cv.ellipse(sx, sy, rx, ry, tee);
    cv.hline(sx - rx * 2 / 3, sy + ry - 1, rx * 4 / 3, teeD);   // the hem
    if (!side && !back) {
      cv.ellipse(sx, sy - ry, u(0.14f) + 1, u(0.06f) + 1, wh);   // a white crew collar
      cv.set(sx, sy + 1, star); cv.set(sx - 1, sy + 1, star); cv.set(sx + 1, sy + 1, star); cv.set(sx, sy, star); cv.set(sx, sy + 2, star);   // the star print
    }

  } else if (d.body == BD_SADDLE) {
    // saddle on them back + a very chubby-cheeked hamster rider
    int sx, sy;
    if (side) { sx = (b[0] + b[2]) / 2 + (L ? 1 : -1) * u(0.15f); sy = b[1] + (int)(h * 0.38f); }
    else if (back) { sx = (b[0] + b[2]) / 2; sy = b[1] + (int)(h * 0.74f); }   // low on them back, below their head
    else { sx = hx; sy = top + (u(0.21f) + 2) / 2; }   // on their back, behind the head: the hamster peeks over between the ears
    Color leather = rgb(150, 92, 50), dark = rgb(96, 56, 30), gold = rgb(240, 200, 80);
    cv.ellipse(sx, sy + u(0.05f), u(0.34f), u(0.13f) + 1, ol);
    cv.ellipse(sx, sy + u(0.05f), u(0.32f), u(0.11f) + 1, leather);
    cv.hline(sx - u(0.3f), sy + u(0.12f), u(0.6f), dark);
    cv.vline(sx + u(0.2f), sy + u(0.12f), u(0.25f), dark); cv.set(sx + u(0.2f), sy + u(0.38f), gold);   // stirrup strap
    // hamster
    int hr = u(0.21f) + 2, hy = sy - hr;   // a well-fed hamster
    Color fur = rgb(236, 170, 90), cream = rgb(252, 236, 210), pink = rgb(250, 160, 170);
    cv.ellipse(sx, hy, hr + 1, hr, ol); cv.ellipse(sx, hy, hr, hr - 1, fur);                    // body
    cv.ellipse(sx, hy + hr / 3, hr * 2 / 3, hr / 2, cream);                                      // tummy
    int hh = hy - hr + 1;                                                                        // head
    cv.disc(sx, hh, hr * 3 / 4 + 1, ol); cv.disc(sx, hh, hr * 3 / 4, fur);
    int ck = hr / 2 + 1;                                                                         // EXTRA chubby cheeks
    cv.disc(sx - hr * 3 / 4, hh + ck / 2, ck + 1, ol); cv.disc(sx + hr * 3 / 4, hh + ck / 2, ck + 1, ol);
    cv.disc(sx - hr * 3 / 4, hh + ck / 2, ck, cream); cv.disc(sx + hr * 3 / 4, hh + ck / 2, ck, cream);
    cv.set(sx - hr * 3 / 4, hh + ck / 2, pink); cv.set(sx + hr * 3 / 4, hh + ck / 2, pink);
    cv.disc(sx - hr / 2, hh - hr * 3 / 4, 1, pink); cv.disc(sx + hr / 2, hh - hr * 3 / 4, 1, pink);   // ears
    cv.set(sx - hr / 4 - 1, hh - 1, ol); cv.set(sx + hr / 4 + 1, hh - 1, ol);                   // eyes
    cv.set(sx, hh + 1, pink);                                                                     // nose
    cv.set(sx - 1, hy + 1, cream); cv.set(sx + 1, hy + 1, cream);                                 // little paws
  }
  if (phase == 0) return;
  // ---- neck ---- (all tied at the front: hidden when they face away)
  if (back) {}
  else if (d.neck == NK_BOW) {
    Color c = rgb(220, 40, 70), cd = rgb(150, 20, 50);
    int s = u(0.16f) + 1;
    cv.tri(neckX, neckY, neckX - s * 2, neckY - s, neckX - s * 2, neckY + s, c);
    cv.tri(neckX, neckY, neckX + s * 2, neckY - s, neckX + s * 2, neckY + s, c);
    cv.line(neckX - s * 2, neckY - s, neckX - s * 2, neckY + s, cd); cv.line(neckX + s * 2, neckY - s, neckX + s * 2, neckY + s, cd);
    cv.disc(neckX, neckY, s / 2 + 1, cd);
  } else if (d.neck == NK_SCARF) {
    Color c = rgb(60, 150, 220), st = rgb(250, 250, 250);
    int half = side ? u(0.28f) : u(0.45f), th = u(0.12f) + 2;
    cv.rect(neckX - half, neckY - th / 2, half * 2, th, c);
    for (int x = neckX - half + 2; x < neckX + half; x += 5) cv.vline(x, neckY - th / 2, th, st);
    int tx = side ? neckX + (L ? 1 : -1) * u(0.1f) : neckX + half / 2;
    cv.rect(tx - th / 2, neckY, th, u(0.38f), c); cv.hline(tx - th / 2, neckY + u(0.2f), th, st);
    cv.frame(neckX - half, neckY - th / 2, half * 2, th, rgb(30, 80, 140));
  } else if (d.neck == NK_BELL) {
    int half = side ? u(0.26f) : u(0.42f), th = u(0.06f) + 2;
    cv.rect(neckX - half, neckY - th / 2, half * 2, th, rgb(70, 140, 255));
    int br = u(0.09f) + 2;
    cv.disc(neckX, neckY + th / 2 + br - 1, br, rgb(160, 120, 20)); cv.disc(neckX, neckY + th / 2 + br - 1, br - 1, rgb(255, 214, 64));
    cv.set(neckX - 1, neckY + th / 2 + br - 2, rgb(255, 250, 200));
  } else if (d.neck == NK_BANDANA) {   // a red bandana, knotted behind, its point hanging down the chest
    const Color c = rgb(214, 50, 56), cd = rgb(150, 26, 40), dot = rgb(255, 244, 236);
    const int half = side ? u(0.26f) : u(0.42f), th = u(0.08f) + 2, drop = u(0.36f) + 2;
    cv.tri(neckX - half, neckY, neckX + half, neckY, neckX, neckY + drop, c);
    cv.line(neckX - half, neckY, neckX, neckY + drop, cd); cv.line(neckX + half, neckY, neckX, neckY + drop, cd);
    cv.rect(neckX - half, neckY - th / 2, half * 2, th, c); cv.hline(neckX - half, neckY - th / 2, half * 2, cd);
    cv.set(neckX, neckY + drop / 3, dot); cv.set(neckX - half / 3, neckY + drop / 6 + 1, dot); cv.set(neckX + half / 3, neckY + drop / 6 + 1, dot);
    if (drop > 8) cv.set(neckX, neckY + drop * 2 / 3, dot);
  } else if (d.neck == NK_LACE) {   // Grandma's frilly lace collar: a ruff of little scallops with a cameo in the middle
    const int half = side ? u(0.26f) : u(0.44f), r = u(0.07f) + 1, n = side ? 3 : 5;
    const Color lace = rgb(252, 248, 240), laceD = rgb(200, 190, 200);
    for (int i = 0; i <= n; i++) {
      const int px = neckX - half + i * 2 * half / n, py = neckY + (i & 1);
      cv.disc(px, py, r + 1, laceD); cv.disc(px, py, r, lace);
    }
    cv.hline(neckX - half, neckY - r, half * 2 + 1, laceD);
    if (!side) { const int cr = u(0.06f) + 1; cv.disc(neckX, neckY + r, cr + 1, rgb(170, 120, 40)); cv.disc(neckX, neckY + r, cr, rgb(240, 170, 160)); }
  } else if (d.neck == NK_PEARLS) {   // a string of pearls, dipping in the middle
    const int half = side ? u(0.2f) : u(0.42f), r = u(0.045f) + 1, n = side ? 2 : 3;
    const int lift = side ? u(0.16f) : 0, dip = side ? u(0.06f) + 1 : u(0.12f) + 1;   // side on: higher up the neck, and a shallower dip
    for (int i = -n; i <= n; i++) {
      const int px = neckX + i * half / n, py = neckY - lift + (n * n - i * i) * dip / (n * n);
      cv.disc(px, py, r, rgb(150, 150, 176)); cv.disc(px, py, r - 1 > 0 ? r - 1 : 0, rgb(250, 248, 255)); cv.set(px, py, rgb(255, 255, 255));
    }
  } else if (d.neck == NK_TIE) {   // a smart striped necktie on a white collar
    const Color c = rgb(44, 84, 190), cd = rgb(24, 46, 120), st = rgb(250, 208, 70), wh = rgb(250, 250, 250);
    const int half = side ? u(0.22f) : u(0.36f), th = u(0.06f) + 2, kw = u(0.07f) + 1, bw = u(0.12f) + 1, len = u(0.42f) + 2;
    cv.rect(neckX - half, neckY - th / 2, half * 2, th, wh); cv.frame(neckX - half, neckY - th / 2, half * 2, th, rgb(170, 170, 190));   // collar
    cv.rect(neckX - kw, neckY - th / 2, kw * 2 + 1, th + 1, cd);                                                                        // the knot
    cv.tri(neckX - kw, neckY + th / 2, neckX + kw, neckY + th / 2, neckX + bw, neckY + len - bw, c);                                    // the blade
    cv.tri(neckX - kw, neckY + th / 2, neckX + bw, neckY + len - bw, neckX - bw, neckY + len - bw, c);
    cv.tri(neckX - bw, neckY + len - bw, neckX + bw, neckY + len - bw, neckX, neckY + len, c);
    for (int s = 1; s <= 2; s++) { const int yy = neckY + th / 2 + (len - bw) * s / 3; cv.line(neckX - bw + 1, yy + 1, neckX + bw - 1, yy - 1, st); }
    cv.line(neckX - bw, neckY + len - bw, neckX, neckY + len, cd); cv.line(neckX + bw, neckY + len - bw, neckX, neckY + len, cd);
  }
  // ---- eyes ----
  if (d.glasses && !sleeping && !back) {
    int er = u(0.13f) + 2, gap = side ? 0 : u(0.22f) + 1;
    int ex = side ? hx + (L ? -1 : 1) * u(0.12f) : hx;
    if (eg.ok) {   // centre each lens on an eye
      const int sep = eR - eL;
      er = side ? (int)(w * eg.d * 0.6f) + 2 : (int)(sep * 0.42f) + 1;
      if (er < 2) er = 2;
      if (!side) { ex = (eL + eR) / 2; gap = sep / 2 - er / 2; }
      else ex = eL;
    }
    auto lens = [&](int cx, int which) {   // which: 0 the left lens, 1 the right (or the only one, side on)
      if (d.glasses == GL_STAR) {   // star-shaped shades
        const Color gold = rgb(255, 214, 64), goldD = rgb(170, 120, 20);
        const int r = er > 3 ? er - 1 : 2;   // (a touch smaller than the eye spacing, so the two stars stay apart)
        cv.tri(cx, eyeY - r - 1, cx - r - 1, eyeY + r / 2 + 1, cx + r + 1, eyeY + r / 2 + 1, goldD); cv.tri(cx, eyeY + r + 1, cx - r - 1, eyeY - r / 2 - 1, cx + r + 1, eyeY - r / 2 - 1, goldD);
        cv.tri(cx, eyeY - r, cx - r, eyeY + r / 2, cx + r, eyeY + r / 2, gold); cv.tri(cx, eyeY + r, cx - r, eyeY - r / 2, cx + r, eyeY - r / 2, gold);
        cv.rect(cx - r / 3, eyeY - r / 3, r * 2 / 3 + 1, r * 2 / 3 + 1, rgb(60, 40, 20));   // dark glass in the middle
        cv.set(cx - r / 3, eyeY - r / 3, rgb(255, 252, 210));
      } else if (d.glasses == GL_MONOCLE) {   // a gold-rimmed monocle on a chain (the eye shows through)
        const Color gold = rgb(250, 208, 70), goldD = rgb(170, 120, 20);
        cv.ring(cx, eyeY, er + 1, goldD); cv.ring(cx, eyeY, er, gold);
        cv.set(cx - er / 2, eyeY - er / 2, rgb(255, 255, 255));
        const int dir = side ? (L ? 1 : -1) : 1;
        cv.line(cx + dir * er, eyeY + er / 2, cx + dir * (er + 1), eyeY + er * 2 + 2, gold);
      } else if (d.glasses == GL_GOGGLES) {   // brass explorer's goggles: thick round rims, amber glass, a leather strap
        const Color brass = rgb(214, 160, 70), brassD = rgb(140, 96, 36), glass = rgb(250, 190, 90);
        cv.disc(cx, eyeY, er + 1, ol); cv.disc(cx, eyeY, er, brass); cv.disc(cx, eyeY, er - 2 > 0 ? er - 2 : 1, glass);
        cv.ring(cx, eyeY, er, brassD);
        cv.set(cx - er / 2, eyeY - er / 2, rgb(255, 250, 220));
        const int sx = which ? cx + er + 1 : cx - er - 1, dirS = which ? 1 : -1;   // the strap, out to the side of the head
        cv.hline(dirS > 0 ? sx : sx - u(0.12f), eyeY - 1, u(0.12f) + 1, rgb(96, 58, 34)); cv.hline(dirS > 0 ? sx : sx - u(0.12f), eyeY, u(0.12f) + 1, rgb(70, 40, 24));
      } else if (d.glasses == GL_3D) {   // paper 3-D specs: one red lens, one blue
        const Color fr = rgb(248, 248, 248), tint = which ? rgb(40, 190, 230) : rgb(232, 50, 60);
        cv.rect(cx - er - 1, eyeY - er + 1, er * 2 + 3, er * 2 - 1, fr); cv.frame(cx - er - 1, eyeY - er + 1, er * 2 + 3, er * 2 - 1, ol);
        cv.rect(cx - er + 1, eyeY - er + 3, er * 2 - 1, er * 2 - 5 > 1 ? er * 2 - 5 : 1, tint);
      } else if (d.glasses == GL_HEART) {
        Color pk = rgb(255, 80, 140);
        cv.disc(cx - er / 2, eyeY - er / 3, er / 2 + 1, pk); cv.disc(cx + er / 2, eyeY - er / 3, er / 2 + 1, pk);
        cv.tri(cx - er - 1, eyeY - er / 4, cx + er + 1, eyeY - er / 4, cx, eyeY + er, pk);
        cv.set(cx - er / 2, eyeY - er / 2, rgb(255, 200, 220));
      } else {
        cv.ellipse(cx, eyeY, er + 1, er - 1, ol); cv.ellipse(cx, eyeY, er, er - 2, rgb(30, 30, 50));
        cv.set(cx - er / 2, eyeY - 1, rgb(160, 200, 255));
      }
    };
    if (side) lens(ex, 1);
    else if (d.glasses == GL_MONOCLE) lens(ex + gap + er / 2, 1);   // one eye only
    else { lens(ex - gap - er / 2, 0); lens(ex + gap + er / 2, 1); cv.hline(ex - gap + er / 2, eyeY - 1, gap * 2 - er + 1, d.glasses == GL_3D ? rgb(248, 248, 248) : ol); }
  }
  // ---- hats ----
  if (d.hat) {
    const int cx = side ? hx + (L ? 1 : -1) * u(0.05f) : hx;
    switch (d.hat) {
      case HAT_TOP: {
        int bw = u(0.42f), cw = u(0.26f), ch = u(0.5f);
        cv.rect(cx - cw - 1, hatY - ch - 1, cw * 2 + 2, ch + 1, ol); cv.rect(cx - cw, hatY - ch, cw * 2, ch, rgb(36, 32, 44));
        cv.rect(cx - cw, hatY - u(0.16f), cw * 2, u(0.1f) + 1, rgb(200, 40, 60));
        cv.rect(cx - bw, hatY - 1, bw * 2, u(0.07f) + 2, rgb(36, 32, 44)); cv.hline(cx - bw, hatY - 2, bw * 2, ol);
        break; }
      case HAT_PARTY: {
        int bw = u(0.22f), ch = u(0.62f);
        cv.tri(cx - bw - 1, hatY + 1, cx + bw + 1, hatY + 1, cx, hatY - ch - 1, ol);
        cv.tri(cx - bw, hatY, cx + bw, hatY, cx, hatY - ch, rgb(80, 200, 240));
        for (int s = 1; s < 3; s++) { int yy = hatY - ch * s / 3; int ww = bw * (3 - s) / 3; cv.hline(cx - ww, yy, ww * 2, rgb(255, 230, 90)); }
        cv.disc(cx, hatY - ch, u(0.07f) + 1, rgb(255, 100, 160));
        break; }
      case HAT_CROWN: {
        int bw = u(0.3f), ch = u(0.24f);
        cv.rect(cx - bw, hatY - ch / 2, bw * 2, ch / 2 + 1, rgb(250, 210, 60));
        for (int s = -1; s <= 1; s++) cv.tri(cx + s * bw * 2 / 3 - u(0.08f) - 1, hatY - ch / 2, cx + s * bw * 2 / 3 + u(0.08f) + 1, hatY - ch / 2, cx + s * bw * 2 / 3, hatY - ch - 1, rgb(250, 210, 60));
        cv.hline(cx - bw, hatY + 1, bw * 2, rgb(170, 120, 20));
        cv.set(cx, hatY - ch / 4, rgb(230, 40, 60)); cv.set(cx - bw * 2 / 3, hatY - ch / 4, rgb(60, 140, 240)); cv.set(cx + bw * 2 / 3, hatY - ch / 4, rgb(60, 200, 120));
        break; }
      case HAT_WIZARD: {
        // a tall, outlined cone with a lit left edge and a shaded right side, a bent tip, a gold band, and stars
        // scattered evenly (staggered rows) over the whole cone
        const int bw = u(0.46f), cwd = u(0.28f), ch = u(0.95f), lean = u(0.1f);
        const int tipx = cx + lean, tipy = hatY - ch;
        const Color co = rgb(112, 78, 204), coD = rgb(78, 52, 162), coL = rgb(156, 126, 240), brim = rgb(92, 62, 176), star = rgb(255, 238, 120), gold = rgb(250, 206, 70);
        cv.ellipse(cx, hatY, bw + 1, u(0.09f) + 2, ol); cv.ellipse(cx, hatY, bw, u(0.09f) + 1, brim);
        cv.tri(cx - cwd - 1, hatY + 1, cx + cwd + 1, hatY + 1, tipx, tipy - 1, ol);
        cv.tri(cx - cwd, hatY, cx + cwd, hatY, tipx, tipy, co);
        cv.tri(cx + cwd / 3, hatY, cx + cwd, hatY, tipx, tipy, coD);                       // the shaded right side
        cv.line(cx - cwd * 2 / 3, hatY - 1, tipx - 1, tipy + 2, coL);                      // the lit left edge
        const int bandH = u(0.1f) + 1;
        cv.rect(cx - cwd, hatY - bandH, cwd * 2, bandH, gold); cv.hline(cx - cwd, hatY - bandH, cwd * 2, rgb(255, 232, 130));
        const bool bigStars = cwd >= 6;                                                    // (a plus-shaped star where there's room, else a pixel)
        static const float SF[6] = {0.26f, 0.30f, 0.46f, 0.60f, 0.62f, 0.78f}, SR[6] = {-0.5f, 0.5f, 0.0f, -0.35f, 0.35f, 0.0f};   // height along the cone, across it
        for (int s = 0; s < 6; s++) {
          const int sy = hatY - (int)(ch * SF[s] + 0.5f), sx = cx + (int)(lean * SF[s]) + (int)(cwd * (1.0f - SF[s]) * SR[s] + (SR[s] < 0 ? -0.5f : 0.5f));
          cv.set(sx, sy, star);
          if (bigStars && (s == 2 || s == 5 || (s & 1) == 0)) { cv.set(sx - 1, sy, star); cv.set(sx + 1, sy, star); cv.set(sx, sy - 1, star); cv.set(sx, sy + 1, star); }
        }
        break; }
      case HAT_SANTA: {
        int bw = u(0.3f), ch = u(0.5f);
        cv.tri(cx - bw, hatY, cx + bw, hatY, cx + u(0.35f), hatY - ch, rgb(220, 40, 50));
        cv.disc(cx + u(0.35f), hatY - ch, u(0.08f) + 1, rgb(250, 250, 250));
        cv.rect(cx - bw - 1, hatY - u(0.06f), bw * 2 + 2, u(0.12f) + 2, rgb(250, 250, 250));
        break; }
      case (Hat)HAT_PUMPKIN: {   // Halloween: a little jack-o'-lantern cap
        const int bw = u(0.3f), ch = u(0.3f);
        cv.ellipse(cx, hatY - ch / 2, bw + 1, ch / 2 + 2, ol); cv.ellipse(cx, hatY - ch / 2, bw, ch / 2 + 1, rgb(236, 128, 40));
        cv.vline(cx - bw / 2, hatY - ch, ch, rgb(206, 100, 30)); cv.vline(cx + bw / 2, hatY - ch, ch, rgb(206, 100, 30));
        cv.rect(cx - 1, hatY - ch - 3, 2, 3, rgb(80, 120, 50));
        cv.set(cx - bw / 3, hatY - ch / 2, rgb(255, 230, 110)); cv.set(cx + bw / 3, hatY - ch / 2, rgb(255, 230, 110));
        break; }
      case HAT_COWBOY: {   // a wide-brimmed leather hat, the brim curling up at the ends
        const Color br = rgb(156, 100, 54), brD = rgb(104, 62, 32), band = rgb(58, 34, 22);
        const int bw = u(0.52f), cw = u(0.24f), ch = u(0.34f) + 1, bh = u(0.07f) + 1;
        cv.ellipse(cx, hatY, bw + 1, bh + 1, ol); cv.ellipse(cx, hatY, bw, bh, br);
        cv.set(cx - bw, hatY - 1, br); cv.set(cx + bw, hatY - 1, br); cv.set(cx - bw - 1, hatY - 2, ol); cv.set(cx + bw + 1, hatY - 2, ol);
        cv.rect(cx - cw - 1, hatY - ch - 1, cw * 2 + 2, ch + 1, ol); cv.rect(cx - cw, hatY - ch, cw * 2, ch, br);
        cv.tri(cx - cw / 2, hatY - ch, cx + cw / 2, hatY - ch, cx, hatY - ch + u(0.1f) + 1, brD);        // the dent in the crown
        cv.rect(cx - cw, hatY - bh - u(0.07f) - 1, cw * 2, u(0.07f) + 1, band);
        cv.set(cx, hatY - bh - u(0.07f) - 1, rgb(250, 208, 70));
        break; }
      case HAT_FLOWER: {
        static const Color fc[3] = {rgb(255, 120, 170), rgb(255, 220, 80), rgb(180, 140, 255)};
        int bw = u(0.36f);
        cv.hline(cx - bw, hatY, bw * 2, rgb(70, 150, 70));
        for (int s = 0; s < 5; s++) { int fx = cx - bw + s * bw / 2; cv.disc(fx, hatY - 1, u(0.06f) + 1, fc[s % 3]); cv.set(fx, hatY - 1, rgb(255, 250, 220)); }
        break; }
    }
  }
}

// Staring at something off screen: find each eye in the drawn sprite, fill it with iris and give it a big round
// pupil pushed toward that side of the screen (and a fresh glint), keeping the eyelid line on top.
static void stareEyes(Canvas& cv, const int b[4], const uint16_t* pal, int dir) {
  const Color iris = pal[10], pupil = pal[11], shine = pal[12];
  const int w = b[2] - b[0], h = b[3] - b[1];
  if (w < 6 || h < 6) return;
  const int y1 = b[1] + h * 2 / 3;
  const SpriteEyePts& E = gSpriteEyes;   // the iris pixels the sprite itself drew
  long sx = 0; int n = 0;
  for (int i = 0; i < E.n; i++) if (E.role[i] == 10 && E.y[i] <= y1) { sx += E.x[i]; n++; }
  if (n < 4) return;
  const int mid = (int)(sx / n);
  for (int side = 0; side < 2; side++) {
    int x0 = 9999, x1 = -1, ey0 = 9999, ey1 = -1;   // this eye's iris box
    for (int i = 0; i < E.n; i++) {
      const int x = E.x[i], y = E.y[i];
      if (E.role[i] != 10 || y > y1 || (side ? x < mid : x >= mid)) continue;
      if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < ey0) ey0 = y; if (y > ey1) ey1 = y;
    }
    if (x1 < x0 || x1 - x0 < 1 || ey1 - ey0 < 1) continue;
    auto inEye = [&](int x, int y) { Color c = cv.get(x, y); return c == iris || c == pupil || c == shine; };
    for (int y = ey0; y <= ey1; y++) for (int x = x0; x <= x1; x++) if (inEye(x, y)) cv.set(x, y, iris);   // clear the old slit
    const int ew = x1 - x0 + 1, eh = ey1 - ey0 + 1;
    const float pcx = (x0 + x1) * 0.5f + dir * ew * 0.22f, pcy = (ey0 + ey1) * 0.5f + eh * 0.08f;
    const float prx = ew * 0.36f + 0.5f, pry = eh * 0.38f + 0.5f;
    for (int y = ey0; y <= ey1; y++) for (int x = x0; x <= x1; x++) {
      const float u = (x - pcx) / prx, v = (y - pcy) / pry;
      if (u * u + v * v <= 1.0f && cv.get(x, y) == iris) cv.set(x, y, pupil);
    }
    const int gx = (int)(pcx - prx * 0.45f), gy = (int)(pcy - pry * 0.5f);   // glint
    if (gx >= x0 && gx <= x1 && gy >= ey0 && gy <= ey1) cv.set(gx, gy, shine);
  }
}

static CatMetrics drawSpriteCat(Canvas& cv, int ox, int oy, const CatDraw& d) {
  int set, body, anim, idx; const uint16_t* pal;
  spriteSel(d, set, body, anim, idx, pal);
  const SpriteFrame& f = spriteFrame(set, body, anim, idx);
  int b[4];
  float sc = d.size < 0.3f ? 0.3f : d.size;
  if (d.silhouette) {   // a mystery: just the shape, no face, no costume, no glow
    static const int8_t rim[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (auto& o : rim) drawSprite(cv, f, pal, ox + o[0], oy + o[1], d.faceLeft, sc, nullptr, -1.0f, rgb(120, 96, 170));   // a faint rim, so the shape reads
    drawSprite(cv, f, pal, ox, oy, d.faceLeft, sc, b, -1.0f, d.silhouette);
    CatMetrics m; m.headX = (b[0] + b[2]) / 2; m.topY = b[1]; m.headY = b[1] + (b[3] - b[1]) * 3 / 10; m.halfWidth = (b[2] - b[0]) / 2;
    if (b[0] > b[2]) { m.headX = ox; m.headY = oy - 20; m.topY = oy - 30; m.halfWidth = 12; }
    return m;
  }
  const bool rainbow = d.breed == BR_RAINBOW;
  if (rainbow && !gShadowKey) {   // an ethereal glow: a soft lavender halo round them, two pixels deep
    static const int8_t farO[8][2] = {{-2, 0}, {2, 0}, {0, -2}, {0, 2}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    static const int8_t nearO[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    const float pulse = 0.5f + 0.5f * sinf(d.t * 2.0f);
    const Color c2 = mix(rgb(110, 84, 180), rgb(150, 120, 210), (int)(pulse * 256)), c1 = mix(rgb(196, 170, 250), rgb(225, 205, 255), (int)(pulse * 256));
    for (auto& o : farO) drawSprite(cv, f, pal, ox + o[0], oy + o[1], d.faceLeft, sc, nullptr, -1.0f, c2);
    for (auto& o : nearO) drawSprite(cv, f, pal, ox + o[0], oy + o[1], d.faceLeft, sc, nullptr, -1.0f, c1);
  }
  drawSprite(cv, f, pal, ox, oy, d.faceLeft, sc, b, rainbow ? d.t : -1.0f);
  if (rainbow && b[0] <= b[2]) {   // sparkles drifting up around them
    const int w = b[2] - b[0], h = b[3] - b[1];
    for (int k = 0; k < 4; k++) {
      const float ph = fmodf(d.t * 0.8f + k * 0.25f, 1.0f);
      if (ph > 0.85f) continue;
      const int sx = b[0] + (int)((0.12f + 0.76f * fmodf(k * 0.37f + 0.1f, 1.0f)) * w), sy = b[3] - (int)(ph * (h + 10));
      const Color c = mix(rgb(255, 255, 255), k & 1 ? rgb(255, 170, 230) : rgb(150, 230, 255), (int)(ph * 200));
      const int r = sc > 0.7f ? 2 : 1;
      cv.hline(sx - r, sy, 2 * r + 1, c); cv.vline(sx, sy - r, 2 * r + 1, c);
    }
  }
  int eyeX = -1, eyeY = -1;   // where the eyes are (before costumes go on top)
  if (gSpriteEyes.n > 0) {
    long sx = 0, sy = 0;
    for (int i = 0; i < gSpriteEyes.n; i++) { sx += gSpriteEyes.x[i]; sy += gSpriteEyes.y[i]; }
    eyeX = (int)(sx / gSpriteEyes.n); eyeY = (int)(sy / gSpriteEyes.n);
  }
  const bool sideView = anim != SA_SIT && anim != SA_SCRATCH && anim != SA_BACK;
  if ((d.body == BD_SADDLE || d.body == BD_WINGS || d.body == BD_BFLY) && !sideView && anim != SA_BACK && b[0] <= b[2]) {   // the rider (or the wings) sits on their back: draw it, then the cat again in front
    drawSpriteCostume(cv, d, b, sideView, anim == SA_SLEEP, pal, set, body, anim, 0);
    drawSprite(cv, f, pal, ox, oy, d.faceLeft, sc, nullptr, rainbow ? d.t : -1.0f);
  }
  if (d.look && anim == SA_SIT) stareEyes(cv, b, pal, d.look);
  drawSpriteCostume(cv, d, b, sideView, anim == SA_SLEEP, pal, set, body, anim);
  CatMetrics m;
  m.eyeX = eyeX; m.eyeY = eyeY;
  if (b[0] > b[2]) { m.headX = ox; m.headY = oy - 20; m.topY = oy - 30; m.halfWidth = 12; return m; }
  bool side = anim != SA_SIT && anim != SA_BACK;
  int w = b[2] - b[0], h = b[3] - b[1];
  m.halfWidth = w / 2;
  m.topY = b[1];
  m.headX = side ? (d.faceLeft ? b[0] + w / 5 : b[2] - w / 5) : (b[0] + b[2]) / 2;
  m.headY = b[1] + (side ? h * 0.4f : h * 0.3f);
  return m;
}

CatMetrics drawCat(Canvas& cv, int ox, int oy, const CatDraw& d) { return drawSpriteCat(cv, ox, oy, d); }

int catReach(const CatDraw& d) {
  int set, body, anim, idx; const uint16_t* pal;
  spriteSel(d, set, body, anim, idx, pal);
  const SpriteFrame& f = spriteFrame(set, body, anim, idx);
  const float sc = d.size < 0.3f ? 0.3f : d.size;
  const int r = f.ax > f.w - 1 - f.ax ? f.ax : f.w - 1 - f.ax;
  return (int)(r * sc + 0.5f);
}

int catHeight(const CatDraw& d) {
  int set, body, anim, idx; const uint16_t* pal;
  spriteSel(d, set, body, anim, idx, pal);
  const SpriteFrame& f = spriteFrame(set, body, anim, idx);
  const float sc = d.size < 0.3f ? 0.3f : d.size;
  return (int)(f.ay * sc + 0.5f);
}

void fitCatHeight(CatDraw& d, int y, int topMin) {
  if (y - topMin < 8) return;
  CatDraw s = d; s.pose = POSE_SIT; s.look = 0; s.blink = false;
  const int h = catHeight(s);
  if (h > y - topMin) d.size *= (float)(y - topMin) / (float)h;
}

void drawCatShadow(Canvas& cv, int ox, int oy, const CatDraw& d, Color floorColor) {
  int set, body, anim, idx; const uint16_t* pal;
  spriteSel(d, set, body, anim, idx, pal);
  static const float wsit[3] = {15, 18, 30}, wside[3] = {24, 27, 43};
  float rx = ((anim == SA_SIT || anim == SA_SCRATCH || anim == SA_BACK) ? wsit[body] : wside[body]) * (d.size < 0.3f ? 0.3f : d.size);
  cv.ellipse(ox, oy + 1, (int)rx, (int)fmaxf(2.0f, rx * 0.2f), gShadowKey ? SHADOW_KEY : shade(floorColor, 72));
}

}  // namespace cat

namespace cat {
int spriteRightExtent(const CatDraw& d) {   // pixels from the sprite's ground anchor to its right edge (scaled)
  int set, body, anim, idx; const uint16_t* pal;
  spriteSel(d, set, body, anim, idx, pal);
  const SpriteFrame& f = spriteFrame(set, body, anim, idx);
  float sc = d.size < 0.3f ? 0.3f : d.size;
  return (int)((f.w - f.ax) * sc + 0.5f);
}
}  // namespace cat
