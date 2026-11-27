// Kit d'interface pixel art : 8 couleurs, contour noir 1 px, aplats nets.
#include "common.h"

namespace ui {

static uint16_t pal(char ch) {
  switch (ch) { case 'K': return P_K; case 'D': return P_D; case 'M': return P_M; case 'L': return P_L; case 'B': return P_B; case 'R': return P_R; case 'G': return P_G; case 'S': return P_S; }
  return 0;
}
void art(int x, int y, const char* const* rows, int h, int sc, char from, uint16_t to) {
  for (int j = 0; j < h; j++) for (int i = 0; rows[j][i]; i++) { char ch = rows[j][i]; if (ch == '.') continue; canvas.fillRect(x + i * sc, y + j * sc, sc, sc, ch == from ? to : pal(ch)); }
}
void tile(int x, int y, uint16_t c) {                       // fond d'icone 64x64 : contour noir, reflet haut-gauche
  canvas.fillRect(x, y, 64, 64, P_K); canvas.fillRect(x + 1, y + 1, 62, 62, c);
  canvas.fillRect(x + 1, y + 1, 62, 1, P_L); canvas.fillRect(x + 1, y + 1, 1, 62, P_L); canvas.fillRect(x + 2, y + 62, 61, 1, P_D);
}
static const char* SKULL[] = {".KKKKKKK.", "KBBBBBBBK", "KBBBBBBBK", "KBKKBKKBK", "KBKKBKKBK", "KBBBKBBBK", ".KBBBBBK.", ".KBKBKBK.", "..KKKKK.."};
static const char* CROWN[] = {"K....K....K", "KG..KGK..GK", "KGKKGGGKKGK", "KGGGGGGGGGK", "KGBGGGGGBGK", "KGGGGGGGGGK", "KKKKKKKKKKK"};
static const char* STAR[] = {"....K....", "...KGK...", "...KGK...", "KKKKGKKKK", "KGGGGGGGK", ".KGGBGGK.", "..KGGGK..", ".KGGKGGK.", ".KKK.KKK."};
static const char* HEART[] = {".KKK.KKK.", "KRRRKRRRK", "KRBRRRRRK", "KRRRRRRRK", ".KRRRRRK.", "..KRRRK..", "...KRK...", "....K...."};
static const char* COIN[] = {"..KKKK..", ".KGGGGK.", "KGGBBGGK", "KGBGGGGK", "KGBGGGGK", "KGGGGGGK", ".KGGGGK.", "..KKKK.."};
void skull(int x, int y, int sc) { art(x, y, SKULL, 9, sc); }
void crown(int x, int y, int sc) { art(x, y, CROWN, 7, sc); }
void star(int x, int y, int sc) { art(x, y, STAR, 9, sc); }
void heart(int x, int y, int sc) { art(x, y, HEART, 8, sc); }
void coin(int x, int y, int sc) { art(x, y, COIN, 8, sc); }

void window(int x, int y, int w, int h, uint16_t bar, const char* title) {
  canvas.fillRect(x + 3, y + 3, w, h, P_K);                                 // ombre portee
  canvas.fillRect(x, y, w, h, P_K);
  canvas.fillRect(x + 1, y + 1, w - 2, h - 2, P_L);                         // cadre acier
  canvas.fillRect(x + 2, y + 2, w - 4, h - 4, P_K);
  canvas.fillRect(x + 3, y + 3, w - 6, 16, bar);                            // barre de titre
  canvas.fillRect(x + 3, y + 3, w - 6, 1, P_B);                             // reflet
  canvas.fillRect(x + 3, y + 18, w - 6, 1, P_K);
  canvas.fillRect(x + 3, y + 19, w - 6, h - 22, P_B);                       // corps clair : fort contraste
  canvas.fillRect(x + 3, y + h - 4, w - 6, 1, P_L);
  if (title && *title) { txt(title, x + w / 2 + 1, y + 3, &fonts::Font2, P_K, textdatum_t::top_center); txt(title, x + w / 2, y + 2, &fonts::Font2, P_B, textdatum_t::top_center); }
}
void button(int cx, int y, int w, const char* label, uint16_t col) {
  int x = cx - w / 2;
  canvas.fillRect(x, y + 2, w, 13, P_K);                                    // dessous
  pxBox(x, y, w, 13, col, P_B, P_K);
  txt(label, cx, y + 3, &fonts::Font0, P_B, textdatum_t::top_center);
}
void popText(int cx, int y, const char* s, uint16_t col) {          // etiquette pixel art : bonus = or, danger = rouge, special = vert
  int r = (col >> 11) << 3, g = ((col >> 5) & 63) << 2, b = (col & 31) << 3;
  bool bad = r > 190 && g < 150 && b < 120 && !(g > 150), cool = b > 180 && r < 160;
  bool white = r > 220 && g > 220 && b > 220;
  uint16_t fill = bad ? P_R : (cool ? P_S : (white ? P_L : P_G)), tc = bad ? P_B : P_K;
  int w = (int)strlen(s) * 8 + 6, x = cx - w / 2;
  canvas.fillRect(x, y, w, 17, P_K); canvas.fillRect(x + 1, y + 1, w - 2, 15, fill); canvas.fillRect(x + 1, y + 1, w - 2, 1, P_B);
  txt(s, cx, y + 1, &fonts::Font2, tc, textdatum_t::top_center);
}

void gameOver(const char* title, uint32_t score, uint32_t best, bool record, const char* hint) {
  const int W = 192, H = 108, X = (SCR_W - W) / 2, Y = 13;
  window(X, Y, W, H, P_R, title);
  if (title && title[0] == 'G') { skull(X + 8, Y + 6, 1); skull(X + W - 17, Y + 6, 1); }
  int by = Y + 20;
  txt(L("SCORE", "SCORE"), SCR_W / 2, by + 1, &fonts::Font0, P_M, textdatum_t::top_center);
  char b[24]; snprintf(b, sizeof b, "%lu", (unsigned long)score);
  txt(b, SCR_W / 2 + 2, by + 11, &fonts::Font4, P_L, textdatum_t::top_center, 1.5f);                 // ombre pleine decalee
  txt(b, SCR_W / 2, by + 9, &fonts::Font4, P_K, textdatum_t::top_center, 1.5f);
  canvas.fillRect(X + 12, by + 42, W - 24, 1, P_L); canvas.fillRect(X + 12, by + 43, W - 24, 1, P_M);
  if (record && score > 0) {
    bool on = (millis() / 350) & 1;
    canvas.fillRect(X + 20, by + 47, W - 40, 15, P_K); canvas.fillRect(X + 21, by + 48, W - 42, 13, on ? P_G : P_B);
    txt(L("NEW RECORD!", "NOUVEAU RECORD !"), SCR_W / 2, by + 48, &fonts::Font2, P_K, textdatum_t::top_center);
    star(X + 8, by + 49, 1); star(X + W - 17, by + 49, 1);
  } else {
    crown(SCR_W / 2 - 42, by + 50, 1);
    snprintf(b, sizeof b, L("BEST %lu", "RECORD %lu"), (unsigned long)best);
    txt(b, SCR_W / 2 - 26, by + 47, &fonts::Font2, P_D, textdatum_t::top_left);
  }
  button(SCR_W / 2, Y + H - 21, 96, hint ? hint : L("A : RETRY", "A : REJOUER"), P_R);
}

void bg(int t) {                                            // fond colore : ciel en bandes, etoiles, 2 couches de montagnes, sol damier
  static const uint16_t SK[6] = {rgb(38, 32, 96), rgb(56, 44, 124), rgb(82, 58, 146), rgb(118, 72, 156), rgb(164, 90, 150), rgb(214, 112, 130)};
  for (int i = 0; i < 6; i++) canvas.fillRect(0, i * 14, SCR_W, i == 5 ? SCR_H - 70 : 14, SK[i]);
  for (int i = 0; i < 18; i++) { int x = (i * 53 + 17 - t / 6 * (1 + (i & 1))) % SCR_W; if (x < 0) x += SCR_W; int y = (i * 37) % 60; canvas.fillRect(x, y, 2, 2, ((t / 20 + i) % 7 == 0) ? P_B : P_L); }
  for (int layer = 0; layer < 2; layer++) {
    uint16_t c = layer ? rgb(46, 38, 104) : rgb(90, 62, 140), hl = layer ? rgb(72, 58, 130) : rgb(140, 92, 160);
    for (int x = 0; x < SCR_W; x += 4) {
      float wx = x + t * (layer ? 0.9f : 0.4f);
      int h = (int)(14 + layer * 8 + sinf(wx * 0.045f + layer * 2.2f) * 9 + sinf(wx * 0.11f + layer) * 4);
      canvas.fillRect(x, 98 - h, 4, h + 1, c); canvas.fillRect(x, 98 - h, 4, 2, hl);
    }
  }
  canvas.fillRect(0, 98, SCR_W, SCR_H - 98, rgb(24, 70, 74));
  int o = (t / 3) % 16;
  for (int y = 98; y < SCR_H; y += 8) for (int x = -16; x < SCR_W + 16; x += 16) canvas.fillRect(x + o + ((y / 8) & 1) * 8, y, 8, 8, rgb(36, 104, 96));
  canvas.fillRect(0, 98, SCR_W, 1, P_K);
}
void night(int h) {                                        // ciel de nuit violet / bleu en bandes plates
  static const uint16_t NB[5] = {rgb(12, 10, 46), rgb(22, 18, 76), rgb(34, 24, 102), rgb(52, 32, 122), rgb(78, 42, 136)};
  for (int i = 0; i < 5; i++) canvas.fillRect(0, h * i / 5, SCR_W, h * (i + 1) / 5 - h * i / 5 + 1, NB[i]);
}
void header(const char* title, uint16_t bar) {
  canvas.fillRect(0, 0, SCR_W, 18, P_K); canvas.fillRect(0, 0, SCR_W, 16, bar); canvas.fillRect(0, 0, SCR_W, 1, P_B); canvas.fillRect(0, 16, SCR_W, 1, P_D);
  txt(title, SCR_W / 2 + 1, 1, &fonts::Font2, P_K, textdatum_t::top_center); txt(title, SCR_W / 2, 0, &fonts::Font2, P_B, textdatum_t::top_center);
}
void footer(const char* hint) {
  canvas.fillRect(0, SCR_H - 14, SCR_W, 14, P_K); canvas.fillRect(0, SCR_H - 14, SCR_W, 1, P_L);
  txt(hint, SCR_W / 2, SCR_H - 11, &fonts::Font0, P_B, textdatum_t::top_center);
}
void field(int x, int y, int w, int h, const char* label, const char* value, bool sel) {
  canvas.fillRect(x + 2, y + 2, w, h, P_K);
  canvas.fillRect(x, y, w, h, sel ? P_G : P_K); canvas.fillRect(x + 1, y + 1, w - 2, h - 2, sel ? P_K : P_D);
  canvas.fillRect(x + 2, y + 2, w - 4, h - 4, P_B);
  canvas.fillRect(x + 2, y + 2, w - 4, 9, sel ? P_G : P_L);
  txt(label, x + w / 2, y + 3, &fonts::Font0, P_K, textdatum_t::top_center);
  txt(value, x + w / 2, y + 13, &fonts::Font4, P_K, textdatum_t::top_center);
  if (sel) { canvas.fillTriangle(x + w / 2 - 4, y - 3, x + w / 2 + 4, y - 3, x + w / 2, y + 1, P_R); canvas.fillTriangle(x + w / 2 - 4, y + h + 2, x + w / 2 + 4, y + h + 2, x + w / 2, y + h - 2, P_R); }
}
void pause() {
  const int W = 112, H = 54, X = (SCR_W - W) / 2, Y = 36;
  window(X, Y, W, H, P_M, "PAUSE");
  canvas.fillRect(X + 40, Y + 26, 6, 16, P_K); canvas.fillRect(X + 41, Y + 27, 4, 14, P_R); canvas.fillRect(X + 66, Y + 26, 6, 16, P_K); canvas.fillRect(X + 67, Y + 27, 4, 14, P_R);   // symbole pause
}
void notice(const char* title, const char* l1, const char* l2, uint16_t bar) {
  int H = l2 ? 72 : 56, Y = (SCR_H - H) / 2, W = 176, X = (SCR_W - W) / 2;
  window(X, Y, W, H, bar, title);
  txt(l1, SCR_W / 2, Y + 24, &fonts::Font2, P_K, textdatum_t::top_center);
  if (l2) txt(l2, SCR_W / 2, Y + 44, &fonts::Font0, P_D, textdatum_t::top_center);
  star(X + 6, Y + 5, 1); star(X + W - 15, Y + 5, 1);
}

}  // namespace ui
