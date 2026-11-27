// ============================================================================
//  COURSE - choisis ta voiture (B / HAUT pour changer, M5 pour valider), puis ta couleur.
//  Penche le M5 pour piloter. M5 maintenu = nitro. HAUT = pause.
// ============================================================================
#include "common.h"

namespace race {

static const uint32_t STEP_US = 20000;
static const int ROAD_L = 58, ROAD_R = 182, PY = 102;
static const int LANE[3] = {79, 120, 161};
enum State { SEL_CAR, SEL_COLOR, PLAY, PAUSED, OVER };
static State state = SEL_CAR;
static Preferences pr;
static uint32_t last = 0, acc = 0;

struct CarDef { const char* name; int speedStars, handStars; };
static const CarDef CARS[3] = {{"COUPE GT", 3, 3}, {"SUPERCAR", 5, 2}, {"ROADSTER", 2, 5}};
static const uint16_t COLS[7] = {rgb(222, 28, 40), rgb(250, 125, 20), rgb(250, 205, 20), rgb(40, 185, 75), rgb(35, 100, 235), rgb(238, 238, 244), rgb(36, 36, 44)};
static const char* CNAME_FR[7] = {"ROUGE", "ORANGE", "JAUNE", "VERT", "BLEU", "BLANC", "NOIR"};
static const char* CNAME_EN[7] = {"RED", "ORANGE", "YELLOW", "GREEN", "BLUE", "WHITE", "BLACK"};
static int carSel = 0, colSel = 0;

struct Veh { float x, y, v; uint8_t kind, col; bool on, passed; };
static Veh veh[8];
struct Deco { float x, y; uint8_t t; }; static Deco deco[14];
struct Pop { float x, y; int t; char s[12]; uint16_t col; }; static Pop pops[3];

static float px = 120, pv = 0, spd = 2.5f, nitro = 1.0f, dist = 0, off = 0, lightF = 1;
static int lives = 3, invul = 0, endT = 0, animT = 0, steps = 0, spawnT = 0, shake = 0;
static uint32_t score = 0, best = 0, bonus = 0;
static bool boosting = false;

static inline uint16_t Lc(int r, int g, int b) { return rgb((int)(r * lightF), (int)(g * lightF), (int)(b * lightF)); }
static void pop(float x, float y, const char* s, uint16_t col) { for (auto &p : pops) if (p.t <= 0) { p.x = x; p.y = y; p.t = 36; p.col = col; strncpy(p.s, s, 11); p.s[11] = 0; return; } }

// --------------------------------------------------------------------------- voitures (vue de dessus, nez en haut)
// kind : 0 coupe GT, 1 supercar, 2 roadster, 3 berline, 4 camion (cabine + remorque), 5 fourgon
// Chaque modele = un profil de largeur (nez -> arriere) + des zones (pare-brise, toit, lunette) : une seule routine pour toutes les tailles (s = 1, 2, 3).
struct Model { int hh; float W; float pts[7][2]; int np; float ws0, ws1, rg0, rg1, gl; bool open; };
static const Model MD[6] = {
  {14, 7.0f, {{0, .60f}, {.06f, .78f}, {.20f, .96f}, {.45f, .92f}, {.72f, 1.0f}, {.94f, .88f}, {1, .74f}}, 7, .30f, .42f, .60f, .72f, .70f, false},     // coupe GT
  {16, 8.0f, {{0, .30f}, {.10f, .68f}, {.25f, .92f}, {.50f, .88f}, {.72f, 1.0f}, {.95f, .93f}, {1, .82f}}, 7, .33f, .44f, .58f, .68f, .56f, false},   // supercar
  {14, 7.0f, {{0, .55f}, {.08f, .78f}, {.22f, .95f}, {.50f, .88f}, {.75f, .98f}, {.95f, .86f}, {1, .72f}}, 7, .30f, .37f, .72f, .72f, .78f, true},    // roadster
  {14, 7.0f, {{0, .70f}, {.05f, .84f}, {.20f, .96f}, {.50f, .94f}, {.80f, .96f}, {.96f, .88f}, {1, .80f}}, 7, .27f, .38f, .62f, .74f, .72f, false},   // berline
  {14, 7.0f, {{0, .70f}, {.05f, .84f}, {.20f, .96f}, {.50f, .94f}, {.80f, .96f}, {.96f, .88f}, {1, .80f}}, 7, .27f, .38f, .62f, .74f, .72f, false},   // (camion : dessin dedie)
  {15, 7.2f, {{0, .74f}, {.04f, .90f}, {.20f, .98f}, {.50f, 1.0f}, {.80f, .98f}, {.97f, .94f}, {1, .90f}}, 7, .20f, .30f, .88f, .88f, .74f, false}    // fourgon
};
static float widthAt(const Model &m, float t) {
  for (int i = 1; i < m.np; i++) if (t <= m.pts[i][0]) { float a = m.pts[i - 1][0], b = m.pts[i][0]; float u = (t - a) / (b - a); return m.pts[i - 1][1] + (m.pts[i][1] - m.pts[i - 1][1]) * u; }
  return m.pts[m.np - 1][1];
}
static inline uint16_t darker(uint16_t c, int pct) { return mixc(c, 0, pct * 256 / 100); }
static inline uint16_t lighter(uint16_t c, int pct) { return mixc(c, 0xFFFF, pct * 256 / 100); }
static const uint16_t SHADOW_C = rgb(30, 32, 40);
static int sheenRow = -1;                                // reflet qui glisse sur la carrosserie (ecrans de choix)

static void drawGeneric(int cx, int cy, int kind, uint16_t col, int s, bool shadow, bool night, bool brake) {
  const Model &m = MD[kind];
  auto X = [&](float u) { return cx + (int)floorf(u * s + 0.5f); };
  auto Y = [&](float u) { return cy + (int)floorf(u * s + 0.5f); };
  auto FR = [&](float x0, float y0, float x1, float y1, uint16_t c) { int a = X(x0), b = Y(y0), w = X(x1) - a, h = Y(y1) - b; if (w < 1) w = 1; if (h < 1) h = 1; canvas.fillRect(a, b, w, h, c); };
  float hh = (float)m.hh; int L = m.hh * 2;
  uint16_t dark = darker(col, 42), side = darker(col, 20), light = lighter(col, 32), roofC = lighter(col, 18);
  uint16_t glass = rgb(20, 32, 54), glassHi = rgb(135, 175, 215), tire = rgb(12, 12, 16), rim = rgb(140, 144, 154), lamp = rgb(255, 246, 190), tail = rgb(228, 28, 30);
  if (shadow) for (int i = 0; i < L; i++) { float w = widthAt(m, (i + 0.5f) / L) * m.W; FR(-w + 1.6f, -hh + i + 1.6f, w + 1.6f, -hh + i + 2.6f, SHADOW_C); }
  // roues (sous la caisse)
  static const float WT[2] = {0.20f, 0.78f};
  for (int k = 0; k < 2; k++) {
    float t = WT[k], w = widthAt(m, t) * m.W, yc = -hh + t * L, tl = (kind == 1 ? 3.4f : 2.9f);
    FR(-w - 1.0f, yc - tl, -w + 1.8f, yc + tl, tire); FR(w - 1.8f, yc - tl, w + 1.0f, yc + tl, tire);
    if (s >= 2) { FR(-w - 0.9f, yc - tl + 0.6f, -w - 0.3f, yc + tl - 0.6f, rim); FR(w + 0.3f, yc - tl + 0.6f, w + 0.9f, yc + tl - 0.6f, rim); }
  }
  // carrosserie, tranche par tranche (nez -> arriere)
  for (int i = 0; i < L; i++) {
    float t = (i + 0.5f) / L, w = widthAt(m, t) * m.W, y0 = -hh + i, y1 = y0 + 1;
    FR(-w, y0, w, y1, dark);                                    // contour sombre
    FR(-w + 1.0f, y0, w - 1.0f, y1, col);
    FR(-w + 1.0f, y0, -w * 0.58f, y1, side); FR(w * 0.58f, y0, w - 1.0f, y1, side);      // flancs plus fonces
    FR(-w * 0.20f, y0, w * 0.20f, y1, light);                   // reflet central (capot / coffre)
    if (s >= 2 && sheenRow >= 0 && abs(i - sheenRow) <= 1) FR(-w + 1.0f, y0, w - 1.0f, y1, lighter(col, 55));
    float gw = w * m.gl;
    if (t >= m.ws0 && t < m.ws1) {                              // pare-brise
      float p = (t - m.ws0) / (m.ws1 - m.ws0); gw = w * m.gl * (0.82f + 0.18f * p);
      FR(-gw, y0, gw, y1, glass);
      float xs = -gw + p * gw * 1.5f; if (xs < gw - 1.4f) FR(xs, y0, xs + 1.5f, y1, glassHi);
    } else if (t >= m.ws1 && t < m.rg0) {
      if (m.open) {                                             // cockpit ouvert
        FR(-gw, y0, gw, y1, rgb(22, 22, 28));
      } else {                                                  // toit : plus clair que la caisse
        FR(-gw, y0, gw, y1, roofC); FR(-gw, y0, -gw + 0.6f, y1, dark); FR(gw - 0.6f, y0, gw, y1, dark);
      }
    } else if (t >= m.rg0 && t < m.rg1 && !m.open && m.rg1 > m.rg0) {   // lunette arriere
      float p = (t - m.rg0) / (m.rg1 - m.rg0); gw = w * m.gl * (1.0f - 0.18f * p);
      FR(-gw, y0, gw, y1, glass);
      if (p < 0.5f) FR(-gw + 0.6f, y0, -gw + 2.2f, y1, glassHi);
    }
  }
  // details par modele
  if (m.open) {                                                 // roadster : sieges, volant, pilote, arceaux
    float cy0 = -hh + m.ws1 * L, cy1 = -hh + m.rg0 * L, w = m.W * 0.62f;
    FR(-w + 0.6f, cy0 + 1.4f, -0.5f, cy1 - 1.2f, rgb(70, 70, 82)); FR(0.5f, cy0 + 1.4f, w - 0.6f, cy1 - 1.2f, rgb(70, 70, 82));
    FR(-w + 0.6f, cy1 - 2.4f, -0.5f, cy1 - 1.2f, rgb(96, 96, 110)); FR(0.5f, cy1 - 2.4f, w - 0.6f, cy1 - 1.2f, rgb(96, 96, 110));
    FR(-w + 1.0f, cy0 + 0.2f, -0.6f, cy0 + 0.9f, rgb(10, 10, 12));                   // volant
    float hx = -w * 0.5f, hy = cy1 - 3.4f;
    canvas.fillCircle(X(hx), Y(hy), s >= 2 ? s + 1 : 2, rgb(238, 200, 160)); canvas.fillCircle(X(hx), Y(hy - 0.2f), s >= 2 ? s : 1, rgb(60, 40, 30));
    FR(-w + 0.4f, cy1 + 0.2f, -w + 1.6f, cy1 + 1.4f, dark); FR(w - 1.6f, cy1 + 0.2f, w - 0.4f, cy1 + 1.4f, dark);   // arceaux
  }
  if (kind == 1) {                                              // supercar : prises d'air, aile arriere, nez effile
    float wA = widthAt(m, 0.5f) * m.W;
    FR(-wA + 0.8f, -hh + 0.40f * L, -wA + 2.2f, -hh + 0.52f * L, rgb(10, 10, 12)); FR(wA - 2.2f, -hh + 0.40f * L, wA - 0.8f, -hh + 0.52f * L, rgb(10, 10, 12));
    FR(-1.0f, -hh + 0.10f * L, 1.0f, -hh + 0.30f * L, rgb(245, 245, 250));                         // bande de course
    FR(-3.2f, -hh + 0.19f * L, -1.4f, -hh + 0.25f * L, rgb(10, 10, 12)); FR(1.4f, -hh + 0.19f * L, 3.2f, -hh + 0.25f * L, rgb(10, 10, 12));
    float wy = hh - 3.6f;                                       // aile
    FR(-m.W - 1.8f, wy, m.W + 1.8f, wy + 2.2f, rgb(16, 16, 20)); FR(-m.W - 1.8f, wy, m.W + 1.8f, wy + 0.6f, rgb(70, 70, 80));
    FR(-m.W - 2.0f, wy - 0.4f, -m.W - 1.0f, wy + 2.8f, col); FR(m.W + 1.0f, wy - 0.4f, m.W + 2.0f, wy + 2.8f, col);
    FR(-1.2f, wy + 2.2f, 1.2f, hh - 1.0f, rgb(16, 16, 20));
  } else if (kind == 0) {                                       // coupe : capot a nervure, echappements
    FR(-0.3f, -hh + 1.0f, 0.3f, -hh + 0.30f * L, lighter(col, 55));
    FR(-3.0f, hh - 0.2f, -2.0f, hh + 0.7f, rgb(70, 72, 80)); FR(2.0f, hh - 0.2f, 3.0f, hh + 0.7f, rgb(70, 72, 80));
  } else if (kind == 3) {                                       // berline : coffre marque, antenne
    FR(-0.25f, -hh + 0.74f * L, 0.25f, -hh + 0.97f * L, dark);                           // joint du coffre
  } else if (kind == 5) {                                       // fourgon : toit nervure, portes arriere
    float yA = -hh + m.ws1 * L, yB = hh - 1.0f, w = widthAt(m, 0.5f) * m.W * 0.78f;
    for (float y = yA + 2.0f; y < yB; y += 3.2f) FR(-w, y, w, y + 0.5f, lighter(col, 45));
    FR(-0.25f, hh - 4.5f, 0.25f, hh, darker(col, 55));
    FR(-m.W + 0.6f, -hh + 0.52f * L, -m.W + 1.2f, hh - 5.0f, darker(col, 45));
  }
  // retros, feux, plaque
  float wm = widthAt(m, m.ws0 + 0.03f) * m.W, ym = -hh + (m.ws0 + 0.03f) * L;
  FR(-wm - 1.5f, ym, -wm + 0.3f, ym + 1.2f, dark); FR(wm - 0.3f, ym, wm + 1.5f, ym + 1.2f, dark);
  float wn = widthAt(m, 0.02f) * m.W, wt2 = widthAt(m, 0.985f) * m.W;
  if (night) { FR(-wn * 0.85f - 0.6f, -hh - 0.3f, -wn * 0.15f, -hh + 1.5f, rgb(255, 255, 235)); FR(wn * 0.15f, -hh - 0.3f, wn * 0.85f + 0.6f, -hh + 1.5f, rgb(255, 255, 235)); }
  else { FR(-wn * 0.85f, -hh + 0.2f, -wn * 0.30f, -hh + 1.2f, lamp); FR(wn * 0.30f, -hh + 0.2f, wn * 0.85f, -hh + 1.2f, lamp); }
  uint16_t tc = brake ? rgb(255, 70, 60) : tail;
  if (kind != 1) { FR(-wt2 * 0.88f, hh - 1.3f, -wt2 * 0.40f, hh - 0.1f, tc); FR(wt2 * 0.40f, hh - 1.3f, wt2 * 0.88f, hh - 0.1f, tc); }
  else { FR(-wt2 * 0.85f, hh - 1.0f, -wt2 * 0.35f, hh - 0.1f, tc); FR(wt2 * 0.35f, hh - 1.0f, wt2 * 0.85f, hh - 0.1f, tc); }
  if (s >= 2) FR(-1.6f, hh - 2.2f, 1.6f, hh - 1.2f, rgb(238, 238, 230));         // plaque
  if (night || brake) { uint16_t gl = brake ? rgb(120, 20, 20) : rgb(70, 14, 14); canvas.drawFastHLine(X(-wt2 * 0.88f), Y(hh + 0.9f), X(-wt2 * 0.40f) - X(-wt2 * 0.88f) + 1, gl); canvas.drawFastHLine(X(wt2 * 0.40f), Y(hh + 0.9f), X(wt2 * 0.88f) - X(wt2 * 0.40f) + 1, gl); }
}

static void drawTruck(int cx, int cy, uint16_t col, int s, bool shadow, bool night) {
  auto X = [&](float u) { return cx + (int)floorf(u * s + 0.5f); };
  auto Y = [&](float u) { return cy + (int)floorf(u * s + 0.5f); };
  auto FR = [&](float x0, float y0, float x1, float y1, uint16_t c) { int a = X(x0), b = Y(y0), w = X(x1) - a, h = Y(y1) - b; if (w < 1) w = 1; if (h < 1) h = 1; canvas.fillRect(a, b, w, h, c); };
  uint16_t tire = rgb(12, 12, 16), box = rgb(218, 222, 228), boxD = rgb(150, 156, 168), glass = rgb(20, 32, 54), glassHi = rgb(135, 175, 215), dk = darker(col, 42);
  if (shadow) { FR(-5.0f, -17.5f, 9.0f, -6.0f, SHADOW_C); FR(-5.0f, -6.0f, 9.2f, 20.0f, SHADOW_C); }
  for (int k = 0; k < 4; k++) { static const float AY[4] = {-14.0f, 8.5f, 12.5f, 16.5f}; float y = AY[k], w = k == 0 ? 6.4f : 7.6f; FR(-w - 0.9f, y - 2.4f, -w + 1.6f, y + 2.4f, tire); FR(w - 1.6f, y - 2.4f, w + 0.9f, y + 2.4f, tire); }
  FR(-4.0f, -8.0f, 4.0f, -6.0f, rgb(30, 30, 36));                                     // sellette
  FR(-7.6f, -7.0f, 7.6f, 19.5f, boxD); FR(-6.6f, -7.0f, 6.6f, 19.5f, box);            // remorque
  for (float y = -3.0f; y < 19.0f; y += 4.0f) FR(-6.6f, y, 6.6f, y + 0.6f, rgb(188, 194, 204));
  FR(-6.6f, 2.0f, 6.6f, 6.0f, col); FR(-6.6f, 2.0f, 6.6f, 2.7f, lighter(col, 40));   // bandeau couleur
  FR(-0.25f, 12.5f, 0.25f, 19.5f, boxD);                                              // portes arriere
  FR(-7.0f, 18.3f, -5.0f, 19.5f, rgb(228, 28, 30)); FR(5.0f, 18.3f, 7.0f, 19.5f, rgb(228, 28, 30));
  if (night) { canvas.drawFastHLine(X(-7.0f), Y(20.4f), X(-5.0f) - X(-7.0f) + 1, rgb(90, 16, 16)); canvas.drawFastHLine(X(5.0f), Y(20.4f), X(7.0f) - X(5.0f) + 1, rgb(90, 16, 16)); }
  canvas.fillRoundRect(X(-6.4f), Y(-19.5f), (X(6.4f) - X(-6.4f)), (Y(-7.0f) - Y(-19.5f)), 2 * s, dk);    // cabine
  canvas.fillRoundRect(X(-5.6f), Y(-19.0f), (X(5.6f) - X(-5.6f)), (Y(-7.4f) - Y(-19.0f)), 2 * s, col);
  FR(-5.0f, -17.2f, 5.0f, -14.6f, glass); FR(-3.0f, -17.2f, -1.6f, -14.6f, glassHi);        // pare-brise
  FR(-4.4f, -13.6f, 4.4f, -9.0f, lighter(col, 22)); FR(-1.0f, -13.4f, 1.0f, -9.2f, lighter(col, 45));   // toit + deflecteur
  FR(-8.0f, -16.2f, -6.2f, -14.4f, dk); FR(6.2f, -16.2f, 8.0f, -14.4f, dk);                // retros
  FR(-5.4f, -19.5f, -2.6f, -18.5f, night ? rgb(255, 255, 235) : rgb(255, 246, 190)); FR(2.6f, -19.5f, 5.4f, -18.5f, night ? rgb(255, 255, 235) : rgb(255, 246, 190));
}

static void drawCar(int cx, int cy, int kind, uint16_t col, int s, bool shadow, bool brake = false) {
  bool night = lightF < 0.66f;
  if (lightF < 0.9f) col = mixc(col, 0, (int)((1.0f - lightF) * 160));
  if (kind == 4) drawTruck(cx, cy, col, s, shadow, night); else drawGeneric(cx, cy, kind, col, s, shadow, night, brake);
}
static int hhOf(int kind) { return kind == 4 ? 19 : (kind == 1 ? 16 : (kind == 5 ? 15 : 14)); }
static int hwOf(int kind) { return kind == 4 ? 8 : 7; }

// --------------------------------------------------------------------------- jeu
static void reset() {
  px = 120; pv = 0; spd = 2.5f; nitro = 1.0f; dist = 0; off = 0; lives = difi(5, 3, 2); invul = 0; endT = 0; steps = 0; spawnT = 40; score = 0; bonus = 0; shake = 0;
  for (auto &v : veh) v.on = false;
  for (auto &p : pops) p.t = 0;
  for (int i = 0; i < 14; i++) deco[i] = {(float)((i & 1) ? rnd(196, 232) : rnd(8, 44)), (float)(i * 10), (uint8_t)rnd(0, 2)};
}
void begin() {
  { Preferences q; if (q.begin("race", true)) { carSel = q.getInt("car", 0) % 3; colSel = q.getInt("col", 0) % 7; q.end(); } }   // voiture et couleur : communes a toutes les difficultes
  pr.begin(dns("race"), false); best = pr.getInt("best", 0);
  reset(); state = SEL_CAR; last = 0; acc = 0;
}
static void gameOver() {
  state = OVER; endT = 0;
  trophy::record(trophy::T_RACING, score); if (score > best) { best = score; pr.putInt("best", best); }
  sfx(260, 120); sfx(110, 320, false);
}
static const float LANE_V[3] = {1.0f, 1.4f, 1.8f};     // chaque voie a sa vitesse : les voitures ne se rattrapent jamais
static bool laneFree(int l) {
  for (auto &v : veh) if (v.on && fabsf(v.x - LANE[l]) < 18 && v.y < 80) return false;       // pas de voiture juste devant le point d'apparition
  return true;
}
static void spawnRow() {
  int mask = 0, n = (rnd(0, 99) < 18) ? 2 : 1;
  for (int tries = 0; tries < 6 && __builtin_popcount(mask) < n; tries++) { int l = rnd(0, 2); if (laneFree(l)) mask |= 1 << l; }
  for (int l = 0; l < 3; l++) if (mask & (1 << l)) {
    for (auto &v : veh) if (!v.on) {
      uint8_t k = rnd(0, 99) < 8 ? 4 : (uint8_t)rnd(0, 3);
      v = {(float)LANE[l] + (int)rnd(0, 4) - 2, -26.f, LANE_V[l], k, (uint8_t)rnd(0, 6), true, false};
      break;
    }
  }
}

static void step() {
  animT++;
  for (auto &p : pops) if (p.t > 0) { p.t--; p.y -= 0.5f; }
  if (shake > 0) shake--;
  if (state == SEL_CAR || state == SEL_COLOR || state == PAUSED) return;
  if (state == OVER) { endT++; spd *= 0.97f; off += spd; for (auto &v : veh) if (v.on) { v.y += spd - v.v; } return; }
  tilt::update();
  const CarDef &cd = CARS[carSel];
  float steerM = 0.7f + 0.12f * cd.handStars, topM = 0.95f + 0.06f * cd.speedStars;
  float t = ctrl::x(); if (fabsf(t) < 0.06f) t = 0; else t = t > 0 ? t - 0.06f : t + 0.06f;
  pv += (t * 4.6f * steerM - pv) * 0.25f;
  px += pv;
  if (px < ROAD_L + 8) { px = ROAD_L + 8; pv = 0; spd *= 0.985f; }
  if (px > ROAD_R - 8) { px = ROAD_R - 8; pv = 0; spd *= 0.985f; }
  boosting = (g_btnCtrl ? (M5.BtnA.isPressed() && M5.BtnB.isPressed()) : M5.BtnA.isPressed()) && nitro > 0.02f;   // boutons : A+B = nitro
  if (boosting) nitro -= 0.006f; else if (nitro < 1.0f) nitro += 0.0015f;
  steps++;
  float vmax = (2.6f + steps * 0.0007f); if (vmax > 4.3f) vmax = 4.3f;
  vmax *= topM / 1.1f;
  float target = (vmax + (boosting ? 1.5f : 0.0f)) * dif(0.9f, 1.0f, 1.1f);
  spd += (target - spd) * 0.04f;
  dist += spd; off += spd;
  score = (uint32_t)(dist / 10) + bonus;
  lightF = 0.72f + 0.28f * cosf(dist * 0.0004f);
  if (invul > 0) invul--;
  if (--spawnT <= 0) { spawnRow(); int T = (int)(70 - spd * 5.0f); spawnT = T < 38 ? 38 : T; }
  for (auto &d : deco) { d.y += spd; if (d.y > SCR_H + 14) { d.y -= SCR_H + 40; d.x = (rnd(0, 1)) ? rnd(196, 232) : rnd(8, 44); d.t = rnd(0, 2); } }
  for (auto &v : veh) if (v.on) {
    v.y += spd - v.v;
    if (v.y > SCR_H + 30) { v.on = false; continue; }
    int hh = hhOf(v.kind), hw = hwOf(v.kind);
    float dx = fabsf(v.x - px), dy = fabsf(v.y - PY);
    if (!invul && dx < hw + 6 && dy < hh + 11) {
      v.on = false; if (--lives <= 0) { gameOver(); return; }
      invul = 90; shake = 14; spd *= 0.55f; pop(px, PY - 22, "CRASH", rgb(255, 90, 60)); sfx(150, 100); sfx(90, 180, false);
      continue;
    }
    if (!v.passed && v.y > PY + hh + 4) {
      v.passed = true;
      if (dx < 21) { bonus += 5; pop(px, PY - 22, L("CLOSE +5","FROLE +5"), rgb(255, 230, 90)); sfx(1500, 30); sfx(1900, 40, false); }
    }
  }
}

// --------------------------------------------------------------------------- dessin
static void drawTree(int x, int y, int t) {
  int r = 6 + t * 2;
  canvas.fillCircle(x + 3, y + 3, r, Lc(25, 70, 32));
  canvas.fillCircle(x, y, r, Lc(34, 105, 44)); canvas.fillCircle(x - 2, y - 2, r - 3, Lc(48, 135, 58)); canvas.fillCircle(x - 3, y - 3, r / 3, Lc(90, 175, 90));
}
static void drawRoad() {
  int o = (int)off;
  for (int y = 0; y < SCR_H; y++) {
    bool band = (((y - o) % 32 + 32) % 32) < 16;
    uint16_t g = band ? Lc(50, 128, 56) : Lc(44, 117, 50);
    canvas.drawFastHLine(0, y, ROAD_L - 8, g); canvas.drawFastHLine(ROAD_R + 8, y, SCR_W - ROAD_R - 8, g);
  }
  canvas.fillRect(ROAD_L, 0, ROAD_R - ROAD_L, SCR_H, Lc(70, 72, 82));
  for (int y = 0; y < SCR_H; y++) {                      // bordures rouge / blanc
    bool rb = (((y - o) % 16 + 16) % 16) < 8;
    uint16_t c = rb ? Lc(225, 40, 40) : Lc(235, 235, 235);
    canvas.drawFastHLine(ROAD_L - 8, y, 6, c); canvas.drawFastHLine(ROAD_R + 2, y, 6, c);
    canvas.drawFastHLine(ROAD_L - 2, y, 2, Lc(235, 235, 235)); canvas.drawFastHLine(ROAD_R, y, 2, Lc(235, 235, 235));
  }
  for (int l = 0; l < 2; l++) { int lx = (LANE[l] + LANE[l + 1]) / 2; for (int y = -32; y < SCR_H; y += 28) { int yy = y + (o % 28); canvas.fillRect(lx - 1, yy, 2, 14, Lc(220, 220, 225)); } }
  // taches d'huile / usure de l'asphalte (fixes par rapport a la route)
  for (int i = 0; i < 6; i++) { int yy = ((i * 53) + o) % SCR_H; canvas.drawFastHLine(ROAD_L + 6 + (i * 37) % 100, yy, 7 + i, Lc(62, 64, 74)); }
}
static void render() {
  canvas.fillScreen(0);
  if (state == SEL_CAR || state == SEL_COLOR) {
    for (int y = 0; y < SCR_H; y += 3) canvas.fillRect(0, y, SCR_W, 3, mixc(rgb(8, 10, 22), rgb(38, 42, 70), y * 256 / SCR_H));
    canvas.fillEllipse(120, 100, 80, 15, rgb(52, 56, 84)); canvas.fillEllipse(120, 100, 64, 10, rgb(70, 76, 110));
    drawCar(120, 72, carSel, COLS[colSel], 2, false);
    canvas.fillTriangle(14, 72, 28, 60, 28, 84, rgb(255, 210, 80)); canvas.fillTriangle(226, 72, 212, 60, 212, 84, rgb(255, 210, 80));
    if (state == SEL_CAR) {
      txt(CARS[carSel].name, SCR_W / 2, 4, &fonts::Font4, TFT_WHITE, textdatum_t::top_center);
      txt(L("SPEED","VITESSE"), 6, 112, &fonts::Font0, rgb(150, 160, 210)); txt(L("HANDLING","MANIAB."), 6, 124, &fonts::Font0, rgb(150, 160, 210));
      for (int i = 0; i < 5; i++) { canvas.fillRect(56 + i * 8, 111, 6, 8, i < CARS[carSel].speedStars ? rgb(255, 210, 80) : rgb(60, 64, 90)); canvas.fillRect(56 + i * 8, 123, 6, 8, i < CARS[carSel].handStars ? rgb(90, 220, 255) : rgb(60, 64, 90)); }
    } else {
      txt((g_lang ? CNAME_FR : CNAME_EN)[colSel], SCR_W / 2, 4, &fonts::Font4, TFT_WHITE, textdatum_t::top_center);
      for (int i = 0; i < 7; i++) { int cx = 120 + (i - 3) * 24; canvas.fillCircle(cx, 121, i == colSel ? 7 : 5, COLS[i]); if (i == colSel) canvas.drawCircle(cx, 121, 9, TFT_WHITE); }
    }
    return;
  }
  drawRoad();
  for (auto &d : deco) drawTree((int)d.x, (int)d.y, d.t);
  // phares la nuit
  if (lightF < 0.66f && state != OVER) {
    int pxi = (int)px;
    canvas.fillTriangle(pxi - 5, PY - 14, pxi - 13, PY - 60, pxi - 1, PY - 60, mixc(Lc(70, 72, 82), rgb(255, 250, 190), 70));
    canvas.fillTriangle(pxi + 5, PY - 14, pxi + 1, PY - 60, pxi + 13, PY - 60, mixc(Lc(70, 72, 82), rgb(255, 250, 190), 70));
  }
  // vehicules (du plus haut au plus bas)
  for (auto &v : veh) if (v.on) drawCar((int)v.x, (int)v.y, v.kind, COLS[v.col], 1, true);
  if (state != OVER || (animT & 8)) {
    if (invul == 0 || (animT & 4)) {
      int sx = (int)px + ((shake > 0) ? (int)rnd(0, 2) - 1 : 0);
      if (boosting) { int f = 4 + (animT & 3) * 3; canvas.fillTriangle(sx - 4, PY + 14, sx + 4, PY + 14, sx, PY + 15 + f, rgb(80, 170, 255)); canvas.fillTriangle(sx - 2, PY + 14, sx + 2, PY + 14, sx, PY + 14 + f / 2, rgb(255, 255, 255)); }
      drawCar(sx, PY, carSel, COLS[colSel], 1, true);
    }
  }
  if (boosting) for (int i = 0; i < 6; i++) { int lx = ROAD_L + 10 + (i * 41 + animT * 7) % (ROAD_R - ROAD_L - 20), ly = (i * 29 + animT * 11) % SCR_H; canvas.drawFastVLine(lx, ly, 10, rgb(200, 210, 240)); }
  for (auto &p : pops) if (p.t > 0) ui::popText((int)p.x, (int)p.y, p.s, p.col);
  char b[24];
  snprintf(b, sizeof b, "%lu", (unsigned long)score); txtO(b, 5, 2, &fonts::Font4, TFT_WHITE, rgb(10, 10, 20));
  for (int i = 0; i < lives; i++) { int hx = SCR_W - 10 - i * 12; canvas.fillTriangle(hx - 4, 6, hx + 4, 6, hx, 13, rgb(235, 50, 60)); canvas.fillCircle(hx - 2, 5, 2, rgb(235, 50, 60)); canvas.fillCircle(hx + 2, 5, 2, rgb(235, 50, 60)); }
  snprintf(b, sizeof b, "%d", (int)(spd * 45)); txtO(b, SCR_W - 6, 108, &fonts::Font4, TFT_WHITE, rgb(10, 10, 20), textdatum_t::top_right); txtO("km/h", SCR_W - 6, 126, &fonts::Font0, TFT_WHITE, rgb(10, 10, 20), textdatum_t::top_right);
  canvas.drawRect(5, 122, 52, 8, rgb(230, 230, 240)); canvas.fillRect(6, 123, (int)(50 * nitro), 6, boosting ? rgb(80, 180, 255) : rgb(60, 130, 230));
  if (state == PAUSED) ui::pause();
  if (state == OVER) ui::gameOver(L("CRASH!", "ACCIDENT !"), score, best, score > 0 && score >= best, endT > 40 ? L("M5 retry  B garage", "M5 rejouer  B garage") : L("...", "..."));
}

void loop() {
  uint32_t now = micros();
  if (last == 0) last = now;
  acc += now - last; last = now; if (acc > 100000) acc = 100000;
  bool a = M5.BtnA.wasPressed(), p = M5.BtnPWR.wasPressed(), b = M5.BtnB.wasPressed();
  switch (state) {
    case SEL_CAR:
      if (b) { carSel = (carSel + 1) % 3; sfx(1200, 20); } else if (p) { carSel = (carSel + 2) % 3; sfx(1000, 20); }
      else if (a) { state = SEL_COLOR; sfx(880, 40); }
      break;
    case SEL_COLOR:
      if (b) { colSel = (colSel + 1) % 7; sfx(1200, 20); } else if (p) { colSel = (colSel + 6) % 7; sfx(1000, 20); }
      else if (a) { { Preferences q; q.begin("race", false); q.putInt("car", carSel); q.putInt("col", colSel); q.end(); } reset(); state = PLAY; sfx(660, 40); sfx(990, 60, false); }
      break;
    case PLAY: if (p) { state = PAUSED; sfx(440, 50); } break;
    case PAUSED: if (p || a) { state = PLAY; sfx(660, 50); } break;
    case OVER:
      if (endT > 40) { if (a) { reset(); state = PLAY; sfx(660, 40); } else if (b) { reset(); state = SEL_CAR; sfx(880, 40); } }
      break;
  }
  while (acc >= STEP_US) { step(); acc -= STEP_US; }
  render();
  bgm::run(13, state == PLAY);
  present(0, 0);
}

void icon(int x, int y) {
  canvas.fillRoundRect(x, y, 64, 64, 6, Lc(70, 72, 82));
  canvas.fillRect(x, y, 6, 64, rgb(50, 128, 56)); canvas.fillRect(x + 58, y, 6, 64, rgb(50, 128, 56));
  for (int i = 0; i < 4; i++) { canvas.fillRect(x + 6, y + 4 + i * 16, 2, 8, rgb(225, 40, 40)); canvas.fillRect(x + 56, y + 4 + i * 16, 2, 8, rgb(225, 40, 40)); canvas.fillRect(x + 31, y + 2 + i * 16, 2, 8, rgb(220, 220, 225)); }
  lightF = 1;
  drawCar(x + 46, y + 22, 3, rgb(240, 240, 245), 1, false);
  drawCar(x + 22, y + 40, 1, rgb(222, 28, 40), 1, true);
}

}  // namespace race
