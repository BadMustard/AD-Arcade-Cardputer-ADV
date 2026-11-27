// Ecran des apparences (skins) : liste a gauche, apercu en direct sur plusieurs jeux a droite.
#include "common.h"

namespace skins {
static int sel = 0;

void begin() { sel = g_skin; }

static void swatch(int x, int y, int i) {                   // 4 petits carres : corps, ombre, reflet, accent
  int sv = g_skin; g_skin = i;
  for (int k = 0; k < 4; k++) { uint16_t c = i ? skin::c(k, 0) : (k == 0 ? P_L : k == 1 ? P_M : k == 2 ? P_B : P_R); canvas.fillRect(x + k * 4, y, 4, 8, c); }
  canvas.drawRect(x - 1, y - 1, 18, 10, P_K);
  g_skin = sv;
}

static void cell(int x, int y, int w, int h) { canvas.fillRect(x, y, w, h, P_K); canvas.fillRect(x + 1, y + 1, w - 2, h - 2, P_D); canvas.fillRect(x + 1, y + 1, w - 2, 1, P_M); }

void loop() {
  int n = skin::count();
  if (M5.BtnB.wasPressed()) { sel = (sel + 1) % n; sfx(1200, 20); }
  if (M5.BtnPWR.wasPressed()) { sel = (sel + n - 1) % n; sfx(1000, 20); }
  bool ok = skin::unlocked(sel);
  if (M5.BtnA.wasPressed()) {
    if (ok) { g_skin = sel; saveSettings(); sfx(988, 50); sfx(1318, 80, false); } else sfx(300, 90);
  }
  uint32_t t = millis();
  ui::bg((int)(t / 60));
  ui::header(L("SKINS", "SKINS"), P_G);
  // ---- liste (gauche) : 7 lignes de 13 px
  int top = 20, rowH = 13, vis = 7, first = sel < vis ? 0 : sel - vis + 1;
  canvas.fillRect(3, top - 1, 84, vis * rowH + 3, P_K); canvas.fillRect(4, top, 82, vis * rowH + 1, P_B);
  for (int r = 0; r < vis && first + r < n; r++) {
    int i = first + r, y = top + r * rowH; bool on = i == sel, un = skin::unlocked(i);
    if (on) { canvas.fillRect(4, y, 82, rowH, P_K); canvas.fillRect(5, y + 1, 80, rowH - 2, P_S); }
    swatch(8, y + 2, i);
    txt(skin::name(i), 29, y + 3, &fonts::Font0, P_K, textdatum_t::top_left);
    if (i == g_skin) { canvas.fillRect(76, y + 6, 2, 3, P_K); canvas.fillRect(78, y + 4, 2, 3, P_K); canvas.fillRect(80, y + 2, 2, 3, P_K); canvas.fillRect(76, y + 5, 2, 3, P_G); canvas.fillRect(78, y + 3, 2, 3, P_G); canvas.fillRect(80, y + 1, 2, 3, P_G); }
    else if (!un) { canvas.fillRect(76, y + 6, 7, 5, P_K); canvas.fillRect(77, y + 7, 5, 3, P_R); canvas.fillRect(77, y + 2, 5, 4, P_K); canvas.fillRect(78, y + 3, 3, 3, P_L); canvas.fillRect(78, y + 6, 3, 2, P_R); }
  }
  // ---- apercu (droite)
  const int WX = 92, WY = 18, WW = 146, WH = 102;
  ui::window(WX, WY, WW, WH, ok ? P_S : P_R, skin::name(sel));
  int sv = g_skin; g_skin = sel;
  const int cw = 44, ch = 33, bx = WX + 5, by = WY + 22;
  for (int k = 0; k < 6; k++) {
    int cx0 = bx + (k % 3) * (cw + 1), cy0 = by + (k / 3) * (ch + 1), mx = cx0 + cw / 2, my = cy0 + ch / 2;
    cell(cx0, cy0, cw, ch);
    canvas.setClipRect(cx0 + 1, cy0 + 1, cw - 2, ch - 2);
    switch (k) {
      case 0: flap::preview(mx + 3, my); break;
      case 1: space::preview(mx, my + 1); break;
      case 2: river::preview(mx, my - 2); break;
      case 3: lander::preview(mx, my - 2); break;
      case 4: snake::preview(mx - 2, my); break;
      case 5: { int r = 7 + (int)(sinf(t * 0.006f) * 1.5f); pxDisc(mx, my, r, skin::c(0, P_R), skin::c(2, P_B), skin::c(1, P_D)); canvas.fillRect(mx - 8, my + 15, 16, 2, P_M); break; }
    }
    canvas.clearClipRect();
  }
  g_skin = sv;
  if (!ok) {
    char b[32]; snprintf(b, sizeof b, L("LOCKED  %d/%d medals", "VERROUILLE  %d/%d med."), trophy::total(), skin::need(sel));
    canvas.fillRect(WX + 5, WY + WH - 13, WW - 10, 10, P_K); canvas.fillRect(WX + 6, WY + WH - 12, WW - 12, 8, P_R);
    txt(b, WX + WW / 2, WY + WH - 11, &fonts::Font0, P_B, textdatum_t::top_center);
  } else if (sel == g_skin) {
    canvas.fillRect(WX + 5, WY + WH - 13, WW - 10, 10, P_K); canvas.fillRect(WX + 6, WY + WH - 12, WW - 12, 8, P_G);
    txt(L("EQUIPPED", "EQUIPE"), WX + WW / 2, WY + WH - 11, &fonts::Font0, P_K, textdatum_t::top_center);
  } else {
    canvas.fillRect(WX + 5, WY + WH - 13, WW - 10, 10, P_K); canvas.fillRect(WX + 6, WY + WH - 12, WW - 12, 8, ((t / 400) & 1) ? P_S : P_B);
    txt(L("A : EQUIP", "A : EQUIPER"), WX + WW / 2, WY + WH - 11, &fonts::Font0, P_K, textdatum_t::top_center);
  }
  ui::footer(L("B:next  PWR:prev  A:equip  hold PWR:menu", "B:suiv  PWR:prec  A:equiper  PWR long:menu"));
  present(0, 0);
}

void icon(int x, int y) {
  ui::tile(x, y, P_M);
  pxDisc(x + 30, y + 34, 24, P_B, P_B, P_L);                                    // palette de peintre
  pxDisc(x + 44, y + 48, 6, P_M, P_D, P_K);
  static const uint16_t C[5] = {P_R, P_G, P_S, P_D, P_L};
  static const int PX[5] = {15, 24, 37, 47, 12}, PY[5] = {38, 22, 17, 28, 52};
  for (int i = 0; i < 5; i++) pxDisc(x + PX[i], y + PY[i], 5, C[i], P_B, P_K);
  for (int i = 0; i < 4; i++) canvas.fillTriangle(x + 52 + i, y + 6, x + 40 + i, y + 24, x + 43 + i, y + 26, i == 0 ? P_K : P_G);   // pinceau
  canvas.fillTriangle(x + 40, y + 24, x + 43, y + 26, x + 38, y + 31, P_R);
  canvas.fillRect(x + 6, y + 6, 4, 4, P_S); canvas.fillRect(x + 7, y + 7, 2, 2, P_B);
}
}  // namespace skins
