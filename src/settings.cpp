// ============================================================================
//  REGLAGES - luminosite, son, delai de veille, stabilisation et calibrage de l'inclinaison
//  B = ligne suivante, HAUT = ligne precedente, M5 = changer / valider
// ============================================================================
#include "common.h"

namespace settings {

static const int NROWS = 15, VIS = 6;
enum { R_BRI, R_VOL, R_MUS, R_CTRL, R_SCREEN, R_DIFF, R_SKIN, R_THEME, R_SLEEP, R_STAB, R_CAL, R_LANG, R_DATE, R_ABOUT, R_BACK };
static const char* ROWS_EN[NROWS] = {"Brightness", "Volume", "Game music", "Controls", "Screen", "Difficulty", "Skin", "Theme", "Sleep", "Stabilize", "Calibrate", "Language", "Date", "About", "Back"};
static const char* ROWS_FR[NROWS] = {"Luminosite", "Volume", "Musique jeux", "Commandes", "Ecran", "Difficulte", "Apparence", "Theme", "Veille", "Stabiliser", "Calibrage", "Langue", "Date", "A propos", "Retour"};
static const char* DIFF_EN[3] = {"EASY", "NORMAL", "HARD"}, *DIFF_FR[3] = {"FACILE", "NORMAL", "DIFFICILE"};
static int skView = 0;
static const char* SLEEPS_EN[5] = {"30 s", "1 min", "2 min", "5 min", "never"};
static const char* SLEEPS_FR[5] = {"30 s", "1 min", "2 min", "5 min", "jamais"};
static int sel = 0, top = 0;
static bool about = false, admin = false, confirmClear = false;
static int aTaps = 0, aSel = 0; static uint32_t aLast = 0, aMsgUntil = 0; static const char* aMsg = "";
static uint32_t stabStart = 0;       // > 0 : compte a rebours de stabilisation en cours

void begin() { sel = 0; top = 0; stabStart = 0; about = false; admin = false; aTaps = 0; skView = g_skin; }

static void toggle(int x, int y, bool on) {                    // interrupteur pixel
  canvas.fillRect(x, y, 30, 12, P_K); canvas.fillRect(x + 1, y + 1, 28, 10, on ? P_S : P_L);
  canvas.fillRect(on ? x + 17 : x + 2, y + 2, 11, 8, P_K); canvas.fillRect(on ? x + 18 : x + 3, y + 3, 9, 6, P_B);
  txt(on ? "ON" : "OFF", on ? x + 8 : x + 21, y + 3, &fonts::Font0, P_K, textdatum_t::top_center);
}
static void bars(int x, int y, int n, int lvl, uint16_t col) {  // jauge a barres croissantes
  for (int k = 0; k < 5; k++) { int h = 4 + k * 2; canvas.fillRect(x + k * 9, y + 11 - h, 7, h, P_K); canvas.fillRect(x + k * 9 + 1, y + 12 - h, 5, h - 2, k < lvl ? col : P_L); }
}

static void render() {
  ui::bg((int)(millis() / 60));
  ui::header(L("SETTINGS", "REGLAGES"), P_G);
  txt("v" AD_VERSION, SCR_W - 4, 4, &fonts::Font0, P_K, textdatum_t::top_right);
  if (sel < top) top = sel;
  if (sel > top + VIS - 1) top = sel - (VIS - 1);
  const int RH = 16, LY = 20;
  canvas.fillRect(3, LY - 1, SCR_W - 6, VIS * RH + 2, P_K); canvas.fillRect(4, LY, SCR_W - 8, VIS * RH, P_B);
  for (int i = top; i < top + VIS && i < NROWS; i++) {
    int y = LY + (i - top) * RH; bool s = i == sel;
    if (s) { canvas.fillRect(4, y, SCR_W - 8, RH, P_K); canvas.fillRect(5, y + 1, SCR_W - 10, RH - 2, P_S); canvas.fillRect(5, y + 1, SCR_W - 10, 1, P_B); }
    else canvas.fillRect(4, y + RH - 1, SCR_W - 8, 1, P_L);
    if (s) { canvas.fillTriangle(8, y + 3, 8, y + 12, 14, y + 7, P_K); canvas.fillTriangle(8, y + 5, 8, y + 10, 12, y + 7, P_R); }
    txt((g_lang ? ROWS_FR : ROWS_EN)[i], 18, y, &fonts::Font2, P_K);
    int vx = SCR_W - 15;
    if (i == R_BRI) bars(vx - 45, y + 2, 5, g_bright + 1, P_G);
    else if (i == R_VOL) { bars(vx - 45, y + 2, 5, g_vol, P_S); if (g_vol == 0) txt(L("OFF", "MUET"), vx - 50, y, &fonts::Font2, P_R, textdatum_t::top_right); }
    else if (i == R_MUS) toggle(vx - 30, y + 2, g_music);
    else if (i == R_CTRL) { static const char* CE[3] = {"ASK", "TILT", "BUTTONS"}; static const char* CF[3] = {"DEMANDER", "INCLINAISON", "BOUTONS"}; txt((g_lang ? CF : CE)[g_ctrl], vx, y, &fonts::Font2, P_K, textdatum_t::top_right); }
    else if (i == R_SCREEN) txt(g_flip ? L("FLIPPED", "RETOURNE") : L("NORMAL", "NORMAL"), vx, y, &fonts::Font2, g_flip ? P_R : P_K, textdatum_t::top_right);
    else if (i == R_DIFF) { uint16_t dc = g_diff == 0 ? P_D : (g_diff == 1 ? P_K : P_R); txt((g_lang ? DIFF_FR : DIFF_EN)[g_diff], vx, y, &fonts::Font2, dc, textdatum_t::top_right); }
    else if (i == R_SKIN) {
      bool ok = skin::unlocked(skView);
      if (ok) { int sv = g_skin; g_skin = skView; for (int k = 0; k < 3; k++) canvas.fillRect(vx - 108 + k * 4 + 1, y + 4, 4, 8, skView ? skin::c(k, 0) : (k == 0 ? P_L : k == 1 ? P_M : P_B)); canvas.drawRect(vx - 108, y + 3, 14, 10, P_K); g_skin = sv; txt(skin::name(skView), vx, y, &fonts::Font2, P_K, textdatum_t::top_right); }
      else {
        char b[24]; snprintf(b, sizeof b, "%d/%d", trophy::total(), skin::need(skView));
        canvas.fillRect(vx - 100, y + 7, 8, 6, P_K); canvas.fillRect(vx - 99, y + 8, 6, 4, P_R); canvas.fillRect(vx - 99, y + 3, 6, 4, P_K); canvas.fillRect(vx - 98, y + 4, 4, 3, P_L);
        txt(b, vx, y, &fonts::Font2, P_R, textdatum_t::top_right);
      }
    }
    else if (i == R_THEME) txt(themeName(g_theme), vx, y, &fonts::Font2, P_K, textdatum_t::top_right);
    else if (i == R_SLEEP) txt((g_lang ? SLEEPS_FR : SLEEPS_EN)[g_sleepIdx], vx, y, &fonts::Font2, P_K, textdatum_t::top_right);
    else if (i == R_LANG) txt(g_lang ? "FRANCAIS" : "ENGLISH", vx, y, &fonts::Font2, P_K, textdatum_t::top_right);
    else if (i == R_STAB || i == R_CAL) { canvas.fillRect(vx - 22, y + 3, 22, 11, P_K); canvas.fillRect(vx - 21, y + 4, 20, 9, P_G); txt("GO", vx - 11, y + 5, &fonts::Font0, P_K, textdatum_t::top_center); }
    else if (i == R_DATE || i == R_ABOUT) { canvas.fillTriangle(vx - 8, y + 3, vx - 8, y + 13, vx, y + 8, P_K); canvas.fillTriangle(vx - 6, y + 6, vx - 6, y + 10, vx - 3, y + 8, P_G); }
  }
  if (top > 0) { canvas.fillTriangle(SCR_W - 9, LY + 1, SCR_W - 14, LY + 7, SCR_W - 4, LY + 7, P_R); }
  if (top + VIS < NROWS) { canvas.fillTriangle(SCR_W - 9, LY + VIS * RH - 1, SCR_W - 14, LY + VIS * RH - 7, SCR_W - 4, LY + VIS * RH - 7, P_R); }
  ui::footer(L("B:down  PWR:up  A:change", "B:bas  PWR:haut  A:changer"));
  if (stabStart) {
    int left = 3 - (int)((millis() - stabStart) / 1000);
    ui::window(28, 30, 184, 70, P_G, L("STABILIZE", "STABILISER"));
    txt(L("Hold the M5 as you play", "Tiens le M5 comme pour jouer"), SCR_W / 2, 52, &fonts::Font0, P_K, textdatum_t::top_center);
    char b[4]; snprintf(b, sizeof b, "%d", left < 1 ? 1 : left);
    txt(b, SCR_W / 2 + 2, 68, &fonts::Font4, P_L, textdatum_t::top_center, 2.0f); txt(b, SCR_W / 2, 66, &fonts::Font4, P_R, textdatum_t::top_center, 2.0f);
  }
}

static const char* PROFILE_URL = "https://burner.m5stack.com/developers/504625";

static void renderAbout() {
  ui::bg((int)(millis() / 60));
  ui::header(L("ABOUT", "A PROPOS"), P_G);
  canvas.fillRect(3, 21, 114, 100, P_K); canvas.fillRect(4, 22, 112, 98, P_B);
  txtO("AD ARCADE", 8, 26, &fonts::Font2, P_G, P_K);
  txt(L("Version", "Version"), 8, 46, &fonts::Font0, P_M); txt(AD_VERSION, 8, 56, &fonts::Font2, P_K);
  txt(L("Created by", "Cree par"), 8, 76, &fonts::Font0, P_M); txt("ADdev", 8, 86, &fonts::Font4, P_R);
  canvas.fillRect(6, 106, 108, 1, P_L);
  txt(L("Scan for my profile", "Scanne mon profil"), 8, 110, &fonts::Font0, P_D);
  canvas.fillRect(SCR_W - 119, 18, 116, 116 - 14 + 14 > 0 ? 104 : 104, P_K);
  canvas.fillRect(SCR_W - 118, 19, 114, 102, TFT_WHITE);
  canvas.qrcode(PROFILE_URL, SCR_W - 117, 20, 100, 3);
  ui::footer("burner.m5stack.com/developers/504625");
}

// ----------------------------------------------------------------------------- mode admin (cache : 6 appuis sur M5 dans "A propos")
enum { A_UNL, A_SKIN, A_NOINTRO, A_FPS, A_MEDALS, A_GAME, A_GMEDAL, A_CLEAR, A_TRIES, A_STREAK, A_SETUP, A_CRT, A_INV, A_RBW, A_CONF, A_BEEP, A_MSG, A_BACK, NADM };
static const int A_DEBUG = A_SETUP;                          // rows <= A_SETUP = debug, ensuite = fun
static const char* ADM_EN[NADM] = {"Unlock skins", "Pick any skin", "Skip intro", "FPS counter", "Set all medals", "Game", "Its medal", "Clear records", "Daily: new tries", "Daily: streak 7", "Redo setup", "CRT lines", "Invert colors", "Rainbow menu", "Confetti!", "Beep party", "Secret message", "Back"};
static const char* ADM_FR[NADM] = {"Debloquer skins", "Choisir un skin", "Sauter l'intro", "Compteur FPS", "Toutes medailles", "Jeu", "Sa medaille", "Effacer records", "Defi: essais a 0", "Defi: serie 7", "Refaire config", "Lignes CRT", "Couleurs inv.", "Menu arc-en-ciel", "Confettis !", "Fete sonore", "Message secret", "Retour"};
static const char* LVL_EN[4] = {"NONE", "BRONZE", "SILVER", "GOLD"}, *LVL_FR[4] = {"AUCUNE", "BRONZE", "ARGENT", "OR"};
static const char* SECRETS_EN[6] = {"ADdev was here!", "Mind the cactus!", "Git gud, human.", "Thanks for testing!", "Tetris loves you.", "Stay hydrated!"};
static const char* SECRETS_FR[6] = {"ADdev etait la !", "Attention au cactus !", "Courage, humain.", "Merci d'avoir teste !", "Tetris t'aime.", "Pense a boire !"};
static int aTop = 0, aGame = 0, aLvl = 3, aSkinPick = 0, aSecret = 0;
static void adminNote(const char* t) { aMsg = t; aMsgUntil = millis() + 1600; }
static void jingle() { static const int F[7] = {523, 587, 659, 784, 880, 1047, 1319}; for (int i = 0; i < 8; i++) sfx(F[rnd(0, 6)], 70, i == 0); }
static void renderAdmin() {
  ui::bg((int)(millis() / 60));
  ui::header(aSel <= A_DEBUG ? "ADMIN - DEBUG" : "ADMIN - FUN", aSel <= A_DEBUG ? P_R : P_S);
  const int VISA = 6, RH = 16, LY = 20;
  if (aSel < aTop) aTop = aSel;
  if (aSel > aTop + VISA - 1) aTop = aSel - (VISA - 1);
  canvas.fillRect(3, LY - 1, SCR_W - 6, VISA * RH + 2, P_K); canvas.fillRect(4, LY, SCR_W - 8, VISA * RH, P_B);
  for (int i = aTop; i < aTop + VISA && i < NADM; i++) {
    int y = LY + (i - aTop) * RH; bool s = i == aSel; int vx = SCR_W - 15;
    if (s) { canvas.fillRect(4, y, SCR_W - 8, RH, P_K); canvas.fillRect(5, y + 1, SCR_W - 10, RH - 2, P_G); canvas.fillRect(5, y + 1, SCR_W - 10, 1, P_B); canvas.fillTriangle(8, y + 3, 8, y + 12, 14, y + 7, P_K); canvas.fillTriangle(8, y + 5, 8, y + 10, 12, y + 7, P_R); }
    else canvas.fillRect(4, y + RH - 1, SCR_W - 8, 1, P_L);
    txt((g_lang ? ADM_FR : ADM_EN)[i], 18, y, &fonts::Font2, P_K);
    switch (i) {
      case A_UNL: toggle(vx - 30, y + 2, g_unlockAll); break;
      case A_NOINTRO: toggle(vx - 30, y + 2, g_noIntro); break;
      case A_FPS: toggle(vx - 30, y + 2, g_fps); break;
      case A_CRT: toggle(vx - 30, y + 2, g_crt); break;
      case A_INV: toggle(vx - 30, y + 2, g_invert); break;
      case A_RBW: toggle(vx - 30, y + 2, g_rainbow); break;
      case A_SKIN: txt(skin::name(g_skin), vx, y, &fonts::Font2, P_R, textdatum_t::top_right); break;
      case A_MEDALS: txt((g_lang ? LVL_FR : LVL_EN)[aLvl], vx, y, &fonts::Font2, P_R, textdatum_t::top_right); break;
      case A_GAME: txt(trophy::nameOf(aGame), vx, y, &fonts::Font2, P_R, textdatum_t::top_right); break;
      case A_GMEDAL: txt((g_lang ? LVL_FR : LVL_EN)[trophy::levelOf(aGame)], vx, y, &fonts::Font2, P_R, textdatum_t::top_right); break;
      case A_CLEAR: if (confirmClear && s) txt(L("A again!", "A encore !"), vx, y, &fonts::Font2, P_R, textdatum_t::top_right); break;
    }
  }
  if (aTop > 0) canvas.fillTriangle(SCR_W - 9, LY + 1, SCR_W - 14, LY + 7, SCR_W - 4, LY + 7, P_R);
  if (aTop + VISA < NADM) canvas.fillTriangle(SCR_W - 9, LY + VISA * RH - 1, SCR_W - 14, LY + VISA * RH - 7, SCR_W - 4, LY + VISA * RH - 7, P_R);
  ui::footer(L("B:down PWR:up A:do", "B:bas PWR:haut A:agir"));
  if (millis() < aMsgUntil) { int w = (int)strlen(aMsg) * 8 + 16; if (w > 226) w = 226; canvas.fillRect(SCR_W / 2 - w / 2, 96, w, 20, P_K); canvas.fillRect(SCR_W / 2 - w / 2 + 1, 97, w - 2, 18, P_S); canvas.fillRect(SCR_W / 2 - w / 2 + 1, 97, w - 2, 1, P_B); txt(aMsg, SCR_W / 2, 98, &fonts::Font2, P_K, textdatum_t::top_center); }
}
static void adminLoop() {
  bool a = M5.BtnA.wasPressed(), b = M5.BtnB.wasPressed(), p = M5.BtnPWR.wasPressed();
  if (b) { aSel = (aSel + 1) % NADM; confirmClear = false; sfx(1200, 15); }
  else if (p) { aSel = (aSel + NADM - 1) % NADM; confirmClear = false; sfx(1000, 15); }
  else if (a) {
    int sk = skin::count(), ok = 1;
    switch (aSel) {
      case A_UNL: g_unlockAll = !g_unlockAll; saveSettings(); break;
      case A_SKIN: aSkinPick = (g_skin + 1) % sk; g_skin = aSkinPick; skView = g_skin; saveSettings(); break;
      case A_NOINTRO: g_noIntro = !g_noIntro; saveSettings(); break;
      case A_FPS: g_fps = !g_fps; break;
      case A_MEDALS: trophy::adminSetAll(aLvl); adminNote(L("Medals set", "Medailles fixees")); aLvl = (aLvl + 1) % 4; sfx(784, 70); sfx(1319, 120, false); ok = 0; break;
      case A_GAME: aGame = (aGame + 1) % trophy::T_COUNT; break;
      case A_GMEDAL: trophy::adminSet(aGame, (trophy::levelOf(aGame) + 1) % 4); sfx(784, 50); sfx(1100, 60, false); ok = 0; break;
      case A_CLEAR: if (!confirmClear) { confirmClear = true; sfx(500, 60); } else { trophy::adminClear(); g_skin = g_unlockAll ? g_skin : 0; skView = g_skin; saveSettings(); confirmClear = false; adminNote(L("Records cleared", "Records effaces")); sfx(300, 120); } ok = 0; break;
      case A_TRIES: daily::adminReset(-1); adminNote(L("Tries reset", "Essais remis a 0")); break;
      case A_STREAK: daily::adminReset(7); adminNote(L("Streak = 7", "Serie = 7")); break;
      case A_SETUP: admin = false; wizard::begin(); ok = 0; break;
      case A_CRT: g_crt = !g_crt; saveSettings(); break;
      case A_INV: g_invert = !g_invert; applyFx(); saveSettings(); break;
      case A_RBW: g_rainbow = !g_rainbow; saveSettings(); break;
      case A_CONF: confetti(); jingle(); ok = 0; break;
      case A_BEEP: jingle(); ok = 0; break;
      case A_MSG: adminNote((g_lang ? SECRETS_FR : SECRETS_EN)[aSecret]); aSecret = (aSecret + 1) % 6; confetti(); ok = 0; break;
      case A_BACK: admin = false; sfx(1000, 20); ok = 0; break;
    }
    if (ok) { sfx(1100, 40); sfx(1500, 60, false); }
  }
  renderAdmin(); present(0, 0); delay(10);
}

void loop() {
  if (session::dateEditing()) { session::dateLoop(); return; }
  if (admin) { adminLoop(); return; }
  if (about) {
    if (M5.BtnA.wasPressed()) {                       // M5 : ne fait rien... sauf 6 appuis rapides = mode admin
      uint32_t now = millis(); aTaps = (now - aLast > 1500) ? 1 : aTaps + 1; aLast = now;
      if (aTaps >= 6) { aTaps = 0; admin = true; aSel = 0; aTop = 0; confirmClear = false; sfx(660, 60); sfx(990, 60, false); sfx(1320, 120, false); }
    }
    if (M5.BtnB.wasPressed() || M5.BtnPWR.wasPressed()) { about = false; aTaps = 0; sfx(1000, 20); }
    renderAbout(); present(0, 0); delay(10); return;
  }
  if (stabStart) {
    if (millis() - stabStart >= 3000) { tilt::zero(); stabStart = 0; sfx(1320, 80); sfx(1760, 120, false); }
  } else {
    bool a = M5.BtnA.wasPressed(), b = M5.BtnB.wasPressed(), p = M5.BtnPWR.wasPressed();
    if (b) { sel = (sel + 1) % NROWS; sfx(1200, 15); }
    else if (p) { sel = (sel + NROWS - 1) % NROWS; sfx(1000, 15); }
    else if (a) {
      switch (sel) {
        case R_BRI: g_bright = (g_bright + 1) % 5; applyBrightness(); saveSettings(); sfx(1100, 20); break;
        case R_VOL: g_vol = (g_vol + 1) % 6; applyVolume(); saveSettings(); sfx(1100, 40); sfx(1500, 60, false); break;
        case R_MUS: g_music = !g_music; saveSettings(); sfx(1100, 40); sfx(1500, 60, false); break;
        case R_CTRL: g_ctrl = (g_ctrl + 1) % 3; saveSettings(); sfx(1100, 20); sfx(1400, 30, false); break;
        case R_SCREEN: g_flip = !g_flip; applyRotation(); saveSettings(); sfx(1100, 40); sfx(1500, 60, false); break;
        case R_DIFF: g_diff = (g_diff + 1) % 3; saveSettings(); sfx(1000 + g_diff * 250, 40); sfx(1400 + g_diff * 250, 50, false); break;
        case R_SKIN: skView = (skView + 1) % skin::count(); if (skin::unlocked(skView)) { g_skin = skView; saveSettings(); sfx(1100, 20); sfx(1500, 30, false); } else sfx(300, 70); break;
        case R_THEME: g_theme = (g_theme + 1) % themeCount(); saveSettings(); sfx(1100, 20); sfx(1400, 30, false); break;
        case R_SLEEP: g_sleepIdx = (g_sleepIdx + 1) % 5; saveSettings(); sfx(1100, 20); break;
        case R_STAB: stabStart = millis(); sfx(880, 40); break;
        case R_CAL: tilt::startCalibration(); break;
        case R_LANG: g_lang ^= 1; saveSettings(); sfx(1100, 40); sfx(1500, 60, false); break;
        case R_DATE: session::dateBegin(); sfx(1100, 30); break;
        case R_ABOUT: about = true; sfx(1100, 30); break;
        case R_BACK: g_quit = true; break;
      }
    }
  }
  render();
  present(0, 0);
  delay(10);
}

void icon(int x, int y) {
  ui::tile(x, y, P_M);
  int cx = x + 32, cy = y + 32;
  for (int k = 0; k < 8; k++) { float a = k * 0.7854f; int tx = cx + (int)lroundf(cosf(a) * 21), ty = cy + (int)lroundf(sinf(a) * 21); pxBox(tx - 5, ty - 5, 10, 10, P_L, P_B, P_D); }
  pxDisc(cx, cy, 19, P_L, P_B, P_D);
  pxDisc(cx, cy, 9, P_K, P_K, P_K); canvas.fillRect(cx - 7, cy - 7, 14, 14, P_M); canvas.fillRect(cx - 7, cy - 7, 14, 14, P_M);
  pxDisc(cx, cy, 7, P_D, P_M, P_K);
  canvas.fillRect(cx - 12, cy - 14, 4, 2, P_B);
}

}  // namespace settings
