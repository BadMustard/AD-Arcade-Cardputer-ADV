// ============================================================================
//  SNAKE - grille 26x13, rendu lisse (le serpent glisse entre les cases), pomme + pomme doree
//  chronometree, rochers aux niveaux superieurs, bosse qui parcourt le corps quand il mange.
//  Inclinaison = direction (axe dominant, pas de demi-tour) ; M5 (A) = tourner a droite,
//  B (cote) = tourner a gauche ; HAUT (PWR) = pause
// ============================================================================
#include "common.h"

namespace snake {

static const uint32_t STEP_US = 20000;
static const int GW = 26, GH = 13, CS = 9, X0 = 3, Y0 = 17, MAXN = GW * GH + 2;
enum State { TITLE, PLAY, PAUSED, DYING, OVER };
static State state = TITLE;
static Preferences pr;
static uint32_t last = 0, acc = 0;

static const int DX[4] = {1, 0, -1, 0}, DY[4] = {0, 1, 0, -1};
static int8_t sx[MAXN], sy[MAXN], px_[MAXN], py_[MAXN];       // serpent : cases actuelles et cases au tick precedent
static int n = 0, dir = 0, pdir = 0;
static int q[2], qn = 0, lastWant = -1;
static float tickAcc = 0, interval = 9;
static int level = 1, eaten = 0, animT = 0, deadT = 0, shake = 0;
static uint32_t score = 0, best = 0;
static float bulge = 99;
static int ax = -1, ay = -1;                                  // pomme
static int gx = -1, gy = -1, gT = 0;                          // pomme doree + temps restant
static const int GOLD_TIME = 330;
struct Rock { int8_t x, y; uint8_t pop; };
static Rock rocks[16]; static int nrocks = 0;
static float hitDx = 0, hitDy = 0;                            // direction du choc quand on meurt
static bool hitWall = false;
struct Part { float x, y, vx, vy; int life; uint16_t c; };
static Part parts[40];
static float fx[MAXN], fy[MAXN];                              // positions ecran du dessin

static void spark(float x, float y, uint16_t c, int k, float spd) {
  for (int i = 0; i < k; i++) for (auto &p : parts) if (p.life <= 0) {
    float a = rnd(0, 628) / 100.f, v = rnd(4, 14) / 10.f * spd;
    p = {x, y, cosf(a) * v, sinf(a) * v - 0.3f, (int)rnd(12, 26), c}; break;
  }
}

static bool onSnake(int x, int y, int skipTail = 0) { for (int i = 0; i < n - skipTail; i++) if (sx[i] == x && sy[i] == y) return true; return false; }
static bool isRock(int x, int y) { for (int i = 0; i < nrocks; i++) if (rocks[i].x == x && rocks[i].y == y) return true; return false; }
static bool freeCell(int x, int y) { return !onSnake(x, y) && !isRock(x, y) && !(x == ax && y == ay) && !(x == gx && y == gy); }

static bool pickFree(int &ox, int &oy, int minHead = 0) {
  for (int tries = 0; tries < 200; tries++) {
    int x = rnd(0, GW - 1), y = rnd(0, GH - 1);
    if (!freeCell(x, y)) continue;
    if (minHead && abs(x - sx[0]) + abs(y - sy[0]) < minHead) continue;
    ox = x; oy = y; return true;
  }
  return false;
}
static void newApple() { if (!pickFree(ax, ay)) ax = ay = -1; }

static void addRock() {                                       // rochers isoles, jamais contre un mur ni contre un autre : toujours un passage
  if (nrocks >= 16) return;
  for (int tries = 0; tries < 300; tries++) {
    int x = rnd(1, GW - 2), y = rnd(1, GH - 2);
    if (!freeCell(x, y)) continue;
    if (abs(x - sx[0]) + abs(y - sy[0]) < 5) continue;
    if (y == sy[0] && ((x - sx[0]) * DX[dir]) > 0) continue;  // pas droit devant
    if (x == sx[0] && ((y - sy[0]) * DY[dir]) > 0) continue;
    bool ok = true;
    for (int i = 0; i < nrocks; i++) if (abs(rocks[i].x - x) <= 1 && abs(rocks[i].y - y) <= 1) ok = false;
    if (abs(x - ax) <= 1 && abs(y - ay) <= 1) ok = false;
    if (!ok) continue;
    rocks[nrocks++] = {(int8_t)x, (int8_t)y, 0}; return;
  }
}

static void reset() {
  n = 4; dir = pdir = 0; qn = 0; lastWant = -1; tickAcc = 0; level = 1; eaten = 0; score = 0; bulge = 99; deadT = 0; shake = 0; hitWall = false;
  for (int i = 0; i < n; i++) { sx[i] = 8 - i; sy[i] = GH / 2; px_[i] = sx[i]; py_[i] = sy[i]; }
  nrocks = 0; gx = gy = -1; gT = 0; ax = ay = -1;
  for (auto &p : parts) p.life = 0;
  interval = dif(11.0f, 9.0f, 7.2f);
  newApple();
}
void begin() {
  pr.begin(dns("snake"), false); best = pr.getInt("best", 0);
  tilt::load();
  reset(); state = TITLE; last = 0; acc = 0; animT = 0;
}

static void die(int nx, int ny) {
  state = DYING; deadT = 0; shake = 14;
  hitDx = (float)(nx - sx[0]); hitDy = (float)(ny - sy[0]);
  trophy::record(trophy::T_SNAKE, score); if (score > best) { best = score; pr.putInt("best", best); }
  sfx(330, 80); sfx(220, 120, false); sfx(110, 260, false);
}

static void tick() {
  if (qn > 0) { int nd = q[0]; q[0] = q[1]; qn--; if (nd != ((dir + 2) & 3)) { pdir = dir; dir = nd; sfx(1500, 6); } }
  int nx = sx[0] + DX[dir], ny = sy[0] + DY[dir];
  bool eat = (nx == ax && ny == ay), gold = (nx == gx && ny == gy);
  bool bad = nx < 0 || ny < 0 || nx >= GW || ny >= GH || isRock(nx, ny) || onSnake(nx, ny, (eat || gold) ? 0 : 1);
  if (bad) { die(nx, ny); return; }
  for (int i = 0; i < n; i++) { px_[i] = sx[i]; py_[i] = sy[i]; }
  bool grow = eat || gold;
  if (grow) { px_[n] = sx[n - 1]; py_[n] = sy[n - 1]; }       // le nouveau bout de queue sort de la derniere case
  int last_ = grow ? n : n - 1;
  for (int i = last_; i > 0; i--) { sx[i] = sx[i - 1]; sy[i] = sy[i - 1]; }
  sx[0] = nx; sy[0] = ny;
  if (grow) n++;
  if (eat || gold) {
    float cx = X0 + nx * CS + 4.5f, cy = Y0 + ny * CS + 4.5f;
    bulge = 0;
    if (gold) { score += 50; gx = gy = -1; gT = 0; spark(cx, cy, rgb(255, 220, 80), 14, 1.5f); sfx(988, 40); sfx(1318, 40, false); sfx(1760, 80, false); }
    else {
      score += 10; eaten++; spark(cx, cy, rgb(255, 90, 70), 8, 1.0f); sfx(880, 30); sfx(1320, 50, false);
      newApple();
      if (eaten % 4 == 3 && gx < 0) { if (pickFree(gx, gy)) gT = GOLD_TIME; }
      int nl = eaten / 5 + 1;
      if (nl > level) { level = nl; interval = fmaxf(dif(5.0f, 4.0f, 3.2f), dif(11.0f, 9.0f, 7.2f) - 0.7f * (level - 1)); if (level >= 2) { addRock(); addRock(); } sfx(660, 50); sfx(990, 80, false); }
    }
    if (score > best) best = score;
  }
}

static void step() {
  animT++;
  for (auto &p : parts) if (p.life > 0) { p.x += p.vx; p.y += p.vy; p.vy += 0.04f; p.life--; }
  for (int i = 0; i < nrocks; i++) if (rocks[i].pop < 12) rocks[i].pop++;
  if (shake > 0) shake--;
  if (state == TITLE || state == PAUSED) return;
  if (state == DYING) { if (++deadT > 55) { state = OVER; deadT = 0; } return; }
  if (state == OVER) { deadT++; return; }
  if (gT > 0) { if (--gT == 0) { gx = gy = -1; } }
  if (bulge < 90) bulge += 1.0f / interval;
  // direction par inclinaison (axe dominant)
  tilt::update();
  float tx = tilt::x(), ty = -tilt::y();
  float mx = fabsf(tx), my = fabsf(ty); int want = -1;
  if (!g_btnCtrl && (mx > 0.35f || my > 0.35f)) want = mx >= my ? (tx > 0 ? 0 : 2) : (ty > 0 ? 1 : 3);   // mode boutons : seulement A / B
  if (want >= 0 && want != lastWant) {
    int basis = qn > 0 ? q[qn - 1] : dir;
    if (want != basis && want != ((basis + 2) & 3) && qn < 2) q[qn++] = want;
  }
  lastWant = want;
  tickAcc += 1.0f;
  if (tickAcc >= interval) { tickAcc -= interval; tick(); }
}

// ----------------------------------------------------------------------------- dessin
static float radAt(float idx, int cnt, float bg) {
  float r = 3.9f;
  float tl = fminf(7.0f, cnt * 0.4f);
  if (idx > cnt - tl) r = fminf(r, 1.6f + (cnt - 1 - idx) * (2.3f / tl));
  if (r < 1.2f) r = 1.2f;
  float d = fabsf(idx - bg); if (d < 2.2f) r += 1.7f * (1 - d / 2.2f);
  return r;
}

// px,py : positions ecran (centres), 0 = tete. hdx,hdy : direction de la tete (unitaire)
static void drawBody(const float* px, const float* py, int cnt, float hdx, float hdy, float bg, bool dead, bool blink, int anim, int tongue) {
  uint16_t OUT = g_skin ? mixc(skin::c(1, 0), 0, 140) : rgb(10, 52, 26);
  uint16_t A = dead ? rgb(150, 150, 128) : skin::c(0, rgb(70, 198, 86)), B = dead ? rgb(124, 124, 108) : skin::c(1, rgb(44, 160, 70)), HL = dead ? rgb(190, 190, 170) : skin::c(2, rgb(158, 240, 142));
  if (blink) { A = rgb(230, 60, 50); B = rgb(190, 40, 40); HL = rgb(255, 150, 120); }
  for (int pass = 0; pass < 2; pass++) {
    for (int i = cnt - 1; i >= 1; i--) for (int s = 0; s < 3; s++) {
      float t = s / 3.0f, idx = i - t;
      float x = px[i] + (px[i - 1] - px[i]) * t, y = py[i] + (py[i - 1] - py[i]) * t;
      float r = radAt(idx, cnt, bg);
      if (pass == 0) canvas.fillCircle((int)(x + 0.5f), (int)(y + 0.5f), (int)(r + 1.0f), OUT);
      else {
        int k = i * 3 + (3 - s);
        canvas.fillCircle((int)(x + 0.5f), (int)(y + 0.5f), (int)(r + 0.3f), ((k / 4) & 1) ? B : A);
      }
    }
  }
  for (int i = cnt - 1; i >= 1; i--) {                          // ecailles : reflet + tache sombre
    float r = radAt(i, cnt, bg); if (r < 2.6f) continue;
    int x = (int)(px[i] + 0.5f), y = (int)(py[i] + 0.5f);
    canvas.drawPixel(x - 1, y - 1, HL);
    if (i & 1) canvas.drawPixel(x + 1, y + 1, OUT);
  }
  // tete
  int hx = (int)(px[0] + 0.5f), hy = (int)(py[0] + 0.5f);
  float nx = hdx, ny = hdy, perx = -hdy, pery = hdx;
  uint16_t HC = dead ? rgb(170, 170, 148) : (g_skin ? mixc(skin::c(0, 0), TFT_WHITE, 50) : rgb(92, 218, 104));
  if (blink) HC = rgb(240, 80, 70);
  canvas.fillCircle(hx, hy, 6, OUT);
  canvas.fillCircle((int)(hx + nx * 2), (int)(hy + ny * 2), 5, OUT);
  canvas.fillCircle(hx, hy, 5, HC);
  canvas.fillCircle((int)(hx + nx * 2), (int)(hy + ny * 2), 4, HC);
  canvas.drawPixel(hx - 2, hy - 2, HL);
  if (tongue > 0) {
    float L = tongue < 8 ? 3 + tongue * 0.9f : 10 - (tongue - 8) * 0.5f;
    float bx = hx + nx * 7, by = hy + ny * 7, ex = hx + nx * (7 + L), ey = hy + ny * (7 + L);
    canvas.drawLine((int)bx, (int)by, (int)ex, (int)ey, rgb(230, 40, 60));
    canvas.drawLine((int)ex, (int)ey, (int)(ex + nx * 2 + perx * 2), (int)(ey + ny * 2 + pery * 2), rgb(230, 40, 60));
    canvas.drawLine((int)ex, (int)ey, (int)(ex + nx * 2 - perx * 2), (int)(ey + ny * 2 - pery * 2), rgb(230, 40, 60));
  }
  for (int sgn = -1; sgn <= 1; sgn += 2) {
    int ex = (int)(hx + nx * 2.0f + perx * 3.2f * sgn + 0.5f), ey = (int)(hy + ny * 2.0f + pery * 3.2f * sgn + 0.5f);
    if (dead) { canvas.drawLine(ex - 1, ey - 1, ex + 1, ey + 1, rgb(20, 20, 20)); canvas.drawLine(ex - 1, ey + 1, ex + 1, ey - 1, rgb(20, 20, 20)); }
    else {
      canvas.fillCircle(ex, ey, 2, TFT_WHITE);
      canvas.fillCircle((int)(ex + nx * 0.8f + 0.5f), (int)(ey + ny * 0.8f + 0.5f), 1, rgb(10, 10, 20));
    }
  }
}

static void drawApple(int cx, int cy, bool goldA, int t) {
  if (goldA) {
    int pulse = (int)(sinf(t * 0.2f) * 1.0f + 1.5f);
    canvas.fillCircle(cx, cy, 5 + pulse / 2, rgb(120, 90, 10));
    canvas.fillCircle(cx, cy, 4, rgb(255, 205, 40)); canvas.drawCircle(cx, cy, 4, rgb(190, 120, 10));
    canvas.fillCircle(cx - 1, cy - 1, 1, rgb(255, 250, 200));
    int k = (t / 4) % 8; if (k < 4) { canvas.drawFastHLine(cx + 3 - k / 2, cy - 5 - k / 2, 3, 0xFFFF); canvas.drawFastVLine(cx + 4 - k / 2, cy - 6 - k / 2, 3, 0xFFFF); }
  } else {
    canvas.fillEllipse(cx + 1, cy + 4, 3, 1, rgb(18, 50, 28));
    canvas.fillCircle(cx, cy + 1, 4, rgb(214, 36, 40));
    canvas.fillCircle(cx - 1, cy, 3, rgb(238, 62, 60));
    canvas.drawPixel(cx - 2, cy - 1, rgb(255, 200, 190)); canvas.drawPixel(cx - 2, cy, rgb(255, 170, 160));
    canvas.drawLine(cx, cy - 2, cx + 1, cy - 4, rgb(100, 60, 20));
    canvas.fillRect(cx + 1, cy - 4, 3, 2, rgb(60, 190, 70));
  }
}

static void drawRock(int x, int y, int pop) {
  int cx = X0 + x * CS, cy = Y0 + y * CS;
  if (pop < 12) { int s = pop * 8 / 12; cy += (8 - s) / 2; canvas.fillRoundRect(cx + 4 - s / 2, cy + 4 - s / 2, s + 1, s + 1, 2, rgb(150, 154, 162)); return; }
  canvas.fillEllipse(cx + 5, cy + 7, 4, 2, rgb(16, 44, 26));
  canvas.fillRoundRect(cx + 1, cy + 1, 8, 8, 3, rgb(96, 100, 110));
  canvas.fillRoundRect(cx + 1, cy + 1, 7, 6, 3, rgb(132, 136, 146));
  canvas.fillRoundRect(cx + 2, cy + 2, 4, 3, 2, rgb(178, 182, 192));
  canvas.drawPixel(cx + 6, cy + 5, rgb(80, 84, 94)); canvas.drawPixel(cx + 5, cy + 6, rgb(80, 84, 94));
}

static void drawGrass(int ox) {
  for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++) {
    uint16_t c = ((x + y) & 1) ? rgb(30, 88, 48) : rgb(34, 98, 54);
    canvas.fillRect(ox + X0 + x * CS, Y0 + y * CS, CS, CS, c);
    int h = (x * 7 + y * 13) % 11;
    if (h < 3) { int tx_ = ox + X0 + x * CS + 2 + h * 2, ty_ = Y0 + y * CS + 3 + (h & 1) * 3; canvas.drawPixel(tx_, ty_, rgb(52, 126, 66)); canvas.drawPixel(tx_ + 1, ty_ - 1, rgb(52, 126, 66)); }
  }
}

static void drawFrame(int ox) {
  uint16_t c1 = rgb(92, 70, 46), c2 = rgb(60, 44, 28), c3 = rgb(130, 102, 70);
  canvas.drawRect(ox + X0 - 1, Y0 - 1, GW * CS + 2, GH * CS + 2, c3);
  canvas.drawRect(ox + X0 - 2, Y0 - 2, GW * CS + 4, GH * CS + 4, c1);
  canvas.drawRect(ox + X0 - 3, Y0 - 3, GW * CS + 6, GH * CS + 6, c2);
}

static void renderTitle() {
  for (int y = 0; y < SCR_H; y += 3) canvas.fillRect(0, y, SCR_W, 3, mixc(rgb(26, 80, 46), rgb(14, 44, 28), y * 256 / SCR_H));
  for (int y = 0; y < 9; y++) for (int x = 0; x < 15; x++) if ((x + y) & 1) canvas.fillRect(x * 16, y * 16, 16, 16, rgb(30, 90, 52));
  int N = 46; float t = animT * 0.05f;
  for (int i = 0; i < N; i++) {                                 // serpent qui ondule en travers de l'ecran
    float hx_ = fmodf(t * 52.0f, 500.0f) - 30.0f, xx = hx_ - i * 4.6f;
    fx[i] = xx; fy[i] = 104 + sinf(xx * 0.045f + t) * 12;
  }
  float hdx = fx[0] - fx[1], hdy = fy[0] - fy[1], hl = sqrtf(hdx * hdx + hdy * hdy); if (hl < 0.01f) hl = 1; hdx /= hl; hdy /= hl;
  drawBody(fx, fy, N, hdx, hdy, 99, false, false, animT, (animT % 70) < 16 ? (animT % 70) : 0);
  drawApple(206, 96, false, animT);
  static const char* TT = "SNAKE";
  for (int i = 0; i < 5; i++) {
    int x = 40 + i * 30, y = 12 + (int)(sinf(animT * 0.08f + i * 0.8f) * 4); char ch[2] = {TT[i], 0};
    txt(ch, x + 2, y + 3, &fonts::Font4, rgb(6, 30, 16), textdatum_t::top_left, 1.6f);
    txt(ch, x, y, &fonts::Font4, mixc(rgb(100, 230, 110), rgb(255, 230, 90), i * 55), textdatum_t::top_left, 1.6f);
  }
  char b[24];
  if ((animT / 25) & 1) txtO(L("PRESS A", "APPUIE SUR A"), SCR_W / 2, 66, &fonts::Font2, TFT_WHITE, rgb(10, 30, 16), textdatum_t::top_center);
  if (best > 0) { snprintf(b, sizeof b, L("best %lu", "record %lu"), (unsigned long)best); txt(b, SCR_W / 2, 46, &fonts::Font2, rgb(255, 224, 120), textdatum_t::top_center); }
}

static void render() {
  if (state == TITLE) { renderTitle(); return; }
  int ox = shake > 0 ? ((shake & 1) ? 2 : -2) : 0;
  canvas.fillScreen(rgb(8, 22, 14));
  drawGrass(ox);
  // rochers
  for (int i = 0; i < nrocks; i++) drawRock(rocks[i].x, rocks[i].y, rocks[i].pop);
  // pommes
  if (ax >= 0) drawApple(ox + X0 + ax * CS + 4, Y0 + ay * CS + 4, false, animT);
  if (gx >= 0) { if (gT > 90 || (gT / 5) & 1) drawApple(ox + X0 + gx * CS + 4, Y0 + gy * CS + 4, true, animT); }
  // serpent : positions interpolees entre le tick precedent et l'actuel
  float p = state == PLAY ? tickAcc / interval : 1.0f; if (p > 1) p = 1;
  int cnt = n;
  for (int i = 0; i < cnt; i++) {
    float a = px_[i], b = sx[i], c = py_[i], d = sy[i];
    if (state == PAUSED || state == DYING || state == OVER) { p = state == PAUSED ? p : 1.0f; }
    fx[i] = ox + X0 + (a + (b - a) * p) * CS + 4.5f; fy[i] = Y0 + (c + (d - c) * p) * CS + 4.5f;
  }
  bool dead = state == DYING || state == OVER;
  if (dead) {                                                   // la tete avance dans l'obstacle puis s'arrete
    float k = fminf(0.5f, deadT * 0.1f);
    fx[0] = ox + X0 + sx[0] * CS + 4.5f + hitDx * CS * k; fy[0] = Y0 + sy[0] * CS + 4.5f + hitDy * CS * k;
    for (int i = 1; i < cnt; i++) { fx[i] = ox + X0 + sx[i] * CS + 4.5f; fy[i] = Y0 + sy[i] * CS + 4.5f; }
  }
  float hdx, hdy;
  if (dead) { hdx = hitDx; hdy = hitDy; }
  else {
    float tx_ = DX[dir] * fminf(1.f, p * 2.5f) + DX[pdir] * (1 - fminf(1.f, p * 2.5f)), ty_ = DY[dir] * fminf(1.f, p * 2.5f) + DY[pdir] * (1 - fminf(1.f, p * 2.5f));
    float l = sqrtf(tx_ * tx_ + ty_ * ty_); if (l < 0.01f) l = 1; hdx = tx_ / l; hdy = ty_ / l;
  }
  int tg = (animT % 110) < 18 ? (animT % 110) : 0;
  drawBody(fx, fy, cnt, hdx, hdy, bulge, dead, state == DYING && (deadT / 4) % 2 == 0, animT, tg);
  for (auto &pp : parts) if (pp.life > 0) canvas.fillRect((int)pp.x, (int)pp.y, 2, 2, pp.life > 8 ? pp.c : mixc(pp.c, rgb(30, 90, 50), 128));
  drawFrame(ox);
  // barre superieure
  canvas.fillRect(0, 0, SCR_W, 14, rgb(10, 24, 16)); canvas.drawFastHLine(0, 14, SCR_W, rgb(92, 70, 46));
  char b[24];
  canvas.fillCircle(8, 7, 4, rgb(220, 40, 44)); canvas.fillRect(8, 1, 3, 2, rgb(60, 190, 70));
  snprintf(b, sizeof b, "%lu", (unsigned long)score); txt(b, 16, -1, &fonts::Font2, TFT_WHITE);
  snprintf(b, sizeof b, L("LEVEL %d", "NIVEAU %d"), level); txt(b, SCR_W / 2, 3, &fonts::Font0, rgb(150, 220, 160), textdatum_t::top_center);
  snprintf(b, sizeof b, "%lu", (unsigned long)best); txt(b, SCR_W - 4, -1, &fonts::Font2, rgb(255, 224, 120), textdatum_t::top_right);
  if (gx >= 0 && state == PLAY) {                               // temps restant de la pomme doree
    int w = gT * 40 / GOLD_TIME; canvas.fillRect(SCR_W / 2 + 40, 4, 40, 6, rgb(60, 44, 10)); canvas.fillRect(SCR_W / 2 + 40, 4, w, 6, rgb(255, 205, 40)); canvas.drawRect(SCR_W / 2 + 40, 4, 40, 6, rgb(190, 120, 10));
  }
  if (state == PAUSED) ui::pause();
  if (state == OVER) ui::gameOver("GAME OVER", score, best, (score) > 0 && (uint32_t)(score) >= (uint32_t)(best));
}

static void turn(int d) {
  if (qn >= 2) return;
  int basis = qn > 0 ? q[qn - 1] : dir;
  q[qn++] = (basis + d + 4) & 3;
}

void loop() {
  uint32_t now = micros();
  if (last == 0) last = now;
  acc += now - last; last = now; if (acc > 100000) acc = 100000;
  bool a = M5.BtnA.wasPressed(), b = M5.BtnB.wasPressed(), p = M5.BtnPWR.wasPressed();
  switch (state) {
    case TITLE: if (a || b) { reset(); state = PLAY; sfx(660, 40); } break;
    case PLAY: if (p) { state = PAUSED; sfx(440, 50); } else { if (a) turn(1); if (b) turn(3); } break;
    case PAUSED: if (p || a || b) { state = PLAY; sfx(660, 50); } break;
    case OVER: if ((a || b) && deadT > 40) { reset(); state = PLAY; sfx(660, 40); } break;
    default: break;
  }
  while (acc >= STEP_US) { step(); acc -= STEP_US; }
  render();
  bgm::run(1, state == PLAY);
  present(0, 0);
}

void icon(int x, int y) {
  for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) canvas.fillRect(x + i * 16, y + j * 16, 16, 16, ((i + j) & 1) ? rgb(30, 88, 48) : rgb(36, 102, 56));
  canvas.fillRoundRect(x, y, 64, 3, 0, rgb(60, 44, 28)); canvas.fillRect(x, y + 61, 64, 3, rgb(60, 44, 28));
  canvas.fillRect(x, y, 3, 64, rgb(60, 44, 28)); canvas.fillRect(x + 61, y, 3, 64, rgb(60, 44, 28));
  static float ix[40], iy[40]; int N = 34;
  for (int i = 0; i < N; i++) {                                 // un S : la tete en haut a droite
    float u = i / (float)(N - 1);
    ix[i] = x + 48 - u * 34 + (u > 0.5f ? 0 : 0); iy[i] = y + 16 + u * 32;
    float ang = u * 6.2832f * 1.0f; ix[i] = x + 32 + cosf(ang + 1.2f) * (18 - u * 4); iy[i] = y + 32 - sinf(ang + 1.2f) * 14 + (u - 0.5f) * 8;
  }
  float hdx = ix[0] - ix[1], hdy = iy[0] - iy[1], l = sqrtf(hdx * hdx + hdy * hdy); if (l < 0.01f) l = 1; hdx /= l; hdy /= l;
  drawBody(ix, iy, N, hdx, hdy, 99, false, false, 0, 6);
  drawApple(x + 52, y + 50, false, 0);
}

void preview(int cx, int cy) {
  float px[8], py[8]; int n = 8; float t = millis() * 0.004f;
  for (int i = 0; i < n; i++) { px[i] = cx + 18 - i * 5.0f; py[i] = cy + sinf(t - i * 0.8f) * 4.0f; }
  drawBody(px, py, n, 1.0f, 0.0f, -9.0f, false, false, (int)(millis() / 50), 0);
}

}  // namespace snake
