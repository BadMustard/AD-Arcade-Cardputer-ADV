// ============================================================================
//  STATS - temps de jeu total, parties lancees, jeu prefere, medailles, serie du defi du jour
//  Le temps est compte par main.cpp (stats::tick) tant qu'un jeu est a l'ecran.
//  A/B = page suivante ; HAUT (PWR) = page precedente ; PWR long = menu
// ============================================================================
#include "common.h"
#include <Preferences.h>

namespace stats {

static const int N = trophy::T_COUNT;
static uint32_t secs[N], plays[N], msAcc[N];
static bool loaded = false, dirty = false;
static uint32_t lastTick = 0, lastFlush = 0;

static void load() {
  if (loaded) return;
  loaded = true;
  Preferences p; if (!p.begin("stats", true)) return;
  char k[8];
  for (int i = 0; i < N; i++) { snprintf(k, sizeof k, "p%d", i); plays[i] = p.getUInt(k, 0); snprintf(k, sizeof k, "t%d", i); secs[i] = p.getUInt(k, 0); }
  p.end();
}
void flush() {
  if (!loaded || !dirty) return;
  Preferences p; if (!p.begin("stats", false)) return;
  char k[8];
  for (int i = 0; i < N; i++) { snprintf(k, sizeof k, "p%d", i); p.putUInt(k, plays[i]); snprintf(k, sizeof k, "t%d", i); p.putUInt(k, secs[i]); }
  p.end(); dirty = false; lastFlush = millis();
}
void play(int idx) { load(); if (idx < 0 || idx >= N) return; plays[idx]++; dirty = true; }
void tick(int idx) {
  uint32_t now = millis(), dt = now - lastTick; lastTick = now;
  if (idx < 0 || idx >= N || dt > 300) return;       // un grand ecart = on vient d'entrer dans le jeu
  load();
  msAcc[idx] += dt;
  while (msAcc[idx] >= 1000) { msAcc[idx] -= 1000; secs[idx]++; dirty = true; }
  if (dirty && now - lastFlush > 60000) flush();
}
int total() { load(); uint32_t t = 0; for (int i = 0; i < N; i++) t += secs[i]; return (int)t; }

static void fmtTime(char* b, size_t n, uint32_t s) {
  if (s >= 3600) snprintf(b, n, "%luh%02lum", (unsigned long)(s / 3600), (unsigned long)((s / 60) % 60));
  else if (s >= 60) snprintf(b, n, "%lum%02lus", (unsigned long)(s / 60), (unsigned long)(s % 60));
  else snprintf(b, n, "%lus", (unsigned long)s);
}

// ----------------------------------------------------------------------------- ecran
static int page = 0;
static const int PER = 5, PGS = (N + PER - 1) / PER + 1;      // page 0 = resume, puis les jeux

void begin() { load(); flush(); page = 0; }

static void box(int x, int y, int w, int h) { canvas.fillRect(x, y, w, h, P_K); canvas.fillRect(x + 1, y + 1, w - 2, h - 2, P_B); canvas.fillRect(x + 1, y + 1, w - 2, 1, P_L); canvas.fillRect(x + 1, y + h - 3, w - 2, 2, P_L); }

static void renderSummary() {
  char b[40]; uint32_t tot = (uint32_t)total(), np = 0; int fav = -1; uint32_t fs = 0;
  for (int i = 0; i < N; i++) { np += plays[i]; if (secs[i] > fs) { fs = secs[i]; fav = i; } }
  box(4, 19, SCR_W - 8, 33);
  txt(L("TOTAL PLAY TIME", "TEMPS DE JEU TOTAL"), SCR_W / 2, 21, &fonts::Font0, P_M, textdatum_t::top_center);
  fmtTime(b, sizeof b, tot); txt(b, SCR_W / 2 + 1, 31, &fonts::Font4, P_G, textdatum_t::top_center, 1.5f); txt(b, SCR_W / 2, 30, &fonts::Font4, P_K, textdatum_t::top_center, 1.5f);
  int y = 54;
  snprintf(b, sizeof b, "%lu", (unsigned long)np);
  box(4, y, 114, 31);
  txt(L("GAMES PLAYED", "PARTIES LANCEES"), 61, y + 4, &fonts::Font0, P_M, textdatum_t::top_center); txt(b, 61, y + 13, &fonts::Font2, P_K, textdatum_t::top_center);
  box(122, y, 114, 31);
  txt(L("FAVOURITE", "JEU PREFERE"), 179, y + 4, &fonts::Font0, P_M, textdatum_t::top_center);
  txt(fav >= 0 && fs > 0 ? gameName(fav) : "-", 179, y + 13, &fonts::Font2, P_R, textdatum_t::top_center);
  y = 87;
  int streak = 0, bestStreak = 0; { Preferences p; if (p.begin("daily", true)) { streak = p.getInt("streak", 0); bestStreak = p.getInt("bestst", 0); p.end(); } }
  box(4, y, 114, 31);
  txt(L("MEDALS", "MEDAILLES"), 61, y + 3, &fonts::Font0, P_M, textdatum_t::top_center);
  snprintf(b, sizeof b, "%d / %d", trophy::total(), trophy::T_COUNT * 6); txt(b, 61, y + 13, &fonts::Font2, P_K, textdatum_t::top_center);
  box(122, y, 114, 31);
  txt(L("DAILY STREAK", "SERIE DU DEFI"), 179, y + 3, &fonts::Font0, P_M, textdatum_t::top_center);
  snprintf(b, sizeof b, L("%d (best %d)", "%d (record %d)"), streak, bestStreak); txt(b, 179, y + 13, &fonts::Font2, P_R, textdatum_t::top_center);
}

static void renderList(int pg) {
  uint32_t mx = 1; for (int i = 0; i < N; i++) if (secs[i] > mx) mx = secs[i];
  char b[24];
  for (int k = 0; k < PER; k++) {
    int id = pg * PER + k; if (id >= N) break;
    int y = 19 + k * 20;
    box(3, y, SCR_W - 6, 19);
    txt(gameName(id), 9, y + 2, &fonts::Font2, P_K, textdatum_t::top_left);
    int bw = (int)(54 * secs[id] / mx);
    canvas.fillRect(110, y + 4, 56, 7, P_K); canvas.fillRect(111, y + 5, 54, 5, P_L); if (bw > 0) canvas.fillRect(111, y + 5, bw, 5, P_G);
    fmtTime(b, sizeof b, secs[id]); txt(b, SCR_W - 34, y + 3, &fonts::Font0, P_K, textdatum_t::top_right);
    snprintf(b, sizeof b, "x%lu", (unsigned long)plays[id]); txt(b, SCR_W - 8, y + 3, &fonts::Font0, P_M, textdatum_t::top_right);
    canvas.fillRect(110, y + 12, 56, 1, P_L);
  }
}

static void render() {
  ui::bg((int)(millis() / 60));
  ui::header("STATS", P_S);
  char b[24]; snprintf(b, sizeof b, "%d/%d", page + 1, PGS); txt(b, SCR_W - 4, 5, &fonts::Font0, P_K, textdatum_t::top_right);
  if (page == 0) renderSummary(); else renderList(page - 1);
  ui::footer(L("A/B: next page  hold PWR: menu", "A/B : page suiv.  PWR long : menu"));
}

void loop() {
  if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) { page = (page + 1) % PGS; sfx(1200, 15); }
  else if (M5.BtnPWR.wasPressed()) { page = (page + PGS - 1) % PGS; sfx(1000, 15); }
  render();
  present(0, 0);
  delay(10);
}

void icon(int x, int y) {
  ui::tile(x, y, P_M);
  pxBox(x + 5, y + 7, 54, 50, P_B, P_B, P_L);
  for (int i = 0; i < 4; i++) canvas.fillRect(x + 8, y + 16 + i * 10, 48, 1, P_L);
  static const int H[4] = {14, 24, 18, 34}; static const uint16_t C[4] = {P_S, P_G, P_M, P_R};
  for (int i = 0; i < 4; i++) pxBox(x + 10 + i * 12, y + 53 - H[i], 9, H[i], C[i], P_B, P_D);
  canvas.fillRect(x + 8, y + 53, 48, 2, P_K);
  canvas.drawLine(x + 13, y + 30, x + 25, y + 22, P_K); canvas.drawLine(x + 25, y + 22, x + 37, y + 28, P_K); canvas.drawLine(x + 37, y + 28, x + 49, y + 13, P_K);
  canvas.fillTriangle(x + 52, y + 10, x + 45, y + 11, x + 50, y + 17, P_K);
}

}  // namespace stats
