// ============================================================================
//  CASSE-BRIQUES - M5 a l'horizontale : pencher a droite / a gauche = la raquette suit
//  M5 (A) = jouer / lancer la balle ; B (cote) = recalibrer ; HAUT (PWR) = pause
// ============================================================================
#include "common.h"

namespace breakout {

static const uint32_t STEP_US = 20000;               // 50 Hz
static const int COLS = 12, MAXROWS = 7, BW = 18, BH = 7, PITCH_X = 20, PITCH_Y = 9, TOP = 20;
static const int PAD_Y = 124, PAD_H = 4, PAD_W = 52, HUD_H = 15;
static const float BALL_R = 3.0f;
enum State { TITLE, READY, PLAY, PAUSED, LEVELUP, OVER };
static State state = TITLE;
static Preferences pr;
static uint32_t last = 0, acc = 0;
static uint8_t hp[MAXROWS][COLS];
static int rows = 4, bricksLeft = 0;
static float padX = 120, padV = 0;
static float bx, by, vx, vy;
static bool stuck = true;
static int lives = 3, level = 1, endT = 0, animT = 0, hits = 0;
static uint32_t score = 0, best = 0;
// ---- effets visuels
struct Part { float x, y, vx, vy; uint8_t life; uint16_t col; };
static Part parts[48]; static float trx[7], try_[7]; static int flashT = 0, padFlash = 0, shake = 0;
static void burst(float x, float y, uint16_t col, int n) {
  for (int i = 0; i < n; i++) for (auto& p : parts) if (!p.life) { p.x = x + rnd(0, 16) - 8; p.y = y + rnd(0, 6) - 3; p.vx = ((int)rnd(0, 100) - 50) * 0.03f; p.vy = -((int)rnd(0, 100)) * 0.025f - 0.4f; p.life = 22 + rnd(0, 14); p.col = col; break; }
}

static const uint16_t ROWCOL[MAXROWS] = {P_R, P_G, P_S, P_L, P_M, P_R, P_G};

static void buildLevel() {
  rows = 4 + (level - 1); if (rows > MAXROWS) rows = MAXROWS;
  bricksLeft = 0;
  memset(hp, 0, sizeof hp);
  for (int r = 0; r < rows; r++) for (int c = 0; c < COLS; c++) {
    if (level > 1 && rnd(0, 99) < 8 + level * 2) continue;          // quelques trous a partir du niveau 2
    hp[r][c] = (level >= 3 && r == 0) ? 2 : 1;                       // rangee du haut : 2 coups
    bricksLeft++;
  }
}
static float baseSpeed() { float s = 2.3f + 0.25f * (level - 1) + hits * 0.012f; s *= dif(0.88f, 1.0f, 1.12f); return s > 4.0f * dif(0.88f, 1.0f, 1.12f) ? 4.0f * dif(0.88f, 1.0f, 1.12f) : s; }
static void stick() { stuck = true; bx = padX; by = PAD_Y - BALL_R - 1; vx = vy = 0; }
static void reset() {
  lives = difi(5, 3, 2); level = 1; score = 0; hits = 0; padX = SCR_W / 2; padV = 0; animT = 0; endT = 0;
  buildLevel(); stick();
}
void begin() {
  pr.begin(dns("breakout"), false); best = pr.getInt("best", 0);
  tilt::load();
  reset(); state = TITLE; last = 0; acc = 0;
}

static void launch() {
  stuck = false; float s = baseSpeed();
  float a = ((int)rnd(0, 40) - 20) * 0.01f + (padV > 0.5f ? 0.25f : (padV < -0.5f ? -0.25f : 0.f));
  vx = s * sinf(a); vy = -s * cosf(a);
  sfx(660, 30); sfx(990, 40, false);
}
static void gameOver() {
  state = OVER; endT = 0;
  trophy::record(trophy::T_BREAKOUT, score); if (score > best) { best = score; pr.putInt("best", best); }
  sfx(300, 120); sfx(120, 300, false);
}

// teste la balle contre les briques ; retourne true si touchee (et detruit/abime la brique)
static bool hitBrick() {
  int c0 = (int)((bx - BALL_R - 1) / PITCH_X), c1 = (int)((bx + BALL_R - 1) / PITCH_X);
  int r0 = (int)((by - BALL_R - TOP) / PITCH_Y), r1 = (int)((by + BALL_R - TOP) / PITCH_Y);
  if (by - BALL_R - TOP < 0) r0 = 0;
  for (int r = r0; r <= r1; r++) for (int c = c0; c <= c1; c++) {
    if (r < 0 || r >= rows || c < 0 || c >= COLS || !hp[r][c]) continue;
    float x0 = 1 + c * PITCH_X, y0 = TOP + r * PITCH_Y;
    float nx = clampf(bx, x0, x0 + BW), ny = clampf(by, y0, y0 + BH);
    float dx = bx - nx, dy = by - ny;
    if (dx * dx + dy * dy <= BALL_R * BALL_R) {
      if (--hp[r][c] == 0) { bricksLeft--; score += 10 * (rows - r); hits++; sfx(700 + (MAXROWS - r) * 110, 40); burst(x0 + BW / 2, y0 + BH / 2, ROWCOL[r], 9); }
      else { sfx(300, 40); burst(x0 + BW / 2, y0 + BH / 2, rgb(220, 224, 240), 4); }
      return true;
    }
  }
  return false;
}

static void step() {
  animT++;
  for (auto& p : parts) if (p.life) { p.life--; p.x += p.vx; p.y += p.vy; p.vy += 0.09f; }
  if (flashT) flashT--;
  if (padFlash) padFlash--;
  if (shake) shake--;
  if (!stuck && state == PLAY) { for (int i = 6; i > 0; i--) { trx[i] = trx[i - 1]; try_[i] = try_[i - 1]; } trx[0] = bx; try_[0] = by; }
  else for (int i = 0; i < 7; i++) { trx[i] = bx; try_[i] = by; }
  if (state == TITLE || state == OVER || state == PAUSED) { if (state == OVER) endT++; return; }
  if (state == LEVELUP) { endT++; return; }
  tilt::update();
  float t = ctrl::x(); if (fabsf(t) < 0.07f) t = 0; else t = t > 0 ? t - 0.07f : t + 0.07f;
  float target = t * 7.5f;
  padV += (target - padV) * 0.3f;
  padX += padV; padX = clampf(padX, PAD_W / 2.f, SCR_W - PAD_W / 2.f);
  if (stuck) { bx = padX; by = PAD_Y - BALL_R - 1; return; }
  // balle : 2 sous-pas par image
  for (int s = 0; s < 2; s++) {
    bx += vx * 0.5f;
    if (bx < BALL_R) { bx = BALL_R; vx = fabsf(vx); sfx(500, 15); }
    if (bx > SCR_W - BALL_R) { bx = SCR_W - BALL_R; vx = -fabsf(vx); sfx(500, 15); }
    if (hitBrick()) { vx = -vx; bx += vx * 0.5f; }
    by += vy * 0.5f;
    if (by < HUD_H + BALL_R) { by = HUD_H + BALL_R; vy = fabsf(vy); sfx(500, 15); }
    if (hitBrick()) { vy = -vy; by += vy * 0.5f; }
    // raquette
    if (vy > 0 && by + BALL_R >= PAD_Y && by - BALL_R <= PAD_Y + PAD_H && bx > padX - PAD_W / 2.f - BALL_R && bx < padX + PAD_W / 2.f + BALL_R) {
      float rel = clampf((bx - padX) / (PAD_W / 2.f + BALL_R), -1.f, 1.f);
      float a = rel * 1.05f + padV * 0.03f; a = clampf(a, -1.1f, 1.1f);
      float sp = baseSpeed();
      vx = sp * sinf(a); vy = -sp * cosf(a); by = PAD_Y - BALL_R;
      sfx(440, 25); padFlash = 8; burst(bx, PAD_Y, rgb(200, 220, 255), 3);
    }
  }
  if (bricksLeft <= 0) { state = LEVELUP; endT = 0; sfx(988, 80); sfx(1318, 80, false); sfx(1760, 160, false); return; }
  if (by > SCR_H + BALL_R) {
    if (--lives <= 0) { gameOver(); return; }
    sfx(200, 100); sfx(110, 160, false); shake = 12; stick();
  }
}

static void backdrop() {
  // pixel art : aplats nets, pas de degrade. Ciel noir, lune, skyline sombre avec fenetres dorees (change de silhouette selon le niveau)
  ui::night(76);
  canvas.fillRect(0, 76, SCR_W, 59, rgb(40, 28, 96));
  int th = (level - 1) & 3;
  canvas.fillCircle(210, 100, 13, P_K); canvas.fillCircle(210, 100, 12, P_M); canvas.fillCircle(209, 99, 10, P_L); canvas.fillRect(205, 95, 3, 3, P_M); canvas.fillRect(212, 102, 3, 2, P_M); canvas.fillRect(203, 92, 2, 2, P_B);
  for (int x = 0; x < SCR_W; x += 8) {
    uint32_t h = (uint32_t)((x / 8) * 2246822519u + th * 31); int hh = th & 1 ? 8 + (h >> 20) % 10 : 10 + (h >> 20) % 24;
    canvas.fillRect(x, SCR_H - hh - 1, 8, hh + 1, P_K); canvas.fillRect(x + 1, SCR_H - hh, 6, hh, P_M); canvas.fillRect(x + 1, SCR_H - hh, 6, 1, P_L); canvas.fillRect(x + 6, SCR_H - hh + 1, 1, hh - 1, P_D);
    if (!(th & 1) && ((h >> 12) & 3) == 0) canvas.fillRect(x + 3, SCR_H - hh + 4, 2, 2, P_G);
  }
}
static void brick(int x, int y, uint16_t col, uint16_t lt, uint16_t dk, int hpv) {
  if (hpv == 2) { col = P_L; lt = P_B; dk = P_M; }
  pxBox(x, y, BW, BH, col, lt, dk);
  if (hpv == 2) { canvas.drawPixel(x + 2, y + BH - 3, P_K); canvas.drawPixel(x + BW - 3, y + BH - 3, P_K); }
}
static void render() {
  backdrop();
  int sx = shake ? (int)rnd(0, 4) - 2 : 0;
  static const uint16_t BODY[MAXROWS] = {P_R, P_G, P_S, P_L, P_M, P_R, P_G}, DK[MAXROWS] = {P_D, P_R, P_M, P_M, P_D, P_D, P_R};
  for (int r = 0; r < rows; r++) for (int c = 0; c < COLS; c++) if (hp[r][c]) brick(1 + c * PITCH_X + sx, TOP + r * PITCH_Y, BODY[r], P_B, DK[r], hp[r][c]);
  for (auto& p : parts) if (p.life) { int sz = p.life > 12 ? 2 : 1; canvas.fillRect((int)p.x, (int)p.y, sz, sz, p.life < 8 ? P_M : p.col); }
  int px = (int)(padX - PAD_W / 2);
  uint16_t body = skin::c(0, P_L), acc = skin::c(3, P_R);
  pxBox(px, PAD_Y - 1, PAD_W, PAD_H + 2, body, P_B, P_M);
  uint16_t glow = padFlash ? P_B : acc;
  canvas.fillRect(px + 1, PAD_Y, 6, PAD_H, glow); canvas.fillRect(px + PAD_W - 7, PAD_Y, 6, PAD_H, glow);
  canvas.fillRect(px + PAD_W / 2 - 4, PAD_Y, 8, PAD_H, P_M);
  { int fl = 2 + (animT & 1) + (int)fabsf(padV); int yy = PAD_Y + PAD_H + 1;
    canvas.fillTriangle(px + 11, yy, px + 17, yy, px + 14, yy + fl + 1, P_K); canvas.fillTriangle(px + 12, yy, px + 16, yy, px + 14, yy + fl, P_G);
    canvas.fillTriangle(px + PAD_W - 17, yy, px + PAD_W - 11, yy, px + PAD_W - 14, yy + fl + 1, P_K); canvas.fillTriangle(px + PAD_W - 16, yy, px + PAD_W - 12, yy, px + PAD_W - 14, yy + fl, P_G); }
  if (state != TITLE) {
    uint16_t bc = g_skin ? skin::c(2, P_B) : P_B;
    if (!stuck) { canvas.fillCircle((int)trx[1], (int)try_[1], 2, P_L); canvas.fillCircle((int)trx[3], (int)try_[3], 1, P_M); canvas.fillCircle((int)trx[5], (int)try_[5], 1, P_D); }
    pxDisc((int)bx, (int)by, 3, bc, P_B, P_L);
  }
  // HUD
  canvas.fillRect(0, 0, SCR_W, HUD_H, P_D); canvas.fillRect(0, HUD_H, SCR_W, 1, P_K); canvas.fillRect(0, HUD_H - 1, SCR_W, 1, P_M);
  char b[24];
  snprintf(b, sizeof b, "%lu", (unsigned long)score); txt(b, 6, 1, &fonts::Font2, P_B);
  snprintf(b, sizeof b, L("LVL %d","NIV %d"), level); txt(b, SCR_W / 2, 1, &fonts::Font2, P_G, textdatum_t::top_center);
  for (int i = 0; i < lives; i++) { int hx = SCR_W - 10 - i * 12, hy = 3; canvas.fillRect(hx - 4, hy - 1, 9, 8, P_K); canvas.fillRect(hx - 3, hy, 3, 3, P_R); canvas.fillRect(hx + 1, hy, 3, 3, P_R); canvas.fillRect(hx - 3, hy + 2, 7, 2, P_R); canvas.fillRect(hx - 2, hy + 4, 5, 1, P_R); canvas.fillRect(hx - 1, hy + 5, 3, 1, P_R); canvas.drawPixel(hx - 2, hy + 1, P_B); }
  if (state == TITLE) {
    canvas.fillRoundRect(16, 70, 208, 52, 6, P_D); canvas.drawRoundRect(16, 70, 208, 52, 6, P_L);
    txt(L("BREAKOUT","CASSE-BRIQUES"), SCR_W / 2, 74, &fonts::Font4, P_G, textdatum_t::top_center);
    if (best > 0) { snprintf(b, sizeof b, L("best %lu","record %lu"), (unsigned long)best); txt(b, SCR_W - 4, 124, &fonts::Font0, P_G, textdatum_t::top_right); }
  }
  if (state == PAUSED) ui::pause();
  if (state == LEVELUP) { char b2[24]; snprintf(b2, sizeof b2, "+%lu", (unsigned long)score); ui::notice(L("LEVEL CLEAR!", "NIVEAU REUSSI !"), b2, L("A : next level", "A : niveau suivant"), P_S); }
  if (state == OVER) ui::gameOver("GAME OVER", score, best, score > 0 && score >= best);
}

void loop() {
  uint32_t now = micros();
  if (last == 0) last = now;
  acc += now - last; last = now; if (acc > 100000) acc = 100000;
  bool a = M5.BtnA.wasPressed(), p = M5.BtnPWR.wasPressed();
  switch (state) {
    case TITLE: if (a) { reset(); state = PLAY; sfx(660, 40); } break;
        case PLAY: if (p) { state = PAUSED; sfx(440, 50); } else if (a && stuck) launch(); break;
    case PAUSED: if (p || a) { state = PLAY; sfx(660, 50); } break;
    case LEVELUP: if (a && endT > 40) { level++; buildLevel(); stick(); state = PLAY; sfx(660, 40); } break;
    case OVER: if (a && endT > 40) { reset(); state = PLAY; sfx(660, 40); } break;
    default: break;
  }
  while (acc >= STEP_US) { step(); acc -= STEP_US; }
  render();
  bgm::run(10, state == PLAY);
  present(0, 0);
}

void icon(int x, int y) {
  canvas.fillRoundRect(x, y, 64, 64, 6, P_D);
  for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) if (!(r == 2 && c == 1)) pxBox(x + 3 + c * 15, y + 5 + r * 9, 14, 8, ROWCOL[r], P_B, r == 1 ? P_R : P_M);
  pxDisc(x + 40, y + 42, 3, P_B, P_B, P_L);
  canvas.fillRect(x + 36, y + 35, 2, 2, P_M); canvas.fillRect(x + 34, y + 31, 2, 2, P_D);
  pxBox(x + 18, y + 53, 30, 6, P_L, P_B, P_M); canvas.fillRect(x + 19, y + 54, 5, 4, P_R); canvas.fillRect(x + 42, y + 54, 5, 4, P_R);
}

}  // namespace breakout
