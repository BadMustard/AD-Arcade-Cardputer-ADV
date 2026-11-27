// ============================================================================
//  LUNAR LANDER - gravite, poussee (A ou B maintenu), orientation par inclinaison, carburant.
//  Terrain aleatoire sans fin (il defile), zones d'atterrissage a multiplicateur x1 / x2 / x5,
//  conditions d'atterrissage (vitesse verticale + horizontale + angle), niveaux de plus en plus durs.
//  HAUT (PWR) = pause
// ============================================================================
#include "common.h"

namespace lander {

static const uint32_t STEP_US = 20000;
static const int WW = 720;                       // largeur du monde (periodique)
enum State { TITLE, READY, FLY, LANDED, CRASH, PAUSED, OVER };
static State state = TITLE;
static Preferences pr;
static uint32_t last = 0, acc = 0;

static int16_t H[WW];
static int padC[3], padW[3], padM[3];
static float x, y, vx, vy, ang, fuel, camx;
static float grav, thrustPow, vyMax = 1.0f, vxMax = 0.55f, angMax = 0.21f;
static int level = 1, lives = 3, stateT = 0, animT = 0, shake = 0, flameT = 0, lastPad = 0, gain = 0;
static uint32_t score = 0, best = 0;
static float fuel0 = 120, startVx = 0.4f;
static bool thrusting = false;
static const char* crashWhy = "";

struct Part { float x, y, vx, vy; int life, max; uint16_t c; uint8_t kind; };   // kind 0 = point, 1 = debris
static Part parts[80];

static inline int wrap(int v) { v %= WW; return v < 0 ? v + WW : v; }
static inline int gh(float wx) { return H[wrap((int)floorf(wx))]; }

static void spawn(float px, float py, float pvx, float pvy, int life, uint16_t c, uint8_t kind = 0) {
  for (auto &p : parts) if (p.life <= 0) { p = {px, py, pvx, pvy, life, life, c, kind}; return; }
}

// ----------------------------------------------------------------------------- terrain
static void genTerrain() {
  int d = level - 1; if (d > 8) d = 8;
  float amp[14], ph[14];
  for (int k = 0; k < 14; k++) { amp[k] = (k < 6 ? 15.0f : 5.0f) / powf(k + 1, 0.8f) * (0.7f + 0.11f * d); ph[k] = rnd(0, 628) / 100.f; }
  for (int i = 0; i < WW; i++) {
    float h = 100;
    for (int k = 0; k < 14; k++) h += amp[k] * sinf(6.2832f * (k + 1) * i / WW + ph[k]);
    H[i] = (int16_t)clampf(h, 74, 124);
  }
  int baseW[3] = {46, 34, 26}, mult[3] = {1, 2, 5};
  int sep = WW / 3, off = rnd(0, WW - 1);
  for (int p = 0; p < 3; p++) {
    int w = baseW[p] - 2 * d; if (w < 18 + p * 2) w = 18 + p * 2;
    int c = wrap(off + p * sep + (int)rnd(0, 60) - 30);
    float s = 0; for (int i = -w / 2; i <= w / 2; i++) s += H[wrap(c + i)];
    int lvl = (int)clampf(s / (w + 1), 92, 116);
    for (int i = -w / 2 - 10; i <= w / 2 + 10; i++) {          // plateau + rampes douces
      int idx = wrap(c + i), ad = abs(i) - w / 2;
      if (ad <= 0) H[idx] = lvl; else H[idx] = (int16_t)(lvl + (H[idx] - lvl) * ad / 10);
    }
    padC[p] = c; padW[p] = w; padM[p] = mult[p];
  }
}

static int padAt(float wx) {                      // numero de zone sous wx (ou -1)
  for (int p = 0; p < 3; p++) { int dx = wrap((int)floorf(wx) - padC[p] + WW / 2) - WW / 2; if (abs(dx) <= padW[p] / 2) return p; }
  return -1;
}

// ----------------------------------------------------------------------------- partie
static void levelParams() {
  int d = level - 1; if (d > 8) d = 8;
  grav = (0.0100f + 0.0007f * d) * dif(0.85f, 1.0f, 1.15f);
  thrustPow = 0.026f;
  fuel0 = (135 - 8 * d); if (fuel0 < 72) fuel0 = 72; fuel0 *= dif(1.25f, 1.0f, 0.85f);
  startVx = 0.30f + 0.07f * d;
}
static void startRun() {                          // (re)pose le module en haut du terrain
  levelParams();
  x = padC[1] + (rnd(0, 1) ? -1 : 1) * 190.0f + (float)rnd(0, 60) - 30; y = 18; vx = (padC[1] - x) > 0 ? startVx : -startVx; vy = 0.1f; ang = 0; fuel = fuel0;
  camx = x; stateT = 0; shake = 0; thrusting = false;
  for (auto &p : parts) p.life = 0;
  state = READY;
}
static void newGame() { level = 1; lives = difi(5, 3, 2); score = 0; genTerrain(); startRun(); }

void begin() {
  pr.begin(dns("lander"), false); best = pr.getInt("best", 0);
  tilt::load();
  level = 1; genTerrain(); levelParams();
  x = 0; y = 40; vx = 0.3f; vy = 0; ang = 0; camx = 0; fuel = 100;
  state = TITLE; last = 0; acc = 0; animT = 0;
}

static void explode() {
  state = CRASH; stateT = 0; shake = 18;
  for (int i = 0; i < 34; i++) { float a = rnd(0, 628) / 100.f, v = rnd(5, 28) / 10.f; spawn(x, y, cosf(a) * v + vx * 0.5f, sinf(a) * v - 0.8f, rnd(20, 45), i % 3 == 0 ? rgb(255, 250, 200) : (i % 3 == 1 ? rgb(255, 170, 40) : rgb(230, 70, 30))); }
  for (int i = 0; i < 9; i++) { float a = rnd(0, 628) / 100.f, v = rnd(6, 20) / 10.f; spawn(x, y, cosf(a) * v, sinf(a) * v - 1.2f, rnd(40, 70), rgb(200, 200, 212), 1); }
  lives--;
  sfx(300, 60); sfx(200, 90, false); sfx(120, 140, false); sfx(70, 300, false);
}

static void landed(int pad) {
  state = LANDED; stateT = 0; lastPad = pad; ang = 0; vx = vy = 0;
  gain = (100 + (int)fuel * 2) * padM[pad] + level * 20;
  score += gain; trophy::record(trophy::T_LANDER, score); if (score > best) { best = score; pr.putInt("best", best); }
  sfx(660, 70); sfx(880, 70, false); sfx(1100, 70, false); sfx(1320, 160, false);
  for (int i = 0; i < 20; i++) spawn(x + (float)rnd(0, 20) - 10, y + 8, (float)((int)rnd(0, 20) - 10) / 10.f, -(float)rnd(5, 22) / 10.f, rnd(25, 45), rgb(255, 230, 100));
}

static void step() {
  animT++;
  for (auto &p : parts) if (p.life > 0) { p.x += p.vx; p.y += p.vy; if (p.kind == 1) p.vy += 0.03f; else p.vy += 0.01f; p.life--; }
  if (shake > 0) shake--;
  if (state == TITLE) { camx += 0.35f; return; }
  if (state == OVER) { stateT++; return; }   // sans ca stateT restait a 0 et A/B ne relancait jamais la partie
  if (state == PAUSED) return;
  stateT++;
  if (state == READY) { if (stateT > 60) { state = FLY; stateT = 0; } camx += (x + vx * 25 - camx) * 0.08f; x += vx * 0.0f; return; }
  if (state == CRASH) { if (stateT > 90) { if (lives > 0) startRun(); else { state = OVER; stateT = 0; session::onOver(trophy::T_LANDER, score); } } return; }
  if (state == LANDED) return;

  // ----- vol
  tilt::update();
  if (g_btnCtrl) {                                   // boutons : A / B pivotent, A+B = poussee
    bool ba = M5.BtnA.isPressed(), bb = M5.BtnB.isPressed();
    if (ba && !bb) ang += 0.035f; else if (bb && !ba) ang -= 0.035f;
    ang = clampf(ang, -0.75f, 0.75f);
    thrusting = ba && bb && fuel > 0;
  } else {
  float t = tilt::x(); if (fabsf(t) < 0.05f) t = 0; else t = t > 0 ? t - 0.05f : t + 0.05f;
  float target = clampf(t * 1.5f, -1, 1) * 0.75f;
  float da = clampf((target - ang) * 0.18f, -0.06f, 0.06f); ang += da;
  thrusting = (M5.BtnA.isPressed() || M5.BtnB.isPressed()) && fuel > 0;
  }
  float ax_ = 0, ay_ = grav;
  if (thrusting) {
    ax_ += sinf(ang) * thrustPow; ay_ -= cosf(ang) * thrustPow; fuel -= 0.1f; if (fuel < 0) fuel = 0; flameT++;
    if (flameT % 4 == 0) sfx(150 + rnd(0, 40), 40, false);
    float nx = x - sinf(ang) * 9, ny = y + cosf(ang) * 9;
    spawn(nx, ny, -sinf(ang) * 1.6f + vx * 0.5f + (float)((int)rnd(0, 10) - 5) / 20.f, cosf(ang) * 1.6f + vy * 0.3f, rnd(8, 16), rgb(255, 170 + rnd(0, 60), 60));
    float g = (float)gh(x) - y;
    if (g < 30 && g > 0) spawn(x + (float)((int)rnd(0, 40) - 20) / 4, gh(x) - 1, ((float)((int)rnd(0, 40)) - 20) / 14.f, -(float)rnd(0, 8) / 12.f, rnd(14, 26), rgb(150, 150, 160));
  }
  vx += ax_; vy += ay_; x += vx; y += vy;
  if (y < -30) { y = -30; if (vy < 0) vy = 0; }
  camx += (x + vx * 25 - camx) * 0.08f;

  // ----- contact : pieds (+ bas du corps)
  float ca = cosf(ang), sa = sinf(ang);
  static const float PT[5][2] = {{-9, 9}, {9, 9}, {0, 7}, {-6, 3}, {6, 3}};
  bool hit = false; float lfx = 0, rfx = 0;
  for (int i = 0; i < 5; i++) {
    float px = x + PT[i][0] * ca - PT[i][1] * sa, py = y + PT[i][0] * sa + PT[i][1] * ca;
    if (i == 0) lfx = px;
    if (i == 1) rfx = px;
    if (py >= gh(px)) hit = true;
  }
  if (!hit) return;
  int p1 = padAt(lfx), p2 = padAt(rfx);
  bool onPad = p1 >= 0 && p1 == p2;
  bool okV = vy <= vyMax, okH = fabsf(vx) <= vxMax, okA = fabsf(ang) <= angMax;
  if (onPad && okV && okH && okA) { y = (float)gh(x) - 9; landed(p1); }
  else {
    if (!onPad) crashWhy = L("NOT ON A PAD", "HORS ZONE");
    else if (!okV) crashWhy = L("TOO FAST", "TROP RAPIDE");
    else if (!okH) crashWhy = L("TOO MUCH DRIFT", "TROP DE DERIVE");
    else crashWhy = L("TOO TILTED", "TROP INCLINE");
    explode();
  }
}

// ----------------------------------------------------------------------------- dessin
static inline uint32_t hash(uint32_t v) { v ^= v >> 15; v *= 0x2c1b3c6dU; v ^= v >> 12; v *= 0x297a2d39U; v ^= v >> 15; return v; }

static void drawSky(int ox) {
  for (int yy = 0; yy < SCR_H; yy += 4) canvas.fillRect(0, yy, SCR_W, 4, mixc(rgb(2, 2, 14), rgb(18, 12, 38), yy * 256 / SCR_H));
  for (int i = 0; i < 46; i++) {                                           // etoiles (parallaxe lente)
    uint32_t h = hash(i * 977 + 13);
    int sx_ = (int)(((int)(h % SCR_W) - (int)(camx * (0.05f + (h % 5) * 0.02f))) % SCR_W); if (sx_ < 0) sx_ += SCR_W;
    int sy_ = (h >> 8) % 100;
    int b = 90 + (h >> 16) % 140; if (((animT / 20) + i) % 11 == 0) b = 255;
    canvas.drawPixel(sx_ + ox, sy_, rgb(b, b, b));
    if ((h >> 4) % 9 == 0) canvas.drawPixel(sx_ + ox + 1, sy_, rgb(b / 2, b / 2, b / 2));
  }
  int ex = 168 - (int)(camx * 0.02f) % 240, ey = 36; if (ex < -20) ex += 280;       // la Terre, petite et lointaine
  canvas.fillCircle(ex + ox, ey, 10, rgb(30, 90, 200));
  canvas.fillCircle(ex + ox - 3, ey - 2, 4, rgb(46, 150, 80)); canvas.fillCircle(ex + ox + 3, ey + 4, 3, rgb(46, 150, 80));
  canvas.fillCircle(ex + ox + 5, ey - 5, 3, rgb(240, 240, 250));
  canvas.drawCircle(ex + ox, ey, 10, rgb(110, 160, 240));
  for (int k = 0; k < 8; k++) { canvas.drawPixel(ex + ox + 6 + (k & 1), ey + 3 + k, rgb(14, 40, 100)); canvas.drawPixel(ex + ox + 4 + (k & 1), ey + 6 + k / 2, rgb(14, 40, 100)); }
}

static void drawTerrain(int ox) {
  int x0 = (int)floorf(camx) - SCR_W / 2;
  for (int sx_ = 0; sx_ < SCR_W; sx_++) {
    int wx = x0 + sx_, g = H[wrap(wx)];
    int X = sx_ + ox;
    int d = H[wrap(wx + 2)] - H[wrap(wx - 2)];                              // pente : eclairage par la gauche
    uint16_t top = d > 1 ? rgb(150, 150, 164) : (d < -1 ? rgb(214, 214, 226) : rgb(190, 190, 204));
    int pad = padAt((float)wx);
    if (pad >= 0 && abs(wrap(wx - padC[pad] + WW / 2) - WW / 2) <= padW[pad] / 2) {
      canvas.drawFastVLine(X, g, 3, ((wx >> 2) & 1) ? rgb(250, 200, 30) : rgb(30, 30, 40));
      canvas.drawFastVLine(X, g + 3, 3, rgb(148, 148, 160));
      canvas.drawFastVLine(X, g + 6, 14, rgb(112, 112, 126));
      canvas.drawFastVLine(X, g + 20, 18, rgb(84, 84, 98));
      if (g + 38 < SCR_H) canvas.drawFastVLine(X, g + 38, SCR_H - g - 38, rgb(58, 58, 72));
      continue;
    }
    canvas.drawFastVLine(X, g, 2, top);
    canvas.drawFastVLine(X, g + 2, 4, rgb(148, 148, 160));
    canvas.drawFastVLine(X, g + 6, 14, rgb(112, 112, 126));
    canvas.drawFastVLine(X, g + 20, 18, rgb(84, 84, 98));
    if (g + 38 < SCR_H) canvas.drawFastVLine(X, g + 38, SCR_H - g - 38, rgb(58, 58, 72));
    uint32_t h = hash(wx * 31 + 7);
    if (h % 17 == 0) { int cy = g + 5 + (h >> 8) % 28; if (cy < SCR_H) { canvas.drawPixel(X, cy, rgb(60, 60, 74)); canvas.drawPixel(X + 1, cy, rgb(60, 60, 74)); canvas.drawPixel(X, cy - 1, rgb(150, 150, 166)); } }
  }
  for (int p = 0; p < 3; p++) {                                              // balises + multiplicateur
    for (int k = -1; k <= 1; k++) {
      int wc = padC[p] + k * 2 * WW; (void)wc;
    }
    int dx = wrap((int)camx - padC[p] + WW / 2) - WW / 2;                    // decalage ecran du centre de la zone
    int sx_ = SCR_W / 2 - dx;
    if (sx_ < -40 || sx_ > SCR_W + 40) continue;
    int g = H[padC[p]];
    for (int e = -1; e <= 1; e += 2) {
      int bx = sx_ + e * (padW[p] / 2) + ox; canvas.drawFastVLine(bx, g - 5, 5, rgb(180, 180, 190));
      canvas.fillRect(bx - 1, g - 7, 3, 2, ((animT / 12 + (e > 0)) & 1) ? rgb(255, 60, 50) : rgb(255, 220, 200));
    }
    char b[8]; snprintf(b, sizeof b, "x%d", padM[p]);
    txtO(b, sx_ + ox, g - 20, &fonts::Font2, p == 2 ? rgb(255, 120, 90) : (p == 1 ? rgb(255, 220, 90) : rgb(150, 240, 150)), rgb(10, 10, 20), textdatum_t::top_center);
  }
}

static void rotPt(float cx, float cy, float a, float px, float py, int &ox_, int &oy_) {
  float c = cosf(a), s = sinf(a); ox_ = (int)floorf(cx + px * c - py * s + 0.5f); oy_ = (int)floorf(cy + px * s + py * c + 0.5f);
}
static void tri(float cx, float cy, float a, float x1, float y1, float x2, float y2, float x3, float y3, uint16_t col) {
  int ax_, ay_, bx, by, cx_, cy_; rotPt(cx, cy, a, x1, y1, ax_, ay_); rotPt(cx, cy, a, x2, y2, bx, by); rotPt(cx, cy, a, x3, y3, cx_, cy_);
  canvas.fillTriangle(ax_, ay_, bx, by, cx_, cy_, col);
}
static void seg(float cx, float cy, float a, float x1, float y1, float x2, float y2, uint16_t col) {
  int ax_, ay_, bx, by; rotPt(cx, cy, a, x1, y1, ax_, ay_); rotPt(cx, cy, a, x2, y2, bx, by); canvas.drawLine(ax_, ay_, bx, by, col);
}

static void drawShip(float sx_, float sy_, float a, bool flame, int ft) {
  uint16_t LEG = rgb(210, 210, 224), LEGD = rgb(120, 120, 138);
  if (flame) {                                                               // flamme
    float L1 = 6 + (ft % 3) * 2 + (float)rnd(0, 3), L2 = L1 * 0.6f;
    tri(sx_, sy_, a, -2.8f, 8, 2.8f, 8, 0, 8 + L1, rgb(255, 120, 30));
    tri(sx_, sy_, a, -1.6f, 8, 1.6f, 8, 0, 8 + L2, rgb(255, 220, 90));
    tri(sx_, sy_, a, -0.8f, 8, 0.8f, 8, 0, 8 + L2 * 0.6f, rgb(255, 255, 230));
  }
  seg(sx_, sy_, a, -3, 3, -8, 9, LEG); seg(sx_, sy_, a, -4, 3, -9, 9, LEGD);       // jambes
  seg(sx_, sy_, a, 3, 3, 8, 9, LEG); seg(sx_, sy_, a, 4, 3, 9, 9, LEGD);
  seg(sx_, sy_, a, -11, 9, -6, 9, LEG); seg(sx_, sy_, a, 6, 9, 11, 9, LEG);        // pieds
  seg(sx_, sy_, a, -6, 5, -3, 9, LEGD); seg(sx_, sy_, a, 6, 5, 3, 9, LEGD);
  tri(sx_, sy_, a, -2, 6, 2, 6, 3, 10, rgb(90, 90, 104)); tri(sx_, sy_, a, -2, 6, 2, 6, -3, 10, rgb(90, 90, 104));   // tuyere
  tri(sx_, sy_, a, -6, 2, 6, 2, 5, 7, skin::c(0, rgb(180, 140, 50))); tri(sx_, sy_, a, -6, 2, 5, 7, -5, 7, skin::c(1, rgb(160, 122, 40)));    // etage de descente
  int cx_, cy_; rotPt(sx_, sy_, a, 0, -2, cx_, cy_);
  canvas.fillCircle(cx_, cy_, 6, skin::c(1, rgb(150, 150, 164)));                        // cabine
  canvas.fillCircle(cx_, cy_, 5, skin::c(2, rgb(214, 214, 226)));
  int wx, wy; rotPt(sx_, sy_, a, 1.5f, -3, wx, wy);
  canvas.fillCircle(wx, wy, 2, rgb(30, 70, 150)); canvas.drawPixel(wx - 1, wy - 1, rgb(190, 230, 255));
  int ax_, ay_; rotPt(sx_, sy_, a, 0, -8.5f, ax_, ay_); int bx, by; rotPt(sx_, sy_, a, 0, -6, bx, by); canvas.drawLine(ax_, ay_, bx, by, LEG); canvas.drawPixel(ax_, ay_, rgb(255, 80, 80));
}

static void drawHUD() {
  char b[24];
  canvas.fillRect(0, 0, 84, 33, rgb(8, 8, 22)); canvas.drawRect(0, 0, 84, 33, rgb(60, 66, 110));
  txt("FUEL", 3, 3, &fonts::Font0, rgb(150, 160, 220));
  int w = (int)(fuel / fuel0 * 44); if (w < 0) w = 0; if (w > 44) w = 44;
  uint16_t fc = fuel / fuel0 > 0.5f ? rgb(90, 220, 110) : (fuel / fuel0 > 0.22f ? rgb(250, 200, 50) : (((animT / 8) & 1) ? rgb(240, 60, 50) : rgb(120, 30, 30)));
  canvas.drawRect(32, 2, 48, 9, rgb(150, 160, 220)); canvas.fillRect(34, 4, w, 5, fc);
  snprintf(b, sizeof b, "%lu", (unsigned long)score); txt(L("SCORE", "SCORE"), 3, 14, &fonts::Font0, rgb(150, 160, 220)); txt(b, 80, 14, &fonts::Font0, TFT_WHITE, textdatum_t::top_right);
  snprintf(b, sizeof b, "LV %d", level); txt(b, 3, 24, &fonts::Font0, rgb(255, 224, 120));
  for (int i = 0; i < lives; i++) { int lx = 60 + i * 7; canvas.fillTriangle(lx, 24, lx - 3, 31, lx + 3, 31, rgb(214, 214, 226)); }
  // instruments
  canvas.fillRect(SCR_W - 62, 0, 62, 33, rgb(8, 8, 22)); canvas.drawRect(SCR_W - 62, 0, 62, 33, rgb(60, 66, 110));
  int alt = (int)(gh(x) - y - 9); if (alt < 0) alt = 0;
  snprintf(b, sizeof b, "ALT %d", alt); txt(b, SCR_W - 4, 3, &fonts::Font0, rgb(150, 160, 220), textdatum_t::top_right);
  float sv = vy * 10, sh = vx * 10;
  uint16_t cv = vy > vyMax ? rgb(240, 70, 60) : (vy > vyMax * 0.8f ? rgb(250, 200, 50) : rgb(90, 220, 110));
  uint16_t ch = fabsf(vx) > vxMax ? rgb(240, 70, 60) : (fabsf(vx) > vxMax * 0.8f ? rgb(250, 200, 50) : rgb(90, 220, 110));
  uint16_t ca = fabsf(ang) > angMax ? rgb(240, 70, 60) : (fabsf(ang) > angMax * 0.8f ? rgb(250, 200, 50) : rgb(90, 220, 110));
  snprintf(b, sizeof b, "V %d", (int)floorf(sv + 0.5f)); txt(b, SCR_W - 4, 13, &fonts::Font0, cv, textdatum_t::top_right);
  int ay_ = vy >= 0 ? 15 : 19; canvas.fillTriangle(SCR_W - 58, ay_, SCR_W - 52, ay_, SCR_W - 55, vy >= 0 ? 20 : 14, cv);
  snprintf(b, sizeof b, "H %d", (int)floorf(sh + 0.5f)); txt(b, SCR_W - 4, 23, &fonts::Font0, ch, textdatum_t::top_right);
  canvas.fillTriangle(SCR_W - 58, 27, SCR_W - 58, 33 - 0, vx >= 0 ? SCR_W - 52 : SCR_W - 64, 30, ch);
  canvas.fillRect(SCR_W - 62, 31, 62, 2, ca);
}

static void render() {
  int ox = shake > 0 ? ((shake & 1) ? 2 : -2) : 0;
  drawSky(ox);
  drawTerrain(ox);
  float sx_ = SCR_W / 2 + (x - camx) + ox, sy_ = y;
  if (state != CRASH && state != OVER && state != TITLE) drawShip(sx_, sy_, ang, thrusting && state == FLY, flameT);
  if (state == TITLE) { sx_ = 120 + sinf(animT * 0.03f) * 20; sy_ = 52 + sinf(animT * 0.05f) * 4; drawShip(sx_, sy_, sinf(animT * 0.03f + 1.5f) * 0.18f, true, animT); }
  for (auto &p : parts) if (p.life > 0) {
    int px = (int)(SCR_W / 2 + (p.x - camx)) + ox, py = (int)p.y;
    if (p.kind == 1) { canvas.drawLine(px, py, px + (int)(p.vx * 3), py + (int)(p.vy * 3), p.c); }
    else { uint16_t c = p.life * 3 < p.max ? mixc(p.c, rgb(40, 40, 60), 140) : p.c; canvas.fillRect(px, py, p.life > p.max / 2 ? 2 : 1, p.life > p.max / 2 ? 2 : 1, c); }
  }
  if (state == CRASH && stateT < 12) canvas.fillCircle((int)(SCR_W / 2 + (x - camx)) + ox, (int)y, 6 + stateT * 3, stateT < 4 ? rgb(255, 255, 230) : mixc(rgb(255, 190, 60), rgb(120, 30, 10), stateT * 20));
  if (state == TITLE) {
    canvas.fillRect(0, 0, SCR_W, 34, rgb(2, 2, 14));
    txt("LUNAR", SCR_W / 2 + 2, 1, &fonts::Font4, rgb(60, 60, 110), textdatum_t::top_center, 1.2f); txt("LUNAR", SCR_W / 2, -1, &fonts::Font4, rgb(240, 240, 255), textdatum_t::top_center, 1.2f);
    txt("LANDER", SCR_W / 2, 70, &fonts::Font4, rgb(255, 210, 80), textdatum_t::top_center, 1.2f);
    if ((animT / 25) & 1) txtO(L("PRESS A", "APPUIE SUR A"), SCR_W / 2, 108, &fonts::Font2, TFT_WHITE, rgb(10, 10, 30), textdatum_t::top_center);
    if (best > 0) { char b[24]; snprintf(b, sizeof b, L("best %lu", "record %lu"), (unsigned long)best); txt(b, SCR_W / 2, 36, &fonts::Font2, rgb(255, 224, 120), textdatum_t::top_center); }
    return;
  }
  drawHUD();
  if (y < 4) { canvas.fillTriangle((int)sx_ - 5, 40, (int)sx_ + 5, 40, (int)sx_, 34, rgb(255, 224, 120)); }
  char b[32];
  if (state == READY && (stateT / 6) % 4 != 3) { snprintf(b, sizeof b, L("LEVEL %d", "NIVEAU %d"), level); txtO(b, SCR_W / 2, 52, &fonts::Font4, TFT_WHITE, rgb(10, 10, 40), textdatum_t::top_center); }
  if (state == LANDED) { snprintf(b, sizeof b, "+%d  (x%d)", gain, padM[lastPad]); ui::notice(L("LANDED!", "POSE !"), b, nullptr, P_S); }
  if (state == CRASH && stateT > 20) ui::notice("CRASH", crashWhy, nullptr, P_R);
  if (state == PAUSED) ui::pause();
  if (state == OVER) ui::gameOver("GAME OVER", score, best, (score) > 0 && (uint32_t)(score) >= (uint32_t)(best));
}

void loop() {
  uint32_t now = micros();
  if (last == 0) last = now;
  acc += now - last; last = now; if (acc > 100000) acc = 100000;
  bool a = M5.BtnA.wasPressed(), bb = M5.BtnB.wasPressed(), p = M5.BtnPWR.wasPressed();
  State before = state; (void)before;
  switch (state) {
    case TITLE: if (a || bb) { newGame(); sfx(660, 40); } break;
    case FLY: case READY: if (p) { state = PAUSED; thrusting = false; stateT = stateT; sfx(440, 50); } break;
    case PAUSED: if (p) { state = FLY; sfx(660, 50); } break;
    case LANDED: if ((a || bb) && stateT > 50) { level++; genTerrain(); startRun(); sfx(660, 40); } break;
    case OVER: if ((a || bb) && stateT > 50) { newGame(); sfx(660, 40); } break;
    default: break;
  }
  while (acc >= STEP_US) { step(); acc -= STEP_US; }
  render();
  bgm::run(6, state == FLY || state == READY);
  present(0, 0);
}

void icon(int x0, int y0) {
  for (int yy = 0; yy < 64; yy += 4) canvas.fillRect(x0, y0 + yy, 64, 4, mixc(rgb(4, 4, 24), rgb(40, 24, 70), yy * 4));
  for (int i = 0; i < 12; i++) canvas.drawPixel(x0 + (i * 23 + 5) % 62, y0 + (i * 17 + 3) % 36, rgb(220, 220, 240));
  canvas.fillCircle(x0 + 50, y0 + 12, 6, rgb(40, 100, 210)); canvas.fillCircle(x0 + 48, y0 + 11, 2, rgb(46, 150, 80));
  // sol
  for (int i = 0; i < 64; i++) {
    int g = 50 + (int)(sinf(i * 0.2f) * 3) + (i > 40 ? (i - 40) / 3 : 0); if (i > 12 && i < 40) g = 54;
    canvas.drawFastVLine(x0 + i, y0 + g, 64 - g, rgb(110, 110, 124)); canvas.drawFastVLine(x0 + i, y0 + g, 2, rgb(200, 200, 214));
  }
  canvas.fillRect(x0 + 12, y0 + 54, 28, 3, rgb(250, 200, 30)); for (int i = 0; i < 28; i += 8) canvas.fillRect(x0 + 12 + i, y0 + 54, 4, 3, rgb(30, 30, 40));
  drawShip(x0 + 26, y0 + 36, 0.12f, true, 1);
}

void preview(int cx, int cy) { drawShip((float)cx, (float)cy, sinf(millis() * 0.003f) * 0.15f, true, (int)(millis() / 60)); }
}  // namespace lander
