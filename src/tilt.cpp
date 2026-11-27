// ============================================================================
//  Inclinaison via l'accelerometre (MPU6886) avec calibrage automatique des axes
// ============================================================================
#include "common.h"

namespace tilt {

static Preferences pr;
static int   axX = 0, axY = 1;           // quel axe de l'accelerometre = gauche/droite, haut/bas
static float sgX = 1, sgY = 1;           // sens
static float sgZ = 1;                    // sens de l'axe perpendiculaire a l'ecran (+ = l'ecran regarde vers le haut quand le M5 est a plat)
static float zeroV[3] = {0, 0, 0};       // position "a plat"
static float filt[3]  = {0, 0, 0};
static bool  ok = false;

static bool  calib = false;
static int   step = 0;                   // 0 = a plat, 1 = cote droit en bas, 2 = haut de l'ecran en bas
static bool  capturing = false;
static uint32_t capStart = 0; static int capN = 0; static float capSum[3];
static float base[3];
static const char* warn = nullptr;
static const float SCALE = 0.45f;        // 0.45 g (~27 degres) = pleine inclinaison

// Appareil sans capteur de mouvement (ex. Cardputer) : les touches fleches simulent l'inclinaison (g_kbdX / g_kbdY = -1, 0 ou 1)
static bool virt() { return !M5.Imu.isEnabled(); }
static float vx = 0, vy = 0;

static void readRaw(float* a) {
  float x = 0, y = 0, z = 0;
  M5.Imu.getAccel(&x, &y, &z);
  a[0] = x; a[1] = y; a[2] = z;
}

void load() {
  pr.begin("tilt", false);
  ok = pr.getBool("ok", false);
  if (virt()) { ok = true; axX = 0; axY = 1; sgX = sgY = sgZ = 1; zeroV[0] = zeroV[1] = 0; zeroV[2] = 1; return; }
  if (ok) {
    axX = pr.getInt("ax", 0); axY = pr.getInt("ay", 1);
    sgX = pr.getInt("sx", 1) >= 0 ? 1.f : -1.f; sgY = pr.getInt("sy", 1) >= 0 ? 1.f : -1.f;
    sgZ = pr.getInt("sz", 1) >= 0 ? 1.f : -1.f;
    if (axX < 0 || axX > 2 || axY < 0 || axY > 2 || axX == axY) ok = false;
  }
}
bool valid() { return ok; }
bool calibrating() { return calib; }

void startCalibration() { if (virt()) return; calib = true; step = 0; capturing = false; warn = nullptr; }

void update() {
  if (virt()) { vx += 0.35f * (g_kbdX - vx); vy += 0.35f * (g_kbdY - vy); return; }
  float a[3]; readRaw(a);
  for (int i = 0; i < 3; i++) filt[i] += 0.35f * (a[i] - filt[i]);
}

static int mode = 0;                     // 0 = a plat, 1 = debout (paysage, comme une console)
static void detectMode() { int axZ = 3 - axX - axY; mode = fabsf(sgZ * zeroV[axZ]) > 0.7f ? 0 : 1; }
int posture() { return mode; }

void zero() {
  if (virt()) return;
  float s[3] = {0, 0, 0}; const int N = 10;
  for (int i = 0; i < N; i++) { float a[3]; readRaw(a); for (int k = 0; k < 3; k++) s[k] += a[k]; delay(6); }
  for (int k = 0; k < 3; k++) { zeroV[k] = s[k] / N; filt[k] = zeroV[k]; }
  detectMode();
}

// Au lancement d'un jeu : on compare la pose actuelle a celle memorisee au demarrage.
// Differente (par ex. demarre a plat, joue debout) -> on re-stabilise silencieusement, sinon on garde la stabilisation unique.
bool adapt() {
  if (!ok || virt()) return false;
  float s[3] = {0, 0, 0}; const int N = 6;
  for (int i = 0; i < N; i++) { float a[3]; readRaw(a); for (int k = 0; k < 3; k++) s[k] += a[k]; delay(5); }
  float dmax = 0; for (int k = 0; k < 3; k++) { s[k] /= N; float d = fabsf(s[k] - zeroV[k]); if (d > dmax) dmax = d; }
  if (dmax < 0.30f) return false;
  for (int k = 0; k < 3; k++) { zeroV[k] = s[k]; filt[k] = s[k]; }
  detectMode();
  return true;
}

static float xRaw() { return ok ? clampf(sgX * (filt[axX] - zeroV[axX]) / SCALE, -1.f, 1.f) : 0.f; }
float x() { if (virt()) return vx; return g_flip ? -xRaw() : xRaw(); }   // ecran retourne : gauche/droite inverses
// y() s'adapte a la facon dont on tient le M5 au moment de la stabilisation :
//  - a plat (ecran vers le haut) : le haut de l'ecran qui descend = + (axe Y de l'accelerometre)
//  - debout / en paysage (comme une Game Boy) : on pousse le haut de l'ecran vers l'avant = + (axe perpendiculaire a l'ecran)
//  - entre les deux (tenu en biais) : melange continu. Dans tous les cas, gauche/droite = pencher le cote droit vers le bas.
static float yRaw();
float y() { if (virt()) return vy; return g_flip ? -yRaw() : yRaw(); }
static float yRaw() {
  if (!ok) return 0.f;
  int axZ = 3 - axX - axY;
  float u0 = sgY * zeroV[axY], w0 = sgZ * zeroV[axZ];
  float n = sqrtf(u0 * u0 + w0 * w0);
  float pu = 1.f, pw = 0.f;                                  // direction de bascule "haut vers le bas" dans le plan (Y ecran, perpendiculaire)
  if (n > 0.5f) { pu = w0 / n; pw = -u0 / n; }
  float du = sgY * (filt[axY] - zeroV[axY]), dw = sgZ * (filt[axZ] - zeroV[axZ]);
  return clampf((du * pu + dw * pw) / SCALE, -1.f, 1.f);
}

// ----------------------------- assistant ------------------------------------
static void drawDevice(int cx, int cy, int hi) {
  // le M5 vu de face, a l'horizontale ; hi : 0 = rien, 1 = cote droit, 2 = haut  (pixel art, palette commune)
  pxBox(cx - 38, cy - 15, 76, 30, P_M, P_L, P_D);
  canvas.fillRect(cx - 32, cy - 10, 46, 20, P_K); canvas.fillRect(cx - 31, cy - 9, 44, 18, P_D);
  canvas.fillRect(cx - 28, cy - 5, 14, 2, P_S); canvas.fillRect(cx - 28, cy - 1, 22, 2, P_L); canvas.fillRect(cx - 28, cy + 3, 10, 2, P_G);
  pxDisc(cx + 24, cy, 5, P_R, P_G, P_D);
  canvas.fillRect(cx + 17, cy - 12, 3, 4, P_K); canvas.fillRect(cx + 17, cy + 8, 3, 4, P_K);
  if (hi == 1) { canvas.fillRect(cx + 40, cy - 15, 3, 30, P_G); canvas.fillTriangle(cx + 47, cy - 5, cx + 47, cy + 5, cx + 55, cy, P_G); canvas.drawTriangle(cx + 47, cy - 5, cx + 47, cy + 5, cx + 55, cy, P_K); }
  if (hi == 2) { canvas.fillRect(cx - 38, cy - 18, 76, 3, P_G); canvas.fillTriangle(cx - 6, cy - 31, cx + 6, cy - 31, cx, cy - 22, P_G); canvas.drawTriangle(cx - 6, cy - 31, cx + 6, cy - 31, cx, cy - 22, P_K); }
}

void calibrationStep() {
  update();
  const char* help[3] = {L("Hold the M5 flat and still","Tiens le M5 a plat, immobile"), L("Tilt the RIGHT side down","Penche le cote DROIT en bas"), L("Tilt the TOP of the screen down","Penche le HAUT de l'ecran en bas")};
  uint32_t tt = millis() / 60;
  ui::bg((int)tt);
  char ti[32]; snprintf(ti, sizeof ti, "%s %d/3", L("CALIBRATION", "CALIBRAGE"), step + 1);
  ui::window(6, 3, 228, 112, step == 0 ? P_S : step == 1 ? P_G : P_R, ti);
  int bob = (int)((tt / 6) & 1);
  drawDevice(SCR_W / 2, 59 + (capturing ? 0 : bob), step);
  for (int i = 0; i < 3; i++) { int dx = SCR_W / 2 - 14 + i * 14; pxDisc(dx, 28, 3, i < step ? P_S : (i == step ? P_G : P_L), P_B, P_D); }
  txt(help[step], SCR_W / 2, 84, &fonts::Font0, P_K, textdatum_t::top_center);
  if (warn) { canvas.fillRect(20, 94, 200, 11, P_R); canvas.drawRect(20, 94, 200, 11, P_K); txt(warn, SCR_W / 2, 96, &fonts::Font0, P_B, textdatum_t::top_center); }
  else if (capturing) { uint32_t el = millis() - capStart; int w = (int)(el < 900 ? el : 900) * 196 / 900; canvas.fillRect(20, 96, 200, 9, P_K); canvas.fillRect(22, 98, w, 5, P_S); }
  ui::footer(capturing ? L("hold still...", "ne bouge plus...") : L("A : capture, then stay still", "A : mesurer, puis immobile"));

  if (!capturing) {
    if (M5.BtnA.wasPressed()) { capturing = true; capStart = millis(); capN = 0; capSum[0] = capSum[1] = capSum[2] = 0; warn = nullptr; }
  } else {
    uint32_t el = millis() - capStart;
    if (el > 400) { float a[3]; readRaw(a); for (int k = 0; k < 3; k++) capSum[k] += a[k]; capN++; }   // 400 ms de latence, puis moyenne
    if (el > 900 && capN > 0) {
      float v[3]; for (int k = 0; k < 3; k++) v[k] = capSum[k] / capN;
      capturing = false;
      if (step == 0) { for (int k = 0; k < 3; k++) base[k] = v[k]; step = 1; sfx(880, 60); }
      else {
        float d[3]; for (int k = 0; k < 3; k++) d[k] = v[k] - base[k];
        int best = -1; float bv = 0;
        for (int k = 0; k < 3; k++) { if (step == 2 && k == axX) continue; if (fabsf(d[k]) > bv) { bv = fabsf(d[k]); best = k; } }
        if (best < 0 || bv < 0.20f) { warn = L("not enough tilt, try again","pas assez incline, recommence"); sfx(200, 150); }
        else if (step == 1) { axX = best; sgX = (d[best] > 0 ? 1.f : -1.f) * (g_flip ? -1.f : 1.f); step = 2; sfx(880, 60); }
        else {
          axY = best; sgY = (d[best] > 0 ? 1.f : -1.f) * (g_flip ? -1.f : 1.f);
          for (int k = 0; k < 3; k++) zeroV[k] = base[k];
          pr.putBool("ok", true); pr.putInt("ax", axX); pr.putInt("ay", axY); sgZ = base[3 - axX - axY] >= 0 ? 1.f : -1.f; pr.putInt("sz", (int)sgZ);
          pr.putInt("sx", (int)sgX); pr.putInt("sy", (int)sgY);
          ok = true; calib = false; detectMode(); sfx(1320, 80); sfx(1760, 120, false);
        }
      }
    }
  }
  present(0, 0);
}

}  // namespace tilt
