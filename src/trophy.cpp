// ============================================================================
//  TROPHEES - medailles bronze / argent / or pour chaque jeu
//  - trophy::record() est appele en fin de partie : si une nouvelle medaille est gagnee, un bandeau s'affiche
//  - ecran "TROPHEES" dans le menu : meilleur score + medailles de chaque jeu
//  Seuils : modifier le tableau DEFS (bronze, argent, or) pour regler la difficulte.
// ============================================================================
#include "common.h"

namespace trophy {

struct Def { const char* en; const char* fr; const char* ns; const char* key; int b, s, g; };
static const Def DEFS[T_COUNT] = {
  {"Chrome Dino",  "Chrome Dino",   "chromedino", "hi",   300,  700, 1500},
  {"Flap Bird",    "Flap Bird",     "flap",       "best",  10,   20,   30},
  {"Meteors",      "Meteores",      "space",      "best", 100,  300,  600},   // score / 10 (secondes de survie x 5)
  {"Maze",         "Labyrinthe",    "maze",       "lvl",    4,    8,   12},   // niveau atteint
  {"Breakout",     "Casse-briques", "breakout",   "best", 300,  800, 1800},
  {"Invaders",     "Invaders",      "invaders",   "best", 500, 1500, 3500},
  {"Slalom",       "Slalom",        "ski",        "best", 300,  800, 1600},
  {"Stack",        "Stack",         "stack",      "best",  10,   25,   50},   // etages
  {"Racing",       "Course",        "race",       "best", 400, 1000, 2000},
  {"Flight",       "Avion",         "sky",        "best", 400, 1200, 2500},
  {"River",        "Riviere",       "river",      "best", 300,  800, 1600},
  {"Fish",         "Poisson",       "fish",       "best", 150,  500, 1200},
  {"Tetris",       "Tetris",        "tetris",     "best", 1000, 3000, 8000},
  {"Snake",        "Snake",         "snake",      "best", 150,  400,  800},
  {"Lunar Lander", "Alunissage",    "lander",     "best", 600, 1800, 4000},
  {"Emberwood",    "Emberwood",     "ember",      "best", 5000, 6500, 8000},  // score de fin (golem vaincu)
};

static int medalOf(int id, int v) { const Def& d = DEFS[id]; return v >= d.g ? 3 : (v >= d.s ? 2 : (v >= d.b ? 1 : 0)); }
static int stored(int id, int diff) { Preferences p; if (!p.begin(dnsFor(DEFS[id].ns, diff), true)) return 0; int v = p.getInt(DEFS[id].key, 0); p.end(); return v; }
int medalAt(int id, int diff) { return (id < 0 || id >= T_COUNT) ? 0 : medalOf(id, stored(id, diff)); }
int total() { int t = 0; for (int i = 0; i < T_COUNT; i++) t += medalAt(i, 1) + medalAt(i, 2); return t; }   // la difficulte "facile" est un mode entrainement : pas de medailles

// mode admin (test) : tout en or / tout effacer
void adminGold() {
  for (int id = 0; id < T_COUNT; id++) for (int d = 1; d <= 2; d++) { Preferences p; if (!p.begin(dnsFor(DEFS[id].ns, d), false)) continue; if (p.getInt(DEFS[id].key, 0) < DEFS[id].g) p.putInt(DEFS[id].key, DEFS[id].g); p.end(); }
}
void adminClear() {
  for (int id = 0; id < T_COUNT; id++) for (int d = 0; d <= 2; d++) { Preferences p; if (!p.begin(dnsFor(DEFS[id].ns, d), false)) continue; p.remove(DEFS[id].key); p.end(); }
}

void adminSet(int id, int lvl) {          // mode admin : fixe la medaille d'un jeu (0 = aucune) pour les deux difficultes
  if (id < 0 || id >= T_COUNT) return;
  const Def& d = DEFS[id]; int v = lvl <= 0 ? 0 : (lvl == 1 ? d.b : (lvl == 2 ? d.s : d.g));
  for (int df = 1; df <= 2; df++) { Preferences p; if (!p.begin(dnsFor(d.ns, df), false)) continue; if (v) p.putInt(d.key, v); else p.remove(d.key); p.end(); }
}
void adminSetAll(int lvl) { for (int i = 0; i < T_COUNT; i++) adminSet(i, lvl); }
const char* nameOf(int id) { return (id < 0 || id >= T_COUNT) ? "?" : (g_lang ? DEFS[id].fr : DEFS[id].en); }
int levelOf(int id) { int a = medalAt(id, 1), b = medalAt(id, 2); return a > b ? a : b; }

// ----------------------------------------------------------------------------- dessin d'une medaille
static void drawMedal(int cx, int cy, int r, int lv) {                 // lv 0 = pas gagnee, 1 bronze, 2 argent, 3 or  (pixel art : contour noir, 3 tons)
  if (lv <= 0) { canvas.fillCircle(cx, cy, r, P_K); canvas.fillCircle(cx, cy, r - 1, P_M); canvas.fillCircle(cx, cy, r - 3 > 1 ? r - 3 : 1, P_D); return; }
  uint16_t base = lv == 3 ? P_G : (lv == 2 ? P_L : rgb(184, 116, 60)), light = lv == 3 ? P_B : (lv == 2 ? P_B : rgb(230, 170, 110)), dark = lv == 3 ? rgb(150, 100, 30) : (lv == 2 ? P_M : rgb(110, 62, 30));
  if (r >= 7) { canvas.fillTriangle(cx - 6, cy - r - 5, cx, cy - r + 2, cx - 1, cy - r - 5, P_K); canvas.fillTriangle(cx + 6, cy - r - 5, cx, cy - r + 2, cx + 1, cy - r - 5, P_K);
                canvas.fillTriangle(cx - 5, cy - r - 4, cx, cy - r + 1, cx - 1, cy - r - 4, P_R); canvas.fillTriangle(cx + 5, cy - r - 4, cx, cy - r + 1, cx + 1, cy - r - 4, P_S); }
  pxDisc(cx, cy, r, base, light, dark);
  canvas.fillRect(cx - 1, cy - 2, 3, 5, dark); canvas.fillRect(cx - 2, cy - 1, 5, 3, dark); canvas.fillRect(cx - 1, cy - 1, 2, 2, light);   // etoile / relief central
}

// ----------------------------------------------------------------------------- bandeau "nouvelle medaille"
static uint32_t until = 0, since = 0, lastT = 0; static int bMedal = 0, lastId = -1, lastM = 0; static bool bNew = false;

void record(int id, uint32_t value) {
  if (id < 0 || id >= T_COUNT) return;
  if (id != T_MAZE && id != T_LANDER && id != T_EMBER) session::onOver(id, (int)value);     // fin de partie (defi du jour / 2 joueurs)
  if (g_diff == 0) return;                                                // facile = entrainement, pas de medailles
  int m = medalOf(id, (int)value), pm = medalOf(id, stored(id, g_diff));
  if (m <= 0) return;
  bool isNew = m > pm; uint32_t now = millis();
  if (!isNew && id == lastId && m == lastM && now - lastT < 30000) return;      // pas de repetition pendant la meme partie
  lastId = id; lastM = m; lastT = now;
  bMedal = m; bNew = isNew; since = now; until = now + 3600;
  if (isNew) { sfx(784, 70); sfx(988, 70, false); sfx(1319, 120, false); if (m == 3) sfx(1760, 260, false); }
}

void banner() {
  uint32_t now = millis();
  if (now >= until) return;
  int age = (int)(now - since), y = age < 260 ? -24 + age * 28 / 260 : 4;
  if ((int)(until - now) < 260) y = 4 - (260 - (int)(until - now)) * 28 / 260;
  static const char* NEN[3] = {"BRONZE MEDAL", "SILVER MEDAL", "GOLD MEDAL"}; static const char* NFR[3] = {"MEDAILLE BRONZE", "MEDAILLE ARGENT", "MEDAILLE OR"};
  const char* name = (g_lang ? NFR : NEN)[bMedal - 1];
  int w = 208, x = (SCR_W - w) / 2;
  uint16_t bc = bMedal == 3 ? P_G : (bMedal == 2 ? P_L : rgb(184, 116, 60));
  canvas.fillRect(x + 2, y + 2, w, 24, P_K); canvas.fillRect(x, y, w, 24, P_K); canvas.fillRect(x + 1, y + 1, w - 2, 22, bc); canvas.fillRect(x + 3, y + 3, w - 6, 18, P_B); canvas.fillRect(x + 3, y + 3, w - 6, 1, P_L);
  drawMedal(x + 17, y + 13, 8, bMedal);
  txt(name, x + 32, y + 4, &fonts::Font2, P_K, textdatum_t::top_left);
  if (bNew && (now / 250) % 2 == 0) { canvas.fillRect(x + w - 34, y + 4, 28, 10, P_R); txt("NEW!", x + w - 20, y + 5, &fonts::Font0, P_B, textdatum_t::top_center); }
  if (g_diff == 2) txt(L("HARD", "DIFFICILE"), x + w - 6, y + 14, &fonts::Font0, P_R, textdatum_t::top_right);
}

// ----------------------------------------------------------------------------- ecran "TROPHEES"
static int page = 0;
static const int PER = 5, PPD = (T_COUNT + PER - 1) / PER, PAGES = PPD * 2;     // 3 pages "normal" puis 3 pages "difficile"

void begin() { page = g_diff == 2 ? PPD : 0; }

static void render() {
  ui::bg((int)(millis() / 60));
  int dd = page / PPD + 1, pg = page % PPD;                                          // difficulte affichee : 1 normal, 2 difficile
  int total = 0; for (int i = 0; i < T_COUNT; i++) total += medalAt(i, dd);
  uint16_t accent = dd == 2 ? P_R : P_G;
  char b[24];
  ui::header(dd == 2 ? L("TROPHIES - HARD", "TROPHEES - DIFFICILE") : L("TROPHIES", "TROPHEES"), accent);
  drawMedal(9, 8, 6, 3); snprintf(b, sizeof b, "%d/%d", total, T_COUNT * 3); txt(b, 18, 5, &fonts::Font0, P_B, textdatum_t::top_left);
  snprintf(b, sizeof b, "%d/%d", page + 1, PAGES); txt(b, SCR_W - 4, 5, &fonts::Font0, P_B, textdatum_t::top_right);
  static const uint16_t MC[4] = {P_M, rgb(184, 116, 60), P_L, P_G};
  for (int k = 0; k < PER; k++) {
    int id = pg * PER + k; if (id >= T_COUNT) break;
    int y = 19 + k * 20, v = stored(id, dd), m = medalOf(id, v);
    canvas.fillRect(3, y, SCR_W - 6, 19, P_K); canvas.fillRect(4, y + 1, SCR_W - 8, 17, P_B); canvas.fillRect(4, y + 1, SCR_W - 8, 1, P_L);
    canvas.fillRect(4, y + 1, 5, 17, MC[m]);                                              // liseret = meilleure medaille
    txt((g_lang ? DEFS[id].fr : DEFS[id].en), 13, y + 2, &fonts::Font2, P_K, textdatum_t::top_left);
    // score + barre de progression vers la prochaine medaille
    const Def& d = DEFS[id]; int th[3] = {d.b, d.s, d.g};
    int lo = m == 0 ? 0 : th[m - 1], hi = m >= 3 ? th[2] : th[m];
    int fw = m >= 3 ? 50 : (v <= lo ? 0 : (int)(50L * (v - lo) / (hi - lo)));
    if (fw > 50) fw = 50;
    snprintf(b, sizeof b, "%d", v); txt(v > 0 ? b : "-", 118, y + 3, &fonts::Font0, v > 0 ? P_K : P_M, textdatum_t::top_left);
    canvas.fillRect(117, y + 11, 54, 6, P_K); canvas.fillRect(118, y + 12, 52, 4, P_L);
    if (fw > 0) canvas.fillRect(119, y + 12, fw < 3 ? 3 : fw, 4, m >= 3 ? P_G : MC[m + 1] == P_M ? P_S : MC[m + 1]);
    for (int lv = 1; lv <= 3; lv++) drawMedal(SCR_W - 48 + (lv - 1) * 18, y + 10, 6, m >= lv ? lv : 0);
  }
  if (g_diff == 0) ui::footer(L("Easy mode = practice, no medals", "Mode facile = entrainement, sans medailles"));
  else {
    ui::footer("");
    for (int i = 0; i < PAGES; i++) {                                                   // pastilles de page : 3 normal | 3 difficile
      int x = SCR_W / 2 - (PAGES * 9 + 6) / 2 + i * 9 + (i >= PPD ? 6 : 0);
      if (i == page) { canvas.fillRect(x, SCR_H - 9, 7, 5, P_B); canvas.fillRect(x + 1, SCR_H - 8, 5, 3, accent); } else canvas.fillRect(x + 1, SCR_H - 8, 5, 3, P_M);
    }
  }
}

void loop() {
  if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) { page = (page + 1) % PAGES; sfx(1200, 15); }
  else if (M5.BtnPWR.wasPressed()) { page = (page + PAGES - 1) % PAGES; sfx(1000, 15); }
  render();
  present(0, 0);
  delay(10);
}

static const char* CUP[] = {".KKKKKKKKKKK.", "KGBBGGGGGGGDK", "KGBGGGGGGGGDK", "KGBGGGGGGGGDK", "KGGGGGGGGGGDK", ".KGGGGGGGGDK.", "..KGGGGGGDK..", "...KGGGGDK...", "....KGGDK....", ".....KGK.....", ".....KGK.....", "...KKKGKKK...", "..KGGGGGGGK..", "..KKKKKKKKK.."};
void icon(int x, int y) {
  ui::tile(x, y, P_D);
  for (int s = -1; s <= 1; s += 2) { int hx = s < 0 ? x + 4 : x + 49; canvas.fillRect(hx, y + 12, 11, 14, P_K); canvas.fillRect(hx + 2, y + 14, 7, 10, P_D); canvas.fillRect(hx + (s < 0 ? 0 : 7), y + 14, 4, 10, P_D); canvas.fillRect(hx + 1, y + 13, 9, 2, P_G); canvas.fillRect(hx + 1, y + 24, 9, 2, P_G); canvas.fillRect(hx + (s < 0 ? 1 : 8), y + 15, 2, 9, P_G); }
  ui::art(x + 13, y + 7, CUP, 14, 3);
  canvas.fillRect(x + 28, y + 17, 7, 7, P_K); canvas.fillRect(x + 29, y + 18, 5, 5, P_S); canvas.fillRect(x + 29, y + 18, 2, 2, P_B);
  canvas.fillRect(x + 20, y + 50, 24, 8, P_K); canvas.fillRect(x + 21, y + 51, 22, 6, P_R); canvas.fillRect(x + 21, y + 51, 22, 1, P_G);
}

}  // namespace trophy
