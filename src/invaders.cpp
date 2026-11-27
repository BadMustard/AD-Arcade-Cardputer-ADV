// ============================================================================
//  INVADERS - M5 a l'horizontale : pencher = deplacer le canon ; M5 (A) = tirer
//  B (cote) = recalibrer (ecran titre) ; HAUT (PWR) = pause
// ============================================================================
#include "common.h"

namespace invaders {

static const uint32_t STEP_US = 20000;
static const int ACOLS = 8, AROWS = 4, APX = 22, APY = 15, AW = 16, AH = 14;
static const int SHIP_Y = 117, HUD_H = 14, GROUND_Y = 131;
enum State { TITLE, READY, PLAY, PAUSED, WAVEUP, OVER };
static State state = TITLE;
static Preferences pr;
static uint32_t last = 0, acc = 0;

// 3 sortes d'envahisseurs, 2 images chacun (8 x 7 points, dessines en x2)
static const uint8_t SPR[3][2][7] = {
  {{0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x24, 0x42}, {0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x5A, 0xA5}},
  {{0x24, 0xBD, 0xFF, 0xDB, 0x7E, 0x24, 0x42}, {0x24, 0x3C, 0xFF, 0xDB, 0x7E, 0x42, 0x81}},
  {{0x3C, 0x7E, 0xFF, 0xE7, 0xFF, 0x3C, 0x66}, {0x3C, 0x7E, 0xFF, 0xE7, 0xFF, 0x66, 0xC3}},
};
static const uint8_t ROWTYPE[AROWS] = {0, 1, 1, 2};
static const int ROWPTS[AROWS] = {30, 20, 20, 10};
static const uint16_t TYPECOL[3] = {P_G, P_S, P_R};

static uint8_t alive[AROWS][ACOLS]; static int nAlive = 0;
static float gx, gy; static int dir = 1, moveT = 0, frame = 0, noteI = 0;
static float padX = 120, padV = 0;
static int lives = 3, wave = 1, invul = 0, endT = 0, animT = 0;
static uint32_t score = 0, best = 0;
static bool pb = false; static float pbx, pby;
struct Bomb { float x, y; bool on; }; static Bomb bombs[3]; static int bombT = 60;
struct Boom { float x, y; int t; uint16_t col; }; static Boom booms[8];
static bool ufoOn = false; static float ufoX; static int ufoDir = 1, ufoT = 900, ufoPts = 50;
static uint8_t shield[3][5][12];
static const int SH_X[3] = {36, 108, 180}, SH_Y = 99;
static const char* SHAPE[5] = {"011111111110", "111111111111", "111111111111", "111100001111", "111000000111"};

static void boom(float x, float y, uint16_t col) { for (auto &b : booms) if (b.t <= 0) { b = {x, y, 12, col}; return; } }
static void resetShields() { for (int s = 0; s < 3; s++) for (int r = 0; r < 5; r++) for (int c = 0; c < 12; c++) shield[s][r][c] = SHAPE[r][c] == '1'; }

static void startWave() {
  gx = 20; gy = 20 + (wave > 5 ? 4 : wave - 1) * 6; dir = 1; moveT = 0; frame = 0;
  memset(alive, 1, sizeof alive); nAlive = AROWS * ACOLS;
  for (auto &b : bombs) b.on = false;
  pb = false; for (auto &b : booms) b.t = 0;
  bombT = 60; resetShields();
}
static void reset() {
  lives = difi(5, 3, 2); wave = 1; score = 0; padX = SCR_W / 2; padV = 0; invul = 0; animT = 0; endT = 0;
  ufoOn = false; ufoT = rnd(700, 1200);
  startWave();
}
void begin() {
  pr.begin(dns("invaders"), false); best = pr.getInt("best", 0);
  tilt::load();
  reset(); state = TITLE; last = 0; acc = 0;
}

static void gameOver() {
  state = OVER; endT = 0;
  trophy::record(trophy::T_INVADERS, score); if (score > best) { best = score; pr.putInt("best", best); }
  sfx(300, 120); sfx(120, 320, false);
}
static void playerHit() {
  boom(padX, SHIP_Y + 3, rgb(255, 200, 80)); boom(padX - 6, SHIP_Y, rgb(255, 120, 60)); boom(padX + 6, SHIP_Y, rgb(255, 120, 60));
  for (auto &b : bombs) b.on = false;
  sfx(220, 120); sfx(110, 220, false);
  if (--lives <= 0) { gameOver(); return; }
  invul = 100;
}

// un projectile au point (px,py) touche-t-il un bouclier ? (et le ronge)
static bool shieldHit(float px, float py) {
  for (int s = 0; s < 3; s++) {
    int lx = (int)px - SH_X[s], ly = (int)py - SH_Y;
    if (lx < 0 || lx >= 24 || ly < 0 || ly >= 10) continue;
    int cx = lx / 2, cy = ly / 2;
    if (!shield[s][cy][cx]) continue;
    shield[s][cy][cx] = 0;
    for (int k = 0; k < 3; k++) { int nx = cx + (int)rnd(0, 2) - 1, ny = cy + (int)rnd(0, 2) - 1; if (nx >= 0 && nx < 12 && ny >= 0 && ny < 5) shield[s][ny][nx] = 0; }
    return true;
  }
  return false;
}
static void eatShields() {            // les envahisseurs qui descendent detruisent les boucliers
  for (int r = 0; r < AROWS; r++) for (int c = 0; c < ACOLS; c++) if (alive[r][c]) {
    int ax = (int)gx + c * APX, ay = (int)gy + r * APY;
    for (int s = 0; s < 3; s++) {
      if (ax + AW < SH_X[s] || ax > SH_X[s] + 24 || ay + AH < SH_Y || ay > SH_Y + 10) continue;
      for (int cy = 0; cy < 5; cy++) for (int cx = 0; cx < 12; cx++) {
        int px = SH_X[s] + cx * 2, py = SH_Y + cy * 2;
        if (px + 2 > ax && px < ax + AW && py + 2 > ay && py < ay + AH) shield[s][cy][cx] = 0;
      }
    }
  }
}

static int interval() { int v = 2 + (int)(nAlive * dif(0.75f, 0.55f, 0.45f)) - (wave - 1) * 2; return v < 2 ? 2 : v; }
static void moveAliens() {
  int minC = ACOLS, maxC = -1, maxR = -1;
  for (int r = 0; r < AROWS; r++) for (int c = 0; c < ACOLS; c++) if (alive[r][c]) { if (c < minC) minC = c; if (c > maxC) maxC = c; if (r > maxR) maxR = r; }
  if (maxC < 0) return;
  float left = gx + minC * APX, right = gx + maxC * APX + AW;
  bool edge = (dir > 0 && right + 3 > SCR_W - 3) || (dir < 0 && left - 3 < 3);
  if (edge) { gy += 6; dir = -dir; } else gx += dir * 3;
  frame ^= 1;
  static const int NOTES[4] = {131, 117, 104, 98};
  sfx(NOTES[noteI++ & 3], 45);
  eatShields();
  if (gy + maxR * APY + AH >= SHIP_Y - 1) { boom(padX, SHIP_Y + 3, rgb(255, 200, 80)); gameOver(); }
}

static void spawnBomb() {
  int cols[ACOLS], n = 0;
  for (int c = 0; c < ACOLS; c++) for (int r = 0; r < AROWS; r++) if (alive[r][c]) { cols[n++] = c; break; }
  if (!n) return;
  int c = cols[rnd(0, n - 1)], rr = 0;
  for (int r = AROWS - 1; r >= 0; r--) if (alive[r][c]) { rr = r; break; }
  for (auto &b : bombs) if (!b.on) { b = {gx + c * APX + AW / 2.f, gy + rr * APY + AH, true}; return; }
}

static void step() {
  animT++;
  for (auto &b : booms) if (b.t > 0) b.t--;
  if (state == TITLE || state == PAUSED) return;
  if (state == OVER) { endT++; return; }
  if (state == WAVEUP) ui::notice(L("WAVE CLEAR!", "VAGUE REUSSIE !"), L("GET READY", "PREPARE-TOI"), nullptr, P_S);
  tilt::update();
  float t = ctrl::x(); if (fabsf(t) < 0.07f) t = 0; else t = t > 0 ? t - 0.07f : t + 0.07f;
  padV += (t * 4.8f - padV) * 0.3f;
  padX = clampf(padX + padV, 10, SCR_W - 10);
  // ----- PLAY -----
  if (invul > 0) invul--;
  if (++moveT >= interval()) { moveT = 0; moveAliens(); if (state != PLAY) return; }
  if (--bombT <= 0) { spawnBomb(); int v = (int)(rnd(30, 70) * dif(1.4f, 1.0f, 0.75f)) - wave * 4; int mn = difi(18, 14, 10); bombT = v < mn ? mn : v; }
  // ovni
  if (ufoOn) {
    ufoX += ufoDir * 1.2f; if ((animT & 7) == 0) sfx(900 + (animT & 8) * 20, 20);
    if (ufoX < -20 || ufoX > SCR_W + 4) { ufoOn = false; ufoT = rnd(800, 1500); }
  } else if (--ufoT <= 0) { ufoOn = true; ufoDir = rnd(0, 1) ? 1 : -1; ufoX = ufoDir > 0 ? -18 : SCR_W + 2; ufoPts = 50 * (int)rnd(1, 3); }
  // tir du joueur
  if (pb) {
    pby -= 6;
    bool done = false;
    if (pby < HUD_H) done = true;
    if (!done && ufoOn && pbx >= ufoX && pbx <= ufoX + 16 && pby >= 15 && pby <= 24) { score += ufoPts; boom(ufoX + 8, 19, rgb(255, 80, 80)); ufoOn = false; ufoT = rnd(800, 1500); sfx(1400, 60); sfx(1900, 90, false); done = true; }
    for (int r = 0; r < AROWS && !done; r++) for (int c = 0; c < ACOLS && !done; c++) if (alive[r][c]) {
      float ax = gx + c * APX, ay = gy + r * APY;
      if (pbx >= ax && pbx <= ax + AW && pby >= ay && pby <= ay + AH) {
        alive[r][c] = 0; nAlive--; score += ROWPTS[r]; boom(ax + AW / 2, ay + AH / 2, TYPECOL[ROWTYPE[r]]); sfx(600, 30); sfx(300, 50, false); done = true;
      }
    }
    if (!done && shieldHit(pbx, pby)) done = true;
    if (done) pb = false;
  }
  // bombes
  for (auto &b : bombs) if (b.on) {
    b.y += 1.7f + wave * 0.12f;
    if (b.y > GROUND_Y) { b.on = false; continue; }
    if (shieldHit(b.x, b.y + 3)) { b.on = false; continue; }
    if (!invul && b.y + 4 >= SHIP_Y && b.y <= SHIP_Y + 8 && fabsf(b.x - padX) < 8) { b.on = false; playerHit(); if (state != PLAY) return; }
  }
  if (nAlive <= 0) { state = WAVEUP; endT = 0; pb = false; sfx(988, 80); sfx(1318, 80, false); sfx(1760, 160, false); }
}

static const uint16_t TCOL[3] = {P_G, P_S, P_R};
static void drawSpr(const uint8_t* rows, int x, int y, uint16_t col, int sc = 2) {
  if (sc >= 2) for (int j = 0; j < 7; j++) for (int i = 0; i < 8; i++) if (rows[j] & (0x80 >> i)) canvas.fillRect(x + i * sc - 1, y + j * sc - 1, sc + 2, sc + 2, P_K);   // contour noir 1 px
  for (int j = 0; j < 7; j++) for (int i = 0; i < 8; i++) if (rows[j] & (0x80 >> i)) canvas.fillRect(x + i * sc, y + j * sc, sc, sc, j < 1 ? P_B : (j > 4 ? P_M : col));
  if (sc >= 2) { canvas.fillRect(x + 2 * sc, y + 3 * sc, sc, sc, P_K); canvas.fillRect(x + 5 * sc, y + 3 * sc, sc, sc, P_K); }   // yeux
}
static void drawShip(int x, int y, uint16_t col) {
  canvas.fillRect(x - 10, y + 2, 20, 7, P_K); canvas.fillRect(x - 3, y - 6, 7, 9, P_K); canvas.fillRect(x - 7, y, 15, 4, P_K);       // contour
  canvas.fillRect(x - 9, y + 3, 18, 5, P_M); canvas.fillRect(x - 9, y + 3, 18, 1, P_L);                                           // ailes
  canvas.fillRect(x - 6, y + 1, 13, 3, col); canvas.fillRect(x - 2, y - 4, 5, 6, col);                                            // coque
  canvas.fillRect(x - 2, y - 4, 5, 1, P_B); canvas.fillRect(x - 6, y + 1, 13, 1, P_B);
  canvas.fillRect(x - 1, y - 2, 3, 3, P_K); canvas.fillRect(x, y - 2, 1, 1, P_S);                                                 // cockpit spectral
  canvas.fillRect(x - 9, y + 6, 3, 2, P_R); canvas.fillRect(x + 7, y + 6, 3, 2, P_R);
}
static void drawUfo(int x, int y) {
  canvas.fillRect(x, y + 2, 17, 7, P_K); canvas.fillRect(x + 4, y - 1, 9, 4, P_K);
  canvas.fillRect(x + 5, y, 7, 3, P_L); canvas.fillRect(x + 6, y, 3, 1, P_B);                                                      // dome
  canvas.fillRect(x + 1, y + 3, 15, 4, P_R); canvas.fillRect(x + 1, y + 3, 15, 1, P_B); canvas.fillRect(x + 1, y + 6, 15, 1, P_D);  // soucoupe
  for (int i = 0; i < 3; i++) canvas.fillRect(x + 3 + i * 4, y + 4, 2, 2, ((animT / 6 + i) & 1) ? P_G : P_M);
}

static void backdrop() {
  ui::night(96);
  canvas.fillRect(0, 96, SCR_W, SCR_H - 96, rgb(40, 28, 96)); canvas.fillRect(0, 96, SCR_W, 1, P_K);                                                     // bande d'horizon
  canvas.fillCircle(198, 38, 17, P_K); canvas.fillCircle(198, 38, 16, P_M); canvas.fillCircle(197, 37, 14, P_L); canvas.fillCircle(193, 34, 5, P_M); canvas.fillCircle(203, 44, 3, P_M); canvas.fillRect(190, 28, 3, 2, P_B);   // lune
  for (int i = 0; i < 18; i++) { int sx = (i * 97 + 13) % SCR_W, sy = HUD_H + 3 + (i * 53) % 90; bool tw = ((animT / 14 + i) % 6) == 0; canvas.fillRect(sx, sy, tw ? 2 : 1, tw ? 2 : 1, tw ? P_B : P_M); }
}

static void render() {
  backdrop();
  // boucliers
  for (int s = 0; s < 3; s++) for (int r = 0; r < 5; r++) for (int c = 0; c < 12; c++) if (shield[s][r][c]) { uint16_t sc = (r == 0 || !shield[s][r - 1][c]) ? P_B : (r > 2 ? P_M : P_S); canvas.fillRect(SH_X[s] + c * 2, SH_Y + r * 2, 2, 2, sc); }
  // envahisseurs
  for (int r = 0; r < AROWS; r++) for (int c = 0; c < ACOLS; c++) if (alive[r][c]) drawSpr(SPR[ROWTYPE[r]][frame], (int)gx + c * APX, (int)gy + r * APY, TYPECOL[ROWTYPE[r]]);
  if (ufoOn) drawUfo((int)ufoX, 15);
  // tirs
  if (pb) { canvas.fillRect((int)pbx - 1, (int)pby - 3, 4, 9, P_K); canvas.fillRect((int)pbx, (int)pby - 2, 2, 7, P_B); }
  for (auto &b : bombs) if (b.on) { int o = ((int)b.y >> 2) & 1; canvas.fillRect((int)b.x + o - 1, (int)b.y - 3, 4, 8, P_K); canvas.fillRect((int)b.x + o, (int)b.y - 2, 2, 3, P_R); canvas.fillRect((int)b.x + 1 - o, (int)b.y + 1, 2, 3, P_G); }
  // canon
  if (state != OVER && (invul == 0 || (animT & 4))) drawShip((int)padX, SHIP_Y, skin::c(0, P_L));
  // explosions
  for (auto &b : booms) if (b.t > 0) {
    int k = 12 - b.t; int x = (int)b.x, y = (int)b.y;
    int r1 = k < 4 ? 6 - k : (k < 8 ? 3 + k / 2 : 8 - k / 2);
    if (k < 4) { canvas.fillCircle(x, y, r1 + 1, P_K); canvas.fillCircle(x, y, r1, P_B); canvas.fillCircle(x, y, r1 - 2, b.col); }
    else { for (int a = 0; a < 8; a++) { float an = a * 0.7854f; int d = 2 + k; canvas.fillRect(x + (int)(cosf(an) * d) - 1, y + (int)(sinf(an) * d) - 1, 3, 3, P_K); canvas.fillRect(x + (int)(cosf(an) * d), y + (int)(sinf(an) * d), 2, 2, a & 1 ? b.col : P_B); } }
  }
  canvas.fillRect(0, GROUND_Y, SCR_W, SCR_H - GROUND_Y, P_D); canvas.fillRect(0, GROUND_Y, SCR_W, 2, P_K); canvas.fillRect(0, GROUND_Y + 2, SCR_W, 1, P_L); canvas.fillRect(0, GROUND_Y + 3, SCR_W, 1, P_M);
  for (int i = 0; i < 12; i++) canvas.fillRect(8 + i * 20, GROUND_Y + 6, 6, 1, P_M);
  // HUD
  canvas.fillRect(0, 0, SCR_W, HUD_H, P_D); canvas.fillRect(0, HUD_H - 1, SCR_W, 1, P_M); canvas.fillRect(0, HUD_H, SCR_W, 1, P_K);
  char b[32];
  snprintf(b, sizeof b, "%lu", (unsigned long)score); txt(b, 5, 0, &fonts::Font2, P_B);
  snprintf(b, sizeof b, L("WAVE %d","VAGUE %d"), wave); txt(b, SCR_W / 2, 0, &fonts::Font2, P_G, textdatum_t::top_center);
  for (int i = 0; i < lives - (state == OVER ? 0 : 1); i++) drawShip(SCR_W - 14 - i * 20, 4, skin::c(0, P_L));
  if (state == TITLE) {
    backdrop();
    for (int t = 0; t < 3; t++) drawSpr(SPR[t][(animT / 25) & 1], 62 + t * 46, 24, TYPECOL[t]);
    txtO("INVADERS", SCR_W / 2, 48, &fonts::Font4, P_S, P_K, textdatum_t::top_center, 1.5f);
    if (best > 0) { snprintf(b, sizeof b, L("best %lu","record %lu"), (unsigned long)best); txt(b, SCR_W / 2, 112, &fonts::Font2, P_G, textdatum_t::top_center); }
  }
  if (state == PAUSED) ui::pause();
  if (state == WAVEUP) ui::notice(L("WAVE CLEAR!", "VAGUE REUSSIE !"), L("GET READY", "PREPARE-TOI"), nullptr, P_S);
  if (state == OVER) ui::gameOver("GAME OVER", score, best, score > 0 && score >= best);
}

void loop() {
  uint32_t now = micros();
  if (last == 0) last = now;
  acc += now - last; last = now; if (acc > 100000) acc = 100000;
  bool a = M5.BtnA.wasPressed(), p = M5.BtnPWR.wasPressed();
  switch (state) {
    case TITLE: if (a) { reset(); state = PLAY; sfx(660, 40); } break;
        case PLAY:
      if (p) { state = PAUSED; sfx(440, 50); }
      else if ((a || g_btnCtrl) && !pb) {                              // mode boutons : tir automatique
       pb = true; pbx = padX; pby = SHIP_Y - 3; sfx(1500, 20); sfx(900, 30, false); }
      break;
    case PAUSED: if (p || a) { state = PLAY; sfx(660, 50); } break;
    case WAVEUP: break;
    case OVER: if (a && endT > 40) { reset(); state = PLAY; sfx(660, 40); } break;
    default: break;
  }
  while (acc >= STEP_US) { step(); acc -= STEP_US; }
  render();
  bgm::run(11, state == PLAY);
  present(0, 0);
}

void icon(int x, int y) {
  canvas.fillRoundRect(x, y, 64, 64, 6, P_K);
  canvas.fillRect(x + 2, y + 44, 60, 18, P_D);
  for (int r = 0; r < 2; r++) for (int c = 0; c < 3; c++) drawSpr(SPR[r == 0 ? 0 : 1][0], x + 5 + c * 19, y + 5 + r * 16, TYPECOL[r == 0 ? 0 : 1], 2);
  canvas.fillRect(x + 31, y + 36, 2, 7, P_K); canvas.fillRect(x + 31, y + 37, 2, 5, P_B);
  drawShip(x + 32, y + 50, P_L);
  canvas.fillRect(x + 2, y + 60, 60, 2, P_M);
}

}  // namespace invaders
