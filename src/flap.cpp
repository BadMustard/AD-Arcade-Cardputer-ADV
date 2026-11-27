// ============================================================================
//  FLAP BIRD - M5 (A) ou B = battre des ailes ; HAUT (PWR) = pause
//  Oiseau qui s'incline selon sa vitesse, aile animee, plumes ; decor en parallaxe (montagnes,
//  collines, nuages, oiseaux au loin), jour -> soir -> nuit selon le score ; medailles.
// ============================================================================
#include "common.h"

namespace flap {

static const uint32_t STEP_US = 20000;          // 50 Hz
static const float GRAV = 0.20f, FLAP_V = -3.0f, MAX_FALL = 4.2f;
static const int   BIRD_X = 56, BALL_R = 6;
static const int   PIPE_W = 26, PIPE_SPACING = 108;
static const int   GROUND_Y = 120;

enum State { TITLE, PLAY, PAUSED, OVER };
static State state = TITLE;
static float by = 60, vy = 0, ang = 0;
static int   scorePop = 0, score = 0, best = 0, overTicks = 0, animT = 0, wingT = 0, shake = 0, flashT = 0, gapH = 46;
static float speed = 1.7f;
static float groundX = 0, farX = 0, midX = 0;
struct Pipe { float x; float gapY; float base; uint8_t phase; bool scored; };
static Pipe pipes[4]; static int nPipes = 0;
struct Cloud { float x; int y; uint8_t s; };
static Cloud clouds[5];
struct Feather { float x, y, vx, vy; int life; };
static Feather fe[14];
static Preferences pr;
static uint32_t last = 0, acc = 0;

static void addPipe(float x) {
  if (nPipes >= 4) return;
  int lo = 16 + gapH / 2 + 6, hi = GROUND_Y - 16 - gapH / 2 - 6;
  pipes[nPipes++] = {x, 0, (float)rnd(lo, hi), (uint8_t)rnd(0, 255), false};
  pipes[nPipes - 1].gapY = pipes[nPipes - 1].base;
}
static void reset() {
  by = 60; vy = 0; ang = 0; score = 0; nPipes = 0; animT = 0; overTicks = 0; speed = 1.7f * dif(0.88f, 1.0f, 1.12f); gapH = difi(54, 46, 40); shake = 0; flashT = 0;
  for (auto &f : fe) f.life = 0;
  addPipe(SCR_W + 40); addPipe(SCR_W + 40 + PIPE_SPACING);
}
void begin() {
  pr.begin(dns("flap"), false); best = pr.getInt("best", 0);
  state = TITLE; reset(); by = 60; groundX = farX = midX = 0; last = 0; acc = 0;
  for (int i = 0; i < 5; i++) clouds[i] = {(float)(i * 56 + rnd(0, 20)), (int)rnd(8, 56), (uint8_t)rnd(0, 2)};
}

static void puff(float x, float y, int n) {
  for (int i = 0; i < n; i++) for (auto &f : fe) if (f.life <= 0) { f = {x, y, -(float)rnd(3, 12) / 10.f - speed * 0.3f, (float)((int)rnd(0, 14) - 4) / 10.f, (int)rnd(14, 26)}; break; }
}
static void die() {
  state = OVER; overTicks = 0; shake = 10; flashT = 6;
  trophy::record(trophy::T_FLAP, score); if (score > best) { best = score; pr.putInt("best", best); }
  puff(BIRD_X, by, 10);
  sfx(300, 90); sfx(150, 220, false);
}
static void doFlap() { vy = FLAP_V; wingT = 14; sfx(880, 30); sfx(1180, 40, false); puff(BIRD_X - 5, by + 4, 2); }

static void step() {
  animT++;
  groundX -= speed; if (groundX <= -16) groundX += 16;
  farX -= speed * 0.12f; midX -= speed * 0.35f;
  for (int i = 0; i < 5; i++) { clouds[i].x -= 0.25f + clouds[i].s * 0.12f; if (clouds[i].x < -44) { clouds[i].x = SCR_W + rnd(0, 30); clouds[i].y = rnd(8, 56); clouds[i].s = rnd(0, 2); } }
  for (auto &f : fe) if (f.life > 0) { f.x += f.vx; f.y += f.vy; f.vy += 0.03f; f.life--; }
  if (wingT > 0) wingT--;
  if (scorePop > 0) scorePop--;
  if (shake > 0) shake--;
  if (flashT > 0) flashT--;
  if (state == TITLE) { by = 62 + sinf(animT * 0.12f) * 5; ang = cosf(animT * 0.12f) * 0.2f; if (animT % 24 == 0) wingT = 10; return; }
  if (state == OVER) {
    if (overTicks < 200) overTicks++;
    if (by < GROUND_Y - BALL_R) { vy += GRAV; if (vy > MAX_FALL) vy = MAX_FALL; by += vy; ang += 0.08f; if (ang > 1.5f) ang = 1.5f; if (by > GROUND_Y - BALL_R) by = GROUND_Y - BALL_R; }
    return;
  }
  // PLAY
  vy += GRAV; if (vy > MAX_FALL) vy = MAX_FALL;
  by += vy;
  float ta = clampf(vy * 0.17f, -0.45f, 1.25f); ang += (ta - ang) * 0.25f;
  for (int i = 0; i < nPipes; i++) {
    pipes[i].x -= speed;
    if (score >= 12) { pipes[i].phase++; pipes[i].gapY = pipes[i].base + sinf(pipes[i].phase * 0.035f) * fminf(12.f, (score - 10) * 1.0f); }   // tuyaux qui bougent
  }
  if (nPipes > 0 && pipes[0].x + PIPE_W < 0) { memmove(&pipes[0], &pipes[1], sizeof(Pipe) * (nPipes - 1)); nPipes--; }
  if (nPipes > 0 && pipes[nPipes - 1].x < SCR_W - PIPE_SPACING + 4) addPipe(pipes[nPipes - 1].x + PIPE_SPACING);
  for (int i = 0; i < nPipes; i++) {
    Pipe &p = pipes[i];
    if (!p.scored && p.x + PIPE_W < BIRD_X - BALL_R) {
      p.scored = true; score++; scorePop = 10; sfx(1318, 40); sfx(1760, 60, false);
      speed = fminf(2.3f, 1.7f + score * 0.012f) * dif(0.88f, 1.0f, 1.12f); gapH = difi(54, 46, 40) - (score > 25 ? 4 : 0);
    }
    if (BIRD_X + BALL_R - 1 > p.x && BIRD_X - BALL_R + 1 < p.x + PIPE_W) {
      if (by - BALL_R + 2 < p.gapY - gapH / 2 || by + BALL_R - 2 > p.gapY + gapH / 2) { die(); return; }
    }
  }
  if (by + BALL_R >= GROUND_Y) { by = GROUND_Y - BALL_R; die(); return; }
  if (by - BALL_R < 0) { by = BALL_R; if (vy < 0) vy = 0; }
}

// ----------------------------------------------------------------------------- decor
static int phase() { return (score / 10) % 4; }          // 0 jour, 1 couchant, 2 nuit, 3 aube
struct Sky { uint16_t top, bot, hill1, hill2, mount, cloud; };
static Sky skyOf(int ph) {
  switch (ph) {
    case 1: return {rgb(250, 140, 90), rgb(255, 215, 130), rgb(190, 100, 100), rgb(150, 80, 100), rgb(170, 100, 120), rgb(255, 210, 190)};
    case 2: return {rgb(14, 18, 56), rgb(50, 54, 120), rgb(30, 46, 90), rgb(22, 34, 70), rgb(36, 40, 90), rgb(90, 96, 150)};
    case 3: return {rgb(120, 130, 220), rgb(255, 190, 200), rgb(120, 130, 190), rgb(100, 112, 170), rgb(130, 130, 200), rgb(255, 230, 240)};
    default: return {rgb(70, 170, 235), rgb(170, 225, 245), rgb(110, 190, 130), rgb(80, 160, 110), rgb(130, 170, 200), rgb(255, 255, 255)};
  }
}
static void drawBackground(int ox) {
  int ph = phase(); Sky s = skyOf(ph);
  for (int y = 0; y < GROUND_Y; y += 4) canvas.fillRect(0, y, SCR_W, 4, mixc(s.top, s.bot, y * 256 / GROUND_Y));
  if (ph == 2) for (int i = 0; i < 28; i++) { int sx = (i * 53 + 11) % SCR_W, sy = (i * 29 + 5) % 70; if (((animT / 15) + i) % 7) canvas.drawPixel(sx, sy, rgb(220, 220, 255)); }
  if (ph == 2) { canvas.fillCircle(196, 24, 9, rgb(240, 240, 210)); canvas.fillCircle(200, 22, 8, rgb(30, 36, 90)); }
  else if (ph == 0) { canvas.fillCircle(200, 22, 10, rgb(255, 245, 160)); canvas.fillCircle(200, 22, 7, rgb(255, 255, 220)); }
  else { canvas.fillCircle(190, ph == 1 ? 78 : 70, 12, rgb(255, 230, 150)); }
  for (int i = 0; i < 5; i++) {                                                       // nuages
    int cx = (int)clouds[i].x, cy = clouds[i].y, k = clouds[i].s;
    canvas.fillRoundRect(cx, cy, 30 + k * 8, 8, 4, s.cloud); canvas.fillCircle(cx + 10, cy, 6 + k, s.cloud); canvas.fillCircle(cx + 21, cy + 1, 5 + k, s.cloud);
  }
  // montagnes lointaines
  for (int x = 0; x < SCR_W; x += 2) {
    int wx = x - (int)farX; float h = 22 + sinf(wx * 0.021f) * 12 + sinf(wx * 0.053f + 1.3f) * 6;
    int hy = GROUND_Y - 22 - (int)h; canvas.fillRect(x, hy, 2, GROUND_Y - hy, s.mount);
    if (h > 30) canvas.fillRect(x, hy, 2, 2, mixc(s.mount, 0xFFFF, 140));
  }
  // collines + arbres
  for (int x = 0; x < SCR_W; x += 2) {
    int wx = x - (int)midX; float h = 10 + sinf(wx * 0.034f + 0.7f) * 6 + sinf(wx * 0.09f) * 2;
    int hy = GROUND_Y - 6 - (int)h; canvas.fillRect(x, hy, 2, GROUND_Y - hy, s.hill1);
    int tr = ((wx / 2) * 2654435761u) >> 28; if (tr == 3 && ((wx / 2) & 7) == 0) { canvas.fillRect(x, hy - 4, 2, 4, s.hill2); canvas.fillCircle(x + 1, hy - 7, 4, s.hill2); }
  }
  for (int x = 0; x < SCR_W; x += 2) { int wx = x - (int)midX * 2; float h = 4 + sinf(wx * 0.06f + 2.f) * 3; canvas.fillRect(x, GROUND_Y - (int)h, 2, (int)h, s.hill2); }
  // oiseaux lointains
  for (int i = 0; i < 3; i++) { int bx = (int)(SCR_W - fmodf(animT * 0.5f + i * 90.f, 330.f) + 40), byy = 24 + i * 11 + (int)(sinf(animT * 0.1f + i) * 2); int w = ((animT / 8 + i) & 1); canvas.drawLine(bx - 3, byy - w, bx, byy + 1, ph == 2 ? rgb(150, 150, 200) : rgb(60, 70, 90)); canvas.drawLine(bx, byy + 1, bx + 3, byy - w, ph == 2 ? rgb(150, 150, 200) : rgb(60, 70, 90)); }
  (void)ox;
}

static void drawPipe(int x, int top, int bottom, bool isCap) {
  uint16_t g1 = rgb(96, 196, 44), g2 = rgb(56, 140, 32), g3 = rgb(170, 240, 100), dk = rgb(24, 64, 20), g4 = rgb(76, 168, 38);
  if (phase() == 2) { g1 = rgb(50, 120, 60); g2 = rgb(30, 80, 46); g3 = rgb(90, 170, 100); g4 = rgb(40, 100, 54); dk = rgb(10, 36, 24); }
  if (bottom <= top) return;
  int w = isCap ? PIPE_W + 4 : PIPE_W;
  canvas.fillRect(x, top, w, bottom - top, g1);
  canvas.fillRect(x + 3, top, 3, bottom - top, g3);
  canvas.fillRect(x + 7, top, 2, bottom - top, g4);
  canvas.fillRect(x + w - 8, top, 7, bottom - top, g2);
  canvas.fillRect(x + w - 4, top, 3, bottom - top, mixc(g2, 0, 70));
  canvas.drawRect(x, top, w, bottom - top, dk);
  if (isCap) { canvas.drawFastHLine(x + 1, top + 1, w - 2, g3); canvas.drawFastHLine(x + 1, bottom - 2, w - 2, mixc(g2, 0, 100)); }
}

static void rp(float cx, float cy, float a, float px, float py, int &ox, int &oy) { float c = cosf(a), s = sinf(a); ox = (int)floorf(cx + px * c - py * s + 0.5f); oy = (int)floorf(cy + px * s + py * c + 0.5f); }
static void drawBird(int cx, int cy, float a, bool alive) {
  // oiseau violet (original) : huppe rose, ventre creme, aile arrondie qui bat, grand oeil brillant
  const uint16_t OUT = skin::c(1, rgb(52, 24, 96)), body = skin::c(0, rgb(150, 96, 236)), bodyD = skin::c(1, rgb(112, 64, 196)), belly = rgb(255, 238, 214), crest = skin::c(3, rgb(255, 110, 170)),
                 wing = skin::c(1, rgb(96, 54, 186)), wingL = skin::c(2, rgb(190, 150, 255)), beakU = rgb(255, 190, 40), beakL = rgb(240, 130, 30), cheek = rgb(255, 150, 190);
  int x, y, x2, y2, x3, y3;
  // queue : trois plumes
  for (int i = -1; i <= 1; i++) {
    rp(cx, cy, a, -5, i * 2, x, y); rp(cx, cy, a, -14, i * 4 - 1, x2, y2); rp(cx, cy, a, -12, i * 4 + 2, x3, y3);
    canvas.fillTriangle(x, y - 1, x2, y2 - 1, x3, y3 + 1, OUT);
    rp(cx, cy, a, -5, i * 2, x, y); rp(cx, cy, a, -13, i * 4 - 1, x2, y2); rp(cx, cy, a, -11, i * 4 + 1, x3, y3);
    canvas.fillTriangle(x, y, x2, y2, x3, y3, i == 0 ? crest : bodyD);
  }
  // huppe : trois petites plumes
  for (int i = 0; i < 3; i++) {
    rp(cx, cy, a, -1 + i * 3, -6, x, y); rp(cx, cy, a, -2 + i * 4, -11 + (i == 1 ? -1 : 0), x2, y2); rp(cx, cy, a, 1 + i * 3, -6, x3, y3);
    canvas.fillTriangle(x, y, x2, y2, x3, y3, OUT); canvas.drawLine(x2, y2, (x + x3) / 2, (y + y3) / 2, crest);
  }
  // corps : deux disques decales pour faire un ovale, avec contour
  int bx, by_, bx2, by2;
  rp(cx, cy, a, -2, 0, bx, by_); rp(cx, cy, a, 2, 0, bx2, by2);
  canvas.fillCircle(bx, by_, 8, OUT); canvas.fillCircle(bx2, by2, 8, OUT);
  canvas.fillCircle(bx, by_, 7, body); canvas.fillCircle(bx2, by2, 7, body);
  rp(cx, cy, a, 0, -4, x, y); canvas.fillCircle(x, y, 2, skin::c(2, rgb(176, 130, 250)));                         // reflet
  rp(cx, cy, a, 1, 4, x, y); rp(cx, cy, a, -3, 4, x2, y2);
  canvas.fillCircle(x, y, 4, belly); canvas.fillCircle(x2, y2, 3, belly);                             // ventre
  rp(cx, cy, a, 6, 2, x, y); canvas.fillCircle(x, y, 2, cheek);                                       // joue
  // oeil
  int ex, ey; rp(cx, cy, a, 4, -3, ex, ey);
  canvas.fillCircle(ex, ey, 4, OUT); canvas.fillCircle(ex, ey, 3, TFT_WHITE);
  if (alive) { canvas.fillCircle(ex + 1, ey, 2, TFT_BLACK); canvas.drawPixel(ex + 1, ey - 1, TFT_WHITE); }
  else { canvas.drawLine(ex - 2, ey - 2, ex + 2, ey + 2, TFT_BLACK); canvas.drawLine(ex - 2, ey + 2, ex + 2, ey - 2, TFT_BLACK); }
  // bec : deux parties
  rp(cx, cy, a, 8, -1, x, y); rp(cx, cy, a, 15, 1, x2, y2); rp(cx, cy, a, 8, 2, x3, y3);
  canvas.fillTriangle(x, y - 1, x2 + (x2 > x ? 1 : 0), y2, x3, y3, OUT);
  rp(cx, cy, a, 8, -1, x, y); rp(cx, cy, a, 14, 1, x2, y2); rp(cx, cy, a, 8, 2, x3, y3);
  canvas.fillTriangle(x, y, x2, y2, x3, y3, beakU);
  rp(cx, cy, a, 8, 2, x, y); rp(cx, cy, a, 13, 2, x2, y2); rp(cx, cy, a, 8, 5, x3, y3);
  canvas.fillTriangle(x, y, x2, y2, x3, y3, beakL);
  // aile : battement haut / milieu / bas
  int f = wingT > 9 ? 0 : (wingT > 4 ? 1 : (wingT > 0 ? 2 : (((animT / 6) & 1) ? 1 : 2)));
  if (!alive) f = 1;
  int wy_ = f == 0 ? -8 : (f == 1 ? -1 : 6);
  int wx0, wy0, wx1, wy1, wx2, wy2;
  rp(cx, cy, a, -1, 1, wx0, wy0); rp(cx, cy, a, -11, wy_, wx1, wy1); rp(cx, cy, a, -6, wy_ + (f == 0 ? 6 : -2) + 6, wx2, wy2);
  canvas.fillCircle(wx0, wy0, 4, OUT);
  canvas.fillTriangle(wx0, wy0 - 3, wx1, wy1, wx2, wy2, OUT);
  canvas.fillCircle(wx0, wy0, 3, wing);
  canvas.fillTriangle(wx0, wy0 - 2, wx1 + (wx1 < wx0 ? 1 : -1), wy1 + (f == 2 ? -1 : 1), wx2, wy2 - 1, wing);
  canvas.drawLine(wx0, wy0 - 1, wx1 + 1, wy1, wingL);                                                  // plume claire
}

[[maybe_unused]] static void drawMedal(int x, int y, int m) {
  uint16_t c1[4] = {rgb(190, 110, 60), rgb(200, 205, 215), rgb(255, 210, 50), rgb(170, 235, 245)};
  uint16_t c2[4] = {rgb(130, 70, 30), rgb(130, 135, 150), rgb(190, 140, 20), rgb(90, 160, 190)};
  canvas.fillTriangle(x - 5, y - 11, x + 1, y - 11, x - 1, y - 1, rgb(220, 50, 50)); canvas.fillTriangle(x + 5, y - 11, x - 1, y - 11, x + 1, y - 1, rgb(180, 30, 30));
  canvas.fillCircle(x, y + 3, 9, c2[m]); canvas.fillCircle(x, y + 3, 7, c1[m]);
  for (int i = 0; i < 5; i++) { float a = -1.57f + i * 1.2566f; canvas.fillCircle(x + (int)(cosf(a) * 4), y + 3 + (int)(sinf(a) * 4), 1, c2[m]); }
  canvas.drawPixel(x - 3, y, 0xFFFF);
}

static void render() {
  int ox = shake > 0 ? ((shake & 1) ? 2 : -2) : 0;
  drawBackground(ox);
  for (int i = 0; i < nPipes; i++) {
    Pipe &p = pipes[i]; int x = (int)p.x + ox;
    int topEnd = (int)p.gapY - gapH / 2, botStart = (int)p.gapY + gapH / 2;
    drawPipe(x, 0, topEnd - 9, false); drawPipe(x - 2, topEnd - 9, topEnd, true);
    drawPipe(x, botStart + 9, GROUND_Y, false); drawPipe(x - 2, botStart, botStart + 9, true);
  }
  // sol
  uint16_t gt = phase() == 2 ? rgb(40, 110, 50) : rgb(104, 204, 54), gd = phase() == 2 ? rgb(130, 120, 80) : rgb(222, 200, 130);
  canvas.fillRect(0, GROUND_Y, SCR_W, SCR_H - GROUND_Y, gd);
  canvas.fillRect(0, GROUND_Y, SCR_W, 4, gt); canvas.drawFastHLine(0, GROUND_Y, SCR_W, mixc(gt, 0xFFFF, 100)); canvas.drawFastHLine(0, GROUND_Y + 4, SCR_W, mixc(gt, 0, 120));
  for (int x = (int)groundX; x < SCR_W; x += 16) { canvas.fillTriangle(x, GROUND_Y + 5, x + 8, GROUND_Y + 5, x + 4, GROUND_Y + 11, mixc(gd, 0, 40)); canvas.drawPixel(x + 12, GROUND_Y + 8, mixc(gd, 0, 90)); }
  canvas.fillRect(0, GROUND_Y + 12, SCR_W, SCR_H - GROUND_Y - 12, mixc(gd, 0, 30));
  // plumes
  for (auto &f : fe) if (f.life > 0) canvas.fillRect((int)f.x, (int)f.y, 2, 1, f.life > 8 ? rgb(255, 240, 160) : rgb(240, 200, 90));
  drawBird(BIRD_X + ox, (int)by, ang, state != OVER);
  if (state == PLAY || state == PAUSED) { char b[8]; snprintf(b, sizeof b, "%d", score); txtO(b, SCR_W / 2, 6 - (scorePop > 6 ? scorePop - 6 : 0), &fonts::Font4, scorePop ? rgb(255, 240, 120) : TFT_WHITE, rgb(50, 50, 60), textdatum_t::top_center, 1.0f + scorePop * 0.03f); }
  if (state == TITLE) {
    txtO("FLAP BIRD", SCR_W / 2, 14, &fonts::Font4, rgb(255, 230, 60), rgb(110, 60, 0), textdatum_t::top_center, 1.1f);
    if ((animT / 22) & 1) txtO(L("PRESS A", "APPUIE SUR A"), SCR_W / 2, 84, &fonts::Font2, TFT_WHITE, rgb(60, 60, 60), textdatum_t::top_center);
    if (best > 0) { char b[24]; snprintf(b, sizeof b, L("best %d", "record %d"), best); txtO(b, SCR_W / 2, 100, &fonts::Font2, TFT_WHITE, rgb(60, 60, 60), textdatum_t::top_center); }
  }
  if (state == PAUSED) ui::pause();
  if (flashT > 0) canvas.fillRect(0, 0, SCR_W, SCR_H, mixc(rgb(255, 255, 255), rgb(110, 200, 235), 256 - flashT * 40));
  if (state == OVER && overTicks > 10) ui::gameOver("GAME OVER", score, best, score > 0 && score >= best);
}

void loop() {
  uint32_t now = micros();
  if (last == 0) last = now;
  acc += now - last; last = now; if (acc > 100000) acc = 100000;
  bool a = M5.BtnA.wasPressed() || M5.BtnB.wasPressed(), p = M5.BtnPWR.wasPressed();
  switch (state) {
    case TITLE: if (a) { reset(); state = PLAY; doFlap(); } break;
    case PLAY: if (p) { state = PAUSED; sfx(440, 50); } else if (a) doFlap(); break;
    case PAUSED: if (p || a) { state = PLAY; sfx(660, 50); } break;
    case OVER: if (a && overTicks >= 40) { reset(); state = PLAY; doFlap(); } break;
  }
  while (acc >= STEP_US) { if (state != PAUSED) step(); acc -= STEP_US; }
  render();
  bgm::run(0, state == PLAY);
  present(0, 0);
}

void icon(int x, int y) {
  for (int yy = 0; yy < 52; yy += 4) canvas.fillRect(x, y + yy, 64, 4, mixc(rgb(70, 170, 235), rgb(170, 225, 245), yy * 5));
  canvas.fillRect(x, y + 52, 64, 12, rgb(222, 200, 130)); canvas.fillRect(x, y + 52, 64, 3, rgb(104, 204, 54));
  for (int i = 0; i < 64; i += 2) canvas.fillRect(x + i, y + 40 - (int)(sinf(i * 0.12f) * 5), 2, 12 + (int)(sinf(i * 0.12f) * 5), rgb(110, 190, 130));
  canvas.fillRect(x + 42, y, 16, 20, rgb(96, 196, 44)); canvas.fillRect(x + 40, y + 18, 20, 7, rgb(76, 168, 38));
  canvas.fillRect(x + 42, y + 38, 16, 14, rgb(96, 196, 44)); canvas.fillRect(x + 40, y + 34, 20, 7, rgb(76, 168, 38));
  canvas.fillRect(x + 44, y, 3, 20, rgb(170, 240, 100)); canvas.fillRect(x + 44, y + 41, 3, 11, rgb(170, 240, 100));
  canvas.drawRect(x + 42, y, 16, 18, rgb(24, 64, 20)); canvas.drawRect(x + 40, y + 34, 20, 7, rgb(24, 64, 20)); canvas.drawRect(x + 40, y + 18, 20, 7, rgb(24, 64, 20));
  int saveW = wingT; wingT = 0; drawBird(x + 22, y + 30, -0.2f, true); wingT = saveW;
}

void preview(int cx, int cy) { int sw = wingT; wingT = (millis() / 110) % 3 ? 0 : 7; drawBird(cx, cy, sinf(millis() * 0.004f) * 0.2f, true); wingT = sw; }
}  // namespace flap
