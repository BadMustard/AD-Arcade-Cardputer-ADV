// ============================================================================
//  SLALOM - M5 a l'horizontale : pencher a droite / a gauche = le skieur tourne
//  Evite les arbres et les rochers, passe entre les portes (rouge/bleu) pour marquer
//  B (cote) = recalibrer (ecran titre) ; HAUT (PWR) = pause
// ============================================================================
#include "common.h"

namespace ski {

static const uint32_t STEP_US = 20000;
static const int SKY = 40;                 // y du skieur
static const float GATE_HALF = 24;
enum State { TITLE, READY, PLAY, PAUSED, OVER };
static State state = TITLE;
static Preferences pr;
static uint32_t last = 0, acc = 0;

struct Obj { float x, y; uint8_t type; bool scored, on; };      // 0 arbre, 1 petit arbre, 2 rocher, 3 porte
static Obj objs[20];
struct Deco { float x, y; uint8_t s; }; static Deco deco[14];
struct Spec { float x, y; }; static Spec specs[28];
struct Track { float xl, xr, y; }; static Track trk[48]; static int nTrk = 0;
struct Pop { float x, y; int t; char s[10]; uint16_t col; }; static Pop pops[4];

static float sx = 120, sv = 0, speed = 1.8f;
static int lives = 3, invul = 0, endT = 0, animT = 0, combo = 0, gates = 0, missT = 0, steps = 0, slowT = 0;
static float dist = 0, spawnAcc = 0;
static uint32_t score = 0, best = 0, bonus = 0;

static const uint16_t SNOW = rgb(238, 244, 252), SNOW2 = rgb(214, 226, 242), TRACK = rgb(186, 201, 224);

static void pop(float x, float y, const char* s, uint16_t col) { for (auto &p : pops) if (p.t <= 0) { p.x = x; p.y = y; p.t = 40; p.col = col; strncpy(p.s, s, 9); p.s[9] = 0; return; } }

static void spawnObj(float x, uint8_t type) {
  for (auto &o : objs) if (!o.on) { o = {x, (float)SCR_H + 24, type, false, true}; return; }
}
static void spawnRow() {
  int r = rnd(0, 99);
  if (r < 44) {                                          // 1 a 3 arbres
    int n = rnd(1, 3); float xs[3];
    for (int i = 0; i < n; i++) {
      for (int tries = 0; tries < 8; tries++) { xs[i] = rnd(20, 220); bool ok = true; for (int j = 0; j < i; j++) if (fabsf(xs[i] - xs[j]) < 34) ok = false; if (ok) break; }
      spawnObj(xs[i], rnd(0, 2) ? 0 : 1);
    }
  } else if (r < 58) spawnObj(rnd(24, 216), 2);          // rocher
  else if (r < 85) spawnObj(rnd(50, 190), 3);            // porte
}

static void reset() {
  sx = SCR_W / 2; sv = 0; speed = 1.8f * dif(0.88f, 1.0f, 1.1f); lives = difi(5, 3, 2); invul = 0; combo = 0; gates = 0; score = 0; bonus = 0; dist = 0; steps = 0; slowT = 0; spawnAcc = 0; nTrk = 0; endT = 0; missT = 0;
  for (auto &o : objs) o.on = false;
  for (auto &p : pops) p.t = 0;
  for (int i = 0; i < 14; i++) deco[i] = {(float)((i & 1) ? SCR_W - 4 + (int)rnd(0, 8) : 2 - (int)rnd(0, 8)), (float)(i * 13 + rnd(0, 8)), (uint8_t)rnd(0, 1)};
  for (auto &s : specs) s = {(float)rnd(0, SCR_W), (float)rnd(0, SCR_H)};
}
void begin() {
  pr.begin(dns("ski"), false); best = pr.getInt("best", 0);
  tilt::load();
  reset(); state = TITLE; last = 0; acc = 0;
}

static void gameOver() {
  state = OVER; endT = 0;
  trophy::record(trophy::T_SLALOM, score); if (score > best) { best = score; pr.putInt("best", best); }
  sfx(300, 120); sfx(120, 320, false);
}
static void crash() {
  if (--lives <= 0) { gameOver(); return; }
  invul = 80; slowT = 120; combo = 0; speed = 1.4f; sv = 0;
  sfx(180, 90); sfx(100, 140, false); pop(sx, SKY - 14, L("OUCH!","AIE !"), rgb(230, 70, 60));
}

static void step() {
  animT++;
  for (auto &p : pops) if (p.t > 0) { p.t--; p.y -= 0.6f; }
  if (state == TITLE || state == PAUSED) return;
  if (state == OVER) { endT++; }
  if (state == PLAY || state == READY || state == OVER) {
    if (state != OVER) {
      tilt::update();
      float t = ctrl::x(); if (fabsf(t) < 0.07f) t = 0; else t = t > 0 ? t - 0.07f : t + 0.07f;
      sv += (t * 4.0f - sv) * 0.22f;
      sx = clampf(sx + sv, 10, SCR_W - 10);
    }
  }
  float sp = (state == PLAY) ? speed : speed * 0.9f;
  if (state == OVER) { speed *= 0.96f; sp = speed; }
  // defilement du decor
  for (auto &s : specs) { s.y -= sp * 1.1f; if (s.y < 0) { s.y += SCR_H; s.x = rnd(0, SCR_W); } }
  for (auto &d : deco) { d.y -= sp; if (d.y < -26) { d.y = SCR_H + 20 + rnd(0, 10); d.x = (rnd(0, 1)) ? SCR_W - 4 + (int)rnd(0, 8) : 2 - (int)rnd(0, 8); d.s = rnd(0, 1); } }
  for (int i = 0; i < nTrk; i++) trk[i].y -= sp;
  while (nTrk > 0 && trk[0].y < -4) { memmove(&trk[0], &trk[1], sizeof(Track) * (nTrk - 1)); nTrk--; }
  if (state != OVER && nTrk < 48 && !(invul > 0 && (animT & 2))) trk[nTrk++] = {sx - 3, sx + 3, (float)SKY + 9};
  for (auto &o : objs) if (o.on) { o.y -= sp; if (o.y < -26) o.on = false; }
  if (state != PLAY) return;
  // ----- PLAY -----
  steps++;
  float dm_ = dif(0.88f, 1.0f, 1.1f); float base = (1.8f + steps * 0.0009f) * dm_; if (base > 4.3f * dm_) base = 4.3f * dm_;
  if (slowT > 0) { slowT--; base = base * 0.6f; }
  speed += (base - speed) * 0.04f;
  dist += speed; score = (uint32_t)(dist / 9.0f) + bonus;
  spawnAcc += speed;
  float spacing = 42 - speed * 2.2f;
  if (spawnAcc >= spacing) { spawnAcc -= spacing; spawnRow(); }
  if (invul > 0) invul--;
  if (missT > 0) missT--;
  for (auto &o : objs) if (o.on) {
    float cy = SKY + 8;
    if (o.type == 3) {
      if (!o.scored && o.y <= cy) {
        o.scored = true;
        if (fabsf(sx - o.x) < GATE_HALF - 3) { combo++; gates++; int pts = 20 + (combo > 6 ? 6 : combo) * 5; char b[16]; snprintf(b, sizeof b, "+%d", pts); bonus += pts; pop(sx, SKY - 14, b, rgb(70, 190, 70)); sfx(1100 + (combo > 6 ? 6 : combo) * 90, 40); sfx(1500 + (combo > 6 ? 6 : combo) * 90, 50, false); }
        else { combo = 0; missT = 40; pop(sx, SKY - 14, L("MISS","RATE"), rgb(230, 120, 40)); sfx(260, 80); }
      }
      continue;
    }
    if (invul > 0) continue;
    float dx = fabsf(sx - o.x), dy = fabsf(cy - o.y);
    bool hit = false;
    if (o.type == 0) hit = dx < 9 && dy < 6;
    else if (o.type == 1) hit = dx < 8 && dy < 5;
    else hit = dx < 11 && dy < 6;
    if (hit) { o.on = false; crash(); if (state != PLAY) return; break; }
  }
}

// ---------------------------------------------------------------------------
static void drawTree(int x, int y, bool small) {       // (x, y) = pied du tronc
  int h = small ? 15 : 21, w = small ? 5 : 7;
  uint16_t d1 = rgb(24, 96, 52), d2 = rgb(38, 130, 66), tr = rgb(110, 70, 40);
  canvas.fillEllipse(x + 2, y + 1, w + 2, 2, SNOW2);
  canvas.fillRect(x - 1, y - 3, 3, 4, tr);
  canvas.fillTriangle(x - w, y - 3, x + w, y - 3, x, y - h * 6 / 10, d1);
  canvas.fillTriangle(x - w + 1, y - h * 3 / 10 - 2, x + w - 1, y - h * 3 / 10 - 2, x, y - h * 8 / 10, d2);
  canvas.fillTriangle(x - w + 2, y - h * 6 / 10 - 1, x + w - 2, y - h * 6 / 10 - 1, x, y - h, d1);
  canvas.fillTriangle(x - 2, y - h * 9 / 10, x + 2, y - h * 9 / 10, x, y - h, TFT_WHITE);                    // neige au sommet
  canvas.fillRect(x - w + 1, y - 4, 3, 1, TFT_WHITE);
}
static void drawRock(int x, int y) {
  canvas.fillEllipse(x + 1, y + 1, 11, 5, SNOW2);
  canvas.fillEllipse(x, y - 2, 9, 5, rgb(110, 115, 130));
  canvas.fillEllipse(x - 2, y - 4, 5, 2, rgb(160, 165, 180));
  canvas.fillEllipse(x, y - 6, 6, 1, TFT_WHITE);
}
static void drawGate(int x, int y) {
  int xl = x - (int)GATE_HALF, xr = x + (int)GATE_HALF;
  canvas.drawFastVLine(xl, y - 14, 15, rgb(90, 90, 100)); canvas.drawFastVLine(xr, y - 14, 15, rgb(90, 90, 100));
  canvas.fillTriangle(xl, y - 14, xl, y - 7, xl + 9, y - 10, rgb(225, 50, 50));
  canvas.fillTriangle(xr, y - 14, xr, y - 7, xr - 9, y - 10, rgb(50, 90, 225));
  for (int i = xl + 4; i < xr - 3; i += 6) canvas.drawPixel(i, y, TRACK);
}
static void drawSkier(int x, int y, float lean) {
  uint16_t ski = rgb(30, 50, 140), jacket = rgb(235, 60, 55), helmet = rgb(40, 70, 190);
  int l5 = (int)(lean * 5), l3 = (int)(lean * 3);
  for (int s = -1; s <= 1; s += 2) {                                                     // skis
    canvas.drawLine(x + s * 3 - l3, y - 3, x + s * 3 + l5, y + 12, ski); canvas.drawLine(x + s * 3 - l3 + 1, y - 3, x + s * 3 + l5 + 1, y + 12, ski);
    canvas.drawLine(x + s * 6 - l3 / 2, y - 1, x + s * 8 + l5 / 2 + s, y + 9, rgb(90, 90, 100));                    // batons
  }
  canvas.fillEllipse(x + l3 / 2, y + 1, 5, 6, jacket);                                  // corps
  canvas.fillRect(x - 3 + l3 / 2, y + 4, 7, 2, rgb(50, 50, 70));                         // pantalon
  canvas.fillCircle(x + l3 / 2, y - 6, 4, helmet);                                       // casque
  canvas.fillRect(x - 3 + l3 / 2, y - 5, 7, 2, rgb(255, 210, 90));                       // masque
}

static void render() {
  canvas.fillScreen(SNOW);
  for (auto &s : specs) { canvas.drawPixel((int)s.x, (int)s.y, SNOW2); canvas.drawPixel((int)s.x + 1, (int)s.y, SNOW2); }
  // traces
  for (int i = 1; i < nTrk; i++) {
    canvas.drawLine((int)trk[i - 1].xl, (int)trk[i - 1].y, (int)trk[i].xl, (int)trk[i].y, TRACK);
    canvas.drawLine((int)trk[i - 1].xr, (int)trk[i - 1].y, (int)trk[i].xr, (int)trk[i].y, TRACK);
  }
  // objets tries par y (les plus hauts d'abord = derriere)
  int order[20], n = 0;
  for (int i = 0; i < 20; i++) if (objs[i].on) order[n++] = i;
  for (int i = 1; i < n; i++) { int k = order[i], j = i - 1; while (j >= 0 && objs[order[j]].y > objs[k].y) { order[j + 1] = order[j]; j--; } order[j + 1] = k; }
  bool skierDone = false;
  auto drawS = [&]() { if (state != TITLE && (invul == 0 || (animT & 4))) drawSkier((int)sx, SKY, sv / 4.0f); skierDone = true; };
  for (int k = 0; k < n; k++) {
    const Obj &o = objs[order[k]];
    if (!skierDone && o.y > SKY + 8) drawS();
    if (o.type == 3) drawGate((int)o.x, (int)o.y);
    else if (o.type == 2) drawRock((int)o.x, (int)o.y);
    else drawTree((int)o.x, (int)o.y, o.type == 1);
  }
  if (!skierDone) drawS();
  for (auto &d : deco) drawTree((int)d.x, (int)d.y, d.s);
  // infos
  for (auto &p : pops) if (p.t > 0) ui::popText((int)p.x, (int)p.y, p.s, p.col);
  char b[32];
  if (state != TITLE) {
    snprintf(b, sizeof b, "%lu", (unsigned long)score); txtO(b, 5, 2, &fonts::Font4, rgb(30, 50, 120), TFT_WHITE);
    if (combo >= 2) { snprintf(b, sizeof b, "x%d", combo); txtO(b, 5, 26, &fonts::Font2, rgb(220, 60, 50), TFT_WHITE); }
    for (int i = 0; i < lives; i++) { int hx = SCR_W - 12 - i * 13; canvas.fillCircle(hx, 9, 4, rgb(40, 70, 190)); canvas.fillRect(hx - 3, 9, 7, 2, rgb(255, 210, 90)); }
  }
  if (state == TITLE) {
    canvas.fillRoundRect(40, 26, 160, 54, 8, rgb(24, 44, 110)); canvas.drawRoundRect(40, 26, 160, 54, 8, rgb(170, 210, 250)); canvas.drawRoundRect(42, 28, 156, 50, 6, rgb(60, 90, 170));
    txtO("SLALOM", SCR_W / 2, 31, &fonts::Font4, rgb(255, 255, 255), rgb(30, 50, 140), textdatum_t::top_center, 1.2f);
    if (best > 0) { snprintf(b, sizeof b, L("best %lu","record %lu"), (unsigned long)best); txt(b, SCR_W / 2, 62, &fonts::Font2, rgb(200, 225, 250), textdatum_t::top_center); }
    else if ((millis() / 500) & 1) txt(L("PRESS A", "APPUIE SUR A"), SCR_W / 2, 64, &fonts::Font0, rgb(255, 215, 90), textdatum_t::top_center);
  }
  if (state == PAUSED) ui::pause();
  if (state == OVER) ui::gameOver(L("FINISHED!", "TERMINE !"), score, best, (score) > 0 && (uint32_t)(score) >= (uint32_t)(best));
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
    default: break;
  }
  while (acc >= STEP_US) { step(); acc -= STEP_US; }
  render();
  bgm::run(12, state == PLAY);
  present(0, 0);
}

void icon(int x, int y) {
  canvas.fillRoundRect(x, y, 64, 64, 6, SNOW);
  canvas.drawLine(x + 36, y + 30, x + 28, y + 62, TRACK); canvas.drawLine(x + 42, y + 30, x + 34, y + 62, TRACK);
  // clip : les arbres ne doivent pas deborder de l'icone
  drawTree(x + 10, y + 30, false); drawTree(x + 52, y + 24, true); drawTree(x + 56, y + 58, false);
  canvas.drawFastVLine(x + 14, y + 50, 12, rgb(90, 90, 100)); canvas.fillTriangle(x + 14, y + 50, x + 14, y + 56, x + 22, y + 53, rgb(225, 50, 50));
  canvas.drawFastVLine(x + 40, y + 52, 10, rgb(90, 90, 100)); canvas.fillTriangle(x + 40, y + 52, x + 40, y + 57, x + 33, y + 54, rgb(50, 90, 225));
  drawSkier(x + 30, y + 28, 0.25f);
}

}  // namespace ski
