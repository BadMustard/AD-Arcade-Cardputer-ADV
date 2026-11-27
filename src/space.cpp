// ============================================================================
//  METEORES - M5 tenu a l'horizontale : pencher a droite / a gauche = le vaisseau suit
//  M5 (A) = jouer ; B (cote) = recalibrer ; HAUT (PWR) = pause
// ============================================================================
#include "common.h"

namespace space {

static const uint32_t STEP_US = 20000;
static const int SHIP_Y = 116;
enum State { TITLE, READY, PLAY, PAUSED, OVER };
static State state = TITLE;
static Preferences pr;
static uint32_t last = 0, acc = 0;
static float shipX = 120, shipV = 0;
static int lives = 3, invul = 0, overT = 0, animT = 0;
static uint32_t score = 0, best = 0;
static float speedMul = 1.0f; static int spawnT = 0;
struct Met { float x, y, vx, vy, r, ang, spin; uint8_t shape[8]; bool on; };
static Met mets[10];
struct Star { float x, y, v; };
static Star stars[36];
struct Spark { float x, y, vx, vy; int life; };
static Spark sparks[24];

static void spark(float x, float y, int n) {
  for (int i = 0; i < n; i++) for (auto &s : sparks) if (s.life <= 0) { float a = rnd(0, 628) / 100.f, v = rnd(5, 25) / 10.f; s = {x, y, cosf(a) * v, sinf(a) * v, (int)rnd(10, 26)}; break; }
}
static void reset() {
  shipX = SCR_W / 2; shipV = 0; lives = difi(5, 3, 2); invul = 0; score = 0; speedMul = dif(0.8f, 1.0f, 1.25f); spawnT = 30; animT = 0; overT = 0;
  for (auto &m : mets) m.on = false;
  for (auto &s : sparks) s.life = 0;
}
void begin() {
  pr.begin(dns("space"), false); best = pr.getInt("best", 0);
  tilt::load();
  for (auto &s : stars) s = {(float)rnd(0, SCR_W), (float)rnd(0, SCR_H), rnd(4, 20) / 10.f};
  reset(); state = TITLE; last = 0; acc = 0;
}

static void spawn() {
  for (auto &m : mets) if (!m.on) {
    m.on = true; m.r = rnd(5, 14); m.x = rnd(m.r, SCR_W - m.r); m.y = -m.r - 2;
    m.vy = (rnd(10, 22) / 10.f) * speedMul; m.vx = ((int)rnd(0, 20) - 10) / 20.f;
    m.ang = 0; m.spin = ((int)rnd(0, 20) - 10) / 100.f;
    for (int i = 0; i < 8; i++) m.shape[i] = rnd(72, 100);
    return;
  }
}
static void gameOver() {
  state = OVER; overT = 0;
  trophy::record(trophy::T_METEORS, score / 10); if (score / 10 > best) { best = score / 10; pr.putInt("best", best); }
  sfx(300, 120); sfx(120, 300, false);
}

static void step() {
  animT++;
  for (auto &s : stars) { bool big = s.v > 1.5f; canvas.fillRect((int)s.x, (int)s.y, big ? 2 : 1, big ? 2 : 1, big ? P_B : P_L); }
  for (auto &p : sparks) if (p.life > 0) { p.x += p.vx; p.y += p.vy; p.life--; }
  if (state == TITLE) return;
  if (state == OVER) { overT++; for (auto &m : mets) if (m.on) { m.y += m.vy; m.ang += m.spin; if (m.y > SCR_H + 20) m.on = false; } return; }
  tilt::update();
  float t = ctrl::x(); if (fabsf(t) < 0.08f) t = 0; else t = t > 0 ? t - 0.08f : t + 0.08f;
  float target = t * 5.0f;
  shipV += (target - shipV) * 0.25f;
  shipX += shipV; shipX = clampf(shipX, 9, SCR_W - 9);
  // PLAY
  score++;
  speedMul = dif(0.8f, 1.0f, 1.25f) + score / 1500.f; if (speedMul > 2.6f) speedMul = 2.6f;
  if (--spawnT <= 0) { spawn(); spawnT = (int)(rnd(18, 42) / (0.6f + speedMul * 0.4f)); if (spawnT < 6) spawnT = 6; }
  if (invul > 0) invul--;
  for (auto &m : mets) if (m.on) {
    m.x += m.vx; m.y += m.vy; m.ang += m.spin;
    if (m.y > SCR_H + m.r + 2) { m.on = false; continue; }
    float dx = m.x - shipX, dy = m.y - (SHIP_Y + 1);
    float rr = m.r * 0.82f + 5.0f;
    if (!invul && dx * dx + dy * dy < rr * rr) {
      m.on = false; spark(m.x, m.y, 14); spark(shipX, SHIP_Y, 10);
      if (--lives <= 0) { gameOver(); return; }
      invul = 80; sfx(200, 100); sfx(110, 140, false);
    }
  }
}

static void drawMeteor(const Met &m) {
  int cx = (int)m.x, cy = (int)m.y;
  float vl = sqrtf(m.vx * m.vx + m.vy * m.vy); if (vl < 0.01f) vl = 1; float dx = -m.vx / vl, dy = -m.vy / vl;       // direction de la traine (derriere)
  static const uint16_t TC[5] = {P_G, rgb(236, 130, 50), P_R, rgb(120, 50, 130), rgb(60, 40, 120)};
  for (int k = 4; k >= 0; k--) {                                                                                 // traine : 5 disques de plus en plus petits et froids
    int rr = (int)(m.r * (0.85f - k * 0.13f)); if (rr < 1) rr = 1; int d = (int)(m.r * (0.5f + k * 0.75f));
    canvas.fillCircle(cx + (int)(dx * d), cy + (int)(dy * d), rr, TC[k]);
  }
  int px[8], py[8];
  for (int i = 0; i < 8; i++) { float a = m.ang + i * 0.7854f, r = m.r * m.shape[i] / 100.f; px[i] = cx + (int)(cosf(a) * r); py[i] = cy + (int)(sinf(a) * r); }
  const uint16_t LT = rgb(222, 172, 124), MD = rgb(164, 112, 92), DK = rgb(98, 66, 88);
  for (int i = 0; i < 8; i++) {                                                                                  // eclairage depuis le haut-gauche
    float am = m.ang + (i + 0.5f) * 0.7854f, dt = cosf(am) * -0.707f + sinf(am) * -0.707f;
    canvas.fillTriangle(cx, cy, px[i], py[i], px[(i + 1) & 7], py[(i + 1) & 7], dt > 0.35f ? LT : (dt > -0.35f ? MD : DK));
  }
  for (int i = 0; i < 8; i++) canvas.drawLine(px[i], py[i], px[(i + 1) & 7], py[(i + 1) & 7], P_K);              // contour noir
  if (m.r > 6) { int ox = (int)(m.r * 0.3f * cosf(m.ang + 1.0f)), oy = (int)(m.r * 0.3f * sinf(m.ang + 1.0f)); canvas.fillCircle(cx + ox, cy + oy, m.r / 4 + 1, DK); canvas.drawPixel(cx + ox - 1, cy + oy - 1, MD); }
  if (m.r > 10) { int ox = (int)(m.r * 0.5f * cosf(m.ang + 3.6f)), oy = (int)(m.r * 0.5f * sinf(m.ang + 3.6f)); canvas.fillCircle(cx + ox, cy + oy, 2, DK); }
  canvas.fillRect(cx - m.r / 3, cy - m.r / 2, 2, 2, P_B);                                                        // reflet
}
static void drawShip(int x, int y, float bank, bool flame) {
  int w = 10, sh = (int)(bank * 2);
  uint16_t body = skin::c(0, P_L), dk = skin::c(1, P_M), red = skin::c(3, P_R), lt = skin::c(2, P_B);
  if (flame) { int f = 4 + (animT & 3) * 2; canvas.fillTriangle(x - 4, y + 7, x + 4, y + 7, x, y + 9 + f + 2, P_K); canvas.fillTriangle(x - 3, y + 7, x + 3, y + 7, x, y + 8 + f, P_G); canvas.fillTriangle(x - 1, y + 7, x + 1, y + 7, x, y + 7 + f / 2, P_B); }
  canvas.fillTriangle(x - w - 1, y + 9, x, y - 3, x + w + 1, y + 9, P_K);                                    // contour des ailes
  canvas.fillTriangle(x - w, y + 8, x, y - 2, x + w, y + 8, dk);
  canvas.fillTriangle(x - 5 + sh, y + 10, x + sh, y - 13, x + 5 + sh, y + 10, P_K);                         // contour du fuselage
  canvas.fillTriangle(x - 4 + sh, y + 9, x + sh, y - 12, x + 4 + sh, y + 9, body);
  canvas.fillTriangle(x + sh, y - 12, x + sh, y + 9, x + 4 + sh, y + 9, dk);                               // cote ombre
  canvas.fillTriangle(x - 2 + sh, y - 5, x + sh, y - 12, x + 2 + sh, y - 5, red);
  canvas.fillRect(x - 1 + sh, y - 3, 3, 4, P_K); canvas.fillRect(x + sh, y - 2, 1, 2, P_S);               // cockpit
  canvas.fillRect(x - w, y + 6, 3, 3, red); canvas.fillRect(x + w - 2, y + 6, 3, 3, red);
  canvas.drawPixel(x - 2 + sh, y + 2, lt);
}
static void backdrop() {
  ui::night(100);
  canvas.fillRect(0, 100, SCR_W, 35, rgb(40, 28, 96)); canvas.fillRect(0, 100, SCR_W, 1, P_K);
  for (int i = 0; i < 3; i++) canvas.fillCircle(70 + i * 70 + (i & 1) * 20, 40 + i * 14, 20 - i * 3, rgb(34 + i * 4, 24, 86 + i * 6));   // nebuleuse
  canvas.fillCircle(26, 112, 12, P_K); canvas.fillCircle(26, 112, 11, P_R); canvas.fillCircle(24, 110, 9, P_G); canvas.fillCircle(21, 107, 3, P_B); canvas.fillRect(27, 116, 4, 2, P_R);   // planete
}
static void render() {
  backdrop();
  for (auto &s : stars) { uint8_t b = 90 + (int)(s.v * 70); canvas.drawPixel((int)s.x, (int)s.y, rgb(b, b, b)); if (s.v > 1.5f) canvas.drawPixel((int)s.x, (int)s.y - 1, rgb(b / 2, b / 2, b / 2)); }
  for (auto &m : mets) if (m.on) drawMeteor(m);
  for (auto &p : sparks) if (p.life > 0) canvas.fillRect((int)p.x, (int)p.y, 2, 2, p.life > 12 ? rgb(255, 220, 120) : rgb(230, 90, 40));
  if (state != OVER && state != TITLE && (invul == 0 || (animT & 4))) drawShip((int)shipX, SHIP_Y, shipV / 5.0f, true);
  if (state == TITLE) drawShip(SCR_W / 2, 100, 0, true);
  if (state == PLAY || state == PAUSED || state == READY) {
    char b[16]; snprintf(b, sizeof b, "%lu", (unsigned long)(score / 10));
    int sw = (int)strlen(b) * 8 + 10; canvas.fillRect(2, 2, sw, 17, P_K); canvas.fillRect(3, 3, sw - 2, 15, P_D); canvas.fillRect(3, 3, sw - 2, 1, P_M);
    txt(b, 7, 3, &fonts::Font2, P_B);
    { int lw = lives * 14 + 6, lx = SCR_W - lw - 2; canvas.fillRect(lx, 2, lw, 17, P_K); canvas.fillRect(lx + 1, 3, lw - 2, 15, P_D); canvas.fillRect(lx + 1, 3, lw - 2, 1, P_M); }
    for (int i = 0; i < lives; i++) { int hx = SCR_W - 12 - i * 14; canvas.fillTriangle(hx - 5, 17, hx, 4, hx + 5, 17, P_K); canvas.fillTriangle(hx - 3, 16, hx, 6, hx + 3, 16, skin::c(0, P_L)); canvas.fillTriangle(hx, 6, hx, 16, hx + 3, 16, P_M); }
  }
  if (state == TITLE) {
    txtO(L("METEORS","METEORES"), SCR_W / 2, 10, &fonts::Font4, P_G, P_K, textdatum_t::top_center, 1.5f);
    if (best > 0) { char b[24]; snprintf(b, sizeof b, L("best %lu","record %lu"), (unsigned long)best); txt(b, SCR_W / 2, 74, &fonts::Font2, P_G, textdatum_t::top_center); }
  }
  if (state == PAUSED) ui::pause();
  if (state == OVER) ui::gameOver("SHIP DESTROYED", (uint32_t)(score / 10), best, ((uint32_t)(score / 10)) > 0 && (uint32_t)((uint32_t)(score / 10)) >= (uint32_t)(best));
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
    case OVER: if (a && overT > 40) { reset(); state = PLAY; sfx(660, 40); } break;
    default: break;
  }
  while (acc >= STEP_US) { if (state != PAUSED) step(); acc -= STEP_US; }
  render();
  bgm::run(8, state == PLAY);
  present(0, 0);
}

void icon(int x, int y) {
  canvas.fillRoundRect(x, y, 64, 64, 6, P_K);
  canvas.fillRect(x + 2, y + 46, 60, 16, P_D);
  canvas.fillRect(x + 8, y + 8, 2, 2, P_B); canvas.fillRect(x + 50, y + 6, 2, 2, P_L); canvas.fillRect(x + 30, y + 4, 1, 1, P_L); canvas.fillRect(x + 56, y + 34, 2, 2, P_B);
  canvas.fillCircle(x + 46, y + 20, 11, P_K); canvas.fillCircle(x + 46, y + 20, 10, P_M); canvas.fillCircle(x + 43, y + 17, 7, P_L); canvas.fillCircle(x + 50, y + 25, 3, P_D); canvas.fillRect(x + 40, y + 14, 3, 2, P_B);
  canvas.fillCircle(x + 14, y + 32, 7, P_K); canvas.fillCircle(x + 14, y + 32, 6, P_M); canvas.fillCircle(x + 12, y + 30, 4, P_L); canvas.fillCircle(x + 5, y + 24, 3, P_R);
  { int sx = x + 30, sy = y + 46; canvas.fillTriangle(sx - 11, sy + 10, sx, sy - 1, sx + 11, sy + 10, P_K); canvas.fillTriangle(sx - 9, sy + 9, sx, sy + 1, sx + 9, sy + 9, P_M); canvas.fillTriangle(sx - 5, sy + 12, sx, sy - 13, sx + 5, sy + 12, P_K); canvas.fillTriangle(sx - 4, sy + 11, sx, sy - 11, sx + 4, sy + 11, P_L); canvas.fillTriangle(sx - 2, sy - 6, sx, sy - 11, sx + 2, sy - 6, P_R); canvas.fillRect(sx, sy - 3, 2, 3, P_S); canvas.fillTriangle(sx - 3, sy + 12, sx + 3, sy + 12, sx, sy + 17, P_G); }
}

void preview(int cx, int cy) { drawShip(cx, cy, sinf(millis() * 0.003f) * 0.6f, true); }
}  // namespace space
