// ============================================================================
//  M5 ARCADE - elements communs a tous les jeux
// ============================================================================
#pragma once
#include <M5Unified.h>
#include <Preferences.h>
#include <math.h>

#ifndef SCR_ROT
#define SCR_ROT 3          // 1 ou 3 : change le sens de l'ecran (donc haut/bas physiques)
#endif
#define AD_VERSION "1.9.0"   // version affichee dans les reglages
#define SCR_W   240
#define SCR_H   135

extern M5Canvas canvas;    // tampon plein ecran partage par tous les jeux (cree dans main.cpp)

static inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
// generateur aleatoire : vrai hasard, sauf en "defi du jour" / "2 joueurs" ou une graine commune rend les parties comparables
extern uint32_t g_rngState; extern bool g_rngSeeded;
static inline uint32_t rawRand() { if (!g_rngSeeded) return esp_random(); uint32_t x = g_rngState; x ^= x << 13; x ^= x >> 17; x ^= x << 5; g_rngState = x; return x; }
static inline void seedRand(uint32_t s) { g_rngState = s ? s : 0x9E3779B9u; g_rngSeeded = true; }
static inline uint32_t rnd(uint32_t a, uint32_t b) { return a + rawRand() % (b - a + 1); }

// ----------------------------- palette pixel art commune (8 couleurs) : style 16 bits, contour noir, 3 tons par matiere
#define P_K rgb(10, 14, 13)      // noir : contours, ombres profondes
#define P_D rgb(27, 41, 37)      // sombre
#define P_M rgb(59, 82, 71)      // moyen
#define P_L rgb(122, 147, 128)   // clair
#define P_B rgb(212, 222, 200)   // brillant
#define P_R rgb(156, 46, 48)     // sang
#define P_G rgb(214, 162, 72)    // or (avec moderation)
#define P_S rgb(159, 224, 90)    // spectral (avec moderation)
// boite : contour noir 1 px, ton moyen + reflet haut/gauche + ombre bas/droite
static inline void pxBox(int x, int y, int w, int h, uint16_t c, uint16_t lt, uint16_t dk) {
  canvas.fillRect(x, y, w, h, P_K);
  if (w > 2 && h > 2) { canvas.fillRect(x + 1, y + 1, w - 2, h - 2, c); canvas.drawFastHLine(x + 1, y + 1, w - 2, lt); canvas.drawFastVLine(x + 1, y + 1, h - 2, lt); canvas.drawFastHLine(x + 1, y + h - 2, w - 2, dk); canvas.drawFastVLine(x + w - 2, y + 1, h - 2, dk); }
}
// disque : contour noir, ombre bas/droite, reflet haut/gauche
static inline void pxDisc(int cx, int cy, int r, uint16_t c, uint16_t lt, uint16_t dk) {
  canvas.fillCircle(cx, cy, r + 1, P_K); canvas.fillCircle(cx, cy, r, dk); canvas.fillCircle(cx - 1, cy - 1, r > 1 ? r - 1 : 1, c);
  if (r >= 3) canvas.fillRect(cx - r / 2 - 1, cy - r / 2 - 1, 2, 2, lt); else canvas.drawPixel(cx - 1, cy - 1, lt);
}
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

void present(int ox = 0, int oy = 0);   // envoie le tampon a l'ecran (mis a l'echelle sur les petits ecrans, ex. StickC 160x80)
void btnUpdate();   // M5.update() + bouton PWR de remplacement sur les appareils qui n'en ont pas (Nesso N1 : toucher l'ecran)
void sfx(int freq, int ms, bool stopPrev = true);   // son (defini dans main.cpp) ; muet si le son est coupe dans les reglages

// reglages globaux (definis dans main.cpp, modifies dans settings.cpp)
extern int  g_bright;       // 0..4
extern bool g_sound;
extern int  g_theme;        // theme de couleurs du menu (0 = auto)
int  themeCount();
const char* themeName(int i);
extern bool g_music;         // petites musiques dans les jeux
extern int  g_vol;           // volume 0..5 (0 = muet)
extern int  g_diff;          // difficulte : 0 facile, 1 normal, 2 difficile
extern int g_kbdX, g_kbdY;   // pseudo-inclinaison au clavier (Cardputer)
extern bool g_noIntro, g_crt, g_invert, g_rainbow, g_fps;   // options admin (debug / fun)
void confetti(); void applyFx();
extern bool g_unlockAll;     // mode admin : toutes les apparences debloquees
extern int  g_skin;          // apparence du heros des jeux (debloquee avec les medailles)
static inline float dif(float easy, float normal, float hard) { return g_diff == 0 ? easy : (g_diff == 2 ? hard : normal); }
static inline int   difi(int easy, int normal, int hard) { return g_diff == 0 ? easy : (g_diff == 2 ? hard : normal); }
// espace de noms des records selon la difficulte : "flap" (normal), "flapE" (facile), "flapH" (difficile)
static inline const char* dnsFor(const char* base, int d) { static char buf[4][20]; static int k = 0; char* o = buf[k = (k + 1) & 3]; snprintf(o, 20, "%s%s", base, d == 0 ? "E" : (d == 2 ? "H" : "")); return o; }
static inline const char* dns(const char* base) { return dnsFor(base, g_diff); }
// commandes : g_ctrl = reglage (0 = demander avant la partie, 1 = inclinaison, 2 = boutons) ; g_btnCtrl = le jeu en cours se joue aux boutons
extern int  g_ctrl; extern bool g_btnCtrl;
extern bool g_flip;           // ecran retourne de 180 deg (reglage "Screen")
void applyRotation();
uint8_t* idxBuffer();          // tampon d'indices 240x135 partage (Emberwood, Pocket Siege), alloue une seule fois
namespace ctrl {
bool supports(int gameIdx);   // le jeu a un mode boutons ?
void prepare(int gameIdx);    // fixe g_btnCtrl pour ce jeu (reglage + dernier choix)
int  remembered(int gameIdx); void remember(int gameIdx, bool buttons);
const char* hint(int gameIdx);
float x();                    // gauche/droite : inclinaison, ou en mode boutons B maintenu = gauche, A maintenu = droite
}
extern int  g_lang;           // 0 = English (defaut), 1 = Francais
static inline const char* L(const char* en, const char* fr) { return g_lang ? fr : en; }
extern int  g_sleepIdx;     // 0..4 (30 s, 1 min, 2 min, 5 min, jamais)
extern bool g_quit;         // un jeu / l'ecran de reglages met true pour revenir au menu
void applyBrightness(); void applyVolume();
void saveSettings();

// melange de 2 couleurs RGB565 (t = 0..256)
static inline uint16_t mixc(uint16_t a, uint16_t b, int t) {
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31, br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  return (uint16_t)(((ar + (((br - ar) * t) >> 8)) << 11) | ((ag + (((bg - ag) * t) >> 8)) << 5) | (ab + (((bb - ab) * t) >> 8)));
}

// texte : police + couleur + alignement en un seul appel (polices ASCII : pas d'accents)
static inline void txt(const char* s, int x, int y, const lgfx::IFont* f, uint16_t col,
                       textdatum_t d = textdatum_t::top_left, float size = 1.0f) {
  // polices pixel : Font2 -> police 8x16 style PC (net), Font4 -> police 8x8 style C64 agrandie (entier), Font0 reste la GLCD 5x7
  if (f == &fonts::Font2) { f = &fonts::AsciiFont8x16; size = (float)(int)(size + 0.5f); if (size < 1) size = 1; }
  else if (f == &fonts::Font4) { f = &fonts::Font8x8C64; size = (float)(int)(2 * size + 0.5f); }
  canvas.setFont(f); canvas.setTextSize(size); canvas.setTextDatum(d); canvas.setTextColor(col);
  canvas.drawString(s, x, y);
  canvas.setTextSize(1.0f);
}
// texte avec contour (lisible sur n'importe quel fond)
static inline void txtO(const char* s, int x, int y, const lgfx::IFont* f, uint16_t col, uint16_t outline,
                        textdatum_t d = textdatum_t::top_left, float size = 1.0f) {
  for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if (dx || dy) txt(s, x + dx, y + dy, f, outline, d, size);
  txt(s, x, y, f, col, d, size);
}


// ----------------------------- kit d'interface retro (ui.cpp) : fenetres, boutons, icones pixel, Game Over
namespace ui {
void art(int x, int y, const char* const* rows, int h, int sc = 1, char from = 0, uint16_t to = 0);   // from/to : remplace une lettre par une autre couleur
void night(int h);                                                             // ciel de nuit violet/bleu sur les h premiers pixels
void tile(int x, int y, uint16_t c);                                           // fond d'icone 64x64            // pixel art en texte : K D M L B R G S = palette, '.' = vide
void skull(int x, int y, int sc = 1); void crown(int x, int y, int sc = 1); void star(int x, int y, int sc = 1);
void heart(int x, int y, int sc = 1); void coin(int x, int y, int sc = 1);
void window(int x, int y, int w, int h, uint16_t bar, const char* title);      // fenetre claire a barre de titre coloree
void button(int cx, int y, int w, const char* label, uint16_t col);            // bouton retro centre en cx
void gameOver(const char* title, uint32_t score, uint32_t best, bool record, const char* hint = nullptr);
void popText(int cx, int y, const char* s, uint16_t col);
void bg(int t);                                                                  // fond retro : damier vert-gris qui defile doucement
void header(const char* title, uint16_t bar);                                    // barre de titre pleine largeur
void footer(const char* hint);                                                   // barre d'aide en bas
void field(int x, int y, int w, int h, const char* label, const char* value, bool sel);   // case de saisie (date, heure...)
void pause();                                                                    // fenetre PAUSE
void notice(const char* title, const char* l1, const char* l2, uint16_t bar);    // fenetre d'annonce (niveau reussi, pose, victoire...)                       // "+10" : gros texte jaune a contour noir
}

// ----------------------------- assistant du premier demarrage : langue -> calibrage -> date et heure
namespace wizard { bool active(); void begin(); void loop(); }

// ----------------------------- les jeux -------------------------------------
// begin() : appele au lancement ; loop() : appele a chaque tour (dessine et pousse l'ecran) ;
// icon(x, y) : dessine l'icone 64x64 du menu (coin haut-gauche en x, y)
namespace dino  { void begin(); void loop(); void icon(int x, int y); }
namespace flap  { void begin(); void loop(); void icon(int x, int y); }
namespace space { void begin(); void loop(); void icon(int x, int y); }
namespace maze  { void begin(); void loop(); void icon(int x, int y); }
namespace breakout { void begin(); void loop(); void icon(int x, int y); }
namespace invaders { void begin(); void loop(); void icon(int x, int y); }
namespace ski { void begin(); void loop(); void icon(int x, int y); }
namespace stack { void begin(); void loop(); void icon(int x, int y); }
namespace sky { void begin(); void loop(); void icon(int x, int y); }
namespace race { void begin(); void loop(); void icon(int x, int y); }
namespace river { void begin(); void loop(); void icon(int x, int y); }
namespace fish { void begin(); void loop(); void icon(int x, int y); }
namespace tetris { void begin(); void loop(); void icon(int x, int y); }
namespace snake { void begin(); void loop(); void icon(int x, int y); }
namespace lander { void begin(); void loop(); void icon(int x, int y); }
namespace zelda { void begin(); void loop(); void icon(int x, int y); }
namespace td { void begin(); void loop(); void icon(int x, int y); }
namespace pinball { void begin(); void loop(); void icon(int x, int y); }   // Ember Pinball      // Pocket Siege (tower defense)   // Legend of Emberwood (aventure)
namespace music { void begin(); void loop(); void icon(int x, int y); void end(); }
namespace settings { void begin(); void loop(); void icon(int x, int y); }

// ----------------------------- musiques de fond des jeux (boucles originales)
// run(id, on) : a appeler a chaque tour de boucle ; on = la partie est en cours
namespace trophy {
enum Id { T_DINO, T_FLAP, T_METEORS, T_MAZE, T_BREAKOUT, T_INVADERS, T_SLALOM, T_STACK, T_RACING, T_FLIGHT, T_RIVER, T_FISH, T_TETRIS, T_SNAKE, T_LANDER, T_EMBER, T_COUNT };   // T_EMBER = Emberwood (index 15 du menu)
int  total();                          // medailles gagnees (normal + difficile)
int  medalAt(int id, int diff);        // 0..3 : meilleure medaille du jeu a cette difficulte
void record(int id, uint32_t value);   // fin de partie : medaille bronze / argent / or selon la valeur
void banner();                         // bandeau "nouvelle medaille" (appele par present())
void begin(); void loop(); void icon(int x, int y);
void adminGold(); void adminClear(); void adminSet(int id, int lvl); void adminSetAll(int lvl); const char* nameOf(int id); int levelOf(int id);     // mode admin (test) : toutes les medailles en or / tout effacer
}

// ----------------------------- skins (apparences) : debloques avec les medailles
namespace skin {
int  count();                      // nombre d'apparences
const char* name(int i);
int  need(int i);                  // nombre de medailles necessaires
bool unlocked(int i);
// couleur du heros : 0 corps, 1 ombre, 2 reflet, 3 accent ; renvoie def si l'apparence "Classic" est choisie
uint16_t c(int slot, uint16_t def);
}

// ----------------------------- modes speciaux : defi du jour, 2 joueurs, stats
namespace session {
void onOver(int gameId, int score);      // appele par chaque jeu a la fin d'une partie
int  activeGame();                       // jeu en cours dans un mode special (-1 si aucun)
void end();                              // appele au retour au menu
bool dateEditing(); void dateBegin(); void dateLoop();   // reglage de la date et de l'heure (reglages + defi du jour)
void overlay();                          // bandeau dessine par present()
}
namespace daily { void begin(); void loop(); void icon(int x, int y); void adminReset(int streak); }
namespace duo   { void begin(); void loop(); void icon(int x, int y); }
namespace skins { void begin(); void loop(); void icon(int x, int y); }
namespace flap  { void preview(int cx, int cy); }
namespace space { void preview(int cx, int cy); }
namespace lander { void preview(int cx, int cy); }
namespace snake { void preview(int cx, int cy); }
namespace river { void preview(int cx, int cy); }
namespace skin { int need(int i); }
namespace stats { void begin(); void loop(); void icon(int x, int y); void play(int idx); void tick(int idx); void flush(); int total(); }
// acces aux jeux depuis les modes speciaux (definis dans main.cpp) : indices 0..14 = identifiants trophy::Id
void gameBegin(int idx); void gameLoop(int idx); const char* gameName(int idx); void gameIcon(int idx, int x, int y);

namespace bgm { void run(int id, bool on); void stop(); void onSfx(int ms); }

// ----------------------------- inclinaison (IMU) -----------------------------
// Calibrage automatique : on montre quel cote pencher, le module trouve tout seul
// quel axe de l'accelerometre correspond (et dans quel sens). Memorise en flash.
namespace tilt {
  void  load();                 // lit le calibrage sauvegarde
  bool  valid();                // un calibrage existe ?
  void  startCalibration();     // lance l'assistant (3 etapes, bouton A)
  bool  calibrating();          // l'assistant est-il actif ?
  void  calibrationStep();      // a appeler a chaque tour tant que calibrating() : dessine + lit A
  void  update();               // lit l'IMU (1 fois par image)
  bool  adapt();                // au lancement d'un jeu : si le M5 n'est plus tenu comme au demarrage (a plat <-> debout), re-stabilise tout seul ; true si fait
  int   posture();              // 0 = a plat (ecran vers le haut), 1 = debout / paysage (comme une console)
  void  zero();                 // memorise la position "a plat" actuelle (~60 ms) : fait au demarrage + dans Reglages
  float x();                    // -1..1 : + = on penche le cote DROIT vers le bas
  float y();                    // -1..1 : + = on penche le HAUT de l'ecran vers le bas
}
