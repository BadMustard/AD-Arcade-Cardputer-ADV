// ============================================================================
//  TETRIS - champ 10x20, sac de 7, rotation SRS avec wall kicks, piece fantome,
//  2 pieces suivantes, delai de verrouillage, animation de lignes, niveaux, record.
//  Inclinaison gauche/droite = deplacer (repetition auto) ; incliner vers l'avant = chute douce
//  M5 (A) = rotation ; B (cote) = chute instantanee ; HAUT (PWR) = pause
// ============================================================================
#include "common.h"

namespace tetris {

static const uint32_t STEP_US = 20000;
static const int FW = 10, FH = 20, CS = 6, FX = 90, FY = 7;
enum State { TITLE, PLAY, CLEAR, PAUSED, OVER };
static State state = TITLE;
static Preferences pr;
static uint32_t last = 0, acc = 0;

// pieces : I O T S Z J L ; couleurs
static uint16_t PCOL[9];
static const int8_t BASE[7][4][2] = {
  {{0,1},{1,1},{2,1},{3,1}},   // I (boite 4)
  {{1,0},{2,0},{1,1},{2,1}},   // O
  {{1,0},{0,1},{1,1},{2,1}},   // T (boite 3)
  {{1,0},{2,0},{0,1},{1,1}},   // S
  {{0,0},{1,0},{1,1},{2,1}},   // Z
  {{0,0},{0,1},{1,1},{2,1}},   // J
  {{2,0},{0,1},{1,1},{2,1}},   // L
};
static const int BOX[7] = {4, 4, 3, 3, 3, 3, 3};
static int8_t SH[7][4][4][2];  // [piece][rotation][cell][x,y]
// wall kicks SRS (rotation horaire), convention du wiki (y vers le haut) ; on inverse y au moment de l'appliquer
static const int8_t KJ[4][5][2] = {
  {{0,0},{-1,0},{-1,1},{0,-2},{-1,-2}},
  {{0,0},{1,0},{1,-1},{0,2},{1,2}},
  {{0,0},{1,0},{1,1},{0,-2},{1,-2}},
  {{0,0},{-1,0},{-1,-1},{0,2},{-1,2}},
};
static const int8_t KI[4][5][2] = {
  {{0,0},{-2,0},{1,0},{-2,-1},{1,2}},
  {{0,0},{-1,0},{2,0},{-1,2},{2,-1}},
  {{0,0},{2,0},{-1,0},{2,1},{-1,-2}},
  {{0,0},{1,0},{-2,0},{1,-2},{-2,1}},
};

static uint8_t grid[FH][FW];
struct Piece { int t, r, x, y; };
static Piece cur;
static int bag[7], bagN = 0, nextq[3];
static int level = 1, lines = 0, combo = -1;
static uint32_t score = 0, best = 0;
static int gravT = 0, lockT = 0, lockResets = 0, dasT = 0, lastDir = 0;
static int clearT = 0, clearN = 0, clearRows[4], overT = 0, animT = 0, shake = 0, bannerT = 0, lvlT = 0;
static const char* banner = "";
static int appear = 0;

struct Part { float x, y, vx, vy; int life; uint16_t c; };
static Part parts[48];
struct Bg { float x, y, v; uint8_t t; };
static Bg bgs[14];

static void spark(float x, float y, uint16_t c, int n, float spd) {
  for (int i = 0; i < n; i++) for (auto &p : parts) if (p.life <= 0) {
    float a = rnd(0, 628) / 100.f, v = rnd(3, 10) / 10.f * spd;
    p = {x, y, cosf(a) * v, sinf(a) * v - 0.4f, (int)rnd(14, 30), c}; break;
  }
}

static const int GRAV[20] = {40, 36, 32, 28, 23, 19, 15, 11, 7, 5, 4, 4, 4, 3, 3, 3, 2, 2, 2, 1};

static void buildShapes() {
  for (int t = 0; t < 7; t++) {
    for (int c = 0; c < 4; c++) { SH[t][0][c][0] = BASE[t][c][0]; SH[t][0][c][1] = BASE[t][c][1]; }
    for (int r = 1; r < 4; r++) for (int c = 0; c < 4; c++) {
      if (t == 1) { SH[t][r][c][0] = SH[t][0][c][0]; SH[t][r][c][1] = SH[t][0][c][1]; continue; }
      int x = SH[t][r - 1][c][0], y = SH[t][r - 1][c][1];
      SH[t][r][c][0] = BOX[t] - 1 - y; SH[t][r][c][1] = x;
    }
  }
  PCOL[1] = rgb(0, 215, 230); PCOL[2] = rgb(250, 212, 40); PCOL[3] = rgb(176, 76, 224); PCOL[4] = rgb(84, 212, 72);
  PCOL[5] = rgb(232, 62, 62); PCOL[6] = rgb(62, 104, 242); PCOL[7] = rgb(250, 142, 40); PCOL[8] = rgb(110, 116, 140);
}

static int pull() {
  if (bagN == 0) {
    for (int i = 0; i < 7; i++) bag[i] = i;
    for (int i = 6; i > 0; i--) { int j = rnd(0, i); int t = bag[i]; bag[i] = bag[j]; bag[j] = t; }
    bagN = 7;
  }
  return bag[--bagN];
}

static bool collide(int t, int r, int px, int py) {
  for (int c = 0; c < 4; c++) {
    int x = px + SH[t][r][c][0], y = py + SH[t][r][c][1];
    if (x < 0 || x >= FW || y >= FH) return true;
    if (y >= 0 && grid[y][x]) return true;
  }
  return false;
}
static bool grounded() { return collide(cur.t, cur.r, cur.x, cur.y + 1); }

static bool spawn() {
  cur.t = nextq[0]; nextq[0] = nextq[1]; nextq[1] = nextq[2]; nextq[2] = pull();
  cur.r = 0; cur.x = 3; cur.y = cur.t == 0 ? -1 : 0;
  gravT = 0; lockT = 0; lockResets = 0; appear = 6;
  return !collide(cur.t, cur.r, cur.x, cur.y);
}

static void reset() {
  memset(grid, 0, sizeof grid);
  bagN = 0; for (int i = 0; i < 3; i++) nextq[i] = pull();
  level = 1; lines = 0; score = 0; combo = -1; dasT = 0; lastDir = 0; clearT = 0; overT = 0; shake = 0; bannerT = 0; lvlT = 0;
  for (auto &p : parts) p.life = 0;
  spawn();
}

void begin() {
  pr.begin(dns("tetris"), false); best = pr.getInt("best", 0);
  tilt::load(); buildShapes();
  for (auto &b : bgs) b = {(float)rnd(0, SCR_W), (float)rnd(0, SCR_H), rnd(3, 10) / 20.f, (uint8_t)rnd(1, 7)};
  reset(); state = TITLE; last = 0; acc = 0; animT = 0;
}

static bool tryMove(int dx, int dy) {
  if (collide(cur.t, cur.r, cur.x + dx, cur.y + dy)) return false;
  cur.x += dx; cur.y += dy;
  if (grounded() && lockResets < 15) { lockT = 0; lockResets++; }
  return true;
}
static bool rotate() {
  if (cur.t == 1) return false;
  const int8_t(*K)[2] = cur.t == 0 ? KI[cur.r] : KJ[cur.r];
  int nr = (cur.r + 1) & 3;
  for (int k = 0; k < 5; k++) {
    int nx = cur.x + K[k][0], ny = cur.y - K[k][1];
    if (!collide(cur.t, nr, nx, ny)) {
      cur.r = nr; cur.x = nx; cur.y = ny;
      if (grounded() && lockResets < 15) { lockT = 0; lockResets++; }
      return true;
    }
  }
  return false;
}
static int ghostY() { int y = cur.y; while (!collide(cur.t, cur.r, cur.x, y + 1)) y++; return y; }

static void gameOver() {
  state = OVER; overT = 0;
  trophy::record(trophy::T_TETRIS, score); if (score > best) { best = score; pr.putInt("best", best); }
  sfx(300, 120); sfx(180, 160, false); sfx(90, 320, false);
}

static void lockPiece() {
  for (int c = 0; c < 4; c++) {
    int x = cur.x + SH[cur.t][cur.r][c][0], y = cur.y + SH[cur.t][cur.r][c][1];
    if (y >= 0 && y < FH && x >= 0 && x < FW) grid[y][x] = cur.t + 1;
  }
  sfx(200, 30);
  clearN = 0;
  for (int y = 0; y < FH; y++) { bool full = true; for (int x = 0; x < FW; x++) if (!grid[y][x]) { full = false; break; } if (full && clearN < 4) clearRows[clearN++] = y; }
  if (clearN) {
    state = CLEAR; clearT = 0;
    for (int i = 0; i < clearN; i++) for (int x = 0; x < FW; x++) spark(FX + x * CS + 3, FY + clearRows[i] * CS + 3, PCOL[grid[clearRows[i]][x]], 1, 1.4f);
    static const char* NM[5] = {"", "SINGLE", "DOUBLE", "TRIPLE", "TETRIS!"};
    banner = NM[clearN]; bannerT = 60;
    if (clearN == 4) { sfx(660, 70); sfx(880, 70, false); sfx(1320, 70, false); sfx(1760, 160, false); shake = 10; }
    else { sfx(520, 60); sfx(780, 70, false); if (clearN > 1) sfx(1040, 90, false); }
  } else {
    combo = -1;
    if (!spawn()) gameOver();
  }
}

static void finishClear() {
  for (int i = 0; i < clearN; i++) {                      // supprime les lignes (de haut en bas)
    int row = clearRows[i];
    for (int y = row; y > 0; y--) memcpy(grid[y], grid[y - 1], FW);
    memset(grid[0], 0, FW);
  }
  static const int PTS[5] = {0, 100, 300, 500, 800};
  combo++;
  score += PTS[clearN] * level + (combo > 0 ? 50 * combo * level : 0);
  lines += clearN;
  int nl = lines / 10 + 1;
  if (nl > level) { level = nl; lvlT = 70; sfx(988, 60); sfx(1318, 100, false); }
  if (score > best) { best = score; pr.putInt("best", best); }
  state = PLAY;
  if (!spawn()) gameOver();
}

static void hardDrop() {
  int gy = ghostY(), d = gy - cur.y;
  for (int c = 0; c < 4; c++) {                           // traînee
    int x = FX + (cur.x + SH[cur.t][cur.r][c][0]) * CS + 3, y = FY + (cur.y + SH[cur.t][cur.r][c][1]) * CS;
    for (int k = 0; k < 2; k++) spark(x, y + 4 + k * 6, PCOL[cur.t + 1], 1, 0.35f);
  }
  cur.y = gy; score += d * 2; shake = 5; sfx(120, 40);
  lockPiece();
}

static void step() {
  animT++;
  for (auto &b : bgs) { b.y += b.v; if (b.y > SCR_H + 8) { b.y = -8; b.x = rnd(0, SCR_W); b.t = rnd(1, 7); } }
  for (auto &p : parts) if (p.life > 0) { p.x += p.vx; p.y += p.vy; p.vy += 0.05f; p.life--; }
  if (shake > 0) shake--;
  if (bannerT > 0) bannerT--;
  if (lvlT > 0) lvlT--;
  if (appear > 0) appear--;
  if (state == TITLE || state == PAUSED) return;
  if (state == OVER) { overT++; return; }
  if (state == CLEAR) { if (++clearT >= 24) finishClear(); return; }

  tilt::update();
  float tx = tilt::x(), ty = tilt::y();
  if (g_btnCtrl) {                                   // boutons : B gauche, A droite, PWR tourner, A+B tenus = chute
    static int bothT = 0;
    bool ba = M5.BtnA.isPressed(), bb = M5.BtnB.isPressed();
    if (ba && bb) { if (++bothT == 18) { hardDrop(); return; } tx = 0; }          // A+B maintenus = chute (la rotation est sur PWR)
    else { bothT = 0; tx = ba ? 1.0f : (bb ? -1.0f : 0.f); }
    ty = 0;
  }
  // inclinaison moins nerveuse : seuil plus haut pour partir, hysteresis pour s'arreter,
  // un petit delai avant le 1er pas, puis une longue pause avant la repetition (1 inclinaison = 1 case)
  static int armT = 0;
  int dir = lastDir;
  if (tx > 0.40f) dir = 1; else if (tx < -0.40f) dir = -1;
  else if (fabsf(tx) < 0.22f) dir = 0;
  if (dir == 0) { dasT = 0; lastDir = 0; armT = 0; }
  else if (dir != lastDir) {
    if (++armT >= 3) { armT = 0; dasT = 0; lastDir = dir; if (tryMove(dir, 0)) sfx(900, 10); }
  } else {
    armT = 0; dasT++;
    int iv = fabsf(tx) > 0.75f ? 4 : 7;
    if (dasT >= 20 && (dasT - 20) % iv == 0 && tryMove(dir, 0)) sfx(900, 8);
  }
  bool sd = ty > 0.45f;
  int gf = (int)(GRAV[level > 20 ? 19 : level - 1] * dif(1.4f, 1.0f, 0.72f) + 0.5f); if (gf < 1) gf = 1;
  if (++gravT >= (sd ? 2 : gf)) {
    gravT = 0;
    if (tryMove(0, 1)) { if (sd) score += 1; }
  }
  if (grounded()) { if (++lockT >= (sd ? 8 : 26)) { lockPiece(); return; } }
  else lockT = 0;
}

// ----------------------------------------------------------------------------- dessin
static void cell(int x, int y, int s, uint16_t c) {
  uint16_t dk = mixc(c, 0, 150), lt = mixc(c, 0xFFFF, 120);
  canvas.fillRect(x, y, s, s, dk);
  canvas.fillRect(x + 1, y + 1, s - 2, s - 2, c);
  canvas.drawFastHLine(x, y, s - 1, lt); canvas.drawFastVLine(x, y, s - 1, lt);
  canvas.drawFastHLine(x + 1, y + s - 1, s - 1, mixc(c, 0, 190)); canvas.drawFastVLine(x + s - 1, y + 1, s - 1, mixc(c, 0, 190));
  if (s >= 6) canvas.drawPixel(x + 1, y + 1, 0xFFFF);
}
static void miniPiece(int t, int cx, int cy, int s, bool dimmed) {          // piece centree sur (cx, cy)
  int minx = 9, maxx = -9, miny = 9, maxy = -9;
  for (int c = 0; c < 4; c++) { int x = SH[t][0][c][0], y = SH[t][0][c][1]; if (x < minx) minx = x; if (x > maxx) maxx = x; if (y < miny) miny = y; if (y > maxy) maxy = y; }
  int w = (maxx - minx + 1) * s, h = (maxy - miny + 1) * s;
  uint16_t col = dimmed ? mixc(PCOL[t + 1], rgb(20, 22, 44), 110) : PCOL[t + 1];
  for (int c = 0; c < 4; c++) cell(cx - w / 2 + (SH[t][0][c][0] - minx) * s, cy - h / 2 + (SH[t][0][c][1] - miny) * s, s, col);
}

static uint16_t accent() {
  static const uint16_t A[8] = {rgb(80, 150, 255), rgb(60, 210, 200), rgb(90, 220, 110), rgb(250, 212, 60), rgb(250, 140, 50), rgb(240, 80, 80), rgb(220, 90, 230), rgb(160, 110, 255)};
  return A[(level - 1) & 7];
}

static void drawBack() {
  for (int y = 0; y < SCR_H; y += 3) canvas.fillRect(0, y, SCR_W, 3, mixc(rgb(10, 14, 38), rgb(40, 20, 66), y * 256 / SCR_H));
  for (auto &b : bgs) { uint16_t c = mixc(rgb(20, 22, 50), PCOL[b.t], 38); canvas.fillRect((int)b.x, (int)b.y, 5, 5, c); canvas.drawRect((int)b.x, (int)b.y, 5, 5, mixc(c, 0xFFFF, 40)); }
}

static void panelText(const char* label, const char* val, int x, int y) {
  txt(label, x, y, &fonts::Font0, rgb(140, 156, 214));
  txt(val, x, y + 10, &fonts::Font2, TFT_WHITE);
}

static void drawField(int ox) {
  uint16_t ac = accent();
  // cadre
  canvas.fillRect(ox + FX - 4, FY - 4, FW * CS + 8, FH * CS + 8, rgb(14, 16, 34));
  canvas.drawRect(ox + FX - 4, FY - 4, FW * CS + 8, FH * CS + 8, mixc(ac, 0, 90));
  canvas.drawRect(ox + FX - 3, FY - 3, FW * CS + 6, FH * CS + 6, ac);
  canvas.drawRect(ox + FX - 2, FY - 2, FW * CS + 4, FH * CS + 4, mixc(ac, 0, 150));
  for (int x = 0; x < FW; x++) canvas.fillRect(ox + FX + x * CS, FY, CS, FH * CS, (x & 1) ? rgb(12, 14, 30) : rgb(16, 18, 38));
  for (int y = 1; y < FH; y++) for (int x = 1; x < FW; x++) canvas.drawPixel(ox + FX + x * CS, FY + y * CS, rgb(30, 34, 64));
  // cases posees
  for (int y = 0; y < FH; y++) for (int x = 0; x < FW; x++) if (grid[y][x]) {
    bool clr = false; if (state == CLEAR) for (int i = 0; i < clearN; i++) if (clearRows[i] == y) clr = true;
    if (clr) {
      int k = clearT - abs(x * 2 - 9) / 2;                // vague du centre vers les bords
      uint16_t c = k < 0 ? PCOL[grid[y][x]] : ((clearT / 3) & 1 ? 0xFFFF : mixc(PCOL[grid[y][x]], 0xFFFF, 150));
      if (k > 14) continue;
      cell(ox + FX + x * CS, FY + y * CS, CS, c);
    } else cell(ox + FX + x * CS, FY + y * CS, CS, PCOL[grid[y][x]]);
  }
  if (state == OVER) {                                    // le puits se remplit de gris, du bas vers le haut
    int rows = overT / 2; if (rows > FH) rows = FH;
    for (int y = FH - rows; y < FH; y++) for (int x = 0; x < FW; x++) cell(ox + FX + x * CS, FY + y * CS, CS, (x + y) & 1 ? rgb(92, 98, 122) : rgb(80, 86, 108));
  }
  if (state == PLAY || state == PAUSED) {
    if (state == PLAY) {
      int gy = ghostY();
      if (gy != cur.y) for (int c = 0; c < 4; c++) {
        int x = cur.x + SH[cur.t][cur.r][c][0], y = gy + SH[cur.t][cur.r][c][1];
        if (y >= 0) { canvas.drawRect(ox + FX + x * CS, FY + y * CS, CS, CS, mixc(PCOL[cur.t + 1], rgb(14, 16, 34), 120)); canvas.drawPixel(ox + FX + x * CS + 2, FY + y * CS + 2, mixc(PCOL[cur.t + 1], rgb(14, 16, 34), 90)); }
      }
      bool flash = grounded() && (lockT * 3 / 2) % 6 < 2 && lockT > 6;
      for (int c = 0; c < 4; c++) {
        int x = cur.x + SH[cur.t][cur.r][c][0], y = cur.y + SH[cur.t][cur.r][c][1];
        if (y >= 0) cell(ox + FX + x * CS, FY + y * CS, CS, flash ? mixc(PCOL[cur.t + 1], 0xFFFF, 110) : PCOL[cur.t + 1]);
      }
    }
  }
}

static void renderTitle() {
  char b[24];
  {
    drawBack();
    for (int i = 0; i < 4; i++) {                                       // pieces qui tombent en fond
      int t = (i * 3 + 1) % 7; int px = 20 + i * 58 + (i & 1) * 14, py = ((animT * (1 + (i & 1)) / 2 + i * 40) % (SCR_H + 60)) - 30;
      miniPiece(t, px, py, 8, true);
    }
    static const char* TT = "TETRIS";
    for (int i = 0; i < 6; i++) {
      int x = 29 + i * 30, y = 28 + (int)(sinf(animT * 0.08f + i * 0.9f) * 5);
      char ch[2] = {TT[i], 0};
      txt(ch, x + 2, y + 3, &fonts::Font4, rgb(6, 6, 20), textdatum_t::top_left, 1.6f);
      txt(ch, x, y, &fonts::Font4, PCOL[(i % 7) + 1], textdatum_t::top_left, 1.6f);
    }
    if ((animT / 25) & 1) txtO(L("PRESS A", "APPUIE SUR A"), SCR_W / 2, 92, &fonts::Font2, TFT_WHITE, rgb(10, 10, 30), textdatum_t::top_center);
    if (best > 0) { snprintf(b, sizeof b, L("best %lu", "record %lu"), (unsigned long)best); txt(b, SCR_W / 2, 118, &fonts::Font2, rgb(255, 224, 120), textdatum_t::top_center); }
  }
}

static void render() {
  if (state == TITLE) { renderTitle(); return; }
  drawBack();
  int ox = shake > 0 ? ((shake & 1) ? 1 : -1) : 0, oy = shake > 3 ? 1 : 0; (void)oy;
  drawField(ox);
  for (auto &p : parts) if (p.life > 0) canvas.fillRect((int)p.x, (int)p.y, 2, 2, mixc(p.c, 0xFFFF, p.life > 14 ? 90 : 20));
  uint16_t ac = accent();
  char b[24];
  // panneau gauche
  canvas.fillRoundRect(4, 6, 78, 122, 6, rgb(10, 12, 30)); canvas.drawRoundRect(4, 6, 78, 122, 6, mixc(ac, 0, 100));
  snprintf(b, sizeof b, "%lu", (unsigned long)score); panelText("SCORE", b, 10, 12);
  snprintf(b, sizeof b, "%d", lines); panelText("LINES", b, 10, 46);
  snprintf(b, sizeof b, "%d", level); panelText(L("LEVEL", "NIVEAU"), b, 10, 80);
  snprintf(b, sizeof b, "%lu", (unsigned long)best); txt(L("BEST", "RECORD"), 10, 108, &fonts::Font0, rgb(140, 156, 214)); txt(b, 72, 108, &fonts::Font0, rgb(255, 224, 120), textdatum_t::top_right);
  // panneau droit : pieces suivantes
  canvas.fillRoundRect(158, 6, 78, 122, 6, rgb(10, 12, 30)); canvas.drawRoundRect(158, 6, 78, 122, 6, mixc(ac, 0, 100));
  txt("NEXT", 197, 11, &fonts::Font0, rgb(140, 156, 214), textdatum_t::top_center);
  canvas.fillRoundRect(166, 22, 62, 38, 4, rgb(18, 20, 44));
  miniPiece(nextq[0], 197, 41, 7, false);
  canvas.fillRoundRect(172, 66, 50, 26, 4, rgb(16, 18, 38));
  miniPiece(nextq[1], 197, 79, 5, true);
  canvas.fillRoundRect(178, 96, 38, 22, 4, rgb(14, 16, 34));
  miniPiece(nextq[2], 197, 107, 4, true);
  // annonces
  if (bannerT > 0 && state != TITLE && state != OVER) {
    int yy = 52 - (bannerT > 50 ? (bannerT - 50) : 0) * 2;
    int w = strlen(banner) * 12 + 14;
    canvas.fillRoundRect(ox + FX + 30 - w / 2, yy - 2, w, 20, 5, rgb(8, 10, 26)); canvas.drawRoundRect(ox + FX + 30 - w / 2, yy - 2, w, 20, 5, clearN == 4 ? rgb(255, 220, 80) : ac);
    txt(banner, ox + FX + 30, yy, &fonts::Font2, clearN == 4 ? rgb(255, 220, 80) : TFT_WHITE, textdatum_t::top_center);
  }
  if (lvlT > 0 && (lvlT / 5) & 1) { snprintf(b, sizeof b, L("LEVEL %d", "NIVEAU %d"), level); txtO(b, ox + FX + 30, 18, &fonts::Font2, rgb(255, 224, 120), rgb(40, 20, 0), textdatum_t::top_center); }
  if (state == PAUSED) ui::pause();
  if (state == OVER && overT > 40) ui::gameOver("GAME OVER", score, best, score > 0 && score >= best);
}

void loop() {
  uint32_t now = micros();
  if (last == 0) last = now;
  acc += now - last; last = now; if (acc > 100000) acc = 100000;
  bool a = M5.BtnA.wasPressed(), bb = M5.BtnB.wasPressed(), p = M5.BtnPWR.wasPressed();
  switch (state) {
    case TITLE: if (a || bb) { reset(); state = PLAY; sfx(660, 40); } break;
    case PLAY:
      if (g_btnCtrl) { if (p && rotate()) sfx(1100, 12); }                   // mode boutons : PWR = tourner (pas de pause)
      else if (p) { state = PAUSED; sfx(440, 50); } else { if (a && rotate()) sfx(1100, 12); if (bb) hardDrop(); }
      break;
    case PAUSED: if (p || a || bb) { state = PLAY; sfx(660, 50); } break;
    case CLEAR: break;
    case OVER: if ((a || bb) && overT > 70) { reset(); state = PLAY; sfx(660, 40); } break;
  }
  while (acc >= STEP_US) { step(); acc -= STEP_US; }
  render();
  bgm::run(2, state == PLAY || state == CLEAR);
  present(0, 0);
}

void icon(int x, int y) {
  if (PCOL[1] == 0) buildShapes();
  canvas.fillRoundRect(x, y, 64, 64, 6, rgb(24, 28, 70));
  canvas.fillRect(x + 12, y + 4, 40, 56, rgb(10, 12, 30)); canvas.drawRect(x + 11, y + 3, 42, 58, rgb(120, 140, 230));
  // pile de cases
  static const uint8_t ST[5][5] = {{0,0,0,0,0},{0,0,0,0,0},{0,0,7,0,0},{1,1,7,3,0},{6,5,5,3,3}};
  for (int j = 2; j < 5; j++) for (int i = 0; i < 5; i++) if (ST[j][i]) cell(x + 12 + i * 8, y + 4 + 16 + j * 8 + 0, 8, PCOL[ST[j][i]]);
  for (int j = 0; j < 2; j++) (void)j;
  // piece T qui tombe
  static const int8_t T[4][2] = {{1,0},{0,1},{1,1},{2,1}};
  for (int c = 0; c < 4; c++) cell(x + 20 + T[c][0] * 8, y + 8 + T[c][1] * 8, 8, PCOL[3]);
  cell(x + 44, y + 12, 8, PCOL[1]);
  cell(x + 44, y + 20, 8, PCOL[1]);
}

}  // namespace tetris
