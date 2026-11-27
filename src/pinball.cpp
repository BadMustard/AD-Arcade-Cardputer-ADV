// ============================================================================
//  EMBER PINBALL - flipper (jeu original, theme braise / dragon)
//  B = flipper gauche, A = flipper droit, maintenir A (bille au lanceur) = lancer plus fort
//  Secouer la console = bousculer la bille (trop = TILT). PWR = pause.
//  Plateau + geometrie : tools/art/pinball.py -> src/pinball_art.h
// ============================================================================
#include "common.h"
#include "pinball_art.h"

namespace pinball {
using namespace pbart;

static const uint32_t STEP_US = 20000;
static const int NSUB = 6;                   // sous-pas physiques par image
static const int TABX = 56;                   // position du plateau a l'ecran
static const float BALL_R = 3.6f;               // rayon de la bille
static const float GRAV = 0.0105f, VMAX = 2.9f;
#define IX(r, t) ((r) * 5 + (t))
static const uint8_t CWH = 45, CBK = 46, CGD = 47;

static uint8_t* fb = nullptr; static bool swap16 = true;
template <typename Fn> static void forPix(uint8_t id, Fn fn) {
  const Spr& s = SPR[id]; const int w = s.w, h = s.h; const uint8_t* d = BLOB + s.off;
  if (s.fmt == 0) { const uint8_t* lut = d; const uint8_t* px = d + s.nlut; int n = 0;
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++, n++) { uint8_t b = px[n >> 1]; uint8_t v = (n & 1) ? (b & 15) : (b >> 4); if (v) fn(i, j, lut[v - 1]); } }
  else { int n = 0, tot = w * h; const uint8_t* p = d;
    while (n < tot) { int run = p[0]; uint8_t c = p[1]; p += 2; if (c != 255) { int j = n / w, i = n - j * w; for (int k = 0; k < run; k++) { fn(i, j, c); if (++i >= w) { i = 0; j++; } } } n += run; } }
}
static void blit(uint8_t id, int x, int y) {
  forPix(id, [&](int i, int j, uint8_t c) { int X = x + i, Y = y + j; if ((unsigned)X < 240 && (unsigned)Y < 135) fb[Y * 240 + X] = c; });
}
static void blitTable(int camY) {                                     // plateau : seules les lignes visibles sont ecrites
  forPix(S_TABLE, [&](int i, int j, uint8_t c) { int Y = j - camY; if ((unsigned)Y < 135) fb[Y * 240 + TABX + i] = c; });
}
static inline void pset(int x, int y, uint8_t c) { if ((unsigned)x < 240 && (unsigned)y < 135) fb[y * 240 + x] = c; }
static inline uint16_t sw(uint16_t c) { return swap16 ? (uint16_t)((c >> 8) | (c << 8)) : c; }
static void convert() {
  uint16_t* out = (uint16_t*)canvas.getBuffer(); uint16_t lut[48];
  for (int i = 0; i < 48; i++) lut[i] = sw(PAL[i]);
  for (int p = 0; p < 240 * 135; p++) out[p] = lut[fb[p]];
}

// ------------------------------------------------------------------ etat
enum State { TITLE, PLAY, PAUSED, OVER };
static State state = TITLE;
static uint32_t last = 0, acc = 0, tick = 0;
static bool aPend, bPend, pPend;
static Preferences pr; static uint32_t best = 0;
static float bx, by, vx, vy, camY = 165;
static float flA[2], flW[2];                                           // angle + vitesse angulaire des flippers
static const float REST[2] = {0.4887f, 2.6529f}, UP[2] = {-0.384f, 3.5256f};   // 28 / 152 deg ; -22 / 202 deg
static uint32_t score; static int balls, mult, ballSave, saucerT, bumpLit[3], tgtT, msgT, nudges, nudgeT, plunge, launchedT;
static bool inLane, tilted, targetUp[3], lane[3]; static int saucerCd = 0;
static const char* msg = "";
static float lastTx = 0;

static void say(const char* m) { msg = m; msgT = 110; }
static void addScore(uint32_t v) { score += v * mult; }
static void newBall() {
  bx = SHOOT[0]; by = SHOOT[1]; vx = vy = 0; inLane = true; tilted = false; nudges = 0; plunge = 0; ballSave = 0; saucerT = 0;
}
static void newGame() {
  score = 0; balls = 3; mult = 1; for (int i = 0; i < 3; i++) { targetUp[i] = true; lane[i] = false; bumpLit[i] = 0; }
  tgtT = 0; msgT = 0; flA[0] = REST[0]; flA[1] = REST[1]; newBall(); state = PLAY;
}
static void gameOver() {
  state = OVER; sfx(220, 200); sfx(165, 400, false);
  if (score > best) { best = score; pr.begin("pinball", false); pr.putUInt("best", best); pr.end(); }
}

// ------------------------------------------------------------------ physique
static bool collideSeg(float ax, float ay, float qx, float qy, float rad, float e, float& nx, float& ny) {
  float dx = qx - ax, dy = qy - ay, L2 = dx * dx + dy * dy; if (L2 < 0.01f) return false;
  float t = ((bx - ax) * dx + (by - ay) * dy) / L2; t = t < 0 ? 0 : (t > 1 ? 1 : t);
  float cx = ax + dx * t, cy = ay + dy * t, ex = bx - cx, ey = by - cy, d2 = ex * ex + ey * ey, R = BALL_R + rad;
  if (d2 >= R * R) return false;
  float d = sqrtf(d2) + 1e-4f; nx = ex / d; ny = ey / d;
  bx = cx + nx * R; by = cy + ny * R;
  float vn = vx * nx + vy * ny;
  if (vn < 0) { vx -= (1 + e) * vn * nx; vy -= (1 + e) * vn * ny; }
  return true;
}
static void flipperCollide(int s) {
  const int16_t* P = s ? FLIP_R : FLIP_L;
  float ca = cosf(flA[s]), sa = sinf(flA[s]), tx = P[0] + ca * FLIP_LEN, ty = P[1] + sa * FLIP_LEN;
  float dx = tx - P[0], dy = ty - P[1], L2 = dx * dx + dy * dy;
  float t = ((bx - P[0]) * dx + (by - P[1]) * dy) / L2; t = t < 0 ? 0 : (t > 1 ? 1 : t);
  float cx = P[0] + dx * t, cy = P[1] + dy * t, ex = bx - cx, ey = by - cy, d2 = ex * ex + ey * ey;
  float rad = 3.2f - 1.2f * t, R = BALL_R + rad;
  if (d2 >= R * R) return;
  float d = sqrtf(d2) + 1e-4f, nx = ex / d, ny = ey / d;
  bx = cx + nx * R; by = cy + ny * R;
  float rx = cx - P[0], ry = cy - P[1], fvx = -flW[s] * ry, fvy = flW[s] * rx;          // vitesse du flipper au point de contact
  float rvn = (vx - fvx) * nx + (vy - fvy) * ny;
  if (rvn < 0) { vx -= 1.35f * rvn * nx; vy -= 1.35f * rvn * ny; if (fabsf(flW[s]) > 0.05f) sfx(330, 15); }
}
static void physicsStep(bool fl, bool fr) {
  // flippers
  for (int s = 0; s < 2; s++) {
    bool on = (s == 0 ? fl : fr) && !tilted;
    float target = on ? UP[s] : REST[s], dir = target > flA[s] ? 1.f : -1.f;
    float sp = on ? 0.30f : 0.12f, old = flA[s];
    if (fabsf(target - flA[s]) <= sp) flA[s] = target; else flA[s] += dir * sp;
    flW[s] = flA[s] - old;
  }
  if (saucerT > 0) return;
  vy += GRAV;
  float sp = sqrtf(vx * vx + vy * vy); if (sp > VMAX) { vx *= VMAX / sp; vy *= VMAX / sp; }
  bx += vx; by += vy;
  float nx, ny;
  for (int i = 0; i < (int)(sizeof(SEGS) / sizeof(SEGS[0])); i++) {
    const int16_t* g = SEGS[i]; int k = g[4];
    if (k == K_TARGET && !targetUp[g[5]]) continue;
    if (k == K_GATE && by > g[1]) continue;                            // portillon a sens unique
    if (!collideSeg(g[0], g[1], g[2], g[3], k == K_GATE ? 0.5f : 1.0f, k == K_SLING ? 0.2f : 0.45f, nx, ny)) continue;
    if (k == K_SLING) { vx += nx * 1.4f; vy += ny * 1.4f; addScore(10); sfx(700, 15); }
    else if (k == K_TARGET) {
      targetUp[g[5]] = false; addScore(500); sfx(900, 25); sfx(1200, 25, false);
      if (!targetUp[0] && !targetUp[1] && !targetUp[2]) { addScore(2500); tgtT = 120; say(L("TARGETS +2500", "CIBLES +2500")); }
    }
  }
  for (int i = 0; i < 3; i++) {                                         // bumpers
    float dx = bx - BUMPERS[i][0], dy = by - BUMPERS[i][1], R = BALL_R + BUMPERS[i][2], d2 = dx * dx + dy * dy;
    if (d2 < R * R) {
      float d = sqrtf(d2) + 1e-4f; nx = dx / d; ny = dy / d; bx = BUMPERS[i][0] + nx * R; by = BUMPERS[i][1] + ny * R;
      float vn = vx * nx + vy * ny; if (vn < 0) { vx -= 2 * vn * nx; vy -= 2 * vn * ny; }
      vx += nx * 1.3f; vy += ny * 1.3f; bumpLit[i] = 10; addScore(100); sfx(500 + i * 150, 20);
    }
  }
  for (int s = 0; s < 2; s++) flipperCollide(s);
  // couloirs du haut
  for (int i = 0; i < 3; i++) if (by > LANES[i][1] - 6 && by < LANES[i][1] + 6 && fabsf(bx - LANES[i][0]) < 6 && vy > 0 && !lane[i]) {
    lane[i] = true; addScore(250); sfx(1300, 20);
    if (lane[0] && lane[1] && lane[2]) { lane[0] = lane[1] = lane[2] = false; if (mult < 5) mult++; addScore(1000); static char b[24]; snprintf(b, sizeof b, L("MULTIPLIER X%d", "MULTIPLICATEUR X%d"), mult); say(b); sfx(1568, 80, false); }
  }
  // trou (saucer)
  { float dx = bx - SAUCER[0], dy = by - SAUCER[1]; if (!saucerCd && dx * dx + dy * dy < 16 && vx * vx + vy * vy < 6.0f) { saucerT = 55; bx = SAUCER[0]; by = SAUCER[1]; vx = vy = 0; addScore(750); sfx(392, 60); } }
}

// ------------------------------------------------------------------ boucle de jeu
static void step(bool a, bool b, bool aP, bool bP) {
  tick++;
#ifdef ZTEST
  if (getenv("ZLOG") && tick % 50 == 0) fprintf(stderr, "t=%u ball=%.1f,%.1f v=%.2f,%.2f lane=%d plunge=%d saucer=%d score=%lu balls=%d%c", (unsigned)tick, bx, by, vx, vy, (int)inLane, plunge, saucerT, (unsigned long)score, balls, 10);
#endif
  if (msgT > 0) msgT--;
  for (auto& l : bumpLit) if (l > 0) l--;
  if (tgtT > 0 && --tgtT == 0) for (auto& t : targetUp) t = true;
  if (nudgeT > 0) nudgeT--; else nudges = 0;
  // couloirs : les flippers decalent les lumieres (on vise le couloir eteint)
  if (bP) { bool t = lane[0]; lane[0] = lane[1]; lane[1] = lane[2]; lane[2] = t; }
  if (aP && !inLane) { bool t = lane[2]; lane[2] = lane[1]; lane[1] = lane[0]; lane[0] = t; }
  // lanceur
  if (inLane && bx > 112 && by > 270) {
    vx = vy = 0; bx = SHOOT[0]; by = SHOOT[1];
    if (a) { if (plunge < 60) plunge++; }
    else if (plunge > 0) {                                  // meme un appui bref atteint le haut du plateau
      vy = -(2.45f + plunge / 60.0f * 0.45f); plunge = 0; inLane = false; ballSave = 420; launchedT = 0; sfx(160, 60); sfx(240, 80, false);
    }
  } else if (!inLane && bx > 113 && by > 268 && vy >= 0) { inLane = true; plunge = 0; }   // retombee dans le couloir : retour au lanceur
  else inLane = false;
  // bousculer (secouer la console)
  tilt::update(); float tx = tilt::x();
  if (fabsf(tx - lastTx) > 0.45f && !inLane && !tilted) {
    vx += (tx > lastTx ? 0.6f : -0.6f); vy -= 0.25f; nudges++; nudgeT = 200; sfx(120, 40);
    if (nudges > 3) { tilted = true; say("TILT!"); sfx(110, 400); }
  }
  lastTx = tx;
  if (saucerCd > 0) saucerCd--;
  if (saucerT > 0 && --saucerT == 0) { vx = -1.5f; vy = -2.2f; bx -= 3; by -= 4; saucerCd = 45; sfx(600, 40); }
  if (ballSave > 0) ballSave--;
  for (int i = 0; i < NSUB; i++) physicsStep(b, a && !(inLane && bx > 112));
  // vidange
  if (by > TH + 6) {
    if (ballSave > 0) { say(L("BALL SAVED!", "BILLE SAUVEE !")); newBall(); sfx(880, 60); return; }
    balls--; sfx(200, 150);
    if (balls <= 0) { gameOver(); return; }
    say(L("NEXT BALL", "BILLE SUIVANTE")); newBall();
  }
  float target = by - 78; if (target < 0) target = 0; if (target > TH - 135) target = TH - 135;
  camY += (target - camY) * 0.18f;
}

// ------------------------------------------------------------------ rendu
static void drawFlipper(int s, int cam) {
  const int16_t* P = s ? FLIP_R : FLIP_L; float ca = cosf(flA[s]), sa = sinf(flA[s]);
  for (int pass = 0; pass < 2; pass++) for (int t = 0; t <= FLIP_LEN; t++) {
    float x = TABX + P[0] + ca * t, y = P[1] - cam + sa * t, r = (3.2f - t * 0.06f) + (pass ? 0 : 1);
    for (int yy = -4; yy <= 4; yy++) for (int xx = -4; xx <= 4; xx++) {
      float d = sqrtf((float)(xx * xx + yy * yy)); if (d > r) continue;
      uint8_t c = pass == 0 ? IX(5, 0) : (yy < -1 ? CWH : (yy < 1 ? IX(5, 3) : IX(5, 2)));
      if (pass == 1 && t >= FLIP_LEN - 1) c = CWH;
      pset((int)x + xx, (int)y + yy, c);
    }
  }
  pset(TABX + P[0], P[1] - cam, CGD);
}
static void render() {
  int cam = (int)camY;
  memset(fb, CBK, 240 * 135);
  blit(S_PANEL_L, 0, 0); blit(S_PANEL_R, 184, 0);
  blitTable(cam);
  // lumieres
  for (int i = 0; i < 3; i++) { int x = TABX + LANES[i][0], y = LANES[i][1] + 15 - cam; if (lane[i]) for (int yy = -3; yy <= 3; yy++) for (int xx = -3; xx <= 3; xx++) if (xx * xx + yy * yy <= 9) pset(x + xx, y + yy, (xx + yy < -1) ? CWH : CGD); }
  for (int i = 0; i < 4; i++) if (mult >= i + 2) { int x = TABX + INSERTS[i][0], y = INSERTS[i][1] - cam; for (int yy = -3; yy <= 3; yy++) for (int xx = -4; xx <= 4; xx++) if (xx * xx * 9 + yy * yy * 16 <= 144) pset(x + xx, y + yy, (tick / 8 + i) & 1 ? CGD : IX(2, 4)); }
  for (int i = 0; i < 3; i++) blit(targetUp[i] ? S_TARGET_UP : S_TARGET_DOWN, TABX + 6, 138 + i * 16 - cam);
  for (int i = 0; i < 3; i++) blit(bumpLit[i] ? S_BUMPER_LIT : S_BUMPER, TABX + BUMPERS[i][0] - 10, BUMPERS[i][1] - 10 - cam);
  if (saucerT > 0 && (tick & 4)) for (int a = 0; a < 24; a++) pset(TABX + SAUCER[0] + (int)(cosf(a * 0.26f) * 8), SAUCER[1] - cam + (int)(sinf(a * 0.26f) * 8), CGD);
  for (int s = 0; s < 2; s++) drawFlipper(s, cam);
  // ressort du lanceur
  { int top = 294 - cam + plunge / 6; for (int y = top; y < 300 - cam; y++) for (int x = TABX + 114; x < TABX + 123; x++) pset(x, y, ((y - top) % 3 == 0) ? IX(7, 4) : IX(7, 2)); }
  blit(S_BALL, TABX + (int)bx - 4, (int)by - 4 - cam);
  convert();
  // panneau de gauche : score
  char b[32];
  txt(L("SCORE", "SCORE"), 28, 38, &fonts::Font0, rgb(255, 214, 120), textdatum_t::top_center);
  snprintf(b, sizeof b, "%lu", (unsigned long)score); txt(b, 28, 50, &fonts::Font2, rgb(255, 248, 232), textdatum_t::top_center);
  txt(L("BALL", "BILLE"), 28, 72, &fonts::Font0, rgb(255, 214, 120), textdatum_t::top_center);
  for (int i = 0; i < 3; i++) { uint16_t c = i < balls ? rgb(220, 222, 236) : rgb(50, 50, 70); canvas.fillCircle(16 + i * 12, 87, 4, c); if (i < balls) canvas.drawPixel(15 + i * 12, 85, 0xFFFF); }
  snprintf(b, sizeof b, "x%d", mult); txtO(b, 28, 98, &fonts::Font4, mult > 1 ? rgb(255, 196, 60) : rgb(150, 140, 120), rgb(40, 20, 10), textdatum_t::top_center);
  if (msgT > 0 && ((msgT / 6) & 1 || msgT > 60)) {                    // bandeau centre en haut du plateau
    int w = strlen(msg) * 6 + 10; canvas.fillRect(TABX + 64 - w / 2, 4, w, 13, rgb(40, 16, 10)); canvas.drawRect(TABX + 64 - w / 2, 4, w, 13, rgb(242, 184, 58));
    txt(msg, TABX + 64, 7, &fonts::Font0, rgb(255, 214, 90), textdatum_t::top_center);
  }
  if (inLane && state == PLAY) { canvas.fillRect(188, 96, 48, 30, rgb(20, 24, 50)); txt(L("HOLD A", "TIENS A"), 212, 100, &fonts::Font0, rgb(255, 214, 120), textdatum_t::top_center); txt(L("LAUNCH", "LANCER"), 212, 112, &fonts::Font0, rgb(255, 248, 232), textdatum_t::top_center); }
  else { txt("B", 196, 112, &fonts::Font2, rgb(255, 214, 120), textdatum_t::top_center); txt("A", 228, 112, &fonts::Font2, rgb(255, 214, 120), textdatum_t::top_center); txt(L("flippers", "flippers"), 212, 100, &fonts::Font0, rgb(200, 200, 220), textdatum_t::top_center); }
}
static void titleScreen() {
  camY += ((float)(TH - 135) * (0.5f + 0.5f * sinf(tick * 0.01f)) - camY) * 0.05f;
  render();
  canvas.fillRoundRect(64, 34, 112, 58, 6, rgb(30, 18, 12)); canvas.drawRoundRect(64, 34, 112, 58, 6, rgb(242, 184, 58));
  txtO("EMBER", 120, 40, &fonts::Font4, rgb(255, 160, 60), rgb(60, 14, 10), textdatum_t::top_center);
  txt("PINBALL", 120, 58, &fonts::Font2, rgb(255, 214, 120), textdatum_t::top_center);
  char b[32]; if (best) { snprintf(b, sizeof b, L("BEST %lu", "RECORD %lu"), (unsigned long)best); txt(b, 120, 76, &fonts::Font0, rgb(200, 190, 170), textdatum_t::top_center); }
  if ((tick / 25) & 1) txt(L("PRESS A", "APPUIE SUR A"), 120, 100, &fonts::Font0, rgb(255, 248, 232), textdatum_t::top_center);
}

void begin() {
  fb = idxBuffer(); if (fb) memset(fb, CBK, 240 * 135);
  canvas.drawPixel(0, 0, 0x1234); swap16 = ((uint16_t*)canvas.getBuffer())[0] != 0x1234;
  pr.begin("pinball", true); best = pr.getUInt("best", 0); pr.end();
  tilt::load(); state = TITLE; last = 0; acc = 0; tick = 0; aPend = bPend = pPend = false;
  score = 0; balls = 3; mult = 1; for (int i = 0; i < 3; i++) { targetUp[i] = true; lane[i] = false; }
  flA[0] = REST[0]; flA[1] = REST[1]; bx = SHOOT[0]; by = SHOOT[1]; camY = TH - 135;
}
void loop() {
  if (!fb) { canvas.fillScreen(0); txt("NO MEMORY", 120, 60, &fonts::Font2, 0xFFFF, textdatum_t::top_center); present(0, 0); return; }
  uint32_t now = micros(); if (!last) last = now;
  acc += now - last; last = now; if (acc > 100000) acc = 100000;
  aPend |= M5.BtnA.wasPressed(); bPend |= M5.BtnB.wasPressed(); pPend |= M5.BtnPWR.wasPressed();
  while (acc >= STEP_US) {
    bool aP = aPend, bP = bPend, pP = pPend; aPend = bPend = pPend = false;
    switch (state) {
      case TITLE: tick++; if (aP) { newGame(); sfx(660, 60); sfx(990, 100, false); } break;
      case PLAY: if (pP) { state = PAUSED; sfx(440, 50); break; } step(M5.BtnA.isPressed(), M5.BtnB.isPressed(), aP, bP); break;
      case PAUSED: tick++; if (pP || aP) state = PLAY; break;
      case OVER: tick++; if (aP && tick > 40) newGame(); break;
    }
    acc -= STEP_US;
  }
  if (state == TITLE) titleScreen();
  else {
    render();
    if (state == PAUSED) ui::pause();
    if (state == OVER) ui::gameOver(L("GAME OVER", "PARTIE FINIE"), score, best, score > 0 && score >= best);
  }
  bgm::run(13, state == PLAY);
  present(0, 0);
}
void icon(int x, int y) {
  ui::tile(x, y, rgb(30, 40, 90));
  canvas.fillRect(x + 8, y + 6, 48, 52, rgb(20, 30, 70));
  canvas.fillCircle(x + 22, y + 22, 7, rgb(230, 80, 60)); canvas.fillCircle(x + 22, y + 22, 3, rgb(255, 214, 72));
  canvas.fillCircle(x + 42, y + 20, 7, rgb(230, 80, 60)); canvas.fillCircle(x + 42, y + 20, 3, rgb(255, 214, 72));
  canvas.fillCircle(x + 32, y + 36, 4, rgb(220, 222, 236)); canvas.drawPixel(x + 31, y + 35, 0xFFFF);
  for (int t = 0; t < 12; t++) { canvas.fillCircle(x + 16 + t, y + 50 + t / 3, 2, rgb(230, 80, 60)); canvas.fillCircle(x + 48 - t, y + 50 + t / 3, 2, rgb(230, 80, 60)); }
}
}  // namespace pinball
