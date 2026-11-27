// ============================================================================
//  MODES SPECIAUX : DEFI DU JOUR, 2 JOUEURS et reglage de la date
//  - Defi du jour : un jeu et une graine par jour (calcules a partir de la date de l'horloge interne),
//    3 essais, meilleur score du jour, serie de jours consecutifs, historique sur 7 jours.
//  - 2 joueurs : on se passe le M5, meme graine pour les deux (memes obstacles au depart), manches 1 ou 3.
//  Les jeux ne savent rien de tout ca : ils appellent trophy::record() a la fin de la partie, qui appelle session::onOver().
// ============================================================================
#include "common.h"
#include <Preferences.h>

// ----------------------------------------------------------------------------- utilitaires
static const uint8_t ELIG[] = {0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};   // jeux avec un "game over" et un score (tous sauf le labyrinthe)
static const int NELIG = sizeof(ELIG);

static uint32_t hash32(uint32_t x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
static long daysFromCivil(int y, int m, int d) {                                     // jours depuis 1970-01-01
  y -= m <= 2; long era = (y >= 0 ? y : y - 399) / 400; unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}
static int daysInMonth(int y, int m) { static const int D[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; return m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 29 : D[(m - 1) % 12]; }
static int weekdayOf(long day) { return (int)(((day + 4) % 7 + 7) % 7); }          // 0 = dimanche
static const char* WD_EN[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}, *WD_FR[7] = {"dim", "lun", "mar", "mer", "jeu", "ven", "sam"};
static const char* MO_EN[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"}, *MO_FR[12] = {"jan", "fev", "mar", "avr", "mai", "juin", "juil", "aout", "sep", "oct", "nov", "dec"};

static bool clockOk() { return M5.Rtc.isEnabled(); }
static bool readDate(int& y, int& m, int& d) {
  if (!clockOk()) return false;
  auto dt = M5.Rtc.getDate(); y = dt.year; m = dt.month; d = dt.date;
  return y >= 2025 && y <= 2099 && m >= 1 && m <= 12 && d >= 1 && d <= 31;
}
static void buildDefault(int& y, int& m, int& d) {                                    // date de compilation du firmware : bon point de depart
  static const char* MN[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char mon[4] = {__DATE__[0], __DATE__[1], __DATE__[2], 0}; m = 1; for (int i = 0; i < 12; i++) if (!strcmp(mon, MN[i])) m = i + 1;
  d = atoi(__DATE__ + 4); y = atoi(__DATE__ + 7);
}

static void panel(int x, int y, int w, int h) { canvas.fillRect(x, y, w, h, P_K); canvas.fillRect(x + 1, y + 1, w - 2, h - 2, P_B); canvas.fillRect(x + 1, y + 1, w - 2, 1, P_L); canvas.fillRect(x + 1, y + h - 3, w - 2, 2, P_L); }
static void header(const char* t, uint16_t col) { ui::bg((int)(millis() / 60)); ui::header(t, P_G); }
static void footer(const char* t) { ui::footer(t); }

// ----------------------------------------------------------------------------- bandeau (dessine par present())
namespace session {
static char ov1[48] = "", ov2[48] = ""; static uint32_t ovUntil = 0, ovSince = 0; static uint16_t ovCol = 0;
static void say(const char* a, const char* b, uint16_t col, uint32_t ms) { snprintf(ov1, sizeof ov1, "%s", a); snprintf(ov2, sizeof ov2, "%s", b ? b : ""); ovCol = col; ovSince = millis(); ovUntil = ovSince + ms; }
void overlay() {
  uint32_t now = millis(); if (now >= ovUntil) return;
  int age = (int)(now - ovSince), left = (int)(ovUntil - now), h = ov2[0] ? 26 : 16, y = SCR_H - h - 3;
  if (age < 200) y += (200 - age) * (h + 4) / 200; else if (left < 200) y += (200 - left) * (h + 4) / 200;
  int w = 196, x = (SCR_W - w) / 2;
  canvas.fillRect(x + 2, y + 2, w, h, P_K); canvas.fillRect(x, y, w, h, P_K); canvas.fillRect(x + 1, y + 1, w - 2, h - 2, P_B); canvas.fillRect(x + 1, y + 1, 4, h - 2, P_G);
  txt(ov1, SCR_W / 2 + 2, y + 3, &fonts::Font0, P_K, textdatum_t::top_center);
  if (ov2[0]) txt(ov2, SCR_W / 2 + 2, y + 15, &fonts::Font0, P_R, textdatum_t::top_center);
}

// ----------------------------------------------------------------------------- partie "speciale" en cours
static int  activeIdx = -1;                 // jeu lance par un mode special (-1 = aucun)
static int  savedDiff = -1;
static void enterRun(int game, uint32_t seed, int diff) {
  if (savedDiff < 0) savedDiff = g_diff;
  if (diff >= 0) g_diff = diff;
  seedRand(seed); activeIdx = game;
}
static void exitRun() { if (savedDiff >= 0) { g_diff = savedDiff; savedDiff = -1; } g_rngSeeded = false; activeIdx = -1; }
int activeGame() { return activeIdx; }

// ----------------------------------------------------------------------------- reglage de la date et de l'heure
static bool dEd = false; static int dY = 2026, dM = 1, dD = 1, dH = 12, dMi = 0, dField = 0; static uint32_t dRep = 0;
bool dateEditing() { return dEd; }
void dateBegin() { dEd = true; dField = 0; if (!readDate(dY, dM, dD)) buildDefault(dY, dM, dD); auto t = M5.Rtc.getTime(); dH = t.hours; dMi = t.minutes; if (dH < 0 || dH > 23) dH = 12; if (dMi < 0 || dMi > 59) dMi = 0; }
static void dateChange(int s) {
  if (dField == 0) { dD += s; int n = daysInMonth(dY, dM); if (dD > n) dD = 1; if (dD < 1) dD = n; }
  else if (dField == 1) { dM += s; if (dM > 12) dM = 1; if (dM < 1) dM = 12; if (dD > daysInMonth(dY, dM)) dD = daysInMonth(dY, dM); }
  else if (dField == 2) { dY += s; if (dY > 2060) dY = 2025; if (dY < 2025) dY = 2060; if (dD > daysInMonth(dY, dM)) dD = daysInMonth(dY, dM); }
  else if (dField == 3) { dH = (dH + s + 24) % 24; }
  else { dMi = (dMi + s + 60) % 60; }
  sfx(1000 + dField * 120, 10);
}
void dateLoop() {
  uint32_t now = millis();
  if (M5.BtnA.wasPressed()) { dateChange(1); dRep = now + 450; }
  else if (M5.BtnA.isPressed() && dRep && now > dRep) { dateChange(1); dRep = now + 110; }
  if (M5.BtnPWR.wasPressed()) { dateChange(-1); dRep = now + 450; }
  else if (M5.BtnPWR.isPressed() && dRep && now > dRep) { dateChange(-1); dRep = now + 110; }
  if (!M5.BtnA.isPressed() && !M5.BtnPWR.isPressed()) dRep = 0;
  if (M5.BtnB.wasPressed()) {
    if (dField < 4) { dField++; sfx(1200, 15); }
    else {
      m5::rtc_date_t dt; dt.year = dY; dt.month = dM; dt.date = dD; dt.weekDay = weekdayOf(daysFromCivil(dY, dM, dD));
      M5.Rtc.setDate(dt);
      m5::rtc_time_t tm; tm.hours = dH; tm.minutes = dMi; tm.seconds = 0; M5.Rtc.setTime(tm);
      dEd = false; sfx(988, 60); sfx(1318, 90, false); return;
    }
  }
  ui::bg(now / 50);
  ui::header(L("DATE & TIME", "DATE ET HEURE"), P_R);
  static const char* LB_EN[5] = {"DAY", "MONTH", "YEAR", "HOUR", "MIN"}, *LB_FR[5] = {"JOUR", "MOIS", "ANNEE", "HEURE", "MIN"};
  int vals[5] = {dD, dM, dY, dH, dMi}, xs[5] = {8, 76, 144, 8, 76}, ys[5] = {26, 26, 26, 72, 72}, ws[5] = {60, 60, 88, 60, 60};
  for (int i = 0; i < 5; i++) { char b[8]; snprintf(b, sizeof b, i == 2 ? "%d" : "%02d", vals[i]); ui::field(xs[i], ys[i], ws[i], 34, (g_lang ? LB_FR : LB_EN)[i], b, i == dField); }
  txt(":", 72, 80, &fonts::Font4, P_K, textdatum_t::top_center, 1.0f);
  // carte d'apercu : jour de la semaine + date lisible
  long dn = daysFromCivil(dY, dM, dD); char b[40];
  canvas.fillRect(146, 74, 88, 34, P_K); canvas.fillRect(147, 75, 86, 32, P_B); canvas.fillRect(147, 75, 86, 9, P_R);
  txt((g_lang ? WD_FR : WD_EN)[weekdayOf(dn)], 190, 75, &fonts::Font0, P_B, textdatum_t::top_center);
  snprintf(b, sizeof b, "%d %s", dD, (g_lang ? MO_FR : MO_EN)[dM - 1]); txt(b, 190, 87, &fonts::Font2, P_K, textdatum_t::top_center);
  ui::footer(dField < 4 ? L("A:+  PWR:-  B:next", "A:+  PWR:-  B:suivant") : L("A:+  PWR:-  B:OK", "A:+  PWR:-  B:valider"));
  present(0, 0); delay(10);
}

// ----------------------------------------------------------------------------- assistant du premier demarrage
}  // namespace session
namespace wizard {
enum { NONE, LANG, CALIB, CLOCK, DONE };
static int st = NONE; static uint32_t t0 = 0;
bool active() { return st != NONE; }
void begin() { st = LANG; t0 = millis(); }
static void logo(int y) {
  const char* t = "AD ARCADE";
  txt(t, SCR_W / 2 + 3, y + 3, &fonts::Font4, P_R, textdatum_t::top_center, 1.5f);
  txtO(t, SCR_W / 2, y, &fonts::Font4, P_G, P_K, textdatum_t::top_center, 1.5f);
}
void loop() {
  uint32_t now = millis(); int t = (int)((now - t0) / 50);
  if (st == LANG) {
    static int sel = 0;
    if (M5.BtnB.wasPressed() || M5.BtnPWR.wasPressed()) { sel ^= 1; g_lang = sel; sfx(1000, 15); }
    if (M5.BtnA.wasPressed()) { g_lang = sel; saveSettings(); sfx(988, 60); sfx(1318, 90, false); tilt::startCalibration(); st = CALIB; return; }
    ui::bg(t);
    logo(8);
    ui::window(36, 44, 168, 70, P_G, "LANGUAGE / LANGUE");
    for (int i = 0; i < 2; i++) {
      int y = 66 + i * 21; bool on = i == sel;
      canvas.fillRect(48, y, 144, 17, P_K); canvas.fillRect(49, y + 1, 142, 15, on ? P_S : P_L); if (on) canvas.fillRect(49, y + 1, 142, 1, P_B);
      txt(i ? "FRANCAIS" : "ENGLISH", 120, y + 1, &fonts::Font2, P_K, textdatum_t::top_center);
      if (on) { canvas.fillTriangle(54, y + 4, 54, y + 12, 60, y + 8, P_R); canvas.fillTriangle(186, y + 4, 186, y + 12, 180, y + 8, P_R); }
    }
    ui::footer("B:change  A:OK");
    present(0, 0); delay(10); return;
  }
  if (st == CALIB) {
    if (tilt::calibrating()) { tilt::calibrationStep(); return; }
    session::dateBegin(); st = CLOCK; return;
  }
  if (st == CLOCK) {
    session::dateLoop();
    if (!session::dateEditing()) { st = DONE; t0 = millis(); }
    return;
  }
  if (st == DONE) {
    if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnPWR.wasPressed()) { st = NONE; sfx(1320, 60); return; }
    ui::bg(t);
    logo(8);
    ui::window(36, 44, 168, 66, P_S, L("ALL SET!", "C'EST PRET !"));
    ui::star(46, 66, 2); ui::star(176, 66, 2);
    txt(L("Have fun!", "Amuse-toi !"), SCR_W / 2, 68, &fonts::Font2, P_K, textdatum_t::top_center);
    if ((t / 8) & 1) ui::button(SCR_W / 2, 90, 100, L("A : START", "A : JOUER"), P_R); else ui::button(SCR_W / 2, 90, 100, L("A : START", "A : JOUER"), P_G);
    ui::footer("AD ARCADE");
    present(0, 0); delay(10);
  }
}
}  // namespace wizard
namespace session {
// ============================================================================ DEFI DU JOUR
}  // namespace session

namespace daily {
using namespace session;
static const int MAXTRIES = 3;
enum { HOME, PLAY, NOCLOCK };
static int phase = HOME; static long today = 0; static int gameOfDay = 0, tries = 0, tbest = 0, streak = 0, bestSt = 0; static uint32_t seed = 0;
static int hist[7]; static long histDay[7];
static bool over = false, armed = true; static uint32_t finishAt = 0; static int anim = 0;
static int lastScore = 0;

static int pickGame(long d) {
  int g = hash32((uint32_t)d) % NELIG, pg = hash32((uint32_t)(d - 1)) % NELIG; if (g == pg) g = (g + 1) % NELIG; return ELIG[g];
}
static void loadDay() {
  int y, m, d; if (!readDate(y, m, d)) { today = 0; return; }
  today = daysFromCivil(y, m, d); gameOfDay = pickGame(today); seed = hash32((uint32_t)today * 7919u + 13u);
  Preferences p; if (!p.begin("daily", true)) { tries = tbest = 0; return; }
  if (p.getLong("td", 0) == today) { tries = p.getInt("tries", 0); tbest = p.getInt("tbest", 0); } else { tries = 0; tbest = 0; }
  streak = p.getInt("streak", 0); bestSt = p.getInt("bestst", 0);
  long lp = p.getLong("lastp", 0); if (lp != today && lp != today - 1) streak = 0;     // serie rompue
  char k[8]; for (int i = 0; i < 7; i++) { snprintf(k, sizeof k, "hd%d", i); histDay[i] = p.getLong(k, 0); snprintf(k, sizeof k, "hs%d", i); hist[i] = p.getInt(k, 0); }
  p.end();
}
static void saveResult(int score) {
  Preferences p; if (!p.begin("daily", false)) return;
  long lp = p.getLong("lastp", 0); int st = p.getInt("streak", 0);
  if (lp != today) { st = (lp == today - 1) ? st + 1 : 1; p.putLong("lastp", today); p.putInt("streak", st); if (st > p.getInt("bestst", 0)) p.putInt("bestst", st); }
  streak = st; bestSt = p.getInt("bestst", 0);
  tries++; if (score > tbest) tbest = score;
  p.putLong("td", today); p.putInt("tries", tries); p.putInt("tbest", tbest);
  int i = (int)(today % 7); char k[8]; snprintf(k, sizeof k, "hd%d", i); p.putLong(k, today); snprintf(k, sizeof k, "hs%d", i); p.putInt(k, tbest);
  histDay[i] = today; hist[i] = tbest;
  p.end();
}

void adminReset(int st) {                      // mode admin : remet les essais du jour a zero, fixe la serie
  Preferences p; if (!p.begin("daily", false)) return;
  p.putInt("tries", 0); p.putInt("tbest", 0); p.remove("td");
  if (st >= 0) { int y, m, d; if (readDate(y, m, d)) p.putLong("lastp", daysFromCivil(y, m, d) - 1); p.putInt("streak", st); if (st > p.getInt("bestst", 0)) p.putInt("bestst", st); }
  p.end();
}
void begin() {
  phase = HOME; anim = 0; exitRun();
  if (!clockOk()) { phase = NOCLOCK; return; }
  int y, m, d; if (!readDate(y, m, d)) { dateBegin(); return; }
  loadDay();
}
void onOver(int id, int score) {
  if (phase != PLAY || id != gameOfDay || over) return;
  over = true; lastScore = score; saveResult(score);
  char a[48], b[48];
  snprintf(a, sizeof a, L("SCORE %d   BEST OF THE DAY %d", "SCORE %d   MEILLEUR DU JOUR %d"), score, tbest);
  if (tries >= MAXTRIES) { finishAt = millis() + 3600; snprintf(b, sizeof b, "%s", L("Daily challenge done!", "Defi du jour termine !")); }
  else snprintf(b, sizeof b, L("%d try left - press A to retry", "%d essai restant - A pour rejouer"), MAXTRIES - tries);
  say(a, b, tries >= MAXTRIES ? rgb(255, 205, 60) : rgb(120, 210, 255), tries >= MAXTRIES ? 3400 : 4500);
  armed = true;
}
static void startRun() {
  enterRun(gameOfDay, seed, 1);                       // difficulte normale pour tout le monde
  gameBegin(gameOfDay); over = false; armed = true; phase = PLAY;
  char a[48]; snprintf(a, sizeof a, L("DAILY CHALLENGE   try %d/%d", "DEFI DU JOUR   essai %d/%d"), tries + 1, MAXTRIES);
  say(a, L("Same start for everyone today", "Meme depart pour tout le monde"), rgb(255, 160, 80), 3000);
}

static void renderHome() {
  int y, m, d; readDate(y, m, d);
  char b[48];
  ui::bg((int)(millis() / 60));
  ui::header(L("DAILY CHALLENGE", "DEFI DU JOUR"), P_R);
  snprintf(b, sizeof b, "%d %s", d, (g_lang ? MO_FR : MO_EN)[m - 1]);
  int pw = (int)strlen(b) * 6 + 8; canvas.fillRect(SCR_W - pw - 3, 3, pw, 12, P_K); canvas.fillRect(SCR_W - pw - 2, 4, pw - 2, 10, P_B); txt(b, SCR_W - 6, 6, &fonts::Font0, P_K, textdatum_t::top_right);
  // carte du jeu du jour
  panel(4, 21, 94, 92);
  canvas.fillRect(15, 24, 72, 66, P_K); gameIcon(gameOfDay, 19, 25);
  canvas.fillRect(15, 24, 72, 1, P_G); canvas.fillRect(15, 89, 72, 1, P_G);
  txt(gameName(gameOfDay), 51, 94, &fonts::Font2, P_K, textdatum_t::top_center);
  // carte des scores
  panel(102, 21, 134, 92);
  txt(L("TRIES", "ESSAIS"), 109, 25, &fonts::Font0, P_M, textdatum_t::top_left);
  for (int i = 0; i < MAXTRIES; i++) { int cx = 142 + i * 14; pxDisc(cx, 29, 5, i < tries ? P_L : P_G, i < tries ? P_B : P_B, i < tries ? P_M : P_R); }
  int fx = 206, fy = 28; bool fire = streak > 0;
  pxDisc(fx, fy + 4, 5, fire ? P_R : P_L, fire ? P_G : P_B, P_K); canvas.fillTriangle(fx - 5, fy + 3, fx + 5, fy + 3, fx, fy - 8, P_K); canvas.fillTriangle(fx - 3, fy + 3, fx + 3, fy + 3, fx, fy - 5, fire ? P_R : P_L);
  if (fire) canvas.fillRect(fx - 1, fy + 3, 3, 3, P_G);
  snprintf(b, sizeof b, "%d", streak); txt(b, fx + 8, fy - 3, &fonts::Font2, fire ? P_R : P_M, textdatum_t::top_left);
  txt(L("BEST TODAY", "MEILLEUR DU JOUR"), 109, 41, &fonts::Font0, P_M, textdatum_t::top_left);
  snprintf(b, sizeof b, tries ? "%d" : "-", tbest); txt(b, 110, 51, &fonts::Font4, P_G, textdatum_t::top_left); txt(b, 109, 50, &fonts::Font4, P_K, textdatum_t::top_left);
  if (tries && tbest > 0) { ui::crown(216, 50); }
  // 7 derniers jours
  int mx = 1; for (int i = 0; i < 7; i++) { long dd = today - 6 + i; int ix = (int)(((dd % 7) + 7) % 7); if (histDay[ix] == dd && hist[ix] > mx) mx = hist[ix]; }
  for (int i = 0; i < 7; i++) {
    long dd = today - 6 + i; int ix = (int)(((dd % 7) + 7) % 7); int v = histDay[ix] == dd ? hist[ix] : -1; int bx = 109 + i * 18;
    canvas.fillRect(bx - 1, 99, 16, 1, P_K);
    if (v >= 0) { int h = 3 + (int)(22L * v / mx); canvas.fillRect(bx - 1, 99 - h - 1, 16, h + 1, P_K); canvas.fillRect(bx, 99 - h, 14, h, i == 6 ? P_G : P_S); canvas.fillRect(bx, 99 - h, 14, 1, P_B); canvas.fillRect(bx + 11, 99 - h + 1, 3, h - 1, i == 6 ? P_R : P_M); }
    else canvas.fillRect(bx + 4, 97, 6, 2, P_L);
    txt((g_lang ? WD_FR : WD_EN)[weekdayOf(dd)], bx + 7, 102, &fonts::Font0, i == 6 ? P_R : P_M, textdatum_t::top_center);
  }
  // bouton "jouer" qui pulse
  bool done = tries >= MAXTRIES;
  int pulse = (int)(2.5f * sinf(anim * 0.12f) + 2.5f);
  if (done) { canvas.fillRect(3, 116, 192, 18, P_K); canvas.fillRect(4, 117, 190, 16, P_L); txt(L("Done - come back tomorrow!", "Termine - a demain !"), 99, 117, &fonts::Font2, P_K, textdatum_t::top_center); }
  else { canvas.fillRect(3, 116, 192, 18, P_K); canvas.fillRect(4, 117, 190, 16, pulse > 2 ? P_S : P_G); canvas.fillRect(4, 117, 190, 1, P_B); txt(L("A : PLAY", "A : JOUER"), 99, 117, &fonts::Font2, P_K, textdatum_t::top_center); }
  canvas.fillRect(198, 116, 39, 18, P_K); canvas.fillRect(199, 117, 37, 16, P_B);
  txt(L("B:date", "B:date"), 217, 121, &fonts::Font0, P_K, textdatum_t::top_center);
}

void loop() {
  if (dateEditing()) { dateLoop(); if (!dateEditing()) { begin(); } return; }
  anim++;
  if (phase == NOCLOCK) {
    header(L("DAILY CHALLENGE", "DEFI DU JOUR"), rgb(255, 160, 80));
    txt(L("No clock on this device", "Pas d'horloge sur cet appareil"), SCR_W / 2, 50, &fonts::Font2, TFT_WHITE, textdatum_t::top_center);
    txt(L("The daily challenge needs the", "Le defi du jour a besoin de"), SCR_W / 2, 74, &fonts::Font0, rgb(140, 150, 205), textdatum_t::top_center);
    txt(L("built-in clock to know the date.", "l'horloge interne pour la date."), SCR_W / 2, 86, &fonts::Font0, rgb(140, 150, 205), textdatum_t::top_center);
    footer(L("hold PWR: menu", "PWR long : menu")); present(0, 0); delay(10); return;
  }
  if (phase == HOME) {
    if (M5.BtnB.wasPressed()) { dateBegin(); sfx(1200, 15); return; }
    if (M5.BtnA.wasPressed() && tries < MAXTRIES) { startRun(); return; }
    renderHome(); present(0, 0); delay(10); return;
  }
  // PLAY
  if (armed && M5.BtnA.wasPressed()) { seedRand(seed); armed = false; if (!finishAt) over = false; }   // nouvelle partie : on repart de la graine du jour
  gameLoop(gameOfDay);
  if (over && finishAt && millis() >= finishAt) { finishAt = 0; exitRun(); bgm::stop(); phase = HOME; }
}
static const char* FLAME[] = {"...KK...", "..KGGK..", "..KGGK..", ".KGGGGK.", "KGGRRGGK", "KGRRRRGK", "KGRBBRGK", ".KGRRGK.", "..KKKK.."};
void icon(int x, int y) {
  ui::tile(x, y, P_D);
  for (int i = 0; i < 2; i++) { canvas.fillRect(x + 17 + i * 26, y + 5, 6, 12, P_K); canvas.fillRect(x + 18 + i * 26, y + 6, 4, 9, P_L); }
  pxBox(x + 8, y + 11, 48, 46, P_B, P_B, P_L);
  canvas.fillRect(x + 9, y + 12, 46, 12, P_K); canvas.fillRect(x + 10, y + 13, 44, 10, P_R); canvas.fillRect(x + 10, y + 13, 44, 1, P_G);
  for (int i = 0; i < 2; i++) { canvas.fillRect(x + 17 + i * 26, y + 5, 6, 12, P_K); canvas.fillRect(x + 18 + i * 26, y + 6, 4, 9, P_B); }
  canvas.fillRect(x + 12, y + 17, 5, 2, P_B); canvas.fillRect(x + 22, y + 17, 5, 2, P_B); canvas.fillRect(x + 32, y + 17, 5, 2, P_B);
  ui::art(x + 24, y + 29, FLAME, 9, 3);
}

}  // namespace daily

// ============================================================================ 2 JOUEURS
namespace duo {
using namespace session;
enum { PICK, READY, PLAY, PASS, RESULT };
static int phase = PICK, pick = 0, game = 0, rounds = 1, round = 0, pl = 0, sc[2], wins[2], cur = 0; static uint32_t seed = 0, overAt = 0; static bool over = false, armed = false, final_ = false;
static int anim = 0, mode = 1, soloBest = 0; static bool newBest = false;      // mode : 0 = 1 joueur, 1 = 2 joueurs (1 manche), 2 = 2 joueurs (au meilleur de 3)
static const uint16_t PC[2] = {P_S, P_R};

void begin() { phase = PICK; exitRun(); anim = 0; }
void onOver(int id, int score) {
  if (phase != PLAY || id != game || over) return;
  over = true; cur = score; overAt = millis() + 1700;
}
static void newSeed() { seed = hash32(millis() * 2654435761u + (uint32_t)esp_random()); }
static void startTurn() {
  enterRun(game, seed, -1); gameBegin(game); over = false; armed = true; phase = PLAY;
  char a[40]; snprintf(a, sizeof a, L("PLAYER %d", "JOUEUR %d"), pl + 1); if (mode == 0) say(L("SOLO", "SOLO"), L("Beat your best score!", "Bats ton meilleur score !"), PC[0], 2600); else say(a, L("Same start for both players", "Meme depart pour les deux joueurs"), PC[pl], 2600);
}
static void startMatch() { round = 0; pl = 0; wins[0] = wins[1] = 0; newSeed(); phase = READY; exitRun(); }

static void badge(int x, int y, int p, bool big) {
  canvas.fillRoundRect(x, y, big ? 74 : 56, big ? 20 : 15, 6, PC[p]);
  char b[16]; snprintf(b, sizeof b, "P%d", p + 1); txt(b, x + (big ? 37 : 28), y + (big ? 2 : 0), big ? &fonts::Font2 : &fonts::Font2, P_K, textdatum_t::top_center);
}
static void crown(int cx, int cy, uint16_t c) {
  canvas.fillRect(cx - 8, cy, 16, 5, c); canvas.fillTriangle(cx - 8, cy, cx - 8, cy - 7, cx - 3, cy, c); canvas.fillTriangle(cx - 3, cy, cx, cy - 9, cx + 3, cy, c); canvas.fillTriangle(cx + 3, cy, cx + 8, cy - 7, cx + 8, cy, c);
}

void loop() {
  anim++;
  char b[48];
  if (phase == PICK) {
    if (M5.BtnB.wasPressed()) { pick = (pick + 1) % NELIG; sfx(1200, 15); }
    if (M5.BtnPWR.wasPressed()) { pick = (pick + NELIG - 1) % NELIG; sfx(1000, 15); }
    if (M5.BtnA.wasPressed()) { game = ELIG[pick]; rounds = mode == 2 ? 3 : 1; soloBest = 0; startMatch(); sfx(880, 40); sfx(1320, 60, false); return; }
    header(L("2 PLAYERS", "2 JOUEURS"), P_G);
    int sl = (int)(sinf(anim * 0.08f) * 2);
    for (int k = -1; k <= 1; k++) {
      int idx = (pick + k + NELIG) % NELIG; int x = SCR_W / 2 - 32 + k * 84;
      if (k == 0) { canvas.fillRoundRect(x - 4, 20 + sl, 72, 72, 8, P_B); canvas.drawRoundRect(x - 4, 20 + sl, 72, 72, 8, P_G); gameIcon(ELIG[idx], x, 24 + sl); }
      else { canvas.fillRoundRect(x, 28, 64, 64, 6, P_B); gameIcon(ELIG[idx], x, 28); for (int yy = 28; yy < 92; yy++) for (int xx = x + ((yy & 1) ? 0 : 1); xx < x + 64; xx += 2) canvas.drawPixel(xx, yy, P_M); }
    }
    txt(gameName(ELIG[pick]), SCR_W / 2, 98, &fonts::Font2, P_K, textdatum_t::top_center);
    snprintf(b, sizeof b, "%d/%d", pick + 1, NELIG); txt(b, SCR_W - 5, 3, &fonts::Font0, P_M, textdatum_t::top_right);
    footer(L("B/PWR: choose   A: select", "B/PWR : choisir   A : valider")); present(0, 0); delay(10); return;
  }
  if (phase == READY) {
    if (round == 0 && pl == 0 && M5.BtnB.wasPressed()) { mode = (mode + 1) % 3; rounds = mode == 2 ? 3 : 1; sfx(1200, 15); }
    if (M5.BtnA.wasPressed()) { startTurn(); return; }
    header(L("2 PLAYERS", "2 JOUEURS"), P_G);
    txt(gameName(game), SCR_W / 2, 20, &fonts::Font2, P_K, textdatum_t::top_center);
    if (rounds > 1) { snprintf(b, sizeof b, L("ROUND %d / %d", "MANCHE %d / %d"), round + 1, rounds); txt(b, SCR_W / 2, 38, &fonts::Font0, P_M, textdatum_t::top_center); }
    badge(SCR_W / 2 - 37, 52, pl, true);
    txt(L("get ready!", "prepare-toi !"), SCR_W / 2, 78, &fonts::Font4, P_K, textdatum_t::top_center);
    if (rounds > 1 && (round > 0 || pl > 0)) { snprintf(b, sizeof b, "P1  %d - %d  P2", wins[0], wins[1]); txt(b, SCR_W / 2, 106, &fonts::Font0, P_R, textdatum_t::top_center); }
    else if (round == 0 && pl == 0) { snprintf(b, sizeof b, L("Mode: %s  (B: change)", "Mode : %s  (B : changer)"), mode == 0 ? L("1 player", "1 joueur") : (mode == 1 ? L("2 players", "2 joueurs") : L("2 players, best of 3", "2 joueurs, meilleur de 3"))); txt(b, SCR_W / 2, 106, &fonts::Font0, P_R, textdatum_t::top_center); }
    footer(L("A: start", "A : demarrer")); present(0, 0); delay(10); return;
  }
  if (phase == PLAY) {
    if (armed && M5.BtnA.wasPressed()) { seedRand(seed); armed = false; }
    gameLoop(game);
    if (over && millis() >= overAt) {
      sc[pl] = cur; bgm::stop(); exitRun();
      if (mode == 0) { newBest = cur > soloBest; if (newBest) soloBest = cur; final_ = true; phase = RESULT; sfx(988, 80); sfx(1318, 80, false); if (newBest) sfx(1760, 160, false); }
      else if (pl == 0) { phase = PASS; sfx(660, 80); sfx(880, 80, false); }
      else {
        final_ = false;
        if (sc[0] > sc[1]) wins[0]++; else if (sc[1] > sc[0]) wins[1]++;
        if (rounds == 1 || round >= rounds - 1 || wins[0] >= 2 || wins[1] >= 2) final_ = true;
        phase = RESULT; sfx(988, 80); sfx(1318, 80, false); sfx(1760, 160, false);
      }
    }
    return;
  }
  if (phase == PASS) {
    if (M5.BtnA.wasPressed()) { pl = 1; startTurn(); return; }
    header(L("2 PLAYERS", "2 JOUEURS"), P_G);
    badge(SCR_W / 2 - 37, 22, 0, true);
    snprintf(b, sizeof b, "%d", sc[0]); txt(b, SCR_W / 2, 46, &fonts::Font4, P_K, textdatum_t::top_center);
    int ax = SCR_W / 2 + (int)(sinf(anim * 0.15f) * 8);
    canvas.fillTriangle(ax + 16, 86, ax + 16, 102, ax + 30, 94, PC[1]); canvas.fillRect(ax - 24, 90, 40, 8, PC[1]);
    txt(L("Pass the M5 to", "Passe le M5 au"), SCR_W / 2 - 6, 77 - 12, &fonts::Font0, P_M, textdatum_t::top_center);
    badge(SCR_W / 2 - 28, 108 - 6, 1, false);
    footer(L("A: player 2 ready", "A : joueur 2 pret")); present(0, 0); delay(10); return;
  }
  // RESULT
  if (mode == 0) {
    if (M5.BtnA.wasPressed()) { newSeed(); pl = 0; startTurn(); return; }
    if (M5.BtnB.wasPressed()) { phase = PICK; sfx(1200, 20); return; }
    header(L("RESULT", "RESULTAT"), P_R);
    canvas.fillRoundRect(40, 22, 160, 64, 8, P_B); canvas.drawRoundRect(40, 22, 160, 64, 8, PC[0]);
    txt(gameName(game), SCR_W / 2, 26, &fonts::Font2, P_K, textdatum_t::top_center);
    snprintf(b, sizeof b, "%d", sc[0]); txt(b, SCR_W / 2, 46, &fonts::Font4, P_K, textdatum_t::top_center);
    if (newBest) { if ((anim / 12) % 2 == 0) txt(L("NEW BEST!", "NOUVEAU RECORD !"), SCR_W / 2, 90, &fonts::Font2, P_R, textdatum_t::top_center); }
    else { snprintf(b, sizeof b, L("Best this session: %d", "Meilleur de la session : %d"), soloBest); txt(b, SCR_W / 2, 92, &fonts::Font0, P_M, textdatum_t::top_center); }
    footer(L("A: retry   B: other game", "A : rejouer   B : autre jeu")); present(0, 0); delay(10); return;
  }
  if (M5.BtnA.wasPressed()) {
    if (!final_) { round++; pl = 0; newSeed(); phase = READY; sfx(1200, 20); }
    else { startMatch(); sfx(880, 30); }
    return;
  }
  if (M5.BtnB.wasPressed() && final_) { phase = PICK; sfx(1200, 20); return; }
  header(final_ ? L("RESULT", "RESULTAT") : L("ROUND RESULT", "RESULTAT DE LA MANCHE"), P_R);
  int win = sc[0] > sc[1] ? 0 : (sc[1] > sc[0] ? 1 : -1);
  for (int p = 0; p < 2; p++) {
    int x = 6 + p * 118; bool w = win == p;
    canvas.fillRoundRect(x, 18, 110, 68, 8, w ? P_B : P_B); if (w) canvas.drawRoundRect(x, 18, 110, 68, 8, PC[p]);
    badge(x + 18, 21, p, true);
    snprintf(b, sizeof b, "%d", sc[p]); txt(b, x + 55, 42, &fonts::Font4, w ? P_K : P_M, textdatum_t::top_center);
    if (w) crown(x + 55, 78 + (int)(sinf(anim * 0.2f) * 2), P_G);
  }
  if (win < 0) snprintf(b, sizeof b, "%s", L("DRAW!", "EGALITE !"));
  else if (final_) snprintf(b, sizeof b, L("PLAYER %d WINS!", "JOUEUR %d GAGNE !"), win + 1);
  else snprintf(b, sizeof b, L("PLAYER %d WINS THE ROUND", "JOUEUR %d GAGNE LA MANCHE"), win + 1);
  txt(b, SCR_W / 2, 90, &fonts::Font2, win < 0 ? P_K : PC[win], textdatum_t::top_center);
  if (rounds > 1) { snprintf(b, sizeof b, "P1  %d - %d  P2", wins[0], wins[1]); txt(b, SCR_W / 2, 109, &fonts::Font0, P_R, textdatum_t::top_center); }
  footer(final_ ? L("A: rematch   B: other game", "A : revanche   B : autre jeu") : L("A: next round", "A : manche suivante"));
  present(0, 0); delay(10);
}
static const char* HERO[] = {"..KKKKKK..", ".KSSSSSSK.", "KSSSSSSSSK", "KSKKSSKKSK", "KSKBSSKBSK", "KSSSSSSSSK", ".KSSRRSSK.", "..KKKKKK..", ".KSSSSSSK.", "KSSSSSSSSK", "KSSKKKKSSK", ".KKK..KKK."};
void icon(int x, int y) {
  ui::tile(x, y, P_M);
  for (int i = 0; i < 6; i++) { canvas.fillRect(x + 2 + i * 11, y + 2 + (i & 1) * 2, 6, 2, P_L); }
  ui::art(x + 4, y + 8, HERO, 12, 2, 'S', P_S);
  ui::art(x + 36, y + 8, HERO, 12, 2, 'S', P_G);
  canvas.fillRect(x + 4, y + 33, 24, 2, P_K); canvas.fillRect(x + 36, y + 33, 24, 2, P_K);
  pxBox(x + 16, y + 40, 32, 18, P_R, P_G, P_D);
  txt("2P", x + 32, y + 42, &fonts::Font2, P_B, textdatum_t::top_center);
  canvas.fillRect(x + 10, y + 46, 5, 3, P_B); canvas.fillRect(x + 49, y + 46, 5, 3, P_B);
}

}  // namespace duo

// ----------------------------------------------------------------------------- aiguillage des fins de partie
namespace session {
void onOver(int gameId, int score) { daily::onOver(gameId, score); duo::onOver(gameId, score); }
void end() { exitRun(); dEd = false; ovUntil = 0; }
}
