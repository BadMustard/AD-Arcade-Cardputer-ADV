// ============================================================================
//  AD ARCADE - M5StickC Plus2
//  Menu facon console + jeux. Menu : B (cote) = suivant, HAUT (PWR) = precedent, M5 (A) = lancer
//  Dans un jeu : maintenir HAUT (PWR) 1,5 s = retour au menu
// ============================================================================

//#ifdef AD_CARDPUTER
//#include <M5Cardputer.h>
//#endif

#if defined(ARDUINO_M5STACK_CARDPUTER) || defined(ARDUINO_M5STACK_CARDPUTER_ADV)
  #define a_cardputer 1
  #include <M5Cardputer.h>
#endif

#ifndef SCR_ROT
  #if a_cardputer
    #define SCR_ROT 0
  #else
    #define SCR_ROT 3
  #endif
#endif

#include "common.h"
#include "icons_art.h"

M5Canvas canvas(&M5.Display);

int  g_bright = 2;
bool g_sound = true;
bool g_music = true;
int  g_sleepIdx = 1;
int  g_theme = 0;                             // theme de couleurs du menu
int  g_lang = 0;                              // 0 = English, 1 = Francais
int  g_vol = 3;                               // volume 0..5
int  g_diff = 1;                              // 0 facile, 1 normal, 2 difficile
int g_kbdX = 0, g_kbdY = 0;
bool g_noIntro = false, g_crt = false, g_invert = false, g_rainbow = false, g_fps = false;
bool g_unlockAll = false;                     // mode admin
int  g_skin = 0;                              // apparence du heros
uint32_t g_rngState = 1; bool g_rngSeeded = false;
bool g_quit = false;
int  g_ctrl = 0; bool g_btnCtrl = false;     // commandes : 0 demander, 1 inclinaison, 2 boutons
bool g_flip = false;                          // ecran retourne
static int g_baseRot = SCR_ROT;
uint8_t* idxBuffer() { static uint8_t* b = nullptr; if (!b) b = (uint8_t*)malloc(240 * 135); return b; }
void applyRotation() { M5.Display.setRotation((g_baseRot + (g_flip ? 2 : 0)) & 3); }
static const int BRI[5] = {30, 70, 120, 180, 255};
static const uint32_t SLEEP_MS[5] = {30000, 60000, 120000, 300000, 0};
static Preferences mp;

static const int VOLS[6] = {0, 40, 80, 140, 200, 255};
void applyBrightness() { M5.Display.setBrightness(BRI[g_bright]); }
void applyVolume() { g_sound = g_vol > 0; M5.Speaker.setVolume(VOLS[g_vol]); }
void saveSettings() { mp.putInt("ctl", g_ctrl); mp.putBool("flip", g_flip); mp.putInt("bri", g_bright); mp.putInt("vol", g_vol); mp.putBool("snd", g_sound); mp.putBool("mus", g_music); mp.putInt("slp", g_sleepIdx); mp.putInt("lang", g_lang); mp.putInt("theme", g_theme); mp.putInt("diff", g_diff); mp.putInt("skin", g_skin); mp.putBool("unl", g_unlockAll); mp.putBool("ni", g_noIntro); mp.putBool("crt", g_crt); mp.putBool("inv", g_invert); mp.putBool("rbw", g_rainbow); }

// ----------------------------------------------------------------------------- skins (apparences du heros)
namespace skin {
struct Sk { const char* en; const char* fr; int need; uint8_t col[4][3]; };     // col : corps, ombre, reflet, accent
static const Sk SK[] = {
  {"Classic", "Classique", 0,  {{0,0,0},{0,0,0},{0,0,0},{0,0,0}}},
  {"Aqua",    "Aqua",      4,  {{60,200,230},{20,100,140},{170,240,255},{255,220,90}}},
  {"Flame",   "Flamme",    10, {{255,130,40},{170,50,20},{255,210,120},{255,240,80}}},
  {"Jungle",  "Jungle",    18, {{80,200,90},{30,100,50},{170,240,150},{255,220,60}}},
  {"Candy",   "Bonbon",    28, {{255,130,190},{170,60,120},{255,200,230},{120,230,255}}},
  {"Royal",   "Royal",     40, {{80,100,240},{30,40,140},{160,180,255},{255,215,70}}},
  {"Golden",  "Dore",      55, {{255,205,60},{150,100,10},{255,240,160},{255,255,255}}},
};
int count() { return sizeof(SK) / sizeof(SK[0]); }
const char* name(int i) { return g_lang ? SK[i].fr : SK[i].en; }
int need(int i) { return SK[i].need; }
bool unlocked(int i) { return g_unlockAll || trophy::total() >= SK[i].need; }
uint16_t c(int slot, uint16_t def) { if (g_skin <= 0 || g_skin >= count()) return def; const uint8_t* k = SK[g_skin].col[slot & 3]; return rgb(k[0], k[1], k[2]); }
}
void applyFx() { M5.Display.invertDisplay(g_invert); }

struct Conf { float x, y, vx, vy; uint16_t c; int t; };
static Conf cf[48];
void confetti() { static const uint16_t C[5] = {P_R, P_G, P_S, P_B, P_L}; for (auto &p : cf) p = {(float)rnd(10, SCR_W - 10), (float)rnd(-40, 0), rnd(-10, 10) / 10.f, rnd(8, 20) / 10.f, C[rnd(0, 4)], (int)rnd(60, 160)}; }

void present(int ox, int oy) {
  trophy::banner(); session::overlay();
  for (auto &p : cf) if (p.t > 0) { p.t--; p.x += p.vx + sinf(p.y * 0.2f) * 0.4f; p.y += p.vy; if (p.y > SCR_H) p.t = 0; canvas.fillRect((int)p.x, (int)p.y, 3, 3, P_K); canvas.fillRect((int)p.x, (int)p.y, 2, 2, p.c); }
  if (g_crt) for (int y = 1; y < SCR_H; y += 3) canvas.drawFastHLine(0, y, SCR_W, P_K);
  if (g_fps) { static uint32_t t0 = 0; static int n = 0, fps = 0; n++; if (millis() - t0 >= 1000) { fps = n; n = 0; t0 = millis(); } char b[8]; snprintf(b, sizeof b, "%d", fps); canvas.fillRect(SCR_W - 20, 0, 20, 9, P_K); txt(b, SCR_W - 2, 1, &fonts::Font0, P_S, textdatum_t::top_right); }
  static float z = -1;
  if (z < 0) { int w = M5.Display.width(), h = M5.Display.height(); if (w < h) { int t = w; w = h; h = t; } z = (w >= SCR_W && h >= SCR_H) ? 0 : fminf((float)w / SCR_W, (float)h / SCR_H); }
  if (z == 0) canvas.pushSprite(ox, oy);
  else canvas.pushRotateZoom(&M5.Display, M5.Display.width() / 2.0f, M5.Display.height() / 2.0f, 0, z, z);   // petit ecran : reduction, bandes noires si besoin
}

static bool g_pwrHold = false;   // vrai une frame quand l'utilisateur demande "retour au menu" (PWR maintenu ou A+B maintenus)

// M5.update() + corrections de boutons selon l'appareil :
//  - StickC / StickC Plus / StickS3 : le bouton PWR est lu par le circuit d'alimentation (AXP192 / M5PM1). La bibliotheque ne
//    donne alors que wasClicked()/wasHold() : wasPressed()/isPressed()/pressedFor() restent faux. On les recree ici
//    (un clic = appui virtuel de 120 ms). Le StickS3 ne signale jamais "maintenu" : on utilise donc aussi A+B.
//  - Nesso N1 : pas de 3e bouton, un doigt sur l'ecran joue le role de PWR.
//  - Tous : A+B maintenus ~1,2 s = retour au menu (marche meme si PWR ne peut pas etre maintenu).
void btnUpdate() {
#ifdef a_cardputer
  // M5.update() remet BtnA a l'etat du bouton Go (G0) a chaque image : l'appui clavier (Entree) etait alors
  // annule par l'anti-rebond et A ne marchait jamais au clavier. On garde l'etat de BtnA et on le recalcule nous-memes.
  m5::Button_Class keepA = M5.BtnA;
  M5Cardputer.update();
  M5.BtnA = keepA;
#else
  M5.update();
#endif
  uint32_t now = millis();
  g_pwrHold = false;
#ifdef a_cardputer
  {                                                                                        // Cardputer : le clavier joue le role des boutons et du tilt
    auto &kb = M5Cardputer.Keyboard;
    auto &ks = kb.keysState();
    bool kA = kb.isKeyPressed((char)KEY_ENTER) || kb.isKeyPressed(' ') || ks.enter || ks.space;
    bool kB = kb.isKeyPressed((char)KEY_TAB) || ks.tab || ks.ctrl;                  // Ctrl = B aussi (Cardputer-ADV)
    bool kP = kb.isKeyPressed((char)KEY_BACKSPACE) || kb.isKeyPressed('`') || ks.del;
    g_kbdX = (kb.isKeyPressed('/') ? 1 : 0) - (kb.isKeyPressed(',') ? 1 : 0);
    g_kbdY = (kb.isKeyPressed('.') ? 1 : 0) - (kb.isKeyPressed(';') ? 1 : 0);
    // le clavier de l'ADV (TCA8418) peut ne signaler un appui bref que pendant 1 image : on le prolonge 80 ms
    // pour qu'il passe l'anti-rebond des boutons (sinon "Entree" ne valide pas toujours)
    static uint32_t uA = 0, uB = 0, uP = 0;
    if (kA) uA = now + 80; if (kB) uB = now + 80; if (kP) uP = now + 80;
    bool goBtn = digitalRead(0) == LOW;                                                // bouton G0 (Go)
    M5.BtnA.setRawState(now, goBtn || kA || now < uA); M5.BtnB.setRawState(now, kB || now < uB); M5.BtnPWR.setRawState(now, kP || now < uP);
  }
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
  M5.BtnPWR.setRawState(now, M5.Touch.getCount() > 0);
#else
  bool pmicBtn = false;
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  pmicBtn = true;
#else
  { auto bd = M5.getBoard(); pmicBtn = (bd == m5::board_t::board_M5StickC || bd == m5::board_t::board_M5StickCPlus); }
#endif
  if (pmicBtn) {
    static uint32_t until = 0;
    if (M5.BtnPWR.wasHold()) g_pwrHold = true;
    if (M5.BtnPWR.wasClicked() || M5.BtnPWR.wasHold()) until = now + 120;
    M5.BtnPWR.setRawState(now, now < until);
  }
#endif
  static uint32_t chord = 0;
  if (M5.BtnA.isPressed() && M5.BtnB.isPressed()) { if (!chord) chord = now; else if (now - chord > 1200) { g_pwrHold = true; chord = 0; } }
  else chord = 0;
}

void sfx(int freq, int ms, bool stopPrev) { if (g_sound) { bgm::onSfx(ms); M5.Speaker.tone(freq, ms, -1, stopPrev); } }

struct GameDef { const char* en; const char* fr; void (*begin)(); void (*loop)(); void (*icon)(int, int); };
static const GameDef GAMES[] = {
  {"CHROME DINO",    "CHROME DINO",    dino::begin,     dino::loop,     dino::icon},
  {"FLAP BIRD",      "FLAP BIRD",      flap::begin,     flap::loop,     flap::icon},
  {"METEORS",        "METEORES",       space::begin,    space::loop,    space::icon},
  {"MAZE",           "LABYRINTHE",     maze::begin,     maze::loop,     maze::icon},
  {"BREAKOUT",       "CASSE-BRIQUES",  breakout::begin, breakout::loop, breakout::icon},
  {"INVADERS",       "INVADERS",       invaders::begin, invaders::loop, invaders::icon},
  {"SLALOM",         "SLALOM",         ski::begin,      ski::loop,      ski::icon},
  {"STACK",          "STACK",          stack::begin,    stack::loop,    stack::icon},
  {"RACING",         "COURSE",         race::begin,     race::loop,     race::icon},
  {"FLIGHT",         "AVION",          sky::begin,      sky::loop,      sky::icon},
  {"RIVER",          "RIVIERE",        river::begin,    river::loop,    river::icon},
  {"FISH",           "POISSON",        fish::begin,     fish::loop,     fish::icon},
  {"TETRIS",         "TETRIS",         tetris::begin,   tetris::loop,   tetris::icon},
  {"SNAKE",          "SNAKE",          snake::begin,    snake::loop,    snake::icon},
  {"LUNAR LANDER",   "ALUNISSAGE",     lander::begin,   lander::loop,   lander::icon},
  {"EMBERWOOD",      "EMBERWOOD",      zelda::begin,    zelda::loop,    zelda::icon},
  {"POCKET SIEGE",   "POCKET SIEGE",   td::begin,       td::loop,       td::icon},
  {"EMBER PINBALL",  "EMBER PINBALL",  pinball::begin,  pinball::loop,   pinball::icon},
  {"DAILY",          "DEFI DU JOUR",   daily::begin,    daily::loop,    daily::icon},
  {"2 PLAYERS",      "2 JOUEURS",        duo::begin,      duo::loop,      duo::icon},
  {"TROPHIES",       "TROPHEES",       trophy::begin,   trophy::loop,   trophy::icon},
  {"STATS",          "STATS",          stats::begin,    stats::loop,    stats::icon},
  {"MUSIC",          "MUSIQUE",        music::begin,    music::loop,    music::icon},
  {"SKINS",          "SKINS",          skins::begin,    skins::loop,    skins::icon},
  {"SETTINGS",       "REGLAGES",       settings::begin, settings::loop, settings::icon},
};
static const int NGAMES = sizeof(GAMES) / sizeof(GAMES[0]);
void gameBegin(int idx) { stats::play(idx); ctrl::prepare(idx); GAMES[idx].begin(); }
void gameLoop(int idx) { GAMES[idx].loop(); }
const char* gameName(int idx) { return L(GAMES[idx].en, GAMES[idx].fr); }
// icones du menu au style Emberwood (tools/art/icons.py -> icons_art.h) ; sinon l'icone dessinee par le jeu
static bool artIcon(int idx, int x, int y) {
  if (idx < 0 || idx >= (int)sizeof(iart::ICON_FOR) || iart::ICON_FOR[idx] == 255) return false;
  const iart::Icon& ic = iart::ICONS[iart::ICON_FOR[idx]];
  const uint16_t* pal = iart::PAL + ic.pal; const uint8_t* p = iart::DATA + ic.data;
  int n = 0;
  while (n < 64 * 64) {
    int run = p[0]; uint16_t c = pal[p[1]]; p += 2;
    int j = n / 64, i = n - j * 64;
    while (run > 0) { int len = 64 - i < run ? 64 - i : run; canvas.drawFastHLine(x + i, y + j, len, c); run -= len; n += len; i = 0; j++; }
  }
  return true;
}
void gameIcon(int idx, int x, int y) { if (!artIcon(idx, x, y)) GAMES[idx].icon(x, y); }
// ----------------------------------------------------------------------------- commandes : inclinaison ou boutons
namespace ctrl {
// jeux (index du menu) jouables aux boutons + aide affichee sur l'ecran de choix
static const int8_t SUP[] = {12, 13};          // Tetris, Snake (les autres jeux restent a l'inclinaison)
bool supports(int g) { for (int8_t s : SUP) if (s == g) return true; return false; }
int remembered(int g) { Preferences p; p.begin("ctrl", true); char k[6]; snprintf(k, sizeof k, "g%d", g); int v = p.getInt(k, 0); p.end(); return v; }
void remember(int g, bool b) { Preferences p; p.begin("ctrl", false); char k[6]; snprintf(k, sizeof k, "g%d", g); p.putInt(k, b ? 1 : 0); p.end(); }
void prepare(int g) { g_btnCtrl = supports(g) && (g_ctrl == 2 || (g_ctrl == 0 && remembered(g) == 1));
#ifdef ZTEST
  fprintf(stderr, "prepare %d ctrl=%d rem=%d -> %d%c", g, g_ctrl, remembered(g), (int)g_btnCtrl, 10);
#endif
}
const char* hint(int g) {
  switch (g) {
    case 8:  return L("A: right  B: left  A+B: nitro", "A : droite  B : gauche  A+B : nitro");
    case 5:  return L("A: right  B: left  (auto fire)", "A : droite  B : gauche  (tir auto)");
    case 12: return L("B/A: move  PWR: rotate  hold A+B: drop", "B/A : bouger  PWR : tourner  A+B : chute");
    case 13: return L("A: turn right  B: turn left", "A : tourne a droite  B : a gauche");
    case 14: return L("A/B: rotate  A+B: thrust", "A/B : pivoter  A+B : poussee");
    default: return L("A: right  B: left", "A : droite  B : gauche");
  }
}
static float bv = 0;
float x() {
  if (!g_btnCtrl) return tilt::x();
  bool a = M5.BtnA.isPressed(), b = M5.BtnB.isPressed();
  float target = (a && !b) ? 0.62f : ((b && !a) ? -0.62f : 0.f);
  bv += (target - bv) * 0.30f; if (fabsf(bv) < 0.01f) bv = 0;
  return bv;
}
}  // namespace ctrl

static int sel = 0, running = -1;
static int choosing = -1, chooseSel = 0;      // ecran "inclinaison ou boutons ?" avant un jeu
static void drawChoose() {
  ui::bg((int)(millis() / 60));
  ui::header(gameName(choosing), P_G);
  txt(L("How do you want to play?", "Comment veux-tu jouer ?"), SCR_W / 2, 22, &fonts::Font0, P_K, textdatum_t::top_center);
  for (int k = 0; k < 2; k++) {
    int x = 14 + k * 112, y = 34, w = 100, h = 62; bool on = k == chooseSel;
    canvas.fillRect(x - 1, y - 1, w + 2, h + 2, P_K); canvas.fillRect(x, y, w, h, on ? P_S : P_B);
    if (on) { canvas.fillRect(x, y, w, 2, P_G); canvas.drawRect(x - 3, y - 3, w + 6, h + 6, P_K); }
    int cx = x + w / 2, cy = y + 24;
    if (k == 0) {                                      // M5 penche
      for (int i = -14; i <= 14; i++) { int yy = cy + i / 3; canvas.drawFastVLine(cx + i, yy - 8, 16, P_K); }
      for (int i = -12; i <= 12; i++) { int yy = cy + i / 3; canvas.drawFastVLine(cx + i, yy - 6, 12, P_M); }
      canvas.fillTriangle(cx - 30, cy + 8, cx - 22, cy + 3, cx - 22, cy + 13, P_R); canvas.fillTriangle(cx + 30, cy - 8, cx + 22, cy - 13, cx + 22, cy - 3, P_R);
    } else {                                           // boutons A / B
      for (int b = 0; b < 2; b++) { int bx = cx - 18 + b * 24; canvas.fillCircle(bx + 6, cy, 10, P_K); canvas.fillCircle(bx + 6, cy, 8, b ? P_R : P_G); txt(b ? "A" : "B", bx + 6, cy - 6, &fonts::Font2, P_K, textdatum_t::top_center); }
    }
    txt(k == 0 ? L("TILT", "INCLINAISON") : L("BUTTONS", "BOUTONS"), cx, y + h - 16, &fonts::Font0, P_K, textdatum_t::top_center);
  }
  txt(chooseSel ? ctrl::hint(choosing) : L("Tilt the console to steer", "Incline la console pour diriger"), SCR_W / 2, 104, &fonts::Font0, P_K, textdatum_t::top_center);
  ui::footer(L("B: change   A: play", "B : changer   A : jouer"));
}
static float slide = 0;                       // animation de defilement (en tuiles)
static uint32_t lastAct = 0;                  // dernier appui (pour l'ecran de veille)
static int saver = 0;                         // 0 = normal, 1 = veille (animation), 2 = ecran eteint
static int batLevel = 100; static bool batChg = false; static uint32_t batT = 0;

// ----------------------------------------------------------------------------- menu
static void dim(int x, int y) { for (int j = 0; j < 64; j += 2) canvas.drawFastHLine(x, y + j, 64, P_K); }      // trame sombre : tuile non selectionnee

static void drawBattery(int x, int y, int lvl, bool chg) {            // pile pixel art : 4 segments + pourcentage
  uint16_t col = lvl <= 15 ? P_R : (lvl <= 35 ? P_G : P_S);
  canvas.fillRect(x, y, 22, 11, P_K); canvas.fillRect(x + 22, y + 3, 3, 5, P_K); canvas.fillRect(x + 1, y + 1, 20, 9, P_B);
  int seg = (lvl + 12) / 25; if (seg > 4) seg = 4; if (seg < 1 && lvl > 0) seg = 1;
  for (int i = 0; i < seg; i++) canvas.fillRect(x + 2 + i * 5, y + 2, 4, 7, col);
  if (chg) { canvas.fillTriangle(x + 12, y + 1, x + 7, y + 6, x + 11, y + 6, P_K); canvas.fillTriangle(x + 10, y + 5, x + 14, y + 5, x + 9, y + 10, P_K); }
  char b[8]; snprintf(b, sizeof b, "%d%%", lvl); txt(b, x + 28, y + 2, &fonts::Font0, P_B, textdatum_t::top_left);
}

static uint16_t rainbow(int h) {
  h &= 255; int r, g, b;
  if (h < 85) { r = 255 - h * 3; g = h * 3; b = 0; } else if (h < 170) { h -= 85; r = 0; g = 255 - h * 3; b = h * 3; } else { h -= 170; r = h * 3; g = 0; b = 255 - h * 3; }
  return rgb(r, g, b);
}
struct Star { float x, y, v; };
static Star stars[40]; static bool starsInit = false;
static void initStars() { for (auto &s : stars) s = {(float)rnd(0, SCR_W), (float)rnd(0, SCR_H), 0.15f + (float)rnd(0, 100) / 120.0f}; starsInit = true; }

static uint32_t toastT = 0; static bool adapted = false;                                    // message "FLAT / UPRIGHT" apres une (re)stabilisation
static const uint16_t ACC_RGB[21][3] = {                       // couleur d'ambiance de chaque entree
  {120,200,120},{250,210,60},{120,160,255},{110,220,200},{250,120,90},{120,230,130},{90,190,255},{255,170,70},{240,70,70},{90,170,255},{70,200,230},{70,150,255},{170,90,240},{90,220,90},{200,200,220},{255,150,60},{90,220,170},{255,200,60},{120,210,255},{255,110,190},{170,176,200}};
[[maybe_unused]] static uint16_t accOf(int idx) { const uint16_t* c = ACC_RGB[idx % 21]; return rgb(c[0], c[1], c[2]); }

// ----------------------------------------------------------------------------- themes de couleurs du menu
struct Theme { const char* en; const char* fr; uint8_t acc[3], top[3], bot[3], mtnFar[3], mtnNear[3], ground[3]; };
static const Theme THEMES[] = {
  {"Auto",    "Auto",     {0, 0, 0},       {8, 8, 30},   {66, 26, 92},  {24, 14, 52}, {34, 18, 66}, {20, 10, 44}},   // couleur propre a chaque jeu
  {"Ocean",   "Ocean",    {60, 200, 255},  {4, 12, 34},  {10, 64, 110}, {8, 30, 62},  {10, 44, 84}, {6, 24, 50}},
  {"Sunset",  "Coucher",  {255, 130, 60},  {30, 8, 28},  {140, 44, 56}, {60, 16, 40}, {86, 24, 48}, {40, 10, 30}},
  {"Forest",  "Foret",    {90, 230, 120},  {4, 22, 16},  {16, 74, 50},  {8, 40, 30},  {12, 58, 40}, {6, 34, 24}},
  {"Sakura",  "Sakura",   {255, 130, 200}, {32, 8, 36},  {130, 44, 116},{66, 18, 70}, {94, 28, 90}, {44, 12, 50}},
  {"Gold",    "Or",       {255, 210, 80},  {22, 16, 4},  {100, 70, 12}, {50, 34, 8},  {74, 52, 10}, {36, 24, 6}},
  {"Mono",    "Mono",     {220, 224, 240}, {10, 10, 14}, {54, 54, 66},  {26, 26, 34}, {38, 38, 48}, {18, 18, 24}},
};
int  themeCount() { return sizeof(THEMES) / sizeof(THEMES[0]); }
const char* themeName(int i) { return g_lang ? THEMES[i].fr : THEMES[i].en; }
static inline uint16_t thc(const uint8_t* c) { return rgb(c[0], c[1], c[2]); }


static void drawMenu() {
  uint32_t t = millis();
  static const uint16_t PALA[4] = {P_S, P_G, P_R, P_B};
  uint16_t acc = g_rainbow ? PALA[(t / 140) & 3] : (g_theme ? thc(THEMES[g_theme].acc) : PALA[sel & 3]);
  ui::bg((int)(t / (g_rainbow ? 12 : 50)));
  // --- barre du haut
  canvas.fillRect(0, 0, SCR_W, 21, P_K); canvas.fillRect(0, 0, SCR_W, 19, P_D); canvas.fillRect(0, 0, SCR_W, 1, P_M); canvas.fillRect(0, 19, SCR_W, 1, P_G);
  drawBattery(4, 4, batLevel, batChg);
  if (!(toastT && t - toastT < 2200)) { const char* T = "AD ARCADE"; txt(T, SCR_W / 2 + 22, 3, &fonts::Font4, P_R, textdatum_t::top_center); txtO(T, SCR_W / 2 + 21, 2, &fonts::Font4, P_G, P_K, textdatum_t::top_center); }
  char b[24]; snprintf(b, sizeof b, "%d/%d", sel + 1, NGAMES); txt(b, SCR_W - 4, 6, &fonts::Font0, P_B, textdatum_t::top_right);
  // --- carrousel
  float pulse = (sinf(t * 0.008f) + 1) * 0.5f;
  for (int d = -1; d <= 1; d++) {
    int idx = (sel + d + NGAMES) % NGAMES;
    float px = SCR_W / 2 - 32 + (d - slide) * 82;
    int ix = (int)px, iy = 28 + (d == 0 ? (((t / 260) & 1) ? 0 : -1) : 5);
    if (ix > SCR_W || ix < -64) continue;
    float cen = 1.0f - fminf(1.0f, fabsf(d - slide));            // 1 = pile au centre
    canvas.fillRect(ix + 6, 98, 52, 3, P_K);                      // ombre au sol
    if (cen > 0.5f) {
      canvas.fillRect(ix - 6, iy - 6, 76, 76, P_K);               // cadre : noir, couleur d'ambiance, noir
      canvas.fillRect(ix - 5, iy - 5, 74, 74, pulse > 0.5f ? acc : P_B);
      canvas.fillRect(ix - 2, iy - 2, 68, 68, P_K);
      canvas.fillRect(ix - 5, iy - 5, 74, 1, P_B);
      for (int k = 0; k < 4; k++) { int cx = ix - 6 + (k & 1) * 74, cy = iy - 6 + (k >> 1) * 74; canvas.fillRect(cx - 1, cy - 1, 4, 4, P_K); canvas.fillRect(cx, cy, 2, 2, P_G); }
    }
    gameIcon(idx, ix, iy);
    if (cen < 0.5f) dim(ix, iy);
  }
  { int bob = (int)((t / 250) & 1); uint16_t ar = P_G;
    canvas.fillTriangle(3 - bob, 62, 14 - bob, 50, 14 - bob, 74, P_K); canvas.fillTriangle(6 - bob, 62, 13 - bob, 53, 13 - bob, 71, ar);
    canvas.fillTriangle(SCR_W - 3 + bob, 62, SCR_W - 14 + bob, 50, SCR_W - 14 + bob, 74, P_K); canvas.fillTriangle(SCR_W - 6 + bob, 62, SCR_W - 13 + bob, 53, SCR_W - 13 + bob, 71, ar); }
  // --- plaque du nom + pagination
  const char* nm = L(GAMES[sel].en, GAMES[sel].fr);
  canvas.fillRect(16, 103, 208, 21, P_K); canvas.fillRect(17, 104, 206, 19, P_B); canvas.fillRect(17, 104, 206, 2, P_L); canvas.fillRect(17, 121, 206, 2, P_L);
  canvas.fillRect(19, 106, 3, 3, acc); canvas.fillRect(218, 106, 3, 3, acc); canvas.fillRect(19, 118, 3, 3, acc); canvas.fillRect(218, 118, 3, 3, acc);
  txt(nm, SCR_W / 2 + 1, 106, &fonts::Font4, P_L, textdatum_t::top_center); txt(nm, SCR_W / 2, 105, &fonts::Font4, P_K, textdatum_t::top_center);
  for (int i = 0; i < NGAMES; i++) {                                                     // points de pagination
    int x = SCR_W / 2 - (NGAMES * 9) / 2 + i * 9 + 1; bool on = i == sel;
    canvas.fillRect(x, 127, on ? 6 : 4, on ? 6 : 4, P_K); canvas.fillRect(x + 1, 128, on ? 4 : 2, on ? 4 : 2, on ? acc : P_M);
  }
  if (toastT && t - toastT < 2200) {                                                     // posture detectee par le gyroscope
    bool flat = tilt::posture() == 0; int w = 104;                                         // a la place du titre (ne cache plus l'icone)
    canvas.fillRect(SCR_W / 2 + 21 - w / 2, 2, w, 15, P_K); canvas.fillRect(SCR_W / 2 + 22 - w / 2, 3, w - 2, 13, P_S);
    txt(flat ? L("FLAT MODE", "MODE A PLAT") : L("UPRIGHT MODE", "MODE DEBOUT"), SCR_W / 2 + 21, 6, &fonts::Font0, P_K, textdatum_t::top_center);
  }
}

// ----------------------------------------------------------------------------- ecran de veille
static float svx = 60, svy = 30, svdx = 0.9f, svdy = 0.7f; static int svGame = 0;
static void drawSaver() {
  canvas.fillScreen(0);
  for (int i = 0; i < 20; i++) { int sx = (i * 61 + 7) % SCR_W, sy = (i * 37 + 11) % SCR_H; if (((millis() / 400) + i) % 5) canvas.drawPixel(sx, sy, rgb(70, 74, 100)); }
  svx += svdx; svy += svdy; bool hit = false;
  if (svx < 0) { svx = 0; svdx = -svdx; hit = true; } if (svx > SCR_W - 64) { svx = SCR_W - 64; svdx = -svdx; hit = true; }
  if (svy < 0) { svy = 0; svdy = -svdy; hit = true; } if (svy > SCR_H - 64) { svy = SCR_H - 64; svdy = -svdy; hit = true; }
  if (hit) svGame = (svGame + 1) % NGAMES;
  gameIcon(svGame, (int)svx, (int)svy);
  present(0, 0);
}

static void toMenu() {
  music::end(); session::end(); stats::flush(); bgm::stop(); running = -1; slide = 0; g_quit = false; lastAct = millis(); saver = 0; if (adapted) { toastT = millis(); adapted = false; }
  M5.Speaker.stop(); applyBrightness(); g_btnCtrl = false; choosing = -1;
}

// ----------------------------------------------------------------------------- intro facon demo scene des annees 90
static void intro() {
  const char* SCROLL = "     WELCOME TO AD ARCADE ***  18 GAMES IN YOUR POCKET ***  TILT  PRESS  PLAY ***  "
                       "TETRIS  SNAKE  LUNAR LANDER  FLAP BIRD  RACING  FLIGHT  AND MORE ***  "
                       "25 FAMOUS TUNES TO PLAY ***  CODED ON AN ESP32 WITH LOVE ***  GREETINGS TO ALL M5 FANS ***  ENJOY !!!     ";
  const int SL = strlen(SCROLL);
  const char* SUB = "WELCOME  -  M5STICKC PLUS2 EDITION";
  static const int ARP[16] = {220, 262, 330, 440, 330, 262, 220, 262, 196, 247, 294, 392, 294, 247, 196, 247};
  if (!starsInit) initStars();
  float sx = SCR_W; int t = 0;
  btnUpdate();
  while (true) {
    btnUpdate();
    if (t > 15 && (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnPWR.wasPressed())) break;
    sx -= 5.0f; if (sx + SL * 12 < 0) break;
    canvas.fillScreen(rgb(4, 2, 16));
    for (int y = 0; y < SCR_H; y += 3) canvas.fillRect(0, y, SCR_W, 3, mixc(rgb(4, 2, 16), rgb(30, 12, 52), y * 256 / SCR_H));
    for (int k = 0; k < 3; k++) {                                              // barres "copper"
      int cy = 56 + (int)(sinf(t * 0.055f + k * 1.9f) * 38);
      uint16_t base = k == 0 ? rgb(255, 90, 40) : (k == 1 ? rgb(40, 200, 255) : rgb(230, 60, 255));
      for (int d = -8; d <= 8; d++) { int lvl = (9 - abs(d)) * 28; uint16_t c = mixc(rgb(8, 2, 20), base, lvl > 256 ? 256 : lvl); if (d == 0) c = mixc(base, TFT_WHITE, 120); canvas.drawFastHLine(0, cy + d, SCR_W, c); }
    }
    for (auto &st : stars) { st.x -= st.v * 2.5f; if (st.x < 0) { st.x += SCR_W; st.y = rnd(0, SCR_H); } canvas.drawPixel((int)st.x, (int)st.y, st.v > 0.8f ? TFT_WHITE : rgb(150, 150, 210)); }
    // logo : lettres qui tombent puis ondulent
    const char* LOGO = "AD ARCADE";
    for (int i = 0; i < 9; i++) {
      int tt = t - i * 4; if (tt < 0) continue;
      int y = 30 + (tt < 18 ? -((18 - tt) * (18 - tt)) / 3 : 0) + (int)(sinf(t * 0.14f + i * 0.7f) * 7);
      int x = 12 + i * 24; char ch[2] = {LOGO[i], 0}; if (ch[0] == ' ') continue;
      txt(ch, x + 3, y + 3, &fonts::Font0, rgb(10, 4, 30), textdatum_t::top_left, 4.0f);
      txt(ch, x + 1, y + 1, &fonts::Font0, TFT_WHITE, textdatum_t::top_left, 4.0f);
      txt(ch, x, y, &fonts::Font0, rainbow(i * 26 + t * 5), textdatum_t::top_left, 4.0f);
    }
    int n = (t - 50) / 2; if (n > (int)strlen(SUB)) n = strlen(SUB);
    if (n > 0) { char sb[40]; memcpy(sb, SUB, n); sb[n] = 0; txt(sb, SCR_W / 2, 78, &fonts::Font0, (t / 4) & 1 ? rgb(255, 230, 120) : TFT_WHITE, textdatum_t::top_center, 1.0f); }
    canvas.fillRect(0, 98, SCR_W, 37, rgb(0, 0, 0)); canvas.drawFastHLine(0, 98, SCR_W, rainbow(t * 4)); canvas.drawFastHLine(0, 99, SCR_W, rainbow(t * 4 + 60)); canvas.drawFastHLine(0, 134, SCR_W, rainbow(t * 4 + 120));
    for (int i = 0; i < SL; i++) {
      int x = (int)sx + i * 12; if (x < -12 || x > SCR_W) continue;
      char ch[2] = {SCROLL[i], 0}; if (ch[0] == ' ') continue;
      int y = 109 + (int)(sinf(t * 0.2f + i * 0.5f) * 7);
      txt(ch, x + 1, y + 1, &fonts::Font0, rgb(40, 20, 60), textdatum_t::top_left, 2.0f);
      txt(ch, x, y, &fonts::Font0, rainbow(i * 14 + t * 6), textdatum_t::top_left, 2.0f);
    }
    present(0, 0);
    if (t % 4 == 0) sfx(ARP[(t / 4) & 15] * ((t / 64) & 1 ? 2 : 1), 90);
    if (t % 16 == 0) sfx(ARP[(t / 16) & 3] / 2, 120, false);
    t++; delay(24);
  }
  M5.Speaker.stop();
  for (int f = 0; f < 12; f++) {                                              // fondu en damier
    for (int by = 0; by < 9; by++) for (int bx = 0; bx < 15; bx++) if (((bx * 7 + by * 3) % 13) < f + 1) canvas.fillRect(bx * 16, by * 16, 16, 16, rgb(12, 10, 30));
    present(0, 0); delay(25);
  }
  sfx(660, 70); sfx(880, 70, false); sfx(1320, 140, false);
}

void setup() {
  auto cfg = M5.config();
#ifdef a_cardputer
  M5Cardputer.begin(cfg, true);
#else
  M5.begin(cfg);
#endif
  if (M5.getBoard() == m5::board_t::board_M5StickCPlus2) { pinMode(4, OUTPUT); digitalWrite(4, HIGH); }   // maintien de l'alimentation (StickC Plus2 uniquement)
  M5.Display.setRotation(SCR_ROT);
  if (M5.Display.width() < M5.Display.height()) g_baseRot = (SCR_ROT + 1) & 3;   // securite : toujours en paysage (selon la version de M5GFX)
  applyRotation();
  mp.begin("arcade", false);
  sel = mp.getInt("sel", 0); if (sel < 0 || sel >= NGAMES) sel = 0;
  g_bright = mp.getInt("bri", 2); if (g_bright < 0 || g_bright > 4) g_bright = 2;
  g_vol = mp.isKey("vol") ? mp.getInt("vol", 3) : (mp.getBool("snd", true) ? 3 : 0); if (g_vol < 0 || g_vol > 5) g_vol = 3;
  g_diff = mp.getInt("diff", 1); if (g_diff < 0 || g_diff > 2) g_diff = 1;
  g_unlockAll = mp.getBool("unl", false); g_noIntro = mp.getBool("ni", false); g_crt = mp.getBool("crt", false); g_invert = mp.getBool("inv", false); g_rainbow = mp.getBool("rbw", false); applyFx();
  g_skin = mp.getInt("skin", 0); if (g_skin < 0 || g_skin >= skin::count()) g_skin = 0;
  g_music = mp.getBool("mus", true);
  g_lang = mp.getInt("lang", 0) ? 1 : 0; g_theme = mp.getInt("theme", 0); if (g_theme < 0 || g_theme >= themeCount()) g_theme = 0;
  g_sleepIdx = mp.getInt("slp", 1); if (g_sleepIdx < 0 || g_sleepIdx > 4) g_sleepIdx = 1;
  g_ctrl = mp.getInt("ctl", 0); if (g_ctrl < 0 || g_ctrl > 2) g_ctrl = 0; g_flip = mp.getBool("flip", false); applyRotation();
  applyBrightness();
  applyVolume();
  canvas.setColorDepth(16);
  canvas.createSprite(SCR_W, SCR_H);
  tilt::load();
  if (g_skin && !skin::unlocked(g_skin)) g_skin = 0;                    // securite : apparence non debloquee
  if (!g_noIntro) intro();
  if (tilt::valid()) { tilt::zero(); if (M5.Imu.isEnabled()) toastT = millis(); }             // stabilisation unique : la position tenue au demarrage = "a plat" pour tous les jeux
  else wizard::begin();                         // premier demarrage : assistant (langue, calibrage, date + heure)
  lastAct = millis();
}

void loop() {
  btnUpdate();
  bool anyBtn = M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnPWR.wasPressed();
  if (wizard::active()) { wizard::loop(); lastAct = millis(); return; }
  if (tilt::calibrating()) { tilt::calibrationStep(); lastAct = millis(); return; }
  if (running >= 0) {
    if (M5.BtnPWR.pressedFor(1500) || g_pwrHold || g_quit) { sfx(440, 60); sfx(330, 90, false); toMenu(); return; }
    GAMES[running].loop();
    stats::tick(running < trophy::T_COUNT ? running : session::activeGame());
    return;
  }
  if (choosing >= 0) {                                                      // ecran de choix des commandes
    if (M5.BtnB.wasPressed() || M5.BtnPWR.wasPressed()) { chooseSel ^= 1; sfx(1200, 15); }
    if (M5.BtnA.wasPressed()) {
      ctrl::remember(choosing, chooseSel == 1); sfx(880, 40); sfx(1320, 60, false);
      if (!chooseSel && tilt::adapt()) adapted = true;
      g_quit = false; sel = choosing; choosing = -1; gameBegin(sel); running = sel; lastAct = millis(); return;
    }
    drawChoose(); present(0, 0); lastAct = millis(); return;
  }
  uint32_t now = millis();
  // ----- ecran de veille (uniquement dans le menu)
  if (saver) {
    if (anyBtn) { saver = 0; applyBrightness(); lastAct = now; return; }          // le 1er appui reveille seulement
    if (saver == 1) { drawSaver(); delay(16); if (now - lastAct > SLEEP_MS[g_sleepIdx] + 60000) { saver = 2; M5.Display.setBrightness(0); } }
    else delay(40);
    return;
  }
  if (anyBtn) lastAct = now;
  if (SLEEP_MS[g_sleepIdx] && now - lastAct > SLEEP_MS[g_sleepIdx]) { saver = 1; M5.Display.setBrightness(BRI[g_bright] > 40 ? 40 : BRI[g_bright]); return; }
  if (now - batT > 3000 || batT == 0) {
    batT = now; int l = M5.Power.getBatteryLevel(); if (l >= 0) batLevel = l > 100 ? 100 : l;
    batChg = M5.Power.isCharging() == m5::Power_Class::is_charging;
  }
  if (M5.BtnB.wasPressed()) { sel = (sel + 1) % NGAMES; slide = -1; sfx(1200, 20); }
  if (M5.BtnPWR.wasPressed()) { sel = (sel + NGAMES - 1) % NGAMES; slide = 1; sfx(1000, 20); }
  if (slide != 0) { slide *= 0.7f; if (fabsf(slide) < 0.03f) slide = 0; }
  if (M5.BtnA.wasPressed()) {
    mp.putInt("sel", sel); sfx(880, 50); sfx(1320, 90, false);
    if (ctrl::supports(sel) && g_ctrl == 0) { choosing = sel; chooseSel = ctrl::remembered(sel); return; }   // inclinaison ou boutons ?
    if (tilt::adapt()) adapted = true;                                      // detecte a plat / debout et re-stabilise si besoin
    g_quit = false; if (sel < trophy::T_COUNT) gameBegin(sel); else GAMES[sel].begin(); running = sel;
    return;
  }
  drawMenu();
  present(0, 0);
}
