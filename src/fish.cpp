// ============================================================================
//  FISH - eat the smaller fish, flee from the bigger ones ! Tilt the M5 in every direction to swim.
//  TOP button (PWR) = pause
// ============================================================================
#include "common.h"

namespace fish {

static const uint32_t STEP_US = 20000;
static const int TOP = 16, FLOOR = 118;
enum State { TITLE, PLAY, PAUSED, OVER };
static State state = TITLE;
static Preferences pr;
static uint32_t last = 0, acc = 0;

// --------------------------------------------------------------------------- especes
struct Sp { uint16_t back, mid, belly, fin; uint8_t ratio; uint8_t pat; uint16_t acc; };
//  pat : 0 uni, 1 rayures verticales, 2 taches, 3 ligne laterale, 4 "chirurgien" bleu, 5 poisson-clown
static const Sp SP[9] = {
  /* 0 vairon vert  */ {rgb(36, 118, 84),  rgb(84, 204, 132),  rgb(204, 246, 204), rgb(40, 150, 100),  44, 3, rgb(30, 90, 60)},
  /* 1 chirurgien   */ {rgb(22, 56, 168),  rgb(54, 124, 242),  rgb(130, 196, 252), rgb(252, 222, 44),  62, 4, rgb(8, 16, 80)},
  /* 2 papillon     */ {rgb(212, 150, 20), rgb(252, 216, 60),  rgb(255, 246, 172), rgb(250, 200, 50),  66, 1, rgb(36, 30, 40)},
  /* 3 ange violet  */ {rgb(88, 48, 160),  rgb(162, 112, 232), rgb(224, 194, 252), rgb(132, 82, 204),  60, 2, rgb(244, 232, 255)},
  /* 4 turquoise    */ {rgb(18, 108, 130), rgb(62, 192, 202),  rgb(192, 242, 242), rgb(30, 142, 162),  42, 3, rgb(250, 252, 130)},
  /* 5 rose         */ {rgb(190, 66, 120), rgb(246, 132, 172), rgb(255, 212, 228), rgb(222, 90, 142),  52, 2, rgb(255, 242, 242)},
  /* 6 barracuda    */ {rgb(56, 72, 100),  rgb(122, 142, 172), rgb(218, 228, 238), rgb(70, 86, 112),   34, 3, rgb(38, 48, 70)},
  /* 7 merou        */ {rgb(108, 38, 30),  rgb(192, 92, 60),   rgb(238, 172, 130), rgb(150, 60, 40),   60, 2, rgb(255, 222, 182)},
  /* 8 clown (joueur)*/{rgb(206, 80, 10),  rgb(252, 138, 28),  rgb(255, 196, 104), rgb(238, 112, 20),  56, 5, rgb(250, 250, 250)},
};

struct Fish { float x, by, vx, s, ph, amp; uint8_t kind; bool on; };
static Fish fs[12];
struct Bub { float x, y, r, v; }; static Bub bubs[16];
struct Pop { float x, y; int t; char s[10]; uint16_t col; }; static Pop pops[3];
struct Part { float x, y, vx, vy; int t; uint16_t col; uint8_t kind; }; static Part parts[48];
struct BgFish { float x, y, v, s; }; static BgFish bg[6];

static float fx = 120, fy = 70, fvx = 0, fvy = 0, ps = 8, psT = 8, face = 1;
static int dirP = 1, lives = 3, level = 1, invul = 0, endT = 0, animT = 0, spawnT = 0, graceT = 0;
static int mouthT = 0, combo = 0, comboT = 0, shakeT = 0, flashT = 0, ventT = 0;
static uint32_t score = 0, best = 0;

static uint16_t waterCol(int y) { return mixc(rgb(70, 188, 228), rgb(5, 28, 82), y * 256 / SCR_H); }
static void pop(float x, float y, const char* s, uint16_t col) { for (auto &p : pops) if (p.t <= 0) { p.x = x; p.y = y; p.t = 40; p.col = col; strncpy(p.s, s, 9); p.s[9] = 0; return; } }
static void emit(float x, float y, int n, uint8_t kind, uint16_t col) {
  for (int i = 0; i < n; i++) for (auto &p : parts) if (p.t <= 0) {
    float a = rnd(0, 628) / 100.f, v = 0.4f + rnd(0, 14) * 0.1f;
    if (kind == 1) p = {x + (int)rnd(0, 8) - 4, y + (int)rnd(0, 6) - 3, (float)((int)rnd(0, 6) - 3) * 0.06f, -(0.5f + rnd(0, 10) * 0.08f), (int)rnd(30, 60), col, 1};
    else p = {x, y, cosf(a) * v, sinf(a) * v - (kind == 0 ? 0.2f : 0.f), (int)rnd(14, 26), col, kind};
    break;
  }
}
static void bubble(float x, float y, int n) { emit(x, y, n, 1, rgb(196, 238, 255)); }

static void reset() {
  fx = 120; fy = 70; fvx = fvy = 0; ps = psT = 8; face = 1; dirP = 1; lives = difi(5, 3, 2); level = 1; invul = 0; endT = 0; spawnT = 20; score = 0;
  mouthT = combo = comboT = shakeT = flashT = 0; graceT = 90;
  for (auto &f : fs) f.on = false;
  for (auto &p : pops) p.t = 0;
  for (auto &p : parts) p.t = 0;
  for (auto &b : bubs) b = {(float)rnd(0, SCR_W), (float)rnd(0, SCR_H), (float)rnd(1, 3), 0.3f + rnd(0, 10) * 0.08f};
  for (auto &b : bg) b = {(float)rnd(0, SCR_W), (float)rnd(TOP + 8, FLOOR - 24), (rnd(0, 1) ? 1 : -1) * (0.15f + rnd(0, 10) * 0.02f), (float)rnd(3, 5)};
}
void begin() {
  pr.begin(dns("fish"), false); best = pr.getInt("best", 0);
  tilt::load();
  reset(); state = TITLE; last = 0; acc = 0;
}
static void gameOver() {
  state = OVER; endT = 0;
  trophy::record(trophy::T_FISH, score); if (score > best) { best = score; pr.putInt("best", best); }
  sfx(300, 120); sfx(120, 320, false);
}

// --------------------------------------------------------------------------- apparition (jamais injuste)
static int countFish(bool danger) { int n = 0; for (auto &f : fs) if (f.on && ((f.s > ps * 0.98f) == danger)) n++; return n; }
static int freeSlots() { int n = 0; for (auto &f : fs) if (!f.on) n++; return n; }
static void put(float x, float y, float vx, float s, uint8_t kind, float ph, float amp) {
  for (auto &f : fs) if (!f.on) { f = {x, y, vx, s, ph, amp, kind, true}; return; }
}
static void spawn() {
  int edible = countFish(false), danger = countFish(true);
  int maxDanger = 2 + level / 2 + difi(-1, 0, 1); if (maxDanger < 1) maxDanger = 1; if (maxDanger > difi(3, 4, 5)) maxDanger = difi(3, 4, 5);
  int pPred = 26 + level * 3; if (pPred > 44) pPred = 44;
  bool pred = state == PLAY && graceT <= 0 && danger < maxDanger && edible >= 2 && (int)rnd(0, 99) < pPred;
  bool left = rnd(0, 1);
  float lvlK = (1.0f + level * 0.07f) * dif(0.9f, 1.0f, 1.12f);
  if (pred) {
    float s = ps * (1.15f + rnd(0, 70) * 0.01f); if (s > 44) s = 44; if (s < 9) s = 9;
    uint8_t kind = rnd(0, 1) ? 6 : 7;
    float sp = (kind == 6 ? 1.05f : 0.7f) * lvlK * (1.25f - s * 0.012f); if (sp < 0.45f) sp = 0.45f;
    float y = fy;
    for (int tries = 0; tries < 8; tries++) {                                // pas pile dans l'axe du joueur : on a le temps de voir venir
      y = (float)rnd(TOP + (int)(s * 0.6f) + 2, FLOOR - (int)(s * 0.6f) - 2);
      if (fabsf(y - fy) > ps * 0.56f + s * 0.6f + 12) break;
    }
    put(left ? -s * 2 : SCR_W + s * 2, y, left ? sp : -sp, s, kind, rnd(0, 628) / 100.f, kind == 6 ? 1.0f : 2.0f);
    return;
  }
  int n = 1 + (int)rnd(0, 2); if (n > freeSlots()) n = freeSlots();
  uint8_t kind = rnd(0, 5);
  float s0 = ps * (0.34f + rnd(0, 52) * 0.01f); if (s0 < 4) s0 = 4;
  float sp = (0.55f + rnd(0, 9) * 0.1f) * lvlK * (1.4f - s0 * 0.02f); if (sp < 0.4f) sp = 0.4f;
  float y = (float)rnd(TOP + (int)s0 + 2, FLOOR - (int)s0 - 4);
  for (int i = 0; i < n; i++) {                                             // petit banc
    float s = s0 * (0.88f + rnd(0, 24) * 0.01f); if (s < 4) s = 4;
    float ox = i * (s * 2.8f + 4);
    put(left ? -s * 2 - ox : SCR_W + s * 2 + ox, clampf(y + (int)rnd(0, 14) - 7, TOP + s + 2, FLOOR - s - 4), left ? sp : -sp, s, kind, rnd(0, 628) / 100.f, 2.5f + rnd(0, 20) * 0.1f);
  }
}

static void step() {
  animT++;
  for (auto &p : pops) if (p.t > 0) { p.t--; p.y -= 0.5f; }
  for (auto &p : parts) if (p.t > 0) { p.t--; p.x += p.vx; p.y += p.vy; if (p.kind == 1) { p.x += sinf(p.y * 0.2f) * 0.15f; if (p.y < 2) p.t = 0; } else { p.vx *= 0.94f; p.vy *= 0.94f; } }
  for (auto &b : bubs) { b.y -= b.v; b.x += sinf(b.y * 0.1f + b.r) * 0.25f; if (b.y < -4) { b.y = SCR_H + 4; b.x = rnd(0, SCR_W); b.r = (float)rnd(1, 3); b.v = 0.3f + b.r * 0.22f; } }
  for (auto &b : bg) { b.x += b.v; if (b.x < -20) b.x = SCR_W + 20; if (b.x > SCR_W + 20) b.x = -20; }
  if (--ventT <= 0) { emit(206, FLOOR + 3, 1, 1, rgb(196, 238, 255)); ventT = 8 + (int)rnd(0, 10); }
  if (state == PAUSED) return;
  if (state == OVER) endT++;
  if (shakeT > 0) shakeT--;
  if (flashT > 0) flashT--;
  if (mouthT > 0) mouthT--;
  if (comboT > 0 && --comboT == 0) combo = 0;
  // les poissons nagent aussi dans le titre / game over
  for (auto &f : fs) if (f.on) {
    f.x += f.vx; f.ph += 0.07f;
    if (state == PLAY && level >= 2 && f.s > ps * 0.98f && fabsf(f.x - fx) < 90) f.by += clampf(fy - f.by, -0.14f, 0.14f) * 0.6f;       // le predateur te devine un peu
    else if (state == PLAY && f.s <= ps * 0.98f && fabsf(f.x - fx) < 46 && fabsf(f.by - fy) < 30) f.by += (f.by > fy ? 0.22f : -0.22f);  // les proies fuient
    float yy = f.by + sinf(f.ph) * f.amp; float bb = f.s * 0.6f; f.by = clampf(f.by, TOP + bb + f.amp, FLOOR - bb - f.amp - 2); (void)yy;
    if (f.x < -f.s * 4 || f.x > SCR_W + f.s * 4) f.on = false;
  }
  if (graceT > 0) graceT--;
  if (state != PLAY) { if (--spawnT <= 0) { spawn(); spawnT = 40; } return; }
  tilt::update();
  float tx = tilt::x(), ty = -tilt::y();
  if (fabsf(tx) < 0.06f) tx = 0;
  if (fabsf(ty) < 0.06f) ty = 0;
  const float mx = 2.7f;
  fvx += (clampf(tx * mx * 1.15f, -mx, mx) - fvx) * 0.085f; fvy += (clampf(ty * mx * 1.15f, -mx, mx) - fvy) * 0.085f;
  if (tx == 0 && ty == 0) fvy += sinf(animT * 0.08f) * 0.012f;
  fx += fvx; fy += fvy;
  psT = clampf(psT, 6, 40); ps += (psT - ps) * 0.15f;
  float a = ps, b = ps * 0.56f;
  fx = clampf(fx, a, SCR_W - a); fy = clampf(fy, TOP + b, FLOOR - b);
  if (fvx > 0.2f) dirP = 1; else if (fvx < -0.2f) dirP = -1;
  face += (dirP - face) * 0.2f;
  if (invul > 0) invul--;
  if (--spawnT <= 0) { spawn(); int T = 30 - level * 2; spawnT = T < 13 ? 13 : T; }
  if (sqrtf(fvx * fvx + fvy * fvy) > 1.2f && (animT & 7) == 0) bubble(fx - dirP * ps, fy, 1);
  for (auto &f : fs) if (f.on) {
    float fy2 = f.by + sinf(f.ph) * f.amp;
    float fa = f.s, fb = f.s * SP[f.kind].ratio * 0.01f, dx = fabsf(f.x - fx), dy = fabsf(fy2 - fy);
    bool bigger = f.s > ps * 0.98f;
    float k = bigger ? 0.7f : 0.8f;                                                  // un peu indulgent
    if (dx < (a + fa) * k && dy < (b + fb) * (bigger ? 0.78f : 0.9f)) {
      if (!bigger) {                                                                // on le mange
        f.on = false; mouthT = 10;
        combo = comboT > 0 ? combo + 1 : 1; comboT = 70;
        int m = combo > 4 ? 4 : combo; int pts = (int)(f.s * 2) * m; score += pts;
        psT += f.s * 0.085f;
        bubble(f.x, fy2, 3); emit(f.x, fy2, 7, 0, rgb(255, 232, 120));
        sfx(700 + (int)f.s * 10 + combo * 60, 30); sfx(1000 + (int)f.s * 10 + combo * 60, 40, false);
        char t[16]; snprintf(t, sizeof t, "+%d", pts); pop(f.x, fy2 - f.s, t, rgb(255, 235, 120));
        if (combo >= 2) { char c[16]; snprintf(c, sizeof c, "x%d", m); pop(fx, fy - ps - 10, c, rgb(255, 160, 60)); }
        if (psT >= 26) {
          level++; psT = ps = 8; invul = 60; graceT = 70; combo = 0;
          pop(SCR_W / 2.f, 46, L("NEXT LEVEL", "NIVEAU SUIVANT"), rgb(120, 255, 190));
          emit(fx, fy, 16, 0, rgb(255, 240, 150)); bubble(fx, fy, 8);
          sfx(988, 80); sfx(1318, 80, false); sfx(1760, 140, false);
          for (auto &o : fs) if (o.on && o.s > ps) o.on = false;
        }
      } else if (!invul) {                                                          // il nous mange (on perd une vie)
        if (--lives <= 0) { gameOver(); return; }
        invul = 100; psT = psT * 0.85f; if (psT < 6) psT = 6; fvx = -dirP * 2.2f; shakeT = 10; flashT = 12; combo = 0;
        bubble(fx, fy, 6); emit(fx, fy, 8, 2, rgb(230, 70, 50));
        pop(fx, fy - 14, L("OUCH!", "AIE !"), rgb(255, 90, 60)); sfx(180, 100); sfx(100, 160, false);
      }
    }
  }
}

// --------------------------------------------------------------------------- dessin des poissons
static void drawFish(float x, float y, float s, float faceV, uint8_t kind, bool danger, float ph, int mouth) {
  Sp spc = SP[kind]; if (kind == 8 && g_skin) { spc.back = skin::c(1, 0); spc.mid = skin::c(0, 0); spc.belly = skin::c(2, 0); spc.fin = skin::c(0, 0); spc.acc = skin::c(3, 0); }
  const Sp &sp = spc;
  int dir = faceV >= 0 ? 1 : -1; float sq = fabsf(faceV); if (sq < 0.3f) sq = 0.3f;
  int a = (int)(s * sq + 0.5f); if (a < 2) a = 2;
  int b = (int)(s * sp.ratio * 0.01f + 0.5f); if (b < 2) b = 2;
  int xi = (int)x, yi = (int)y;
  float wag = sinf(ph * 2.2f);
  uint16_t fin = danger ? mixc(sp.fin, rgb(120, 20, 30), 80) : sp.fin, finL = mixc(fin, 0xFFFF, 90), finD = mixc(fin, 0, 90);
  // ---- queue
  int xt = xi - dir * (a - 1), tl = a * 55 / 100 + 2, spr = b * 95 / 100 + 1, ty = (int)(wag * b * 0.5f);
  canvas.fillTriangle(xt, yi - b / 3, xt - dir * tl, yi - spr + ty, xt - dir * tl * 65 / 100, yi + ty * 7 / 10, fin);
  canvas.fillTriangle(xt, yi + b / 3, xt - dir * tl, yi + spr + ty, xt - dir * tl * 65 / 100, yi + ty * 7 / 10, fin);
  canvas.fillTriangle(xt, yi - b / 3, xt, yi + b / 3, xt - dir * tl * 65 / 100, yi + ty * 7 / 10, fin);
  if (a >= 6) {
    canvas.drawLine(xt, yi - b / 3, xt - dir * tl, yi - spr + ty, finL); canvas.drawLine(xt, yi + b / 3, xt - dir * tl, yi + spr + ty, finL);
    canvas.drawLine(xt, yi, xt - dir * tl * 65 / 100, yi + ty * 7 / 10, finD);
  }
  // ---- nageoire dorsale + ventrale
  int dsw = (int)(wag * 1.5f), dh = b * 7 / 10 + 1;
  int dx1 = xi - dir * a / 3, dx2 = xi + dir * a / 4, dyb = yi - b + 1 + (kind == 6 ? 1 : 0);
  canvas.fillTriangle(dx1, dyb, dx2, dyb, dx1 + dir * a / 10 - dir * 2 + dsw, dyb - dh, fin);
  if (a >= 8) canvas.drawLine(dx1, dyb, dx1 + dir * a / 10 - dir * 2 + dsw, dyb - dh, finL);
  canvas.fillTriangle(xi - dir * a / 6, yi + b - 1, xi + dir * a / 8, yi + b - 1, xi - dir * a / 3, yi + b + b / 2 + 1 - dsw / 2, finD);
  // ---- corps (colonnes degradees)
  uint16_t cBack = sp.back, cUp = mixc(sp.back, sp.mid, 128), cMid = sp.mid, cLow = mixc(sp.mid, sp.belly, 128), cBel = sp.belly;
  uint16_t edge = danger ? mixc(sp.back, rgb(235, 40, 40), 170) : mixc(sp.back, 0, 110);
  for (int ox = -a; ox <= a; ox++) {
    float p = (float)ox * dir / a;                                       // -1 queue ... +1 tete
    float k = 1.0f - p * p; if (k <= 0) continue;
    int hh = (int)(b * sqrtf(k) * (0.8f + 0.2f * p) + 0.5f); if (hh < 1) hh = 1;
    int len = hh * 2 + 1, y0 = yi - hh, x = xi + ox;
    uint16_t c0 = cBack, c1 = cUp, c2 = cMid, c3 = cLow, c4 = cBel;
    if (sp.pat == 1 && ((p > 0.14f && p < 0.32f) || (p > -0.5f && p < -0.34f) || (p > 0.58f && p < 0.7f))) { c0 = c1 = c2 = sp.acc; c3 = mixc(sp.acc, 0xFFFF, 40); c4 = mixc(sp.acc, 0xFFFF, 60); }
    if (sp.pat == 5) {
      bool band = (p > 0.3f && p < 0.52f) || (p > -0.56f && p < -0.34f);
      bool edg = fabsf(p - 0.3f) < 0.07f || fabsf(p - 0.52f) < 0.07f || fabsf(p + 0.56f) < 0.07f || fabsf(p + 0.34f) < 0.07f;
      if (edg) { c0 = c1 = c2 = c3 = c4 = rgb(40, 24, 20); } else if (band) { c0 = rgb(226, 226, 226); c1 = c2 = rgb(250, 250, 250); c3 = c4 = rgb(255, 240, 220); }
    }
    if (len < 7) { canvas.fillRect(x, y0, 1, len / 2 + 1, c1); canvas.fillRect(x, yi + len / 2 - (len > 3 ? 1 : 0), 1, len / 2 + 1 - (len > 3 ? 0 : 1), c3); if (len > 3) canvas.drawPixel(x, yi, c2); }
    else {
      int e1 = y0 + len * 22 / 100, e2 = y0 + len * 40 / 100, e3 = y0 + len * 62 / 100, e4 = y0 + len * 80 / 100, e5 = y0 + len;
      canvas.fillRect(x, y0, 1, e1 - y0, c0); canvas.fillRect(x, e1, 1, e2 - e1, c1); canvas.fillRect(x, e2, 1, e3 - e2, c2);
      canvas.fillRect(x, e3, 1, e4 - e3, c3); canvas.fillRect(x, e4, 1, e5 - e4, c4);
    }
    canvas.drawPixel(x, y0, edge); canvas.drawPixel(x, y0 + len - 1, mixc(cBel, 0, 70));
  }
  // ---- motifs
  if (sp.pat == 2 && a >= 7) { int r = b / 4 > 1 ? b / 4 : 1; canvas.fillCircle(xi - dir * a / 3, yi - b / 4, r, sp.acc); canvas.fillCircle(xi + dir * a / 10, yi + b / 5, r, sp.acc); canvas.fillCircle(xi - dir * a * 6 / 10, yi, r > 1 ? r - 1 : 1, sp.acc); canvas.fillCircle(xi + dir * a / 3, yi - b / 2, r > 1 ? r - 1 : 1, sp.acc); }
  if (sp.pat == 3) canvas.drawLine(xi - dir * a * 6 / 10, yi, xi + dir * a / 2, yi - b / 6, kind == 4 ? sp.acc : mixc(sp.mid, 0xFFFF, 90));
  if (sp.pat == 4 && a >= 6) { for (int ox = -a * 6 / 10; ox <= a / 2; ox += 1) { float p = (float)ox * dir / a; int yy = yi - b / 5 + (int)(sinf(p * 3.2f) * b * 0.3f); canvas.drawPixel(xi + ox, yy, sp.acc); canvas.drawPixel(xi + ox, yy + 1, sp.acc); } }
  if (a >= 14) for (int ox = -a / 2; ox < a / 2; ox += 3) canvas.drawPixel(xi + ox, yi - b / 6 + ((ox / 3) & 1) * 2, mixc(cMid, 0xFFFF, 60));       // ecailles
  if (a >= 9) canvas.drawLine(xi + dir * a * 38 / 100, yi - b * 6 / 10, xi + dir * a * 32 / 100, yi + b * 6 / 10, mixc(cMid, 0, 90));                  // ouie
  // ---- nageoire pectorale
  { int px0 = xi + dir * a / 10, py0 = yi + b / 5, fl = (int)(sinf(ph * 2.2f + 1.f) * b * 0.15f);
    canvas.fillTriangle(px0, py0, px0 - dir * a * 36 / 100, py0 + b * 2 / 10 + fl, px0 - dir * a / 10, py0 + b * 6 / 10 + fl, finL); }
  // ---- bouche
  int hx = xi + dir * a;
  if (danger || mouth > 0) {
    int mo = mouth > 0 ? (mouth > 5 ? 10 - mouth : mouth) : 2; int op = b * (mouth > 0 ? 2 + mo : 3) / 10 + 1;
    canvas.fillTriangle(hx, yi, hx, yi + op, hx - dir * a * 32 / 100, yi + op / 2, rgb(86, 10, 22));
    if (danger && a >= 7) for (int t = 0; t < 3; t++) { int tx2 = hx - dir * (1 + t * 2); canvas.fillTriangle(tx2, yi, tx2 - dir * 2, yi, tx2 - dir, yi + 2 + a / 12, TFT_WHITE); }
  } else canvas.drawLine(hx, yi + b / 6, hx - dir * a * 26 / 100, yi + b / 4, mixc(sp.back, 0, 160));
  // ---- oeil
  int er = b * 36 / 100; if (er < 1) er = 1; int ex = xi + dir * (a * 56 / 100), ey = yi - b / 4;
  if (b <= 2) { canvas.drawPixel(ex, ey, TFT_WHITE); canvas.drawPixel(ex + dir, ey, 0); }
  else {
    canvas.fillCircle(ex, ey, er + 1, mixc(sp.back, 0, 160));
    canvas.fillCircle(ex, ey, er, TFT_WHITE);
    if (er >= 3) canvas.fillCircle(ex + dir, ey, er - 1, danger ? rgb(225, 30, 30) : rgb(230, 170, 40));
    canvas.fillCircle(ex + dir * (er >= 2 ? 1 : 0), ey, er >= 4 ? er / 2 : 1, danger && er < 3 ? rgb(220, 20, 20) : 0);
    if (er >= 3) canvas.drawPixel(ex - dir, ey - er / 2, TFT_WHITE);
    if (danger && a >= 7) { canvas.drawLine(ex - dir * (er + 2), ey - er - 2, ex + dir * (er + 1), ey - er + 1, rgb(24, 8, 8)); canvas.drawLine(ex - dir * (er + 2), ey - er - 1, ex + dir * (er + 1), ey - er + 2, rgb(24, 8, 8)); }
  }
}

// --------------------------------------------------------------------------- decor
static void seaweed(int bx, int h, int ph, uint16_t c1, uint16_t c2) {
  int px = bx, py = FLOOR + 3;
  for (int sgm = 1; sgm <= 5; sgm++) {
    int nx = bx + (int)(sinf(animT * 0.05f + ph + sgm * 0.7f) * (sgm * 1.4f)), ny = FLOOR + 3 - h * sgm / 5;
    uint16_t c = sgm & 1 ? c1 : c2;
    canvas.drawLine(px, py, nx, ny, c); canvas.drawLine(px + 1, py, nx + 1, ny, c);
    px = nx; py = ny;
  }
}
static void coral(int cx, int base, uint16_t c1, uint16_t c2, int h) {
  canvas.fillEllipse(cx, base, 7, 3, mixc(c1, 0, 140));
  for (int i = -2; i <= 2; i++) {
    int tx = cx + i * 4 + (i * i), ty = base - h + (i * i) * 2 + (i & 1) * 2;
    canvas.drawLine(cx + i, base, tx, ty, c1); canvas.drawLine(cx + i + 1, base, tx + 1, ty, c1);
    if (i != 0) canvas.drawLine(tx, ty + 4, tx + (i > 0 ? 3 : -3), ty - 1, c1);
    canvas.fillCircle(tx, ty, 2, c2);
  }
}
static void rock(int cx, int cy, int rx, int ry, uint16_t c) {
  canvas.fillEllipse(cx, cy, rx, ry, mixc(c, 0, 110));
  canvas.fillEllipse(cx - 1, cy - 1, rx - 1, ry - 1, c);
  canvas.fillEllipse(cx - rx / 3, cy - ry / 3, rx / 2, ry / 3, mixc(c, 0xFFFF, 60));
}

static void renderScene(float camX) {
  // ----- eau + rayons de lumiere
  uint16_t rowC[SCR_H / 3 + 1];
  for (int y = 0, i = 0; y < SCR_H; y += 3, i++) { rowC[i] = waterCol(y); canvas.fillRect(0, y, SCR_W, 3, rowC[i]); }
  for (int r = 0; r < 3; r++) {
    float x0 = 28 + r * 78 + sinf(animT * 0.012f + r * 2.1f) * 12; int amt = 14 + (int)((sinf(animT * 0.03f + r) + 1) * 6);
    for (int y = 0, i = 0; y < FLOOR; y += 3, i++) {
      int cx = (int)(x0 + y * 0.32f), w = 12 + y / 5, lift = amt + (FLOOR - y) / 7;
      canvas.fillRect(cx - w / 2, y, w, 3, mixc(rowC[i], rgb(190, 240, 255), lift));
      canvas.fillRect(cx - w / 5, y, w * 2 / 5, 3, mixc(rowC[i], rgb(220, 250, 255), lift + 10));
    }
  }
  // ----- recif lointain (parallaxe) + petits poissons de fond
  int off = (int)(-camX * 0.05f);
  uint16_t far1 = mixc(rgb(8, 40, 96), rgb(40, 110, 160), 60), far2 = mixc(rgb(6, 32, 86), rgb(20, 70, 130), 90);
  for (int x = 0; x < SCR_W; x += 3) {
    float u = (x - off) * 0.045f; int h = (int)(16 + 9 * sinf(u) + 5 * sinf(u * 2.3f + 1) + 3 * sinf(u * 5.1f)); if (h < 4) h = 4;
    canvas.fillRect(x, FLOOR + 4 - h, 3, h, far1);
    int h2 = (int)(8 + 5 * sinf(u * 1.7f + 2)); canvas.fillRect(x, FLOOR + 4 - h2, 3, h2, far2);
  }
  for (auto &b : bg) { int xi = (int)b.x, yi = (int)b.y, d = b.v > 0 ? 1 : -1; uint16_t c = mixc(rgb(8, 44, 104), rgb(30, 100, 160), 40); canvas.fillEllipse(xi, yi, (int)b.s, (int)(b.s * 0.45f) + 1, c); canvas.fillTriangle(xi - d * (int)b.s, yi, xi - d * (int)(b.s * 1.7f), yi - 2, xi - d * (int)(b.s * 1.7f), yi + 2, c); }
  // ----- surface qui brille
  for (int x = 0; x < SCR_W; x += 4) { int hgt = 2 + (int)((sinf(x * 0.09f + animT * 0.1f) + 1) * 1.2f); canvas.fillRect(x, 0, 4, hgt, rgb(176, 232, 250)); }
  // ----- sable
  for (int y = FLOOR + 3, i = 0; y < SCR_H; y += 2, i++) canvas.fillRect(0, y, SCR_W, 2, mixc(rgb(230, 206, 144), rgb(176, 150, 96), i * 256 / 8));
  for (int x = -10; x < SCR_W + 10; x += 22) canvas.fillEllipse(x + (x * 7 % 9), FLOOR + 4, 14, 3, rgb(232, 208, 148));
  for (int i = 0; i < 10; i++) {                                                                                      // reflets d'eau sur le sable
    int cx = (int)(i * 26 + animT * 0.5f) % (SCR_W + 20) - 10, cy = FLOOR + 6 + (i * 5) % 8; int w = 4 + (int)((sinf(animT * 0.12f + i * 1.7f) + 1) * 3);
    canvas.drawFastHLine(cx, cy, w, rgb(250, 238, 186)); }
  for (int x = 6; x < SCR_W; x += 17) canvas.drawFastHLine(x, FLOOR + 10 + (x % 5), 5, rgb(196, 170, 112));
  // ----- rochers, coraux, algues (derriere les poissons)
  rock(26, FLOOR + 5, 13, 7, rgb(112, 118, 140)); rock(34, FLOOR + 7, 8, 5, rgb(96, 102, 126)); rock(196, FLOOR + 6, 11, 6, rgb(120, 114, 136));
  coral(66, FLOOR + 4, rgb(236, 92, 130), rgb(255, 160, 190), 15); coral(160, FLOOR + 4, rgb(250, 140, 50), rgb(255, 206, 110), 12);
  for (int i = 0; i < 9; i++) { int bx = 8 + i * 28 + (i * 5) % 7; if (i == 2 || i == 5) continue; seaweed(bx, 14 + (i * 5) % 16, i, rgb(28, 130 + (i * 17) % 60, 66), rgb(50, 170 + (i * 9) % 40, 90)); }
  // event a bulles
  canvas.fillEllipse(206, FLOOR + 4, 4, 2, rgb(70, 60, 56));
  for (auto &b : bubs) { int r = (int)b.r; canvas.drawCircle((int)b.x, (int)b.y, r, rgb(190, 236, 255)); if (r >= 2) canvas.drawPixel((int)b.x - 1, (int)b.y - 1, TFT_WHITE); }
}

static void renderFront() {
  for (auto &p : parts) if (p.t > 0) {
    if (p.kind == 1) { canvas.drawCircle((int)p.x, (int)p.y, 1 + (p.t > 40), p.col); }
    else if (p.kind == 0) { uint16_t c = (p.t & 2) ? p.col : TFT_WHITE; canvas.fillRect((int)p.x, (int)p.y, 2, 2, c); if (p.t > 10) { canvas.drawPixel((int)p.x - 1, (int)p.y, c); canvas.drawPixel((int)p.x + 2, (int)p.y, c); } }
    else canvas.fillCircle((int)p.x, (int)p.y, p.t > 12 ? 2 : 1, p.col);
  }
  for (int x = 4; x < SCR_W; x += 31) { int h = 5 + (x * 3) % 5; canvas.drawLine(x, SCR_H - 1, x + (int)(sinf(animT * 0.06f + x) * 2), SCR_H - h, rgb(40, 150, 80)); canvas.drawLine(x + 1, SCR_H - 1, x + 1 + (int)(sinf(animT * 0.06f + x) * 2), SCR_H - h, rgb(30, 120, 66)); }
}

static void panel(int x, int y, int w, int h, uint16_t border) {
  canvas.fillRoundRect(x + 2, y + 3, w, h, 8, rgb(2, 14, 36));
  canvas.fillRoundRect(x, y, w, h, 8, rgb(8, 36, 80)); canvas.fillRoundRect(x + 2, y + 2, w - 4, h / 2 - 2, 6, rgb(14, 52, 108));
  canvas.drawRoundRect(x, y, w, h, 8, border);
  canvas.drawCircle(x + 10, y + h - 8, 3, rgb(120, 190, 230)); canvas.drawCircle(x + 18, y + h - 15, 2, rgb(120, 190, 230)); canvas.drawCircle(x + w - 12, y + 10, 2, rgb(120, 190, 230));
}

static void render() {
  renderScene(state == TITLE ? 0 : fx - 120);
  for (auto &f : fs) if (f.on) drawFish(f.x, f.by + sinf(f.ph) * f.amp, f.s, f.vx > 0 ? 1.f : -1.f, f.kind, state != TITLE && f.s > ps * 0.98f, f.ph, 0);
  if (state != TITLE && (state != OVER || (animT & 8)) && (invul == 0 || (animT & 4))) {
    float g = 1.0f + (mouthT > 0 ? 0.08f : 0.f);
    drawFish(fx, fy, ps * g, face, 8, false, animT * 0.18f + fvx, mouthT);
  }
  renderFront();
  if (flashT > 0) { for (int i = 0; i < 3; i++) { uint16_t c = mixc(rgb(255, 40, 40), 0, 40 + i * 70 + (12 - flashT) * 6); canvas.drawRect(i, i, SCR_W - 2 * i, SCR_H - 2 * i, c); } }
  for (auto &p : pops) if (p.t > 0) ui::popText((int)p.x, (int)p.y, p.s, p.col);
  char b[24];
  if (state != TITLE) {
    snprintf(b, sizeof b, "%lu", (unsigned long)score); txtO(b, 5, 0, &fonts::Font4, TFT_WHITE, rgb(0, 30, 70));
    if (combo >= 2) { snprintf(b, sizeof b, "x%d", combo > 4 ? 4 : combo); txtO(b, 5, 26, &fonts::Font2, rgb(255, 170, 60), rgb(0, 30, 70)); }
    for (int i = 0; i < lives; i++) { int hx = SCR_W - 12 - i * 14; canvas.fillEllipse(hx, 8, 4, 3, rgb(250, 130, 30)); canvas.fillTriangle(hx + 3, 8, hx + 7, 5, hx + 7, 11, rgb(250, 130, 30)); canvas.drawPixel(hx - 2, 7, TFT_WHITE); }
    canvas.fillRoundRect(SCR_W / 2 - 31, 2, 62, 8, 4, rgb(4, 24, 60)); canvas.drawRoundRect(SCR_W / 2 - 31, 2, 62, 8, 4, rgb(170, 225, 250));
    int w = (int)(56 * clampf((psT - 8) / 18.f, 0, 1)); if (w > 0) canvas.fillRoundRect(SCR_W / 2 - 28, 4, w, 4, 2, rgb(120, 255, 190));
    snprintf(b, sizeof b, "LV %d", level); txtO(b, SCR_W / 2, 12, &fonts::Font0, TFT_WHITE, rgb(0, 30, 70), textdatum_t::top_center);
  }
  if (state == TITLE) {
    panel(40, 26, 160, 56, rgb(150, 215, 245));
    txt(L("FISH", "POISSON"), SCR_W / 2 + 1, 33, &fonts::Font4, rgb(120, 50, 0), textdatum_t::top_center, 1.2f);
    txt(L("FISH", "POISSON"), SCR_W / 2, 31, &fonts::Font4, rgb(255, 176, 64), textdatum_t::top_center, 1.2f);
    if (best > 0) { snprintf(b, sizeof b, L("best %lu", "record %lu"), (unsigned long)best); txt(b, SCR_W / 2, 62, &fonts::Font2, rgb(200, 230, 250), textdatum_t::top_center); }
  }
  if (state == PAUSED) ui::pause();
  if (state == OVER) ui::gameOver(L("EATEN!", "MANGE !"), score, best, (score) > 0 && (uint32_t)(score) >= (uint32_t)(best));
}

void loop() {
  uint32_t now = micros();
  if (last == 0) last = now;
  acc += now - last; last = now; if (acc > 100000) acc = 100000;
  bool a = M5.BtnA.wasPressed(), p = M5.BtnPWR.wasPressed();
  switch (state) {
    case TITLE: if (a) { reset(); state = PLAY; sfx(660, 40); } break;
    case PLAY: if (p) { state = PAUSED; sfx(440, 50); } break;
    case PAUSED: if (p || a) { state = PLAY; sfx(660, 50); } break;
    case OVER: if (a && endT > 40) { reset(); state = PLAY; sfx(660, 40); } break;
  }
  while (acc >= STEP_US) { step(); acc -= STEP_US; }
  render();
  bgm::run(4, state == PLAY);
  present(shakeT > 0 ? (int)rnd(0, 4) - 2 : 0, shakeT > 0 ? (int)rnd(0, 4) - 2 : 0);
}

void icon(int x, int y) {
  canvas.setClipRect(x, y, 64, 64);
  for (int j = 0; j < 64; j += 2) canvas.fillRect(x, y + j, 64, 2, mixc(rgb(80, 196, 232), rgb(8, 36, 90), j * 4));
  canvas.fillTriangle(x + 8, y, x + 20, y, x + 40, y + 58, mixc(rgb(110, 210, 240), rgb(190, 240, 255), 60));
  canvas.fillTriangle(x + 34, y, x + 42, y, x + 62, y + 50, mixc(rgb(90, 200, 236), rgb(190, 240, 255), 50));
  canvas.fillRect(x, y + 55, 64, 9, rgb(226, 202, 140)); canvas.fillEllipse(x + 40, y + 56, 16, 3, rgb(238, 214, 154));
  for (int k = 0; k < 3; k++) { int bx = x + 6 + k * 5; canvas.drawLine(bx, y + 58, bx + (k - 1) * 2, y + 44 - k * 2, rgb(30, 140 + k * 20, 70)); canvas.drawLine(bx + 1, y + 58, bx + 1 + (k - 1) * 2, y + 44 - k * 2, rgb(30, 140 + k * 20, 70)); }
  canvas.fillEllipse(x + 54, y + 57, 7, 4, rgb(112, 118, 140)); canvas.fillEllipse(x + 52, y + 55, 3, 2, rgb(150, 156, 176));
  drawFish(x + 50, y + 15, 5, -1, 1, false, 1.0f, 0);
  drawFish(x + 20, y + 40, 11, 1, 8, false, 0.5f, 0);
  drawFish(x + 40, y + 28, 15, -1, 6, true, 2.0f, 0);
  canvas.drawCircle(x + 9, y + 18, 3, rgb(190, 235, 255)); canvas.drawCircle(x + 15, y + 9, 2, rgb(190, 235, 255)); canvas.drawCircle(x + 13, y + 26, 1, rgb(190, 235, 255));
  canvas.clearClipRect();
}

}  // namespace fish
