// ============================================================================
//  RIVER - descends la riviere en kayak : penche le M5 pour pagayer a droite / a gauche.
//  Evite les rochers et les troncs, ramasse les pieces. HAUT = pause.
//  Les obstacles sont poses AUTOUR d'un "couloir" libre qui suit la riviere : on peut toujours passer.
// ============================================================================
#include "common.h"

namespace river {

static const uint32_t STEP_US = 20000;
static const int SEGH = 3, PRE = 8, NSEG = 48 + PRE + 2, BY = 100;   // PRE = segments caches au-dessus de l'ecran (la ou naissent les obstacles)
static const float HW = 17.0f;          // demi-largeur LIBRE du couloir (centre du kayak) : kayak ~12 px => couloir ~2,8x sa largeur
static const float SPAWN_Y = -22.0f;
enum State { TITLE, PLAY, PAUSED, OVER };
static State state = TITLE;
static Preferences pr;
static uint32_t last = 0, acc = 0;

struct Seg { float cx, w; uint8_t dl, dr, ol, orr; };         // decor gauche / droite : type + decalage
static Seg seg[NSEG];
static float curDir = 0, curW = 104, tgtW = 104;
struct Obj { float x, y, a, b; uint8_t type; bool on; };       // 0 rocher (a = rayon), 1 tronc (a = demi-longueur, b = angle), 2 piece
static Obj objs[16];
struct Row { float y, cx; };                                    // rangees d'obstacles : position + centre du couloir libre
static Row rows[6]; static int nRows = 0;
struct Pop { float x, y; int t; char s[10]; uint16_t col; }; static Pop pops[3];
struct Part { float x, y, vx, vy; int t; }; static Part parts[48];

static float bx = 120, bv = 0, tf = 0, spd = 1.8f, dist = 0, off = 0, spawnAcc = 0;
static float corrPrev = -1000;
static int lives = 3, invul = 0, endT = 0, animT = 0, steps = 0, shake = 0, rowN = 0, coins = 0;
static uint32_t score = 0, best = 0, bonus = 0;

static void pop(float x, float y, const char* s, uint16_t col) { for (auto &p : pops) if (p.t <= 0) { p.x = x; p.y = y; p.t = 36; p.col = col; strncpy(p.s, s, 9); p.s[9] = 0; return; } }
static void splash(float x, float y, int n = 6) {
  for (auto &p : parts) { if (n <= 0) break; if (p.t > 0) continue; p = {x + ((int)rnd(0, 6) - 3), y, ((int)rnd(0, 20) - 10) * 0.1f, -0.8f - (int)rnd(0, 14) * 0.1f, 14 + (int)rnd(0, 8)}; n--; }
}

static float cxPrev = 120;
static void makeSeg(Seg &s) {
  curDir += ((int)rnd(0, 100) - 50) * 0.0035f - (cxPrev - 120) * 0.0012f;
  curDir = clampf(curDir, -0.9f, 0.9f);
  cxPrev += curDir * 1.6f;
  float lim = curW / 2 + 22;
  cxPrev = clampf(cxPrev, lim, SCR_W - lim);
  curW += (tgtW - curW) * 0.02f;
  s.cx = cxPrev; s.w = curW;
  s.dl = rnd(0, 100) < 14 ? (uint8_t)rnd(1, 4) : 0; s.dr = rnd(0, 100) < 14 ? (uint8_t)rnd(1, 4) : 0;
  s.ol = rnd(6, 26); s.orr = rnd(6, 26);
}
static void reset() {
  bx = 120; bv = 0; tf = 0; spd = 1.8f * dif(0.9f, 1.0f, 1.1f); dist = 0; off = 0; spawnAcc = 30; lives = difi(5, 3, 2); invul = 0; endT = 0; steps = 0; score = 0; bonus = 0; shake = 0; rowN = 0; coins = 0; nRows = 0; corrPrev = -1000;
  curDir = 0; curW = tgtW = 104; cxPrev = 120;
  for (auto &s : seg) makeSeg(s);
  for (auto &o : objs) o.on = false;
  for (auto &p : pops) p.t = 0;
  for (auto &p : parts) p.t = 0;
}
void begin() {
  pr.begin(dns("river"), false); best = pr.getInt("best", 0);
  tilt::load();
  reset(); state = TITLE; last = 0; acc = 0;
}
static void gameOver() {
  state = OVER; endT = 0;
  trophy::record(trophy::T_RIVER, score); if (score > best) { best = score; pr.putInt("best", best); }
  sfx(260, 120); sfx(110, 320, false);
}

// bords de la riviere a la hauteur y (ecran)
static void edges(float y, float &l, float &r, float &c) {
  float f = (y + SEGH * (PRE + 1) - off) / SEGH; int i = (int)f; if (i < 0) i = 0; if (i > NSEG - 2) i = NSEG - 2; float t = f - i; if (t < 0) t = 0; if (t > 1) t = 1;
  c = seg[i].cx + (seg[i + 1].cx - seg[i].cx) * t; float w = seg[i].w + (seg[i + 1].w - seg[i].w) * t;
  l = c - w / 2; r = c + w / 2;
}

// ----------------------------------------------------------------------------- generation des obstacles
static void put(float x, float y, float a, float b, uint8_t type) { for (auto &o : objs) if (!o.on) { o = {x, y, a, b, type, true}; return; } }

static bool placeRock(int sgn, float cx, float l, float r) {
  float rad = (float)rnd(6, 10);
  float minX = cx + sgn * (HW + 3.5f + rad + 1.0f);                       // le bord du rocher touche presque le couloir
  float room = sgn > 0 ? (r - rad * 0.6f) - minX : minX - (l + rad * 0.6f);
  if (room < 0) return false;
  float ex = (float)rnd(0, 100) / 100.f * (room < 14 ? room : 14);
  put(minX + sgn * ex, SPAWN_Y, rad, 0, 0);
  return true;
}
static bool placeLog(int sgn, float cx, float l, float r) {
  float a = (float)rnd(13, 18), ang = ((int)rnd(0, 100) - 50) * 0.0105f;
  float x = cx + sgn * (HW + 7.5f + 2.0f + a * fabsf(cosf(ang)));       // l'extremite la plus proche reste hors du couloir
  if (x < l + 8 || x > r - 8) return false;
  put(x, SPAWN_Y, a, ang, 1);
  return true;
}
static void spawnRow() {
  float l, r, c; edges(SPAWN_Y, l, r, c);
  float lo = l + 8 + HW, hi = r - 8 - HW; if (hi < lo) lo = hi = (l + r) * 0.5f;
  // couloir libre : suit la riviere + decalage lent (slalom). Deplacement limite d'une rangee a l'autre : les couloirs successifs se recouvrent toujours.
  float want = c + sinf(rowN * 0.85f + 1.3f) * (hi - lo) * 0.45f + ((int)rnd(0, 60) - 30) * 0.1f;
  float cx = clampf(want, lo, hi);
  if (corrPrev > -500) { cx = clampf(cx, corrPrev - 26, corrPrev + 26); cx = clampf(cx, lo, hi); }
  corrPrev = cx; rowN++;
  for (int i = 5; i > 0; i--) rows[i] = rows[i - 1];
  rows[0] = {SPAWN_Y, cx}; if (nRows < 6) nRows++;
  int k = rnd(0, 99);
  static int side = 1; side = -side;                                      // cotes alternes
  if (rowN % 5 == 0) {                                                    // pieces dans le couloir
    for (int i = 0; i < 3; i++) put(cx + sinf(i * 1.3f) * 6.f, SPAWN_Y - i * 16.f, 5, 0, 2);
  } else if (k < 16) {
    // rangee vide : on respire
  } else if (k < 66) {
    bool a = placeRock(side, cx, l, r);
    if (rnd(0, 99) < 35) placeRock(-side, cx, l, r);
    if (!a) placeRock(-side, cx, l, r);
  } else {
    if (!placeLog(side, cx, l, r)) placeLog(-side, cx, l, r);
  }
}

static void step() {
  animT++;
  for (auto &p : pops) if (p.t > 0) { p.t--; p.y -= 0.5f; }
  if (shake > 0) shake--;
  if (state == PAUSED) return;
  float sp = state == PLAY ? spd : (state == TITLE ? 1.2f : spd * 0.97f);
  if (state == OVER) { endT++; spd *= 0.97f; sp = spd; }
  for (auto &p : parts) if (p.t > 0) { p.t--; p.x += p.vx; p.y += p.vy + sp * 0.8f; p.vy += 0.14f; }
  off += sp; dist += sp;
  while (off >= SEGH) { off -= SEGH; memmove(&seg[1], &seg[0], sizeof(Seg) * (NSEG - 1)); makeSeg(seg[0]); }
  for (auto &o : objs) if (o.on) { o.y += sp; if (o.y > SCR_H + 30) o.on = false; }
  for (int i = 0; i < nRows; i++) rows[i].y += sp;
  if (state != PLAY) { if (state == TITLE) { float l, r, c; edges(BY, l, r, c); bx += (c - bx) * 0.05f; } return; }
  steps++;
  tilt::update();
  float t = ctrl::x(); if (fabsf(t) < 0.06f) t = 0; else t = t > 0 ? t - 0.06f : t + 0.06f;
  tf += (t - tf) * 0.4f;                                                  // entree lissee
  bv += (tf * 4.0f - bv) * 0.18f; bx += bv;
  float l, r, c; edges(BY, l, r, c);
  if (bx < l + 8) { bx = l + 8; bv = fabsf(bv) * 0.3f; spd *= 0.995f; if ((animT & 3) == 0) splash(bx - 6, BY, 2); }
  if (bx > r - 8) { bx = r - 8; bv = -fabsf(bv) * 0.3f; spd *= 0.995f; if ((animT & 3) == 0) splash(bx + 6, BY, 2); }
  if (animT % 29 == 7) splash(bx - 12, BY - 2, 2);
  if (animT % 29 == 21) splash(bx + 12, BY - 2, 2);   // la pagaie plonge
  float dm_ = dif(0.9f, 1.0f, 1.1f); float base = (1.8f + steps * 0.0007f) * dm_; if (base > 3.9f * dm_) base = 3.9f * dm_;
  spd += (base - spd) * 0.03f;
  tgtW = 104 - fminf(32.f, steps / 70.f);
  score = (uint32_t)(dist / 10) + bonus;
  if (invul > 0) invul--;
  spawnAcc += spd;
  float gap = 56 + spd * 6;                                               // rangees plus espacees quand ca va vite
  if (spawnAcc >= gap) { spawnAcc -= gap; spawnRow(); }
  for (auto &o : objs) if (o.on) {
    bool hit = false;
    for (int k = -1; k <= 1; k++) {
      float py = BY + k * 8, dx, dy;
      if (o.type == 1) {                              // tronc = capsule
        float ca = cosf(o.b), sa = sinf(o.b), x1 = o.x - ca * o.a, y1 = o.y - sa * o.a, x2 = o.x + ca * o.a, y2 = o.y + sa * o.a;
        float vx = x2 - x1, vy = y2 - y1, u = ((bx - x1) * vx + (py - y1) * vy) / (vx * vx + vy * vy); u = clampf(u, 0, 1);
        dx = bx - (x1 + vx * u); dy = py - (y1 + vy * u);
        if (dx * dx + dy * dy < 7.5f * 7.5f) hit = true;
      } else {
        dx = bx - o.x; dy = py - o.y; float rr = o.a + (o.type == 2 ? 6.0f : 3.5f);
        if (dx * dx + dy * dy < rr * rr) hit = true;
      }
    }
    if (!hit) continue;
    if (o.type == 2) { o.on = false; bonus += 10; coins++; pop(bx, BY - 26, "+10", rgb(255, 230, 90)); sfx(1400, 30); sfx(1900, 50, false); continue; }
    if (invul) continue;
    o.on = false; splash(bx, BY - 8, 10);
    if (--lives <= 0) { gameOver(); return; }
    invul = 90; shake = 12; spd *= 0.6f; pop(bx, BY - 26, L("OUCH!","AIE !"), rgb(255, 90, 60)); sfx(160, 100); sfx(100, 160, false);
  }
}

// --------------------------------------------------------------------------- dessin
static uint32_t hash(uint32_t v) { v *= 2654435761u; v ^= v >> 15; v *= 2246822519u; return v ^ (v >> 13); }

static void drawTree(int x, int y, bool pine) {
  canvas.fillEllipse(x + 4, y + 8, 9, 3, rgb(34, 96, 48));                                      // ombre
  if (pine) {
    canvas.fillRect(x - 1, y + 3, 3, 6, rgb(92, 62, 36));
    for (int k = 0; k < 3; k++) { int w = 9 - k * 2, yy = y + 4 - k * 6; canvas.fillTriangle(x - w, yy, x + w, yy, x, yy - 9, k == 0 ? rgb(22, 84, 44) : (k == 1 ? rgb(28, 100, 52) : rgb(38, 118, 62))); canvas.drawLine(x, yy - 9, x - w, yy, rgb(70, 150, 90)); }
  } else {
    canvas.fillRect(x - 1, y + 3, 3, 6, rgb(92, 62, 36));
    canvas.fillCircle(x + 2, y + 2, 8, rgb(22, 80, 40)); canvas.fillCircle(x, y, 8, rgb(30, 110, 50));
    canvas.fillCircle(x - 3, y - 3, 5, rgb(52, 148, 72)); canvas.fillCircle(x - 4, y - 4, 2, rgb(120, 200, 116)); canvas.drawPixel(x + 3, y - 1, rgb(60, 130, 60));
  }
}
static void drawBush(int x, int y) {
  canvas.fillEllipse(x + 2, y + 4, 6, 2, rgb(34, 96, 48));
  canvas.fillCircle(x, y, 5, rgb(40, 125, 56)); canvas.fillCircle(x - 2, y - 1, 3, rgb(70, 160, 80));
  canvas.fillCircle(x + 3, y - 2, 1, rgb(230, 90, 120)); canvas.drawPixel(x - 3, y + 1, rgb(250, 220, 90)); canvas.drawPixel(x + 1, y + 2, rgb(230, 90, 120));
}
static void drawBoulder(int x, int y) { canvas.fillEllipse(x + 1, y + 2, 7, 5, rgb(60, 80, 58)); canvas.fillEllipse(x, y, 6, 4, rgb(125, 130, 135)); canvas.fillEllipse(x - 1, y - 1, 3, 2, rgb(170, 175, 180)); canvas.drawPixel(x + 3, y + 1, rgb(90, 94, 100)); }
static void drawReeds(int x, int y) {
  for (int i = -3; i <= 3; i += 2) { int sw = (int)(sinf(animT * 0.08f + i) * 1.2f); canvas.drawLine(x + i, y + 4, x + i + (i > 0 ? 1 : -1) + sw, y - 6, rgb(90, 150, 60)); }
  canvas.fillRect(x - 1, y - 8, 2, 4, rgb(120, 80, 40)); canvas.fillRect(x + 2, y - 6, 2, 3, rgb(120, 80, 40));
}

static void drawRock(int x, int y, int r) {
  int f = (animT / 6) & 1;
  for (int k = 1; k <= 4; k++) canvas.drawPixel(x - r + (k & 1) * 2, y + r + 2 + k * 2, rgb(150, 205, 240)), canvas.drawPixel(x + r - (k & 1) * 2, y + r + 2 + k * 2, rgb(150, 205, 240));   // sillage
  canvas.fillEllipse(x, y + 1, r + 5 + f, r + 4, rgb(120, 180, 225));                           // ecume : deux anneaux
  canvas.fillEllipse(x, y + 1, r + 3 + f, r + 2, rgb(190, 225, 248));
  canvas.fillEllipse(x + 1, y + 2, r, r - 1, rgb(36, 56, 72));
  canvas.fillCircle(x, y, r, rgb(108, 114, 122)); canvas.fillCircle(x - r / 3, y - r / 3, r / 2, rgb(168, 175, 184)); canvas.fillCircle(x + r / 3, y + r / 3, r / 3, rgb(84, 90, 98));
  canvas.drawPixel(x - r / 2, y - r / 2, rgb(225, 230, 235)); canvas.drawFastHLine(x - r / 3, y + r / 4, r / 2, rgb(70, 140, 80));       // reflet + mousse
}
static void drawLog(const Obj &o) {
  float ca = cosf(o.b), sa = sinf(o.b);
  int x1 = (int)(o.x - ca * o.a), y1 = (int)(o.y - sa * o.a), x2 = (int)(o.x + ca * o.a), y2 = (int)(o.y + sa * o.a);
  canvas.drawLine(x1 - 1, y1 + 5, x2 - 1, y2 + 5, rgb(30, 70, 120));                             // ombre
  for (int k = -3; k <= 3; k++) canvas.drawLine(x1 + k, y1, x2 + k, y2, k == -3 || k == 3 ? rgb(70, 44, 24) : (k < 0 ? rgb(150, 100, 56) : (k == 0 ? rgb(175, 122, 70) : rgb(112, 72, 40))));
  for (int s = 1; s < 5; s++) { int bx2 = x1 + (x2 - x1) * s / 5, by2 = y1 + (y2 - y1) * s / 5; canvas.drawFastHLine(bx2 - 3, by2, 7, rgb(96, 62, 34)); }  // ecorce
  canvas.fillCircle(x2, y2, 4, rgb(205, 165, 105)); canvas.drawCircle(x2, y2, 2, rgb(150, 105, 62)); canvas.drawCircle(x2, y2, 4, rgb(110, 74, 40)); canvas.fillCircle(x1, y1, 4, rgb(100, 66, 36));
  canvas.fillTriangle((x1 + x2) / 2, (y1 + y2) / 2 - 3, (x1 + x2) / 2 + 4, (y1 + y2) / 2 - 8, (x1 + x2) / 2 + 2, (y1 + y2) / 2 - 2, rgb(90, 60, 34));   // petite branche
  canvas.drawLine(x1 - 3, y1 + 6, x2 - 3, y2 + 6, rgb(190, 225, 248));                           // ecume sous le tronc
}
static void drawCoin(int x, int y) {
  int bob = (int)(sinf(animT * 0.2f + x) * 1.5f);
  int w = (int)(fabsf(cosf(animT * 0.15f + y * 0.1f)) * 5) + 1;                                   // la piece tourne
  canvas.fillEllipse(x, y + bob, w + 1, 5, rgb(180, 120, 20)); canvas.fillEllipse(x, y + bob, w, 4, rgb(255, 205, 50));
  if (w > 3) { canvas.drawEllipse(x, y + bob, w - 2, 2, rgb(220, 150, 30)); canvas.drawPixel(x - 2, y + bob - 2, TFT_WHITE); }
  if (((animT + x) & 31) < 3) { canvas.drawPixel(x + 6, y + bob - 5, TFT_WHITE); canvas.drawFastVLine(x + 6, y + bob - 7, 5, rgb(255, 250, 200)); canvas.drawFastHLine(x + 4, y + bob - 5, 5, rgb(255, 250, 200)); }
}
static void drawKayak(int x, int y, float lean) {
  int l = (int)(lean * 2);
  // sillage en V + vague d'etrave
  for (int k = 1; k < 8; k++) { int w = 3 + k * 2, yy = y + 10 + k * 4; uint16_t c = k < 4 ? rgb(225, 242, 252) : rgb(170, 210, 240); canvas.drawFastHLine(x - w - l - 1, yy, 2, c); canvas.drawFastHLine(x + w - l, yy, 2, c); }
  canvas.drawFastHLine(x - 5 - l, y - 15, 3, rgb(230, 245, 255)); canvas.drawFastHLine(x + 3 - l, y - 15, 3, rgb(230, 245, 255));
  canvas.fillEllipse(x + 1, y + 3, 6, 13, rgb(28, 64, 112));                                  // ombre
  canvas.fillEllipse(x, y, 5, 13, skin::c(1, rgb(214, 96, 24)));                                          // coque (flanc sombre)
  canvas.fillEllipse(x - 1, y, 4, 12, skin::c(0, rgb(245, 126, 32)));
  canvas.fillTriangle(x - 3, y - 8, x + 3, y - 8, x, y - 17, skin::c(0, rgb(245, 126, 32)));              // proue
  canvas.fillTriangle(x - 3, y + 8, x + 3, y + 8, x, y + 16, skin::c(1, rgb(214, 96, 24)));               // poupe
  canvas.drawLine(x, y - 17, x, y + 16, skin::c(3, rgb(255, 206, 100)));                                  // liston
  canvas.fillEllipse(x, y - 1, 4, 6, rgb(38, 40, 52));                                        // cockpit
  // pagayeur : le bras et la pagaie suivent le rythme de la nage
  float ph = sinf(animT * 0.22f) * 0.5f;
  int pdx = (int)(cosf(ph) * 12), pdy = (int)(sinf(ph) * 12);
  int lx = x - pdx, ly = y - 3 - pdy, rx = x + pdx, ry = y - 3 + pdy;
  canvas.drawLine(lx, ly, rx, ry, rgb(214, 214, 220));
  canvas.fillEllipse(lx, ly, 2, 3, rgb(56, 78, 112)); canvas.fillEllipse(rx, ry, 2, 3, rgb(56, 78, 112));
  if (pdy > 6) canvas.drawFastHLine(rx - 3, ry + 4, 7, rgb(220, 240, 252));                    // pagaie qui plonge
  if (pdy < -6) canvas.drawFastHLine(lx - 3, ly + 4, 7, rgb(220, 240, 252));
  canvas.fillEllipse(x, y - 1, 4, 3, rgb(226, 58, 58));                                       // gilet
  canvas.drawLine(x - 3, y - 2, x - pdx / 2, y - 3 - pdy / 2, rgb(240, 200, 160)); canvas.drawLine(x + 3, y - 2, x + pdx / 2, y - 3 + pdy / 2, rgb(240, 200, 160));   // bras
  canvas.fillCircle(x, y - 4, 3, rgb(250, 215, 60)); canvas.fillRect(x - 3, y - 4, 6, 1, rgb(60, 60, 70)); canvas.drawPixel(x - 1, y - 6, rgb(255, 245, 170));   // casque
}

static void render() {
  int ox = shake > 0 ? (int)rnd(0, 2) - 1 : 0;
  const uint16_t g1 = rgb(48, 128, 60), g2 = rgb(42, 116, 54), g3 = rgb(70, 158, 78), gd = rgb(34, 100, 46);
  for (int y = 0; y < SCR_H; y++) {
    float l, r, c; edges(y, l, r, c);
    int li = (int)l, ri = (int)r;
    uint32_t wr = (uint32_t)((int)dist - y + 100000);
    canvas.drawFastHLine(0, y, SCR_W, ((y + (int)off) / 10) & 1 ? g2 : g1);                       // herbe rayee
    uint32_t gh = hash(wr * 7 + 1);
    for (int k = 0; k < 3; k++) { int gx = (int)((gh >> (k * 8)) % SCR_W); canvas.drawPixel(gx, y, (k & 1) ? g3 : gd); if (k == 0) canvas.drawPixel(gx, y - 1, g3); }
    if ((gh & 63) == 5) canvas.drawPixel((int)((gh >> 8) % SCR_W), y, rgb(250, 230, 110));        // fleurs
    canvas.drawFastHLine(li - 7, y, 3, rgb(222, 200, 140)); canvas.drawFastHLine(li - 4, y, 4, rgb(176, 156, 108));   // sable puis sable mouille
    canvas.drawFastHLine(ri, y, 4, rgb(176, 156, 108)); canvas.drawFastHLine(ri + 4, y, 3, rgb(222, 200, 140));
    uint16_t water = ((wr / 16) & 1) ? rgb(30, 100, 190) : rgb(36, 112, 200);
    canvas.drawFastHLine(li, y, ri - li, water);
    canvas.drawFastHLine(li, y, 5, ((wr / 16) & 1) ? rgb(62, 150, 206) : rgb(70, 160, 212));       // eau peu profonde, plus claire
    canvas.drawFastHLine(ri - 5, y, 5, ((wr / 16) & 1) ? rgb(62, 150, 206) : rgb(70, 160, 212));
    canvas.drawFastHLine(li + 5, y, 3, ((wr / 16) & 1) ? rgb(44, 124, 200) : rgb(48, 132, 206));
    canvas.drawFastHLine(ri - 8, y, 3, ((wr / 16) & 1) ? rgb(44, 124, 200) : rgb(48, 132, 206));
    uint32_t h = hash(wr);
    int wd = ri - li - 24 > 1 ? ri - li - 24 : 1;
    if ((h & 7) == 0) { int dx = li + 6 + (int)((h >> 8) % (uint32_t)wd); canvas.drawFastHLine(dx, y, 6 + (int)((h >> 4) & 7), rgb(92, 166, 232)); }         // rides claires
    if (((h >> 3) & 15) == 1) { int dx = li + 6 + (int)((h >> 12) % (uint32_t)wd); canvas.drawFastHLine(dx, y, 5 + (int)((h >> 5) & 3), rgb(24, 86, 170)); }    // creux sombres
    if ((((h >> 14) + animT / 6) & 31) == 0) { int dx = li + 6 + (int)((h >> 9) % (uint32_t)wd); canvas.drawPixel(dx, y, rgb(235, 248, 255)); }                // reflets
    if (((wr + animT / 4) & 7) < 2) { canvas.drawPixel(li + 1, y, rgb(215, 235, 250)); canvas.drawPixel(ri - 2, y, rgb(215, 235, 250)); }
    canvas.drawPixel(li, y, rgb(200, 232, 248)); canvas.drawPixel(ri - 1, y, rgb(200, 232, 248));
  }
  // decor des berges
  for (int i = 0; i < NSEG; i++) {
    int y = i * SEGH - SEGH * (PRE + 1) + (int)off + 1; if (y < -12 || y > SCR_H + 12) continue;
    float l, r, c; edges(y, l, r, c);
    for (int side = 0; side < 2; side++) {
      int t = side ? seg[i].dr : seg[i].dl; if (!t) continue; int o = side ? seg[i].orr : seg[i].ol;
      int x = side ? (int)r + 10 + o : (int)l - 10 - o;
      if (x < -10 || x > SCR_W + 10) continue;
      if (t == 1) drawTree(x, y, (o & 1) != 0); else if (t == 2) drawBush(x, y); else if (t == 3) drawBoulder(x, y); else drawReeds(x, y);
    }
  }
  // objets
  for (auto &o : objs) if (o.on) { if (o.type == 0) drawRock((int)o.x, (int)o.y, (int)o.a); else if (o.type == 1) drawLog(o); else drawCoin((int)o.x, (int)o.y); }
  if (state != OVER || (animT & 8)) if (invul == 0 || (animT & 4)) drawKayak((int)bx + ox, BY, bv / 4.0f);
  for (auto &p : parts) if (p.t > 0) { uint16_t c = p.t > 8 ? TFT_WHITE : rgb(190, 225, 248); canvas.drawPixel((int)p.x, (int)p.y, c); if (p.t > 12) canvas.drawPixel((int)p.x + 1, (int)p.y, c); }
  for (auto &p : pops) if (p.t > 0) ui::popText((int)p.x, (int)p.y, p.s, p.col);
  char b[24];
  if (state != TITLE) {
    snprintf(b, sizeof b, "%lu", (unsigned long)score); txtO(b, 5, 2, &fonts::Font4, TFT_WHITE, rgb(10, 30, 60));
    for (int i = 0; i < lives; i++) { int hx = SCR_W - 12 - i * 14; canvas.fillEllipse(hx + 1, 10, 3, 6, rgb(20, 50, 90)); canvas.fillEllipse(hx, 9, 3, 6, rgb(245, 120, 30)); canvas.drawFastVLine(hx, 4, 11, rgb(255, 200, 90)); }
    if (coins > 0) { canvas.fillCircle(11, 33, 4, rgb(180, 120, 20)); canvas.fillCircle(11, 33, 3, rgb(255, 205, 50)); snprintf(b, sizeof b, "%d", coins); txtO(b, 19, 26, &fonts::Font2, rgb(255, 230, 120), rgb(10, 30, 60)); }
  }
  if (state == TITLE) {
    canvas.fillRoundRect(40, 30, 160, 50, 8, rgb(12, 36, 70)); canvas.drawRoundRect(40, 30, 160, 50, 8, rgb(150, 205, 240));
    txt(L("RIVER","RIVIERE"), SCR_W / 2, 34, &fonts::Font4, rgb(255, 215, 90), textdatum_t::top_center, 1.2f);
    if (best > 0) { snprintf(b, sizeof b, L("best %lu","record %lu"), (unsigned long)best); txt(b, SCR_W / 2, 64, &fonts::Font2, rgb(200, 225, 250), textdatum_t::top_center); }
  }
  if (state == PAUSED) ui::pause();
  if (state == OVER) ui::gameOver(L("CAPSIZED!", "CHAVIRE !"), score, best, (score) > 0 && (uint32_t)(score) >= (uint32_t)(best));
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
  bgm::run(5, state == PLAY);
  present(0, 0);
}

void icon(int x, int y) {
  canvas.fillRoundRect(x, y, 64, 64, 6, rgb(36, 112, 198));
  for (int j = 0; j < 64; j++) { int l = 10 + (int)(sinf(j * 0.12f) * 6), r = 54 + (int)(sinf(j * 0.12f) * 6); if (r > 63) r = 63; canvas.drawFastHLine(x, y + j, l, rgb(48, 128, 60)); canvas.drawFastHLine(x + l, y + j, 3, rgb(222, 200, 140)); canvas.drawFastHLine(x + l + 3, y + j, 4, rgb(62, 150, 206)); canvas.drawFastHLine(x + r - 3, y + j, 3, rgb(222, 200, 140)); canvas.drawFastHLine(x + r, y + j, 64 - r, rgb(48, 128, 60)); }
  for (int k = 0; k < 6; k++) canvas.drawFastHLine(x + 16 + (k * 13) % 26, y + 6 + k * 10, 8, rgb(92, 166, 232));
  canvas.fillEllipse(x + 42, y + 15, 10, 8, rgb(190, 225, 248));
  canvas.fillCircle(x + 42, y + 14, 6, rgb(112, 118, 125)); canvas.fillCircle(x + 40, y + 12, 3, rgb(165, 172, 180));
  canvas.fillCircle(x + 22, y + 22, 4, rgb(255, 205, 50)); canvas.drawCircle(x + 22, y + 22, 2, rgb(220, 150, 30));
  drawTree(x + 6, y + 50, false);
  drawKayak(x + 32, y + 44, 0.3f);
}

void preview(int cx, int cy) { drawKayak(cx, cy, sinf(millis() * 0.003f)); }
}  // namespace river
