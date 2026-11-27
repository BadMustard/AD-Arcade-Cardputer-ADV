// ============================================================================
//  CHROME DINO - M5StickC Plus2
//  Version fidele au Dino Game de Chrome : vrais sprites (planche Chromium, BSD),
//  memes couleurs (#535353 sur blanc, mode nuit inverse), memes mecaniques.
//  Compromis d'affichage : echelle 0.7 + jeu a 50 Hz (au lieu de 60) pour rester jouable.
// ============================================================================
#include "common.h"
#include "sprites.h"

namespace dino {

// ----------------------------- CONFIG UTILISATEUR ---------------------------
#define SWAP_SIDE_BUTTONS  0   // 1 = inverse pause (PWR) et accroupi (bouton B)
#define SOUND_ON           1
static const uint32_t STEP_US = 20000;   // 50 Hz (mettre 16667 pour la vitesse Chrome exacte)

// ----------------------------- CONSTANTES CHROME ----------------------------
static const float START_SPEED = 6.0f, MAX_SPEED = 13.0f, ACCEL = 0.001f;
static const float GAP_COEF = 0.6f, MAX_GAP_COEF = 1.5f;
static const int CLEAR_TICKS = 180;          // 3000 ms a 60 images/s
static const int GAMEOVER_TICKS = 72;        // 1200 ms
static const int INTRO_TICKS = 90;           // 1500 ms
static const int INVERT_TICKS = 720;         // 12000 ms de nuit
static const int INVERT_DISTANCE = 700;
static const float GRAV = 0.6f, INIT_JUMP = -10.0f, DROP_V = -5.0f;
static const int T_W = 44, T_H = 47, T_X = 50;
static const int GROUND_YPOS = 93;
static const int MIN_JUMP_Y = GROUND_YPOS - 30;
static const int MAX_JUMP_Y = 30;

// ----------------------------- ECRAN ----------------------------------------
static const float S = 0.7f;                 // unites monde -> pixels
static const int SW = 240, SH = 135;
static const int VIEW_W = 343;               // 240 / 0.7
static const int GP = 112;                   // y ecran de la ligne de sol (monde y = 131)
static Preferences prefs;

static inline int PX(float wx) { return (int)lroundf(wx * S); }
static inline int PYB(float wy) { return GP + (int)lroundf((wy - 131.0f) * S); }  // y ecran d'un y monde

// palette : jour #535353 sur blanc, nuit = inversion CSS (blanc->noir, #535353->#acacac)
static uint16_t lut[16], lutHi[16], lutMoon[16];
static uint16_t bgCol = 0xFFFF;
static float nightF = 0.0f;                  // 0 = jour, 1 = nuit
static bool nightTarget = false;
static inline uint16_t gray565(int v) { return ((v & 0xF8) << 8) | ((v & 0xFC) << 3) | (v >> 3); }
static void computePalette() {
  float bg = 255.0f * (1.0f - nightF), fg = 83.0f + (172.0f - 83.0f) * nightF;
  bgCol = gray565((int)bg);
  for (int l = 0; l < 16; l++) {
    float a = l / 15.0f;
    lut[l] = gray565((int)(bg + (fg - bg) * a));
    lutHi[l] = gray565((int)(bg + (fg - bg) * a * 0.8f));
    lutMoon[l] = gray565((int)(bg + (fg - bg) * a * nightF));
  }
}

// ----------------------------- OBSTACLES ------------------------------------
struct Box { int x, y, w, h; };
struct ObsType { int w, h, yPos; float multSpeed; int minGap; float minSpeed; };
static const ObsType OT[3] = {
  {17, 35, 105, 4, 120, 0.0f},
  {25, 50, 90, 7, 120, 0.0f},
  {46, 40, -1, 999, 150, 8.5f},
};
static const int PT_Y[3] = {100, 75, 50};
static const Box CB_SMALL[3] = {{0,7,5,27},{4,0,6,34},{10,4,7,14}};
static const Box CB_LARGE[3] = {{0,12,7,38},{8,0,7,49},{13,10,10,38}};
static const Box CB_PTERO[5] = {{15,15,16,5},{18,21,24,6},{2,14,4,3},{6,10,4,7},{10,8,6,9}};
static const Box CB_RUN[6]   = {{22,0,17,16},{1,18,30,9},{10,35,14,8},{1,24,29,5},{5,30,21,4},{9,34,15,4}};
static const Box CB_DUCK[1]  = {{1,18,55,25}};

struct Obs { int type, size, y, w, gap, frame; float x, speedOff; int timer; bool followed; };
static Obs obs[8];
static int nObs = 0;
static int hist[2] = {-1, -1};

// ----------------------------- ETAT DU JEU ----------------------------------
enum State { TITLE, INTRO, PLAY, PAUSED, OVER };
static State state = TITLE;
static float speed = START_SPEED, distanceRan = 0;
static int runningTicks = 0, gameOverTicks = 0, introTicks = 0, invertTimer = 0;
static int score = 0, hiScore = 0;
static int flashIter = 0, flashTimer = 0; static bool achievement = false;
static uint32_t frameCount = 0;
static int moonPhase = 6;

// dino
static float dinoX = 0, dinoY = GROUND_YPOS, jumpV = 0;
static bool jumping = false, reachedMin = false, speedDrop = false, ducking = false, relPending = false;
static int jumpBuf = 0, animT = 0, blinkDelay = 120, blinkFor = 0;
static bool duckHeld = false;

// decor
struct Cloud { float x; int y, gap; };
static Cloud clouds[6]; static int nClouds = 0;
static float groundX[2] = {0, 600}; static int groundSrc[2] = {0, 0};
static float moonX = 300;
static float starX[2] = {50, 200}; static int starY[2] = {0, 0};

// ----------------------------- PHYSIQUE DU DINO -----------------------------
static void resetJump() { dinoY = GROUND_YPOS; jumpV = 0; jumping = false; speedDrop = false; reachedMin = false; relPending = false; }
static void endJump() { if (reachedMin && jumpV < DROP_V) jumpV = DROP_V; }
static void startJump() {
  jumping = true; ducking = false; reachedMin = false; speedDrop = false; relPending = false;
  jumpV = INIT_JUMP - (speed / 10.0f);
  sfx(660, 40); sfx(990, 50, false);
}
static void updateJump() {
  dinoY += roundf(speedDrop ? jumpV * 3.0f : jumpV);
  jumpV += GRAV;
  if (dinoY < MIN_JUMP_Y || speedDrop) reachedMin = true;
  if (reachedMin && relPending) { endJump(); relPending = false; }
  if (dinoY < MAX_JUMP_Y || speedDrop) endJump();
  if (dinoY > GROUND_YPOS) resetJump();
}

// ----------------------------- OBSTACLES ------------------------------------
static bool dupCheck(int t) { return hist[0] == t && hist[1] == t; }
static void addObstacle() {
  for (int tries = 0; tries < 12; tries++) {
    int t = rnd(0, 2);
    if (dupCheck(t) || speed < OT[t].minSpeed) continue;
    if (nObs >= 8) return;
    Obs o{};
    o.type = t;
    int sz = rnd(1, 3);
    if (sz > 1 && OT[t].multSpeed > speed) sz = 1;
    o.size = sz; o.w = OT[t].w * sz;
    o.y = OT[t].yPos >= 0 ? OT[t].yPos : PT_Y[rnd(0, 2)];
    o.x = VIEW_W + OT[t].w;
    int minGap = lroundf(o.w * speed + OT[t].minGap * GAP_COEF);
    int maxGap = lroundf(minGap * MAX_GAP_COEF);
    o.gap = rnd(minGap, maxGap);
    o.speedOff = (t == 2) ? (rnd(0, 1) ? 0.8f : -0.8f) : 0.0f;
    o.followed = false;
    obs[nObs++] = o;
    hist[1] = hist[0]; hist[0] = t;
    return;
  }
}
static void updateObstacles() {
  for (int i = 0; i < nObs; i++) {
    Obs &o = obs[i];
    o.x -= floorf(speed + o.speedOff + 0.0001f);
    if (o.type == 2) { if (++o.timer >= 10) { o.frame ^= 1; o.timer = 0; } }
  }
  while (nObs > 0 && obs[0].x + obs[0].w <= 0) { memmove(&obs[0], &obs[1], sizeof(Obs) * (nObs - 1)); nObs--; }
  if (nObs > 0) {
    Obs &l = obs[nObs - 1];
    if (!l.followed && l.x + l.w > 0 && l.x + l.w + l.gap < VIEW_W) { l.followed = true; addObstacle(); }
  } else addObstacle();
}
static bool overlap(const Box &a, const Box &b) {
  return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}
static bool collide(const Obs &o) {
  const ObsType &ot = OT[o.type];
  Box t = {T_X + 1, (int)dinoY + 1, T_W - 2, T_H - 2};
  Box ob = {(int)o.x + 1, o.y + 1, ot.w * o.size - 2, ot.h - 2};
  if (!overlap(t, ob)) return false;
  Box ob_[5]; int n; const Box *src;
  if (o.type == 0) { src = CB_SMALL; n = 3; } else if (o.type == 1) { src = CB_LARGE; n = 3; } else { src = CB_PTERO; n = 5; }
  for (int i = 0; i < n; i++) ob_[i] = src[i];
  if (o.type != 2 && o.size > 1) { ob_[1].w = o.w - ob_[0].w - ob_[2].w; ob_[2].x = o.w - ob_[2].w; }
  const Box *tb = ducking ? CB_DUCK : CB_RUN; int tn = ducking ? 1 : 6;
  for (int i = 0; i < tn; i++) {
    Box a = {tb[i].x + T_X, tb[i].y + (int)dinoY, tb[i].w, tb[i].h};
    for (int j = 0; j < n; j++) {
      Box b = {ob_[j].x + (int)o.x, ob_[j].y + o.y, ob_[j].w, ob_[j].h};
      if (overlap(a, b)) return true;
    }
  }
  return false;
}

// ----------------------------- DECOR (comme Chrome) -------------------------
static void groundUpdate(float inc) {
  int l1 = groundX[0] <= 0 ? 0 : 1;
  int l2 = 1 - l1;
  groundX[l1] -= inc; groundX[l2] = groundX[l1] + 600;
  if (groundX[l1] <= -600) { groundX[l1] += 1200; groundX[l2] = groundX[l1] - 600; groundSrc[l1] = (esp_random() & 1) ? 420 : 0; }
}
static void cloudsUpdate() {
  int inc = (int)ceilf(0.2f * speed);
  for (int i = 0; i < nClouds; i++) clouds[i].x -= inc;
  while (nClouds > 0 && clouds[0].x + 46 <= 0) { memmove(&clouds[0], &clouds[1], sizeof(Cloud) * (nClouds - 1)); nClouds--; }
  bool add = nClouds == 0;
  if (!add && nClouds < 6) { Cloud &l = clouds[nClouds - 1]; add = (l.x + 46 + l.gap < VIEW_W) && (rnd(0, 99) < 50); }
  if (add && nClouds < 6) clouds[nClouds++] = {(float)VIEW_W, (int)rnd(30, 71), (int)rnd(100, 400)};
}

// ----------------------------- CYCLE DE JEU ---------------------------------
static void resetWorld() {
  speed = START_SPEED * dif(0.86f, 1.0f, 1.12f); distanceRan = 0; runningTicks = 0; score = 0; nObs = 0;
  hist[0] = hist[1] = -1; invertTimer = 0; nightTarget = false; achievement = false; flashIter = flashTimer = 0;
  dinoY = GROUND_YPOS; jumpV = 0; jumping = false; ducking = false; speedDrop = false; jumpBuf = 0;
}
static void beginIntro() {
  resetWorld(); state = INTRO; introTicks = 0; dinoX = 0;
  groundX[0] = 0; groundX[1] = 600; groundSrc[0] = 0; groundSrc[1] = 420;
  startJump();
}
static void restartGame() {
  resetWorld(); state = PLAY; dinoX = T_X;
  sfx(660, 40); sfx(990, 50, false);
}
static void crash() {
  state = OVER; gameOverTicks = 0;
  trophy::record(trophy::T_DINO, score); if (score > hiScore) { hiScore = score; prefs.putInt("hi", hiScore); }
  sfx(220, 120); sfx(110, 220, false);
}
static void updateScore() {
  int s = lroundf(ceilf(distanceRan) * 0.025f);
  if (s == score) return;
  score = s;
  if (s > 0 && s % 100 == 0) { achievement = true; flashIter = 0; flashTimer = 0; sfx(1568, 60); sfx(1568, 70, false); }
  // mode nuit : tous les 700 points, pendant 12 s
  if (invertTimer == 0 && s > 0 && s % INVERT_DISTANCE == 0) {
    invertTimer = 1; nightTarget = true;
    moonPhase = (moonPhase + 1) % 7; moonX = VIEW_W - 50; starX[0] = rnd(0, VIEW_W / 2); starX[1] = rnd(VIEW_W / 2, VIEW_W);
    starY[0] = rnd(0, 70); starY[1] = rnd(0, 70);
  }
}
static void commonStep() {
  frameCount++;
  if (nightTarget && nightF < 1.0f) { nightF += 1.0f / 90; if (nightF > 1) nightF = 1; }
  if (!nightTarget && nightF > 0.0f) { nightF -= 1.0f / 90; if (nightF < 0) nightF = 0; }
  computePalette();
  if (nightF > 0) { moonX -= 0.25f; if (moonX < -40) moonX = VIEW_W; starX[0] -= 0.3f; starX[1] -= 0.3f; for (int i = 0; i < 2; i++) if (starX[i] < -9) starX[i] = VIEW_W; }
  if (achievement) {
    if (flashIter < 3) { flashTimer++; if (flashTimer > 30) { flashTimer = 0; flashIter++; } }
    else { achievement = false; flashIter = 0; flashTimer = 0; }
  }
}
static void stepPlay() {
  if (jumpBuf > 0) { if (!jumping) { startJump(); jumpBuf = 0; } else jumpBuf--; }
  if (!jumping) ducking = duckHeld; else ducking = false;
  if (jumping) updateJump();
  animT++;
  runningTicks++;
  bool hasObs = runningTicks > CLEAR_TICKS;
  groundUpdate(floorf(speed)); cloudsUpdate();
  if (hasObs) updateObstacles();
  bool hit = hasObs && nObs > 0 && collide(obs[0]);
  if (!hit) {
    distanceRan += speed;
    if (speed < MAX_SPEED * dif(0.86f, 1.0f, 1.12f)) speed += ACCEL * dif(0.8f, 1.0f, 1.25f);
    updateScore();
    if (invertTimer > INVERT_TICKS) { invertTimer = 0; nightTarget = false; }
    else if (invertTimer) invertTimer++;
  } else crash();
  commonStep();
}
static void stepIntro() {
  if (jumping) updateJump();
  introTicks++;
  dinoX = (float)T_X * introTicks / INTRO_TICKS;
  if (introTicks >= INTRO_TICKS) { dinoX = T_X; state = PLAY; runningTicks = 0; }
  commonStep();
}
static void stepIdle() {
  if (state == OVER) gameOverTicks++;
  if (state == TITLE) {
    if (blinkFor > 0) blinkFor--;
    else if (--blinkDelay <= 0) { blinkFor = 12; blinkDelay = rnd(60, 420); }
  }
  commonStep();
}

// ----------------------------- DESSIN ---------------------------------------
static void blit(const Spr &s, int x, int y, const uint16_t *pal = lut, int srcX = 0, int srcW = -1, int clipX = SW) {
  if (srcW < 0) srcW = s.w;
  for (int j = 0; j < s.h; j++) {
    int yy = y + j; if (yy < 0 || yy >= SH) continue;
    const uint8_t *row = s.d + j * s.w + srcX;
    for (int i = 0; i < srcW; i++) {
      int xx = x + i; if (xx < 0 || xx >= SW || xx >= clipX) continue;
      uint8_t a = row[i]; if (a) canvas.drawPixel(xx, yy, pal[a]);
    }
  }
}
static const Spr *GLYPHS[12] = {&GLYPH0,&GLYPH1,&GLYPH2,&GLYPH3,&GLYPH4,&GLYPH5,&GLYPH6,&GLYPH7,&GLYPH8,&GLYPH9,&GLYPH10,&GLYPH11};
static const Spr *CACT_SM[3] = {&CACT_S1, &CACT_S2, &CACT_S3};
static const Spr *CACT_LG[3] = {&CACT_L1, &CACT_L2, &CACT_L3};
static const Spr *PTERO[2] = {&PTERO0, &PTERO1};
static const Spr *MOONS[7] = {&MOON0,&MOON1,&MOON2,&MOON3,&MOON4,&MOON5,&MOON6};

static void drawNumber(int v, int x, int y, const uint16_t *pal) {
  char b[8]; snprintf(b, sizeof b, "%05d", v % 100000);
  for (int i = 0; i < 5; i++) blit(*GLYPHS[b[i] - '0'], x + i * 8, y, pal);
}
static void drawDino(bool crashed) {
  const Spr *s;
  if (crashed) s = &DINO_CRASH;
  else if (state == TITLE) s = blinkFor > 0 ? &DINO_BLINK : &DINO_IDLE;
  else if (jumping || state == INTRO) s = &DINO_IDLE;
  else if (ducking) s = (((int)(animT / 7.5f)) & 1) ? &DINO_DUCK2 : &DINO_DUCK1;
  else s = ((animT / 5) & 1) ? &DINO_RUN2 : &DINO_RUN1;
  int bottom = PYB(dinoY + T_H);
  blit(*s, PX(dinoX), bottom - s->h);
}
static void render() {
  canvas.fillScreen(bgCol);
  // etoiles + lune (mode nuit)
  if (nightF > 0.02f) {
    blit(STAR0, PX(starX[0]), PYB(starY[0]), lutMoon);
    blit(STAR1, PX(starX[1]), PYB(starY[1]), lutMoon);
    blit(*MOONS[moonPhase], PX(moonX), PYB(30), lutMoon);
  }
  if (state != TITLE) {
    if (state != INTRO) for (int i = 0; i < nClouds; i++) blit(CLOUD, PX(clouds[i].x), PYB(clouds[i].y));
    // sol : 2 segments de 420 px
    int clip = (state == INTRO) ? 31 + (int)((SW - 31) * (float)introTicks / INTRO_TICKS) : SW;
    for (int k = 0; k < 2; k++) blit(GROUND, PX(groundX[k]), GP - 3, lut, groundSrc[k], 420, clip);
    for (int i = 0; i < nObs; i++) {
      const Obs &o = obs[i];
      const Spr *sp = o.type == 0 ? CACT_SM[o.size - 1] : o.type == 1 ? CACT_LG[o.size - 1] : PTERO[o.frame];
      blit(*sp, PX(o.x), PYB(o.y + OT[o.type].h) - sp->h);
    }
  }
  drawDino(state == OVER);
  // score
  if (state != TITLE) {
    bool vis = !(achievement && flashIter < 3 && flashTimer < 15);
    int sx = PX(VIEW_W - 66);
    if (vis) drawNumber(score, sx, 4, lut);
    if (hiScore > 0) {
      int hx = sx - PX(100);
      blit(GLYPH10, hx, 4, lutHi); blit(GLYPH11, hx + 8, 4, lutHi);
      drawNumber(hiScore, hx + 24, 4, lutHi);
    }
  }
  if (state == TITLE) {
    canvas.setFont(&fonts::Font0); canvas.setTextDatum(textdatum_t::top_left); canvas.setTextColor(lut[9]);
    if (hiScore > 0) { int hx = SW - 8 * 8 - 6; blit(GLYPH10, hx, 4, lutHi); blit(GLYPH11, hx + 8, 4, lutHi); drawNumber(hiScore, hx + 24, 4, lutHi); }
  }
  if (state == OVER) {
    blit(GAMEOVER, (SW - GAMEOVER.w) / 2, PYB(42));
    blit(RESTART, (SW - RESTART.w) / 2, PYB(75));
  }
  if (state == PAUSED) {
    canvas.setFont(&fonts::Font0); canvas.setTextDatum(textdatum_t::top_center); canvas.setTextColor(lut[15]);
    canvas.drawString("PAUSE", SW / 2, 32);
  }
}

// ----------------------------- ENTREES --------------------------------------
static void handleInput() {
  bool aP = M5.BtnA.wasPressed(), aR = M5.BtnA.wasReleased();
#if SWAP_SIDE_BUTTONS
  bool pauseEv = M5.BtnB.wasPressed(), duckEv = M5.BtnPWR.wasPressed(); duckHeld = M5.BtnPWR.isPressed();
#else
  bool pauseEv = M5.BtnPWR.wasPressed(), duckEv = M5.BtnB.wasPressed(); duckHeld = M5.BtnB.isPressed();
#endif
  switch (state) {
    case TITLE: if (aP) beginIntro(); break;
    case INTRO: case PLAY:
      if (state == PLAY && pauseEv) { state = PAUSED; sfx(440, 50); break; }
      if (state == PLAY && aP) jumpBuf = 6;
      if (aR) { if (jumping) { if (reachedMin) endJump(); else relPending = true; } jumpBuf = 0; }
      if (duckEv && jumping && !speedDrop) { speedDrop = true; jumpV = 1; }
      break;
    case PAUSED: if (pauseEv) { state = PLAY; sfx(660, 50); } break;
    case OVER: if (aP && gameOverTicks >= GAMEOVER_TICKS) restartGame(); break;
  }
}

// ----------------------------- INTERFACE POUR LE MENU -----------------------
void begin() {
  prefs.begin(dns("chromedino"), false);
  hiScore = prefs.getInt("hi", 0);
  state = TITLE; resetWorld(); nightF = 0; nightTarget = false;
  nClouds = 0; groundSrc[0] = 0; groundSrc[1] = 420; groundX[0] = 0; groundX[1] = 600;
  clouds[nClouds++] = {(float)VIEW_W - 120, 40, 200};
  dinoX = 12; dinoY = GROUND_YPOS; blinkFor = 0; blinkDelay = 120; frameCount = 0;
  computePalette();
  M5.Display.fillScreen(0xFFFF);
}

void loop() {
  static uint32_t last = 0, acc = 0;
  uint32_t now = micros();
  if (last == 0 || (state == TITLE && frameCount == 0)) last = now;
  acc += now - last; last = now;
  if (acc > 100000) acc = 100000;
  handleInput();
  while (acc >= STEP_US) {
    if (state == PLAY) stepPlay(); else if (state == INTRO) stepIntro(); else if (state != PAUSED) stepIdle();
    acc -= STEP_US;
  }
  render();
  bgm::run(7, state == PLAY);
  present(0, 0);
}

// icone 64x64 : un dino qui court sur du sable avec un cactus
void icon(int x, int y) {
  nightF = 0; computePalette();
  canvas.fillRoundRect(x, y, 64, 64, 6, 0xFFFF);
  canvas.drawFastHLine(x + 3, y + 48, 58, lut[15]);
  canvas.drawFastHLine(x + 10, y + 52, 6, lut[8]); canvas.drawFastHLine(x + 34, y + 55, 10, lut[8]);
  blit(DINO_RUN1, x + 8, y + 48 - DINO_RUN1.h + 1);
  blit(CACT_L1, x + 46, y + 48 - CACT_L1.h + 1);
  blit(CLOUD, x + 18, y + 8);
}

}  // namespace dino
