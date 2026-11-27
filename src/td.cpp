// ============================================================================
//  POCKET SIEGE - tower defense (jeu original)
//  12 niveaux dans 4 mondes (prairie, desert, neige, volcan), difficulte croissante.
//  B = emplacement suivant (puis le bouton >> ), A = construire / ameliorer / vendre, PWR = pause.
//  Les pieces gagnees achetent des ameliorations permanentes (boutique de la carte).
//  Art : tools/art/td*.py -> src/td_art.h (export_td.py)
// ============================================================================
#include "common.h"
#include "td_art.h"

namespace td {
using namespace tdart;

static const uint32_t STEP_US = 20000;
static const int OY = 23;
#define IX(r, t) ((r) * 5 + (t))
enum { RG = 0, RF, RE, RW, RP, RT, RB, RS, RK };
enum { TO = 0, TS, TB, TL, TH };
static const uint8_t CWH = 45, CBK = 46, CGD = 47;

// ------------------------------------------------------------------ sprites (meme format que Emberwood)
static uint8_t* fb = nullptr; static bool swap16 = true;
enum { BF_FLIP = 1, BF_FLASH = 2 };
static inline uint8_t flashOf(uint8_t c) { return (c < 45 && c % 5 >= 2) ? CWH : c; }
template <typename Fn> static void forPix(uint16_t id, Fn fn) {
  const Spr& s = SPR[id]; const int w = s.w, h = s.h; const uint8_t* d = BLOB + s.off;
  if (s.fmt == 0) {
    const uint8_t* lut = d; const uint8_t* px = d + s.nlut; int n = 0;
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++, n++) { uint8_t b = px[n >> 1]; uint8_t v = (n & 1) ? (b & 15) : (b >> 4); if (v) fn(i, j, lut[v - 1]); }
  } else {
    int n = 0, tot = w * h; const uint8_t* p = d;
    while (n < tot) { int run = p[0]; uint8_t c = p[1]; p += 2; if (c != 255) for (int k = 0; k < run; k++) { int m = n + k, j = m / w; fn(m - j * w, j, c); } n += run; }
  }
}
static int clipTop = OY;
static void blit(uint16_t id, int x, int y, uint8_t fl = 0) {
  const Spr& s = SPR[id]; const int w = s.w, h = s.h;
  if (x >= 240 || y >= 135 || x + w <= 0 || y + h <= 0) return;
  const bool flip = fl & BF_FLIP, fla = fl & BF_FLASH;
  forPix(id, [&](int i, int j, uint8_t c) {
    int X = x + (flip ? w - 1 - i : i), Y = y + j;
    if ((unsigned)X < 240 && Y >= clipTop && Y < 135) fb[Y * 240 + X] = fla ? flashOf(c) : c;
  });
}
static inline void pset(int x, int y, uint8_t c) { if ((unsigned)x < 240 && y >= clipTop && y < 135) fb[y * 240 + x] = c; }
static void shade(int cx, int cy, int rx, int ry) {
  for (int y = cy - ry; y <= cy + ry; y++) { if (y < clipTop || y >= 135) continue;
    for (int x = cx - rx; x <= cx + rx; x++) { if ((unsigned)x >= 240) continue;
      float a = (float)(x - cx) / (rx + 0.5f), b = (float)(y - cy) / (ry + 0.5f); if (a * a + b * b > 1) continue;
      uint8_t& v = fb[y * 240 + x]; if (v < 45 && v % 5 > 1) v--; } }
}
static inline uint16_t sw(uint16_t c) { return swap16 ? (uint16_t)((c >> 8) | (c << 8)) : c; }
static void blit565(uint16_t id, int x, int y) {
  uint16_t* out = (uint16_t*)canvas.getBuffer();
  forPix(id, [&](int i, int j, uint8_t c) { int X = x + i, Y = y + j; if ((unsigned)X < 240 && (unsigned)Y < 135) out[Y * 240 + X] = sw(PAL[c]); });
}
static int fade = 256;
static void convert(int y0 = OY) {
  uint16_t* out = (uint16_t*)canvas.getBuffer(); uint16_t lut[48];
  for (int i = 0; i < 48; i++) lut[i] = sw(fade >= 256 ? PAL[i] : mixc(0, PAL[i], fade));
  for (int p = y0 * 240; p < 240 * 135; p++) out[p] = lut[fb[p]];
}
static void pop(const char* s, int x, int y, uint16_t col) { txt(s, x + 1, y + 1, &fonts::Font0, rgb(30, 16, 10), textdatum_t::top_center); txt(s, x, y, &fonts::Font0, col, textdatum_t::top_center); }

// ------------------------------------------------------------------ sauvegarde (progression)
enum { U_DMG, U_RANGE, U_GOLD, U_LIVES, U_SPEED, NUPG };
static uint8_t stars[12]; static int coins = 0; static uint8_t upg[NUPG];
static Preferences pr;
static void loadSave() {
  pr.begin("siege", true);
  memset(stars, 0, sizeof stars); memset(upg, 0, sizeof upg);
  char k[6]; for (int i = 0; i < 12; i++) { snprintf(k, sizeof k, "s%d", i); stars[i] = pr.getInt(k, 0); }
  for (int i = 0; i < NUPG; i++) { snprintf(k, sizeof k, "u%d", i); upg[i] = pr.getInt(k, 0); }
  coins = pr.getInt("co", 0); pr.end();
  for (auto& s : stars) if (s > 3) s = 0; for (auto& u : upg) if (u > 5) u = 0;
}
static void save() {
  pr.begin("siege", false); char k[6];
  for (int i = 0; i < 12; i++) { snprintf(k, sizeof k, "s%d", i); pr.putInt(k, stars[i]); }
  for (int i = 0; i < NUPG; i++) { snprintf(k, sizeof k, "u%d", i); pr.putInt(k, upg[i]); }
  pr.putInt("co", coins); pr.end();
}
static bool unlocked(int l) { return l == 0 || stars[l - 1] > 0; }

// ------------------------------------------------------------------ donnees de jeu
enum { T_ARCHER, T_CANNON, T_FROST, T_STORM };
static const int COST[4][3] = {{50, 60, 90}, {80, 100, 140}, {70, 80, 120}, {100, 120, 170}};
static const float TDMG[4][3] = {{6, 10, 15}, {9, 15, 24}, {2, 3, 4}, {7, 11, 17}};
static const float TRNG[4][3] = {{44, 50, 56}, {40, 44, 48}, {38, 42, 48}, {46, 52, 58}};
static const int TCD[4][3] = {{34, 28, 22}, {80, 70, 60}, {50, 45, 40}, {70, 60, 50}};
enum { F_SLIME, F_GOBLIN, F_KNIGHT, F_BAT, F_SHAMAN, F_OGRE };
struct EDef { uint16_t spr; int hp; float spd; int gold, lives, fy; bool fly, armor; };
static const EDef EDEFS[6] = {
  {E_SLIME, 18, 0.45f, 5, 1, 14, false, false}, {E_GOBLIN, 15, 0.80f, 6, 1, 18, false, false}, {E_KNIGHT, 70, 0.30f, 10, 1, 22, false, true},
  {E_BAT, 18, 0.65f, 6, 1, 12, true, false}, {E_SHAMAN, 34, 0.38f, 9, 1, 20, false, false}, {E_OGRE, 520, 0.22f, 60, 5, 32, false, true},
};
static const int UPG_BASE[NUPG] = {120, 150, 100, 200, 180};
static int upgCost(int k) { return UPG_BASE[k] * (upg[k] + 1); }

enum Mode { M_MAP, M_SHOP, M_PLAY, M_PAUSE, M_WIN, M_LOSE };
static Mode mode = M_MAP;
static uint32_t last = 0, acc = 0, tick = 0;
static bool aP, bP, pP, aPend, bPend, pPend;
static int mapSel = 0, shopSel = 0;

static const Level* LV = nullptr; static int lvl = 0;
static float segLen[12], totalLen = 0;
static int lives, livesStart, gold, wave, nWaves, waveT, spawnLeft, spawnT, kills, endT, winStars, reward;
static bool waveOn, fast;
struct Tower { bool on; uint8_t type, lv; int cd, fireT, invested; };
static Tower tw[8];
struct Foe { bool on; uint8_t type; float d, hp, hpMax, spd; int slowT, hurtT, healCd; float slowF; };
static const int NFOE = 48; static Foe foes[NFOE];
struct Proj { bool on; uint8_t kind; float x, y, tx, ty, dmg; int tgt, lv, life; };
static const int NPROJ = 32; static Proj projs[NPROJ];
struct Fx { bool on; uint8_t kind; int16_t x, y; int t; };
static const int NFX = 24; static Fx fxs[NFX];
struct Pop { bool on; int16_t x, y; int t; int val; uint8_t kind; };
static const int NPOP = 14; static Pop pops[NPOP];
struct Bolt { bool on; uint8_t n; int16_t xs[6], ys[6]; int t; };
static Bolt bolts[6];
struct Part { float x, y, vx, vy; uint8_t c, life; };
static const int NPART = 90; static Part parts[NPART];
static int cursor = 0; static bool ring = false; static int ringSel = 0;
static int waveTotal[16];

static int world() { return LV ? LV->world : 0; }
static float dmgMul() { return 1.0f + 0.10f * upg[U_DMG]; }
static float rngMul() { return 1.0f + 0.06f * upg[U_RANGE]; }
static float cdMul() { return 1.0f - 0.06f * upg[U_SPEED]; }
static int typesFor(int w) { return w >= 2 ? 4 : (w >= 1 ? 3 : 2); }

static void posAt(float d, float& x, float& y, int& dir) {
  for (int i = 0; i + 1 < LV->npts; i++) {
    if (d <= segLen[i] || i + 2 == LV->npts) {
      float k = segLen[i] > 0 ? d / segLen[i] : 0; if (k > 1) k = 1;
      float ax = LV->pts[i][0], ay = LV->pts[i][1], bx = LV->pts[i + 1][0], by = LV->pts[i + 1][1];
      x = ax + (bx - ax) * k; y = ay + (by - ay) * k; dir = bx > ax ? 1 : (bx < ax ? -1 : 0); return;
    }
    d -= segLen[i];
  }
  x = LV->pts[LV->npts - 1][0]; y = LV->pts[LV->npts - 1][1]; dir = 1;
}
static void addPart(float x, float y, float vx, float vy, uint8_t c, int life) {
  for (auto& p : parts) if (!p.life) { p = {x, y, vx, vy, c, (uint8_t)life}; return; }
}
static void addFx(uint8_t k, int x, int y) { for (auto& f : fxs) if (!f.on) { f = {true, k, (int16_t)x, (int16_t)y, 0}; return; } }
static void addPop(int x, int y, int val, uint8_t kind) {
  Pop* p = nullptr; for (auto& q : pops) if (!q.on) { p = &q; break; }
  if (!p) { p = &pops[0]; for (auto& q : pops) if (q.t > p->t) p = &q; }
  *p = {true, (int16_t)x, (int16_t)y, 0, val, kind};
}

// ------------------------------------------------------------------ vagues (difficulte croissante)
static int waveCount(int w) { return 5 + w + world() * 2 + (lvl % 3); }
static int waveFoe(int w, int i) {
  int wd = world(); bool last = (w == nWaves - 1);
  if (last && lvl % 3 == 2 && i == 0) return F_OGRE;                 // boss : fin de chaque monde
  int pool[6], n = 0; pool[n++] = F_SLIME; pool[n++] = F_GOBLIN;
  if (wd >= 1 || w >= 4) pool[n++] = F_KNIGHT;
  if (wd >= 1) pool[n++] = F_SHAMAN;
  if (wd >= 2 || (wd == 1 && w >= 5)) pool[n++] = F_BAT;
  if (w % 4 == 3 && n > 2) return pool[2 + (i % (n - 2))];            // vagues "elites"
  return pool[(i * 7 + w * 3) % n];
}
static void startWave() {
  if (wave >= nWaves) return;
  waveOn = true; spawnLeft = waveCount(wave); waveTotal[wave] = spawnLeft; spawnT = 0; sfx(523, 60); sfx(784, 90, false);
}
static void spawnFoe() {
  int idx = waveTotal[wave] - spawnLeft;
  int t = waveFoe(wave, idx);
  for (auto& f : foes) if (!f.on) {
    float hm = (1.0f + 0.12f * wave) * (1.0f + 0.45f * lvl) * dif(0.8f, 1.0f, 1.25f);
    f = {true, (uint8_t)t, -6.0f - (t == F_OGRE ? 10 : 0), EDEFS[t].hp * hm, EDEFS[t].hp * hm, EDEFS[t].spd * (1.0f + 0.03f * lvl), 0, 0, 60, 1};
    return;
  }
}

// ------------------------------------------------------------------ niveau
static void startLevel(int l) {
  lvl = l; LV = &LEVELS[l]; totalLen = 0;
  for (int i = 0; i + 1 < LV->npts; i++) { segLen[i] = fabsf((float)LV->pts[i + 1][0] - LV->pts[i][0]) + fabsf((float)LV->pts[i + 1][1] - LV->pts[i][1]); totalLen += segLen[i]; }
  livesStart = lives = 20 + 3 * upg[U_LIVES]; gold = 120 + 25 * upg[U_GOLD] + 20 * world();
  wave = 0; nWaves = 0; for (int w = 0; w < 16; w++) {} nWaves = 6 + world() * 2 + (l % 3);
  if (nWaves > 16) nWaves = 16;
  waveOn = false; waveT = 600; kills = 0; fast = false; endT = 0;
  memset(tw, 0, sizeof tw); memset(foes, 0, sizeof foes); memset(projs, 0, sizeof projs); memset(fxs, 0, sizeof fxs); memset(pops, 0, sizeof pops); memset(bolts, 0, sizeof bolts); memset(parts, 0, sizeof parts);
  cursor = 0; ring = false; mode = M_PLAY; fade = 0;
#ifdef ZTEST
  if (getenv("ZGOLD")) gold = atoi(getenv("ZGOLD"));
  if (getenv("ZAUTO")) for (int i = 0; i < LV->npads; i++) tw[i] = {true, (uint8_t)(i % 4), 2, 10, 0, 300};
#endif
}

// ------------------------------------------------------------------ combat
static void hurtFoe(Foe& f, float dmg, int x, int y, bool showPop, bool crit = false) {
  f.hp -= dmg; f.hurtT = 6;
  if (showPop && (tick & 1)) addPop(x, y - 10, -(int)(dmg + 0.5f), crit ? 2 : 0);
  if (f.hp <= 0) {
    f.on = false; kills++;
    float fx, fy; int dir; posAt(f.d, fx, fy, dir);
    const EDef& e = EDEFS[f.type]; int g = e.gold + world();
    gold += g; addPop((int)fx, (int)fy - e.fy - 6, g, 1);
    const Spr& s = SPR[e.spr]; float cx = s.w / 2.0f, cy = s.h / 2.0f;
    int ox = (int)fx - s.w / 2, oy = (int)fy - e.fy - (e.fly ? 8 : 0);
    forPix(e.spr, [&](int i, int j, uint8_t c) { if ((i + j) % 3) return; float dx = i - cx, dy = j - cy, d = sqrtf(dx * dx + dy * dy) + 0.5f;
      addPart(ox + i, oy + j, dx / d * 1.3f, dy / d * 1.3f - 0.8f, c, 14 + (i * 7 + j) % 14); });
    addFx(4, (int)fx, (int)fy - e.fy);                                // piece qui jaillit
    sfx(f.type == F_OGRE ? 200 : 900 + rnd(0, 200), f.type == F_OGRE ? 300 : 25);
  }
}
static int findTarget(float tx, float ty, float range, bool canFly) {
  int best = -1; float bd = -1;
  for (int i = 0; i < NFOE; i++) { Foe& f = foes[i]; if (!f.on || f.d < 0) continue;
    if (EDEFS[f.type].fly && !canFly) continue;
    float x, y; int dr; posAt(f.d, x, y, dr);
    float dx = x - tx, dy = y - ty; if (dx * dx + dy * dy > range * range) continue;
    if (f.d > bd) { bd = f.d; best = i; } }
  return best;
}
static void towerStep(int pi) {
  Tower& t = tw[pi]; if (!t.on) return;
  if (t.fireT > 0) t.fireT--;
  if (t.cd > 0) { t.cd--; return; }
  float px = LV->pads[pi][0], py = LV->pads[pi][1] - 18;
  float range = TRNG[t.type][t.lv] * rngMul();
  int ti = findTarget(px, py + 18, range, t.type != T_CANNON);
  if (ti < 0) return;
  Foe& f = foes[ti]; float fx, fy; int dr; posAt(f.d, fx, fy, dr);
  float dmg = TDMG[t.type][t.lv] * dmgMul();
  t.cd = (int)(TCD[t.type][t.lv] * cdMul()); t.fireT = 8;
  if (t.type == T_STORM) {                                            // eclair en chaine
    Bolt* b = nullptr; for (auto& q : bolts) if (!q.on) { b = &q; break; }
    int n = 0, cur = ti; float lx = px, ly = py - 10; bool hit[NFOE] = {false};
    if (b) { b->on = true; b->t = 0; b->xs[0] = (int16_t)lx; b->ys[0] = (int16_t)ly; b->n = 1; }
    while (cur >= 0 && n < 2 + t.lv) {
      Foe& g = foes[cur]; float gx, gy; int d2; posAt(g.d, gx, gy, d2); hit[cur] = true;
      float gyc = gy - EDEFS[g.type].fy / 2 - (EDEFS[g.type].fly ? 8 : 0);
      if (b && b->n < 6) { b->xs[b->n] = (int16_t)gx; b->ys[b->n] = (int16_t)gyc; b->n++; }
      hurtFoe(g, dmg, (int)gx, (int)gyc, true); n++;
      int nx = -1; float nd = 34 * 34;
      for (int i = 0; i < NFOE; i++) { if (!foes[i].on || hit[i]) continue; float x, y; int d3; posAt(foes[i].d, x, y, d3); float dd = (x - gx) * (x - gx) + (y - gy) * (y - gy); if (dd < nd) { nd = dd; nx = i; } }
      cur = nx;
    }
    sfx(1800, 20); sfx(1200, 30, false);
    return;
  }
  for (auto& p : projs) if (!p.on) {
    p = {true, t.type, px, py, fx, fy - EDEFS[f.type].fy / 2.0f, dmg, ti, t.lv, 0};
    break;
  }
  if (t.type == T_CANNON) sfx(160, 60); else if (t.type == T_ARCHER) sfx(1400, 12); else sfx(1900, 15);
}
static void projStep(Proj& p) {
  p.life++;
  Foe* f = (p.tgt >= 0 && foes[p.tgt].on) ? &foes[p.tgt] : nullptr;
  if (f && p.kind != T_CANNON) { float fx, fy; int d; posAt(f->d, fx, fy, d); p.tx = fx; p.ty = fy - EDEFS[f->type].fy / 2.0f - (EDEFS[f->type].fly ? 8 : 0); }
  float sp = p.kind == T_CANNON ? 2.6f : (p.kind == T_ARCHER ? 4.2f : 3.0f);
  float dx = p.tx - p.x, dy = p.ty - p.y, d = sqrtf(dx * dx + dy * dy);
  if (d <= sp || p.life > 120) {
    if (p.kind == T_CANNON) {                                         // explosion de zone
      float r = 14 + 2 * p.lv; addFx(0, (int)p.tx, (int)p.ty); sfx(110, 90);
      for (auto& g : foes) if (g.on && !EDEFS[g.type].fly) { float x, y; int dd; posAt(g.d, x, y, dd); if ((x - p.tx) * (x - p.tx) + (y - p.ty) * (y - p.ty) < r * r) hurtFoe(g, p.dmg, (int)x, (int)y - 10, true); }
    } else if (f) {
      if (p.kind == T_ARCHER) {
        bool crit = rnd(0, 99) < 12; float dm = p.dmg * (crit ? 2.0f : 1.0f) * (EDEFS[f->type].armor ? 0.6f : 1.0f);
        hurtFoe(*f, dm, (int)p.tx, (int)p.ty, true, crit);
      } else {                                                          // givre : ralentit + petite zone
        addFx(1, (int)p.tx, (int)p.ty);
        for (auto& g : foes) if (g.on) { float x, y; int dd; posAt(g.d, x, y, dd); if ((x - p.tx) * (x - p.tx) + (y - p.ty) * (y - p.ty) < 12 * 12) { g.slowT = 90; g.slowF = 0.6f - 0.1f * p.lv; hurtFoe(g, p.dmg, (int)x, (int)y - 10, false); } }
      }
    }
    p.on = false; return;
  }
  p.x += dx / d * sp; p.y += dy / d * sp;
}
static void foeStep(Foe& f) {
  if (f.hurtT > 0) f.hurtT--;
  float sp = f.spd; if (f.slowT > 0) { f.slowT--; sp *= f.slowF; }
  f.d += sp;
  if (f.type == F_SHAMAN && --f.healCd <= 0) {                        // le chaman soigne ses voisins
    f.healCd = 90; float x, y; int dr; posAt(f.d, x, y, dr);
    for (auto& g : foes) if (g.on && &g != &f) { float gx, gy; int d2; posAt(g.d, gx, gy, d2); if ((gx - x) * (gx - x) + (gy - y) * (gy - y) < 30 * 30 && g.hp < g.hpMax) { g.hp += g.hpMax * 0.08f; if (g.hp > g.hpMax) g.hp = g.hpMax; addPart(gx, gy - 12, 0, -0.5f, IX(RG, TH), 20); } }
    sfx(1300, 20);
  }
  if (f.d >= totalLen) {
    f.on = false; lives -= EDEFS[f.type].lives; sfx(180, 120);
    if (lives <= 0) { lives = 0; mode = M_LOSE; endT = 0; reward = 10 + wave * 5; coins += reward; save(); sfx(140, 400); }
  }
}
static void playStep() {
  tick++;
#ifdef ZTEST
  if (getenv("ZLOG") && tick % 250 == 0) { int n = 0; for (auto& f : foes) n += f.on; int t = 0; for (auto& x : tw) t += x.on; fprintf(stderr, "t=%u lvl=%d wave=%d/%d on=%d lives=%d gold=%d foes=%d towers=%d kills=%d%c", (unsigned)tick, lvl, wave, nWaves, (int)waveOn, lives, gold, n, t, kills, 10); }
#endif
  if (fade < 256) { fade += 32; if (fade > 256) fade = 256; }
  // vagues
  if (!waveOn) { if (wave < nWaves && --waveT <= 0) startWave(); }
  else if (spawnLeft > 0) { if (--spawnT <= 0) { spawnFoe(); spawnLeft--; spawnT = world() >= 2 ? 30 : 36; } }
  else {
    bool any = false; for (auto& f : foes) if (f.on) any = true;
    if (!any) {
      waveOn = false; wave++; gold += 12 + wave * 3;
      if (wave >= nWaves) {
        mode = M_WIN; endT = 0; float r = (float)lives / livesStart;
        winStars = r >= 0.9f ? 3 : (r >= 0.5f ? 2 : 1);
        reward = 30 + 25 * winStars + 5 * lvl + (stars[lvl] == 0 ? 40 : 0);
        if (winStars > stars[lvl]) stars[lvl] = winStars;
        coins += reward; save(); sfx(523, 100); sfx(659, 100, false); sfx(784, 100, false); sfx(1047, 300, false);
      } else waveT = 420;
    }
  }
  for (int i = 0; i < LV->npads; i++) towerStep(i);
  for (auto& p : projs) if (p.on) projStep(p);
  for (auto& f : foes) if (f.on) foeStep(f);
  for (auto& x : fxs) if (x.on && ++x.t > 24) x.on = false;
  for (auto& b : bolts) if (b.on && ++b.t > 8) b.on = false;
  for (auto& p : pops) if (p.on && ++p.t > 40) p.on = false;
  for (auto& p : parts) if (p.life) { p.life--; p.x += p.vx; p.y += p.vy; p.vy += 0.08f; }
}

// ------------------------------------------------------------------ commandes en jeu
static int ringCount() {
  if (cursor >= LV->npads) return 0;
  Tower& t = tw[cursor]; if (!t.on) return typesFor(world());
  return t.lv < 2 ? 2 : 1;                                            // ameliorer + vendre (ou vendre)
}
static void ringAction() {
  Tower& t = tw[cursor];
  if (!t.on) {
    int ty = ringSel, c = COST[ty][0];
    if (gold < c) { sfx(200, 80); return; }
    gold -= c; t = {true, (uint8_t)ty, 0, 10, 0, c}; sfx(660, 40); sfx(990, 60, false);
    for (int i = 0; i < 12; i++) addPart(LV->pads[cursor][0] + rnd(0, 20) - 10, LV->pads[cursor][1] + OY - 4, (rnd(0, 20) - 10) / 10.0f, -1.2f, i & 1 ? CGD : IX(RE, TH), 22);
  } else {
    bool up = (t.lv < 2 && ringSel == 0);
    if (up) {
      int c = COST[t.type][t.lv + 1]; if (gold < c) { sfx(200, 80); return; }
      gold -= c; t.lv++; t.invested += c; sfx(784, 60); sfx(1175, 90, false);
      for (int i = 0; i < 16; i++) { float a = i * 0.39f; addPart(LV->pads[cursor][0] + cosf(a) * 12, LV->pads[cursor][1] + OY - 10 + sinf(a) * 6, cosf(a) * 0.6f, -1.0f, CGD, 24); }
    } else { gold += t.invested * 6 / 10; addPop(LV->pads[cursor][0], LV->pads[cursor][1] - 20, t.invested * 6 / 10, 1); t.on = false; sfx(500, 60); }
  }
  ring = false;
}
static void playInput() {
  if (ring) {
    if (bP) { ringSel = (ringSel + 1) % ringCount(); sfx(1200, 12); }
    if (pP) { ring = false; sfx(600, 20); }
    if (aP) ringAction();
    return;
  }
  if (pP) { mode = M_PAUSE; sfx(440, 50); return; }
  if (bP) { cursor = (cursor + 1) % (LV->npads + 1); sfx(1100, 10); }
  if (aP) {
    if (cursor == LV->npads) {                                        // bouton >> : lancer la vague / vitesse x2
      if (!waveOn && wave < nWaves) { gold += waveT / 50; startWave(); } else fast = !fast;
      sfx(880, 30);
    } else { ring = true; ringSel = 0; sfx(1000, 20); }
  }
}

// ------------------------------------------------------------------ rendu du niveau
static void drawFoe(const Foe& f) {
  const EDef& e = EDEFS[f.type]; float x, y; int dir; posAt(f.d, x, y, dir);
  int fr = (tick / (f.type == F_GOBLIN ? 4 : 7) + (int)f.d) & 3;
  if (f.type == F_SHAMAN) fr = (f.healCd > 80) ? 2 + (tick / 6 & 1) : (tick / 10 & 1);
  int lift = e.fly ? 8 + (int)(2 * sinf(tick * 0.2f + f.d)) : 0;
  shade((int)x, (int)y + OY + 2, f.type == F_OGRE ? 14 : 6, 2);
  const Spr& s = SPR[e.spr];
  uint8_t fl = (dir < 0 ? BF_FLIP : 0) | (f.hurtT > 3 ? BF_FLASH : 0);
  int sx = (int)x - s.w / 2, sy = (int)y + OY - e.fy - lift;
  blit(e.spr + fr, sx, sy, fl);
  if (f.slowT > 0 && (tick & 2)) for (int i = 0; i < 3; i++) pset(sx + 3 + i * 5, sy + s.h - 4, IX(RB, TH));
  // barre de vie
  int bw = f.type == F_OGRE ? 22 : 12, bx = (int)x - bw / 2, by = sy - 4;
  for (int i = -1; i <= bw; i++) { pset(bx + i, by - 1, IX(RS, TO)); pset(bx + i, by + 1, IX(RS, TO)); }
  pset(bx - 1, by, IX(RS, TO)); pset(bx + bw, by, IX(RS, TO));
  int fill = (int)(bw * f.hp / f.hpMax + 0.5f);
  for (int i = 0; i < bw; i++) pset(bx + i, by, i < fill ? (f.hp / f.hpMax > 0.35f ? IX(RG, TL) : IX(RE, TH)) : IX(RT, TS));
}
struct DI { int16_t key; uint8_t kind, idx; };
static void renderLevel() {
  clipTop = OY;
  for (int r = 0; r < 7; r++) for (int c = 0; c < 15; c++) blit(LV->tiles[r][c], c * 16, OY + r * 16);
  for (int i = 0; i < LV->npads; i++) blit(PAD + ((cursor == i && !(tick & 8)) ? 1 : 0), LV->pads[i][0] - 10, LV->pads[i][1] + OY - 7);
  // portee de la tour choisie
  if (ring && cursor < LV->npads) {
    Tower& t = tw[cursor]; int ty = t.on ? t.type : ringSel, lv = t.on ? (ringSel == 0 && t.lv < 2 ? t.lv + 1 : t.lv) : 0;
    if (!t.on || ty < 4) { float r = TRNG[ty][lv] * rngMul(); for (int a = 0; a < 90; a++) if ((a + tick / 2) % 3) pset(LV->pads[cursor][0] + (int)(cosf(a * 0.0698f) * r), LV->pads[cursor][1] + OY + (int)(sinf(a * 0.0698f) * r * 0.85f), CWH); }
  }
  DI items[40]; int n = 0;
  for (int i = 0; i < LV->ndec && n < 40; i++) { const Spr& s = SPR[LV->dec[i][0]]; items[n++] = {(int16_t)((int16_t)LV->dec[i][2] + s.h), 0, (uint8_t)i}; }
  for (int i = 0; i < LV->npads && n < 40; i++) if (tw[i].on) items[n++] = {(int16_t)(LV->pads[i][1] + 4), 1, (uint8_t)i};
  for (int i = 0; i < NFOE && n < 40; i++) if (foes[i].on) { float x, y; int d; posAt(foes[i].d, x, y, d); items[n++] = {(int16_t)y, 2, (uint8_t)i}; }
  for (int i = 1; i < n; i++) { DI t = items[i]; int j = i - 1; while (j >= 0 && items[j].key > t.key) { items[j + 1] = items[j]; j--; } items[j + 1] = t; }
  int ey = LV->pts[LV->npts - 1][1];
  blit(PORTAL + (tick / 6) % 4, -6, LV->pts[0][1] + OY - 28);
  for (int i = 0; i < n; i++) {
    const DI& it = items[i];
    if (it.kind == 0) { int16_t dx = (int16_t)LV->dec[it.idx][1], dy = (int16_t)LV->dec[it.idx][2]; uint16_t id = LV->dec[it.idx][0]; if (id == VENT) id += (tick / 10) & 1; blit(id, dx, dy + OY); }
    else if (it.kind == 1) {
      const Tower& t = tw[it.idx]; uint16_t id = TOWER + (t.type * 3 + t.lv) * 2 + ((t.fireT > 0 || (t.type >= T_FROST && (tick / 12) & 1)) ? 1 : 0);
      const Spr& s = SPR[id]; blit(id, LV->pads[it.idx][0] - 12, LV->pads[it.idx][1] + OY - s.h + 4);
      for (int k = 0; k <= t.lv; k++) if (t.lv > 0) { int sx = LV->pads[it.idx][0] - 4 * t.lv + k * 8 - 2; pset(sx, LV->pads[it.idx][1] + OY + 6, CGD); pset(sx + 1, LV->pads[it.idx][1] + OY + 6, CGD); }
    } else drawFoe(foes[it.idx]);
  }
  blit(CASTLE + (tick / 20) % 2, LV->castle[0], LV->castle[1] + OY);
  (void)ey;
  // projectiles
  for (auto& p : projs) if (p.on) {
    if (p.kind == T_CANNON) { blit(FX_BALL, (int)p.x - 3, (int)p.y + OY - 3 - (int)(sinf(p.life * 0.1f) * 6)); }
    else if (p.kind == T_ARCHER) { float dx = p.tx - p.x, dy = p.ty - p.y, d = sqrtf(dx * dx + dy * dy) + 0.01f; for (int k = 0; k < 6; k++) pset((int)(p.x - dx / d * k), (int)(p.y - dy / d * k) + OY, k == 0 ? IX(RS, TH) : (k < 5 ? IX(RW, TL) : CWH)); }
    else blit(FX_FROST, (int)p.x - 4, (int)p.y + OY - 4);
  }
  for (auto& b : bolts) if (b.on) for (int s = 0; s + 1 < b.n; s++) {
    int x0 = b.xs[s], y0 = b.ys[s] + OY, x1 = b.xs[s + 1], y1 = b.ys[s + 1] + OY, m = 10;
    int px = x0, py = y0;
    for (int i = 1; i <= m; i++) { int x = x0 + (x1 - x0) * i / m + (i < m ? (int)rnd(0, 4) - 2 : 0), y = y0 + (y1 - y0) * i / m + (i < m ? (int)rnd(0, 4) - 2 : 0);
      int steps = abs(x - px) > abs(y - py) ? abs(x - px) : abs(y - py); if (steps < 1) steps = 1;
      for (int k = 0; k <= steps; k++) { int xx = px + (x - px) * k / steps, yy = py + (y - py) * k / steps; pset(xx, yy, CWH); pset(xx + 1, yy, b.t < 4 ? CGD : IX(RB, TH)); }
      px = x; py = y; }
  }
  for (auto& f : fxs) if (f.on) {
    if (f.kind == 0) { int k = f.t / 4; if (k < 4) blit(FX_BOOM + k, f.x - 15, f.y + OY - 15); }
    else if (f.kind == 1) { int k = f.t / 5; if (k < 3) blit(FX_ICE + k, f.x - 13, f.y + OY - 13); }
    else if (f.kind == 4) blit(FX_COIN + (f.t / 3) % 4, f.x - 4, f.y + OY - 10 - f.t);
  }
  for (auto& p : parts) if (p.life) pset((int)p.x, (int)p.y + OY, p.c);
  // curseur : coins clignotants autour de l'emplacement + fleche doree qui rebondit au-dessus
  if (cursor < LV->npads) {
    int cx = LV->pads[cursor][0], cy = LV->pads[cursor][1] + OY;
    uint8_t cc = (tick & 8) ? CWH : CGD;
    for (int k = 0; k < 4; k++) {
      int sx = k & 1 ? 1 : -1, sy = k & 2 ? 1 : -1, x0 = cx + sx * 13, y0 = cy + sy * 9;
      for (int i = 0; i < 4; i++) { pset(x0 - sx * i, y0, cc); pset(x0, y0 - sy * i, cc); pset(x0 - sx * i, y0 + sy, IX(RW, TO)); }
    }
    int top = tw[cursor].on ? cy - SPR[TOWER + (tw[cursor].type * 3 + tw[cursor].lv) * 2].h + 2 : cy - 10;
    if (!ring) {
      int ay = top - 11 - (int)(fabsf(sinf(tick * 0.15f)) * 4);
      for (int k = -1; k <= 6; k++) for (int j = -7; j <= 7; j++) if (abs(j) <= 6 - k) pset(cx + j, ay + k, IX(RW, TO));
      for (int k = 0; k < 5; k++) for (int j = -(4 - k); j <= 4 - k; j++) pset(cx + j, ay + k, k == 0 && j < 0 ? CWH : CGD);
    }
  }
  // menu circulaire : option choisie entouree d'un anneau dore, les autres assombries
  if (ring && cursor < LV->npads) {
    int cx = LV->pads[cursor][0], cy = LV->pads[cursor][1] + OY, nc = ringCount();
    static const int8_t OX[4] = {-34, -11, 12, 26}, OYr[4] = {-30, -44, -30, -8};
    for (int i = 0; i < nc; i++) {
      int bx = cx + OX[i], by = cy + OYr[i]; if (by < OY) by = OY; if (bx < 0) bx = 0; if (bx > 218) bx = 218;
      uint16_t id; bool sel = ringSel == i;
      if (!tw[cursor].on) id = RING + i + (sel ? 4 : 0); else id = RING + ((tw[cursor].lv < 2 && i == 0) ? 8 : 9);
      blit(id, bx, by);
      if (!sel) { shade(bx + 11, by + 11, 10, 10); shade(bx + 11, by + 11, 8, 8); }
      else {
        float pulse = 13.5f + ((tick / 4) & 1);
        for (int a = 0; a < 64; a++) { float an = a * 0.0982f; for (float r = pulse - 1; r <= pulse; r += 1) pset(bx + 11 + (int)(cosf(an) * r), by + 11 + (int)(sinf(an) * r), (a + tick / 2) % 8 < 2 ? CWH : CGD); }
        int ay = by - 9 - (int)(fabsf(sinf(tick * 0.2f)) * 3);
        for (int k = 0; k < 4; k++) for (int j = -(3 - k); j <= 3 - k; j++) pset(bx + 11 + j, ay + k, CGD);
      }
    }
  }
}
static void hudAndText() {
  blit565(HUD_BG, 0, 0);
  char b[24];
  snprintf(b, sizeof b, "%d", lives); txt(b, 17, 8, &fonts::Font0, rgb(255, 248, 232));
  snprintf(b, sizeof b, "%d", gold); txt(b, 57, 8, &fonts::Font0, rgb(255, 214, 120));
  snprintf(b, sizeof b, L("WAVE %d/%d", "VAGUE %d/%d"), wave + (waveOn ? 1 : 0) < 1 ? 1 : wave + (waveOn ? 1 : 0), nWaves); txt(b, 140, 3, &fonts::Font0, rgb(255, 248, 232), textdatum_t::top_center);
  for (int i = 0; i < nWaves; i++) canvas.fillRect(140 - nWaves * 3 + i * 6, 13, 4, 4, i < wave ? rgb(242, 184, 58) : rgb(70, 50, 40));
  bool sel = cursor == LV->npads;
  blit565(ICON_FF + (fast ? 1 : 0), 240 - 24, 6);
  if (sel) { canvas.drawRect(240 - 30, 0, 28, 22, rgb(242, 184, 58)); canvas.drawRect(240 - 29, 1, 26, 20, (tick & 8) ? rgb(255, 255, 255) : rgb(242, 184, 58)); }
  if (!waveOn && wave < nWaves) { snprintf(b, sizeof b, "%d", waveT / 50 + 1); txt(b, 240 - 34, 8, &fonts::Font0, rgb(255, 248, 232), textdatum_t::top_right); }
  // textes qui jaillissent
  for (auto& p : pops) if (p.on) {
    int y = p.y + OY - p.t / 2;
    if (p.kind == 1) { snprintf(b, sizeof b, "+%d", p.val); pop(b, p.x, y, rgb(255, 214, 90)); }
    else if (p.kind == 2) { pop("CRIT!", p.x, y - 8, rgb(255, 120, 60)); snprintf(b, sizeof b, "%d", p.val); pop(b, p.x, y, rgb(255, 255, 255)); }
    else { snprintf(b, sizeof b, "%d", p.val); pop(b, p.x, y, rgb(255, 255, 255)); }
  }
  // prix du menu circulaire
  if (ring && cursor < LV->npads) {
    int cx = LV->pads[cursor][0], cy = LV->pads[cursor][1] + OY, nc = ringCount();
    static const int8_t OX[4] = {-34, -11, 12, 26}, OYr[4] = {-30, -44, -30, -8};
    Tower& t = tw[cursor];
    for (int i = 0; i < nc; i++) {
      int bx = cx + OX[i], by = cy + OYr[i]; if (by < OY) by = OY; if (bx < 0) bx = 0; if (bx > 218) bx = 218;
      int price; bool sell = false;
      if (!t.on) price = COST[i][0]; else if (t.lv < 2 && i == 0) price = COST[t.type][t.lv + 1]; else { price = t.invested * 6 / 10; sell = true; }
      snprintf(b, sizeof b, sell ? "+%d" : "%d", price);
      pop(b, bx + 11, by + 22, (sell || gold >= price) ? rgb(255, 214, 90) : rgb(230, 80, 70));
    }
    static const char* NM_EN[4] = {"ARCHER", "CANNON", "FROST", "STORM"}, *NM_FR[4] = {"ARCHER", "CANON", "GIVRE", "ORAGE"};
    const char* nm = !t.on ? (g_lang ? NM_FR : NM_EN)[ringSel] : ((t.lv < 2 && ringSel == 0) ? L("UPGRADE", "AMELIORER") : L("SELL", "VENDRE"));
    static const char* DS_EN[4] = {"fast arrows, crits", "splash, no flyers", "slows enemies", "chain lightning"};
    static const char* DS_FR[4] = {"fleches rapides, critiques", "zone, pas les volants", "ralentit les ennemis", "eclair en chaine"};
    canvas.fillRect(0, 123, 240, 12, rgb(30, 18, 12)); canvas.drawFastHLine(0, 122, 240, rgb(242, 184, 58));
    txt(nm, 4, 125, &fonts::Font0, rgb(255, 214, 90));
    if (!t.on) txt((g_lang ? DS_FR : DS_EN)[ringSel], 236, 125, &fonts::Font0, rgb(230, 220, 200), textdatum_t::top_right);
    else if (t.lv < 2 && ringSel == 0) { snprintf(b, sizeof b, L("LV %d -> %d", "NIV %d -> %d"), t.lv + 1, t.lv + 2); txt(b, 236, 125, &fonts::Font0, rgb(230, 220, 200), textdatum_t::top_right); }
    else { snprintf(b, sizeof b, L("+%d gold", "+%d or"), t.invested * 6 / 10); txt(b, 236, 125, &fonts::Font0, rgb(230, 220, 200), textdatum_t::top_right); }
  } else if (cursor == LV->npads) {
    canvas.fillRect(0, 123, 240, 12, rgb(30, 18, 12)); canvas.drawFastHLine(0, 122, 240, rgb(242, 184, 58));
    txt(!waveOn && wave < nWaves ? L("A: start next wave now (+gold)", "A : lancer la vague (+or)") : (fast ? L("A: normal speed", "A : vitesse normale") : L("A: speed x2", "A : vitesse x2")), 120, 125, &fonts::Font0, rgb(255, 214, 90), textdatum_t::top_center);
  }
}

// ------------------------------------------------------------------ carte du monde + boutique
static const int16_t NODES[12][2] = {{18, 98}, {34, 62}, {22, 30}, {78, 30}, {96, 66}, {84, 100}, {138, 98}, {156, 64}, {142, 30}, {198, 30}, {216, 66}, {204, 100}};
static void renderMap() {
  clipTop = 0;
  static const uint16_t TB[4] = {TILES_MEADOW, TILES_DESERT, TILES_SNOW, TILES_VOLCANO};
  static const uint8_t BY[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  auto worldAt = [&](int X, int Y) {                                   // monde du pixel, fondu sur 40 px a chaque frontiere
    int w = X / 60; if (w > 3) w = 3;
    for (int k = 1; k < 4; k++) { int b = k * 60; if (X > b - 20 && X < b + 20) {
      float t = (X - (b - 20)) / 40.0f + 0.18f * sinf(Y * 0.19f + k * 2.1f) + 0.08f * sinf(Y * 0.53f + k);
      return t * 16 > BY[Y & 3][X & 3] + 0.5f ? k : k - 1; } }
    return w;
  };
  for (int w = 0; w < 4; w++) for (int r = 0; r < 9; r++) for (int c = -2; c < 6; c++) {
    int tx = w * 60 + c * 16, ty = r * 16 - 4; uint16_t id = TB[w] + ((r + c + 8) & 3);
    if (tx > 240 || tx + 16 < 0) continue;
    forPix(id, [&](int i, int j, uint8_t col) { int X = tx + i, Y = ty + j; if ((unsigned)X < 240 && (unsigned)Y < 135 && worldAt(X, Y) == w) fb[Y * 240 + X] = col; });
  }
  for (int i = 0; i + 1 < 12; i++) {
    int x0 = NODES[i][0], y0 = NODES[i][1], x1 = NODES[i + 1][0], y1 = NODES[i + 1][1], n = (int)(sqrtf((float)((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0))) / 4);
    for (int k = 0; k < n; k += 2) { int x = x0 + (x1 - x0) * k / n, y = y0 + (y1 - y0) * k / n; pset(x, y, IX(RW, TO)); pset(x + 1, y, IX(RW, TS)); }
  }
  for (int w = 0; w < 4; w++) { const Level& l = LEVELS[w * 3]; for (int d = 0; d < 2 && d < l.ndec; d++) blit(l.dec[d][0], w * 60 + 22 + d * 14, d ? 74 : 42); }
  for (int i = 0; i < 12; i++) {
    int st = !unlocked(i) ? 0 : (stars[i] ? 1 + stars[i] : 1);
    blit(NODE + st, NODES[i][0] - 9, NODES[i][1] - 9);
    if (stars[i]) for (int k = 0; k < 3; k++) blit(ICON_STAR + (k < stars[i] ? 1 : 0), NODES[i][0] - 14 + k * 9, NODES[i][1] + 9);
  }
  if (mapSel < 12) { int x = NODES[mapSel][0], y = NODES[mapSel][1] - 22 - (int)(fabsf(sinf(tick * 0.12f)) * 4);
    for (int k = 0; k < 5; k++) for (int j = 0; j <= k; j++) { pset(x - j, y + k, CGD); pset(x + j, y + k, CGD); }
    for (int a = 0; a < 40; a++) if ((a + tick / 3) % 4 < 2) pset(NODES[mapSel][0] + (int)(cosf(a * 0.157f) * 12), NODES[mapSel][1] + (int)(sinf(a * 0.157f) * 12), CWH); }
  convert(0);
  uint16_t* out = (uint16_t*)canvas.getBuffer(); (void)out;
  canvas.fillRect(0, 0, 240, 13, rgb(40, 24, 16)); canvas.fillRect(0, 12, 240, 2, rgb(242, 184, 58));
  txt(L("POCKET SIEGE", "POCKET SIEGE"), 4, 3, &fonts::Font0, rgb(255, 248, 232));
  int tot = 0; for (auto s : stars) tot += s;
  char b[32]; snprintf(b, sizeof b, "%d/36", tot); blit565(ICON_STAR + 1, 96, 2); txt(b, 108, 3, &fonts::Font0, rgb(255, 214, 120));
  blit565(FX_COIN, 150, 2); snprintf(b, sizeof b, "%d", coins); txt(b, 162, 3, &fonts::Font0, rgb(255, 214, 120));
  bool shopSel_ = mapSel == 12;
  canvas.fillRect(196, 1, 42, 11, shopSel_ && (tick & 8) ? rgb(255, 214, 90) : rgb(120, 70, 30)); txt(L("SHOP", "BOUTIQUE"), 217, 3, &fonts::Font0, shopSel_ ? rgb(40, 24, 16) : rgb(255, 248, 232), textdatum_t::top_center);
  static const char* WN_EN[4] = {"MEADOW", "DESERT", "SNOW", "VOLCANO"}, *WN_FR[4] = {"PRAIRIE", "DESERT", "NEIGE", "VOLCAN"};
  canvas.fillRect(0, 124, 240, 11, rgb(40, 24, 16));
  if (mapSel < 12) { snprintf(b, sizeof b, "%s %d-%d", (g_lang ? WN_FR : WN_EN)[mapSel / 3], mapSel / 3 + 1, mapSel % 3 + 1); txt(b, 4, 126, &fonts::Font0, rgb(255, 248, 232)); }
  txt(L("B:next  A:play", "B:suivant  A:jouer"), 236, 126, &fonts::Font0, rgb(200, 180, 150), textdatum_t::top_right);
}
static void renderShop() {
  canvas.fillScreen(rgb(120, 70, 30));
  for (int y = 0; y < 135; y += 3) canvas.drawFastHLine(0, y, 240, rgb(100, 56, 24));
  canvas.fillRect(6, 18, 228, 110, rgb(239, 223, 196)); canvas.drawRect(5, 17, 230, 112, rgb(52, 25, 15));
  txt(L("UPGRADES", "AMELIORATIONS"), 8, 3, &fonts::Font2, rgb(255, 248, 232));
  char b[48]; blit565(FX_COIN, 176, 5); snprintf(b, sizeof b, "%d", coins); txt(b, 188, 6, &fonts::Font0, rgb(255, 214, 120));
  static const char* N_EN[NUPG] = {"DAMAGE", "RANGE", "GOLD", "LIVES", "SPEED"}, *N_FR[NUPG] = {"DEGATS", "PORTEE", "OR", "VIES", "CADENCE"};
  static const char* D_EN[NUPG] = {"+10% tower damage", "+6% tower range", "+25 starting gold", "+3 lives", "+6% fire rate"};
  static const char* D_FR[NUPG] = {"+10% degats des tours", "+6% portee des tours", "+25 or de depart", "+3 vies", "+6% cadence de tir"};
  for (int i = 0; i < NUPG; i++) {
    int x = 10 + i * 45; bool s = i == shopSel;
    if (s) canvas.drawRect(x - 2, 20, 44, 82, (tick & 8) ? rgb(242, 184, 58) : rgb(180, 120, 40));
    blit565(SHOP + i, x + 10, 24);
    for (int j = 0; j < 5; j++) canvas.fillRect(x + 3 + j * 7, 48, 5, 4, j < upg[i] ? rgb(242, 184, 58) : rgb(140, 140, 160));
    txt((g_lang ? N_FR : N_EN)[i], x + 20, 58, &fonts::Font0, rgb(52, 34, 26), textdatum_t::top_center);
    if (upg[i] < 5) { snprintf(b, sizeof b, "%d", upgCost(i)); txt(b, x + 20, 72, &fonts::Font0, coins >= upgCost(i) ? rgb(160, 90, 20) : rgb(200, 60, 50), textdatum_t::top_center); }
    else txt("MAX", x + 20, 72, &fonts::Font0, rgb(60, 140, 60), textdatum_t::top_center);
  }
  txt((g_lang ? D_FR : D_EN)[shopSel], 12, 106, &fonts::Font0, rgb(52, 34, 26));
  txt(L("A:buy  B:next  PWR:back", "A:acheter  B:suivant  PWR:retour"), 12, 117, &fonts::Font0, rgb(120, 90, 60));
}

// ------------------------------------------------------------------ boucle
static void step() {
  switch (mode) {
    case M_MAP:
      tick++;
      if (bP) { do { mapSel = (mapSel + 1) % 13; } while (mapSel < 12 && !unlocked(mapSel)); sfx(1100, 10); }
      if (pP) { do { mapSel = (mapSel + 12) % 13; } while (mapSel < 12 && !unlocked(mapSel)); sfx(1000, 10); }
      if (aP) { if (mapSel == 12) { mode = M_SHOP; shopSel = 0; } else startLevel(mapSel); sfx(880, 40); sfx(1320, 60, false); }
      break;
    case M_SHOP:
      tick++;
      if (bP) { shopSel = (shopSel + 1) % NUPG; sfx(1100, 10); }
      if (pP) { mode = M_MAP; sfx(600, 20); }
      if (aP) { if (upg[shopSel] < 5 && coins >= upgCost(shopSel)) { coins -= upgCost(shopSel); upg[shopSel]++; save(); sfx(784, 60); sfx(1175, 90, false); } else sfx(200, 80); }
      break;
    case M_PLAY: playInput(); if (mode == M_PLAY) { playStep(); if (fast && mode == M_PLAY) playStep(); } break;
    case M_PAUSE: tick++; if (pP || aP) mode = M_PLAY; if (bP) { mode = M_MAP; sfx(500, 60); } break;
    case M_WIN: case M_LOSE:
      tick++; endT++;
      for (auto& p : parts) if (p.life) { p.life--; p.x += p.vx; p.y += p.vy; p.vy += 0.08f; }
      if (mode == M_WIN && endT % 6 == 0 && endT < 90) addPart(rnd(60, 180), 30, (rnd(0, 20) - 10) / 10.0f, -1.5f, rnd(0, 1) ? CGD : IX(RT, TH), 40);
      if (aP && endT > 40) { mode = M_MAP; if (mode == M_MAP && lvl + 1 < 12 && unlocked(lvl + 1)) mapSel = lvl + 1; }
      break;
  }
}
void begin() {
  fb = idxBuffer(); if (fb) memset(fb, CBK, 240 * 135);
  canvas.drawPixel(0, 0, 0x1234); swap16 = ((uint16_t*)canvas.getBuffer())[0] != 0x1234;
  loadSave();
#ifdef ZTEST
  if (getenv("ZCOINS")) coins = atoi(getenv("ZCOINS"));
#endif
  mode = M_MAP; mapSel = 0; for (int i = 11; i >= 0; i--) if (unlocked(i)) { mapSel = i; break; }
  last = 0; acc = 0; tick = 0; aPend = bPend = pPend = false; LV = &LEVELS[0];
}
void loop() {
  if (!fb) { canvas.fillScreen(0); txt("NO MEMORY", 120, 60, &fonts::Font2, 0xFFFF, textdatum_t::top_center); present(0, 0); return; }
  uint32_t now = micros(); if (!last) last = now;
  acc += now - last; last = now; if (acc > 100000) acc = 100000;
  aPend |= M5.BtnA.wasPressed(); bPend |= M5.BtnB.wasPressed(); pPend |= M5.BtnPWR.wasPressed();
  while (acc >= STEP_US) { aP = aPend; bP = bPend; pP = pPend; aPend = bPend = pPend = false; step(); acc -= STEP_US; }
  if (mode == M_MAP) renderMap();
  else if (mode == M_SHOP) renderShop();
  else {
    renderLevel(); convert(); hudAndText();
    char b[48];
    if (mode == M_PAUSE) { ui::pause(); txt(L("B: quit to map", "B : retour a la carte"), 120, 112, &fonts::Font0, P_K, textdatum_t::top_center); }
    else if (mode == M_WIN || mode == M_LOSE) {
      canvas.fillRect(40, 34, 160, 70, rgb(30, 20, 40)); canvas.drawRect(40, 34, 160, 70, rgb(242, 184, 58)); canvas.drawRect(41, 35, 158, 68, rgb(120, 80, 30));
      txtO(mode == M_WIN ? L("VICTORY!", "VICTOIRE !") : L("DEFEAT", "DEFAITE"), 120, 40, &fonts::Font2, mode == M_WIN ? rgb(255, 214, 90) : rgb(240, 90, 70), rgb(30, 16, 10), textdatum_t::top_center);
      if (mode == M_WIN) for (int k = 0; k < 3; k++) { bool on = k < winStars && endT > 10 + k * 12; uint16_t id = ICON_STAR + (on ? 1 : 0); const Spr& s = SPR[id];
        uint16_t* out = (uint16_t*)canvas.getBuffer(); int x0 = 96 + k * 18, y0 = 60;
        forPix(id, [&](int i, int j, uint8_t c) { for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) { int X = x0 + i * 2 + dx, Y = y0 + j * 2 + dy; if ((unsigned)X < 240 && (unsigned)Y < 135) out[Y * 240 + X] = sw(PAL[c]); } }); (void)s; }
      else { snprintf(b, sizeof b, L("Wave %d/%d", "Vague %d/%d"), wave + 1, nWaves); txt(b, 120, 64, &fonts::Font0, rgb(255, 248, 232), textdatum_t::top_center); }
      blit565(FX_COIN, 96, 84); snprintf(b, sizeof b, "+%d", reward); txt(b, 108, 85, &fonts::Font0, rgb(255, 214, 90));
      if (endT > 40 && (tick & 16)) txt(L("A: continue", "A : continuer"), 120, 96, &fonts::Font0, rgb(200, 180, 150), textdatum_t::top_center);
    }
  }
  bgm::run(mode == M_MAP || mode == M_SHOP ? 15 : 11, mode != M_PAUSE);
  present(0, 0);
}
void icon(int x, int y) {
  ui::tile(x, y, rgb(70, 140, 60));
  auto big = [&](uint16_t id, int ox, int oy, int sc) { forPix(id, [&](int i, int j, uint8_t c) { canvas.fillRect(x + ox + i * sc, y + oy + j * sc, sc, sc, PAL[c]); }); };
  big(TOWER + (0 * 3 + 2) * 2, 2, 2, 1); big(TOWER + (3 * 3 + 2) * 2, 26, 6, 1); big(E_SLIME, 40, 44, 1); big(E_GOBLIN, 22, 42, 1);
}
}  // namespace td
