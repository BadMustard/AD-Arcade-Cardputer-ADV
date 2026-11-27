// ============================================================================
//  STACK - un bloc glisse, M5 (A) ou B = le poser sur la tour.
//  Ce qui depasse tombe dans le vide : le bloc retrecit. Rate completement = fin.
//  Pose parfaite (a 3 px pres) = pas de perte, 3 parfaits de suite = le bloc regrandit.
//  HAUT (PWR) = pause
// ============================================================================
#include "common.h"

namespace stack {

static const uint32_t STEP_US = 20000;
static const int BH = 9, START_W = 84, MAX_W = 96, MAXL = 220;
enum State { TITLE, PLAY, PAUSED, OVER };
static State state = TITLE;
static Preferences pr;
static uint32_t last = 0, acc = 0;

struct Layer { float x, w; uint8_t hue; };
static Layer LY[MAXL]; static int n = 0;                 // blocs poses (LY[0] = base)
static float mx, mw, mdir; static uint8_t mhue;         // bloc qui glisse
static float cam = 0;
static int combo = 0, perfectT = 0, endT = 0, animT = 0;
static float perfX, perfW; static int perfN;
static uint32_t best = 0;
struct Chunk { float x, y, w, vy; uint16_t col; bool on; }; static Chunk chunks[8];
struct Star { uint8_t x, y, b; }; static Star stars[40];

static uint16_t hsv(int h, float s, float v) {            // (nom conserve) : teinte -> palette commune
  static const uint16_t PC[5] = {P_R, P_G, P_S, P_L, P_B};
  int k = (((h % 360) + 360) % 360) / 72; (void)s; (void)v; return PC[k];
}
static inline float layerY(int i) { return SCR_H - (i + 1) * BH + cam; }          // y ecran du haut du bloc i
static inline uint8_t hueOf(int i) { return (uint8_t)((i * 6) % 240); }              // teinte 0..239 -> x1.5 = 0..360

static void newBlock() {
  mhue = hueOf(n);
  mw = LY[n - 1].w;
  bool fromLeft = (n & 1) == 0;
  mx = fromLeft ? 0 : SCR_W - mw; mdir = fromLeft ? 1 : -1;
}
static float spd() { float m = dif(0.8f, 1.0f, 1.2f); float v = (1.7f + n * 0.05f) * m; return v > 5.0f * m ? 5.0f * m : v; }

static void reset() {
  n = 1; LY[0] = {(SCR_W - START_W) / 2.f, (float)START_W, hueOf(0)};
  combo = 0; perfectT = 0; endT = 0; animT = 0; cam = 0;
  for (auto &c : chunks) c.on = false;
  newBlock();
}
void begin() {
  pr.begin(dns("stack"), false); best = pr.getInt("best", 0);
  for (auto &s : stars) s = {(uint8_t)rnd(0, SCR_W - 1), (uint8_t)rnd(0, SCR_H - 1), (uint8_t)rnd(80, 200)};
  reset(); state = TITLE; last = 0; acc = 0;
}

static void addChunk(float x, float y, float w, uint16_t col) {
  if (w < 1) return;
  for (auto &c : chunks) if (!c.on) { c = {x, y, w, 0.5f, col, true}; return; }
}
static void gameOver() {
  state = OVER; endT = 0;
  uint32_t sc = n - 1;
  trophy::record(trophy::T_STACK, sc); if (sc > best) { best = sc; pr.putInt("best", best); }
  sfx(300, 120); sfx(120, 320, false);
}

static void drop() {
  const Layer &t = LY[n - 1];
  float y = layerY(n);
  uint16_t col = hsv(mhue * 3 / 2, 0.55f, 0.95f);
  float left = fmaxf(mx, t.x), right = fminf(mx + mw, t.x + t.w), ov = right - left;
  if (ov <= 0) {                                 // rate completement : le bloc entier tombe
    addChunk(mx, y, mw, col); sfx(200, 100); sfx(120, 250, false); gameOver(); return;
  }
  if (fabsf(mx - t.x) <= 3.0f) {                 // pose parfaite
    mx = t.x; ov = mw = fminf(mw, t.w); combo++; left = mx;
    if (combo >= 3) { float nw = fminf(MAX_W, mw + 3); float grow = nw - mw; mx -= grow / 2; if (mx < 0) mx = 0; if (mx + nw > SCR_W) mx = SCR_W - nw; mw = nw; ov = nw; left = mx; }
    perfectT = 18; perfX = left; perfW = ov; perfN = n;
    int cb = combo > 8 ? 8 : combo; sfx(880 + cb * 110, 50); sfx(1320 + cb * 110, 70, false);
  } else {                                       // on coupe ce qui depasse
    combo = 0;
    if (mx < t.x) addChunk(mx, y, t.x - mx, col);                       // depasse a gauche
    if (mx + mw > t.x + t.w) addChunk(t.x + t.w, y, mx + mw - (t.x + t.w), col);   // depasse a droite
    sfx(220, 40);
  }
  if (n < MAXL) { LY[n] = {left, ov, mhue}; n++; }
  else { memmove(&LY[0], &LY[MAXL / 2], sizeof(Layer) * (MAXL / 2)); n = MAXL / 2; LY[n++] = {left, ov, mhue}; }   // tres longue partie : on recycle
  newBlock();
}

static void step() {
  animT++;
  for (auto &c : chunks) if (c.on) { c.vy += 0.35f; c.y += c.vy; if (c.y > SCR_H + 12) c.on = false; }
  if (perfectT > 0) perfectT--;
  float camT = fmaxf(0, (n + 1) * BH - (SCR_H - 42));
  cam += (camT - cam) * 0.12f;
  if (state == TITLE) { mx += mdir * 1.8f; if (mx < 0) { mx = 0; mdir = 1; } if (mx + mw > SCR_W) { mx = SCR_W - mw; mdir = -1; } return; }
  if (state == OVER) { endT++; return; }
  mx += mdir * spd();
  if (mx < 0) { mx = 0; mdir = 1; }
  if (mx + mw > SCR_W) { mx = SCR_W - mw; mdir = -1; }
}

static void drawBlock(float x, float y, float w, uint16_t col) {
  int ix = (int)(x + 0.5f), iy = (int)(y + 0.5f), iw = (int)(w + 0.5f);
  if (iw < 1) return;
  uint16_t dk = col == P_R ? P_D : P_M;
  pxBox(ix, iy, iw, BH, col, P_B, dk);
  for (int wx = ix + 4; wx + 2 < ix + iw - 3; wx += 6) canvas.fillRect(wx, iy + 3, 2, 2, P_D);       // fenetres
}

static void render() {
  // ciel : bandes plates, de plus en plus sombres avec la hauteur
  static const uint16_t SKY[3][5] = {{P_D, P_M, P_M, P_L, P_L}, {P_K, P_D, P_D, P_M, P_M}, {P_K, P_K, P_D, P_D, P_D}};
  int stage = n > 30 ? 2 : (n > 12 ? 1 : 0);
  for (int k = 0; k < 5; k++) canvas.fillRect(0, k * 27, SCR_W, 27, SKY[stage][k]);
  if (n > 8) for (auto &s : stars) { int sy = ((int)s.y + (int)(cam * 0.25f)) % SCR_H; canvas.fillRect(s.x, sy, 2, 2, s.b > 150 ? P_B : P_L); }
  // soleil + nuages (bas de la tour) puis skyline et sol
  if (cam < SCR_H) {
    int sy = 34 + (int)(cam * 0.35f);
    pxDisc(188, sy, 11, P_G, P_B, P_R);
    for (int i = 0; i < 3; i++) { int cx = ((i * 97 + animT / 3 + i * 17) % (SCR_W + 60)) - 30, cy = 22 + i * 20 + (int)(cam * 0.5f); canvas.fillRect(cx - 15, cy - 1, 30, 9, P_K); canvas.fillRect(cx - 14, cy, 28, 7, P_B); canvas.fillRect(cx - 8, cy - 4, 14, 5, P_K); canvas.fillRect(cx - 7, cy - 3, 12, 5, P_B); canvas.fillRect(cx - 14, cy + 5, 28, 2, P_L); }
    int gy = (int)(SCR_H + cam);
    for (int x = 0; x < SCR_W; x += 10) { uint32_t hh = (uint32_t)(x * 2654435761u); int bh = 14 + (hh >> 20) % 34; canvas.fillRect(x, gy - bh, 10, bh, P_K); canvas.fillRect(x + 1, gy - bh + 1, 8, bh - 1, (hh >> 9) & 1 ? P_D : P_M); canvas.fillRect(x + 1, gy - bh + 1, 8, 1, P_L); if (((hh >> 12) & 3) == 0) canvas.fillRect(x + 3, gy - bh + 4, 2, 2, P_G); }
    canvas.fillRect(0, gy - 1, SCR_W, 5, P_K); canvas.fillRect(0, gy, SCR_W, 2, P_L);
  }
  // tour
  for (int i = 0; i < n; i++) { float y = layerY(i); if (y > SCR_H || y < -BH) continue; drawBlock(LY[i].x, y, LY[i].w, hsv(LY[i].hue * 3 / 2, 0.55f, 0.95f)); }
  // effet "parfait"
  if (perfectT > 0 && perfN > 0) { float y = layerY(perfN); int k = 18 - perfectT; canvas.drawRect((int)perfX - k, (int)y - k, (int)perfW + 2 * k, BH + 2 * k, P_G); }
  // morceaux qui tombent
  for (auto &c : chunks) if (c.on) drawBlock(c.x, c.y, c.w, c.col);
  // bloc qui glisse
  if (state != OVER) drawBlock(mx, layerY(n), mw, hsv(mhue * 3 / 2, 0.55f, 0.95f));
  // infos
  char b[32];
  if (state != TITLE) { snprintf(b, sizeof b, "%d", n - 1); txtO(b, SCR_W / 2, 2, &fonts::Font4, P_B, P_K, textdatum_t::top_center); }
  if (state != TITLE && combo >= 2) { snprintf(b, sizeof b, L("PERFECT x%d","PARFAIT x%d"), combo); txtO(b, SCR_W / 2, 26, &fonts::Font2, P_G, P_K, textdatum_t::top_center); }
  if (state == TITLE) {
    ui::window(14, 10, 212, 60, P_G, "STACK");
    txt(L("Drop blocks, build high!", "Empile le plus haut !"), SCR_W / 2, 36, &fonts::Font2, P_K, textdatum_t::top_center);
    if (best > 0) { snprintf(b, sizeof b, L("BEST %lu","RECORD %lu"), (unsigned long)best); txt(b, SCR_W / 2, 52, &fonts::Font0, P_D, textdatum_t::top_center); }
  }
  if (state == PAUSED) ui::pause();
  if (state == OVER) ui::gameOver("GAME OVER", (uint32_t)(n > 1 ? n - 1 : 0), best, ((uint32_t)(n > 1 ? n - 1 : 0)) > 0 && (uint32_t)((uint32_t)(n > 1 ? n - 1 : 0)) >= (uint32_t)(best));
}

void loop() {
  uint32_t now = micros();
  if (last == 0) last = now;
  acc += now - last; last = now; if (acc > 100000) acc = 100000;
  bool a = M5.BtnA.wasPressed() || M5.BtnB.wasPressed(), p = M5.BtnPWR.wasPressed();
  switch (state) {
    case TITLE: if (a) { reset(); state = PLAY; sfx(660, 40); } break;
    case PLAY: if (p) { state = PAUSED; sfx(440, 50); } else if (a) drop(); break;
    case PAUSED: if (p || a) { state = PLAY; sfx(660, 50); } break;
    case OVER: if (a && endT > 40) { reset(); state = PLAY; sfx(660, 40); } break;
  }
  while (acc >= STEP_US) { if (state != PAUSED) step(); acc -= STEP_US; }
  render();
  bgm::run(3, state == PLAY);
  present(0, 0);
}

void icon(int x, int y) {
  canvas.fillRoundRect(x, y, 64, 64, 6, P_M);
  canvas.fillRect(x, y + 36, 64, 28, P_D); canvas.fillRect(x + 2, y + 56, 60, 6, P_K); canvas.fillRect(x + 2, y + 56, 60, 1, P_L);
  static const int W[5] = {42, 36, 28, 28, 20}, X[5] = {11, 14, 18, 18, 22};
  static const uint16_t C[5] = {P_R, P_G, P_S, P_L, P_B};
  for (int i = 0; i < 5; i++) { int by = y + 56 - (i + 1) * 9; pxBox(x + X[i], by, W[i], 10, C[i], P_B, C[i] == P_R ? P_D : P_M); for (int wx = x + X[i] + 4; wx + 2 < x + X[i] + W[i] - 3; wx += 6) canvas.fillRect(wx, by + 4, 2, 2, P_D); }
  pxBox(x + 38, y + 4, 22, 10, P_G, P_B, P_M);
}

}  // namespace stack
