// ============================================================================
//  AVION - vol en pseudo-3D. M5 a l'horizontale : pencher = virer / monter / descendre.
//  M5 (A) maintenu = turbo ; B (cote) maintenu = frein ; HAUT (PWR) = pause
//  Passe dans les anneaux dores, evite les tours et le sol, ramasse les orbes bleus (turbo plein).
// ============================================================================
#include "common.h"

namespace sky {

static const uint32_t STEP_US = 20000;
static const float F = 105.0f;                 // focale
static const int   HZ0 = 46;                   // ligne d'horizon au repos
static const float NEAR_Z = 9.0f, FAR_Z = 460.0f;
static const float RING_R = 14.0f, TOWER_W = 6.0f;
static const float CS = 44.0f;                 // taille d'un champ (unites monde)
static const float SC = 30.0f;                 // pas du decor en profondeur
static const int   SKYH = 58;                  // hauteur du degrade de ciel

static const uint16_t SKY0 = rgb(24, 60, 160), SKY1 = rgb(96, 160, 230), HAZE = rgb(236, 208, 184);
static const uint16_t ROADC = rgb(84, 86, 100), GRAVEL = rgb(122, 114, 104), LINEC = rgb(238, 238, 230), YELC = rgb(250, 214, 80);
static const uint16_t W1 = rgb(46, 118, 205), W2 = rgb(74, 146, 226), SAND = rgb(222, 204, 140), HEDGE = rgb(32, 90, 42);
static uint16_t FC[8][4][2];                   // palette des champs : [type][ombre][bande]

enum State { TITLE, PLAY, PAUSED, OVER };
static State state = TITLE;
static Preferences pr;
static uint32_t last = 0, acc = 0;

struct Obj { float x, y, z; uint8_t type, var; bool done, on; };      // 0 anneau, 1 tour (y = hauteur), 2 orbe turbo
static Obj objs[16];
struct Pop { float y; int t; char s[10]; uint16_t col; }; static Pop pops[3];
struct Part { float x, y; int life; uint8_t kind; }; static Part pts[20];          // trainees : 0 contrail, 1 moteur, 2 fumee
static float cloudX[7] = {10, 70, 130, 190, 250, 40, 160}, cloudY[7] = {6, 14, 24, 9, 30, 36, 40};
static const float cloudPar[7] = {0.12f, 0.2f, 0.3f, 0.16f, 0.4f, 0.25f, 0.5f};

static float camX = 0, alt = 24, vx = 0, vy = 0, speed = 2.1f, boost = 100, roll = 0, rollV = 0, camZ = 0, spawnAcc = 0, lastRx = 0, lastRy = 24, pitchOff = 0;
static int lives = 3, invul = 0, combo = 0, steps = 0, animT = 0, endT = 0, shakeT = 0, shX = 0, shY = 0, curHZ = HZ0;
static uint32_t score = 0, best = 0, bonus = 0;
static bool boosting = false, boostLock = false, needRelease = true;

static inline uint32_t hash2(int a, int b) { uint32_t h = (uint32_t)a * 374761393u + (uint32_t)b * 668265263u; h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16); }
static void initTables() {
  static const uint16_t base[8] = {rgb(88, 168, 64), rgb(66, 142, 54), rgb(112, 184, 74), rgb(198, 180, 74), rgb(150, 112, 72), rgb(78, 158, 86), rgb(44, 112, 56), rgb(172, 192, 82)};
  for (int t = 0; t < 8; t++) for (int s = 0; s < 4; s++) { uint16_t c = mixc(base[t], TFT_BLACK, s * 16); FC[t][s][0] = c; FC[t][s][1] = mixc(c, TFT_WHITE, 20); }
}

static void pop(const char* s, uint16_t col) { for (auto &p : pops) if (p.t <= 0) { p.y = 58; p.t = 40; p.col = col; strncpy(p.s, s, 9); p.s[9] = 0; return; } }
static void put(float x, float y, float z, uint8_t t) { for (auto &o : objs) if (!o.on) { o = {x, y, z, t, (uint8_t)rnd(0, 3), false, true}; return; } }
static void emit(float x, float y, uint8_t kind) { for (auto &p : pts) if (p.life <= 0) { p = {x, y, kind == 2 ? 26 : 16, kind}; return; } }

static void spawnAt(float z) {
  int towerP = 28 + (steps / 60 > 20 ? 20 : steps / 60);
  if ((int)rnd(0, 99) < towerP) {
    int n = rnd(0, 99) < 35 ? 2 : 1;
    for (int i = 0; i < n; i++) put((float)((int)rnd(0, 130) - 65), (float)(22 + (int)rnd(0, 36)), z + i * 14, 1);
  } else {
    lastRx = clampf(lastRx + (int)rnd(0, 56) - 28, -48, 48);
    lastRy = clampf(lastRy + (int)rnd(0, 30) - 15, 13, 44);
    put(lastRx, lastRy, z, 0);
    if (rnd(0, 99) < 14) put(clampf(lastRx + (int)rnd(0, 40) - 20, -55, 55), clampf(lastRy + (int)rnd(0, 16) - 8, 10, 48), z + 45, 2);
  }
}

static void reset() {
  camX = 0; alt = 24; vx = vy = 0; speed = 2.1f * dif(0.9f, 1.0f, 1.1f); boost = 100; roll = rollV = 0; camZ = 0; spawnAcc = 0; lastRx = 0; lastRy = 24; pitchOff = 0;
  lives = difi(5, 3, 2); invul = 0; combo = 0; steps = 0; endT = 0; shakeT = 0; score = 0; bonus = 0; boosting = false; boostLock = false; needRelease = true;
  for (auto &o : objs) o.on = false;
  for (auto &p : pops) p.t = 0;
  for (auto &p : pts) p.life = 0;
  for (float z = 150; z <= 450; z += 100) spawnAt(z);
}
void begin() {
  initTables();
  pr.begin(dns("sky"), false); best = pr.getInt("best", 0);
  reset(); state = TITLE; last = 0; acc = 0;
}

static void gameOver() {
  state = OVER; endT = 0;
  trophy::record(trophy::T_FLIGHT, score); if (score > best) { best = score; pr.putInt("best", best); }
  sfx(300, 120); sfx(120, 380, false);
}
static void crash() {
  if (--lives <= 0) { gameOver(); return; }
  invul = 110; combo = 0; alt = 26; vy = 0; speed = 1.6f; boost = boost < 50 ? 50 : boost; shakeT = 18;
  for (auto &o : objs) if (o.on && o.z < 160) o.on = false;
  pop(L("BOOM!","BOUM !"), rgb(240, 80, 60)); sfx(180, 90); sfx(100, 160, false);
}
static float dz(float v) { return fabsf(v) < 0.07f ? 0 : (v > 0 ? v - 0.07f : v + 0.07f); }
static void planePos(int &px, int &py) { px = SCR_W / 2 + (int)(vx * 5) + shX; py = 100 - (int)(vy * 4) + shY + (boosting ? 2 : 0); }

static void step() {
  animT++;
  for (auto &p : pops) if (p.t > 0) { p.t--; p.y -= 0.5f; }
  for (auto &p : pts) if (p.life > 0) { p.life--; p.x += (SCR_W / 2 - p.x) * 0.07f; p.y += (curHZ + 8 - p.y) * 0.07f - (p.kind == 2 ? 0.2f : 0.f); }
  if (state == PAUSED) return;
  if (shakeT > 0) { shakeT--; shX = (int)rnd(0, 6) - 3; shY = (int)rnd(0, 4) - 2; }
  else if (boosting) { shX = (int)rnd(0, 2) - 1; shY = (int)rnd(0, 2) - 1; }
  else shX = shY = 0;
  float pt = clampf(vy * 2.4f, -7, 7);
  pitchOff += (pt - pitchOff) * 0.1f; curHZ = HZ0 + (int)pitchOff;
  if (state == TITLE) { camZ += 1.6f; camX = sinf(animT * 0.02f) * 30; alt = 24 + sinf(animT * 0.03f) * 4; vx = cosf(animT * 0.02f) * 1.2f; roll = cosf(animT * 0.02f) * 0.5f; vy = 0; return; }
  if (state == OVER) {
    endT++; speed *= 0.97f; camZ += speed; roll += 0.09f; vy -= 0.04f; alt += vy; if (alt < 2.5f) alt = 2.5f;
    int px, py; planePos(px, py); if (endT < 70 && (endT & 1)) emit((float)px, (float)py + 6, 2);
    for (auto &o : objs) if (o.on) o.z -= speed;
    return;
  }
  // ----- PLAY -----
  tilt::update();
  steps++;
  float tx = dz(tilt::x()), ty = dz(tilt::y());
  vx += (tx * 3.6f - vx) * 0.14f; vy += (-ty * 2.8f - vy) * 0.14f;
  if (fabsf(camX) > 70 && camX * vx > 0) vx *= 0.8f;                    // bords : on ralentit en douceur
  if (alt > 64 && vy > 0) vy *= 0.8f;
  camX = clampf(camX + vx, -78, 78); alt = clampf(alt + vy, 2.5f, 72);
  float rt = clampf(vx / 3.0f, -1, 1); rollV += (rt - roll) * 0.12f; rollV *= 0.78f; roll += rollV;     // ressort : un peu de rebond
  bool a = M5.BtnA.isPressed(), b = M5.BtnB.isPressed();
  if (!a) needRelease = false;
  if (boost <= 0) boostLock = true;
  if (boost >= 25) boostLock = false;
  boosting = a && !needRelease && !boostLock && !b;
  if (boosting) { boost -= 1.0f; if ((steps & 7) == 0) sfx(330 + (steps & 15) * 6, 30, false); }
  else if (boost < 100) boost += a ? 0.1f : 0.24f;
  float base = (2.1f + (steps * 0.0006f > 1.5f ? 1.5f : steps * 0.0006f)) * dif(0.9f, 1.0f, 1.1f);
  float target = boosting ? base * 1.75f : (b ? base * 0.6f : base);
  speed += (target - speed) * 0.08f;
  camZ += speed; score = (uint32_t)(camZ / 12.0f) + bonus;
  { int px, py; planePos(px, py); float k = 1.0f, wy = clampf(sinf(roll) * 1.15f, -1.2f, 1.2f) * 9 * k;
    if (boosting && (steps & 1)) { emit(px - 27 * k, py + 8 - wy, 0); emit(px + 27 * k, py + 8 + wy, 0); }
    if ((steps % 3) == 0) emit((float)px, (float)py + 9, boosting ? 1 : 0); }
  spawnAcc += speed;
  float gap = 100 - (steps * 0.01f > 25 ? 25 : steps * 0.01f);
  if (spawnAcc >= gap) { spawnAcc -= gap; spawnAt(FAR_Z); }
  if (invul > 0) invul--;
  for (auto &o : objs) if (o.on) {
    o.z -= speed;
    if (!o.done && o.z <= NEAR_Z) {
      o.done = true;
      float dx = o.x - camX, dy = o.y - alt;
      if (o.type == 0) {
        float d = sqrtf(dx * dx + dy * dy);
        if (d < RING_R - 2) {
          combo++; int c = combo > 8 ? 8 : combo; int pts2 = 10 + c * 5 + (d < 4 ? 15 : 0); bonus += pts2;
          char s[10]; snprintf(s, sizeof s, d < 4 ? "TOP +%d" : "+%d", pts2); pop(s, rgb(255, 210, 50));
          sfx(900 + c * 110, 40); sfx(1350 + c * 110, 60, false);
        } else { combo = 0; pop(L("MISS","RATE"), rgb(255, 255, 255)); sfx(240, 90); }
      } else if (o.type == 1) {
        if (invul == 0 && fabsf(dx) < TOWER_W + 3 && alt < o.y + 2) { o.on = false; crash(); if (state != PLAY) return; }
      } else {
        if (fabsf(dx) < 10 && fabsf(dy) < 10) { boost = 100; boostLock = false; bonus += 5; pop("TURBO", rgb(90, 220, 255)); sfx(1500, 40); sfx(2000, 70, false); }
      }
    }
    if (o.z < 2) o.on = false;
  }
  if (alt <= 3.2f && invul == 0) { crash(); if (state != PLAY) return; }
}

// ---------------------------------------------------------------------------
static inline int fogT(float z) { int t = (int)((z - 110) * 256 / (FAR_Z - 110)); return t < 0 ? 0 : (t > 256 ? 256 : t); }
static inline uint16_t fogc(uint16_t c, int t) { return t ? mixc(c, HAZE, t) : c; }

// avion vu de derriere : ailes, empennage, derive, verriere, tuyere + flamme
static void drawPlane(int px, int py, float rl, bool flame, float k, bool blink = false) {
  if (blink) return;
  const uint16_t body = rgb(238, 242, 252), shade = rgb(150, 162, 196), dk = rgb(48, 54, 92), red = rgb(228, 62, 62), redD = rgb(150, 32, 48), cock = rgb(90, 190, 255), wingA = rgb(214, 222, 240), wingB = rgb(120, 134, 176);
  auto S = [&](float v) { return (int)(v * k); };
  int tip = S(27), rt = S(3), wy = (int)(rl * 9 * k);
  int tyL = py + S(5) - wy, tyR = py + S(5) + wy;
  if (flame) {                                                                                     // flamme du turbo
    int fy = py + S(10), fr = S(6) + (animT & 1);
    canvas.fillCircle(px, fy, fr + 2, rgb(255, 90, 30)); canvas.fillCircle(px, fy, fr, rgb(255, 160, 50)); canvas.fillCircle(px, fy, S(3) + 1, rgb(255, 250, 210));
  }
  canvas.fillTriangle(px - S(3), py + S(6), px - S(12), py + S(10) + wy / 3, px - S(3), py + S(10), shade);   // empennage
  canvas.fillTriangle(px + S(3), py + S(6), px + S(12), py + S(10) - wy / 3, px + S(3), py + S(10), shade);
  canvas.fillTriangle(px - rt, py - S(1), px - rt, py + S(8), px - tip, tyL + S(3), wingA);                   // ailes : avant clair, arriere sombre
  canvas.fillTriangle(px - tip, tyL + S(3), px - tip, tyL + S(7), px - rt, py + S(8), wingB);
  canvas.fillTriangle(px + rt, py - S(1), px + rt, py + S(8), px + tip, tyR + S(3), wingA);
  canvas.fillTriangle(px + tip, tyR + S(3), px + tip, tyR + S(7), px + rt, py + S(8), wingB);
  canvas.fillTriangle(px - S(9), py + S(2) + wy / 3 * 0 + (tyL - py - S(5)) / 3, px - S(14), tyL + S(3) - 0, px - S(14), tyL + S(6), red);   // bande rouge d'aile
  canvas.fillTriangle(px + S(9), py + S(2) + (tyR - py - S(5)) / 3, px + S(14), tyR + S(3), px + S(14), tyR + S(6), red);
  canvas.drawLine(px - rt, py - S(1), px - tip, tyL + S(3), dk); canvas.drawLine(px + rt, py - S(1), px + tip, tyR + S(3), dk);
  bool bl = (animT / 6) & 1;
  canvas.fillCircle(px - tip, tyL + S(3), S(2) + 1, bl ? rgb(255, 120, 120) : rgb(190, 40, 40)); canvas.fillCircle(px + tip, tyR + S(3), S(2) + 1, bl ? rgb(120, 255, 150) : rgb(40, 160, 70));
  canvas.fillEllipse(px, py + S(2), S(5), S(8), body);                                                         // fuselage
  canvas.fillEllipse(px + S(2), py + S(4), S(3), S(6), shade);
  canvas.fillEllipse(px - S(1), py + S(1), S(2), S(5), TFT_WHITE);
  canvas.fillCircle(px, py + S(9), S(2) + 1, dk);                                                              // tuyere
  canvas.fillTriangle(px - S(2), py + S(4), px + S(2), py + S(4), px, py - S(12), red);                        // derive
  canvas.fillTriangle(px, py + S(4), px + S(2), py + S(4), px, py - S(12), redD);
  canvas.fillEllipse(px, py - S(2), S(2) + 1, S(4), cock); canvas.drawPixel(px - 1, py - S(4), TFT_WHITE);     // verriere
}

static void drawRing(int sx, int sy, float s, float z, bool next, float groundY) {
  int r = (int)(RING_R * s); if (r < 2) return;
  int th = r / 6 + 1; if (th > 7) th = 7;
  int t = fogT(z);
  uint16_t cOut = fogc(rgb(176, 108, 20), t), cMid = fogc(rgb(255, 200, 52), t), cIn = fogc(rgb(255, 242, 160), t);
  if (next) { bool p = (animT >> 3) & 1; cMid = p ? rgb(255, 255, 255) : rgb(120, 240, 255); cOut = rgb(40, 150, 200); cIn = rgb(210, 250, 255); }
  if (groundY < SCR_H + 40 && groundY > sy + r) {                                                              // poteau + ombre au sol
    canvas.drawLine(sx, sy + r, sx, (int)groundY, fogc(rgb(170, 140, 70), t));
    int er = r / 2 + 1; canvas.fillEllipse(sx, (int)groundY, er, er / 4 + 1, fogc(rgb(30, 80, 40), t));
  }
  if (th >= 3) { canvas.drawCircle(sx, sy, r + 2, fogc(mixc(rgb(255, 220, 120), HAZE, 150), t)); canvas.drawCircle(sx, sy, r + 1, mixc(cMid, HAZE, 110)); }    // halo
  for (int k = 0; k < th; k++) canvas.drawCircle(sx, sy, r - k, k < (th + 2) / 3 ? cOut : (k >= th - (th + 2) / 3 ? cIn : cMid));
  if (th >= 3) canvas.drawCircle(sx, sy, r - th, mixc(cOut, TFT_BLACK, 90));
  if (r > 10) { canvas.fillCircle(sx - r * 7 / 10, sy - r * 7 / 10, th / 2 + 1, TFT_WHITE); }                  // reflet
}

static void drawTower(int sx, float s, float z, float h, int var) {
  int t = fogT(z);
  int w = (int)(TOWER_W * s); if (w < 1) w = 1;
  int top = curHZ + (int)((alt - h) * s), bot = curHZ + (int)(alt * s);
  if (bot <= top) return;
  int H = bot - top;
  if (var == 0) {                                                                                              // pylone treillis rouge et blanc
    uint16_t cr = fogc(rgb(214, 66, 66), t), cw = fogc(rgb(240, 240, 240), t);
    canvas.fillTriangle(sx - w, bot, sx + w, bot, sx, top, fogc(rgb(150, 150, 170), t));
    canvas.fillTriangle(sx - w + w / 2, bot, sx + w - w / 2, bot, sx, top + H / 3, fogc(rgb(60, 80, 130), t) );
    for (int i = 0; i < 5; i++) { int y0 = top + H * i / 5, y1 = top + H * (i + 1) / 5; int w0 = w * (i) / 5, w1 = w * (i + 1) / 5; uint16_t c = (i & 1) ? cw : cr; canvas.drawLine(sx - w0, y0, sx + w1, y1, c); canvas.drawLine(sx + w0, y0, sx - w1, y1, c); canvas.drawLine(sx - w1, y1, sx + w1, y1, c); }
    canvas.drawLine(sx - w, bot, sx, top, cr); canvas.drawLine(sx + w, bot, sx, top, cr);
    canvas.drawLine(sx, top, sx, top - 4 - w / 2, cw);
    if (animT & 8) canvas.fillCircle(sx, top - 5 - w / 2, w / 4 + 1, rgb(255, 80, 80));
  } else if (var == 1) {                                                                                      // immeuble avec fenetres
    uint16_t c1 = fogc(rgb(120, 140, 178), t), c2 = fogc(rgb(80, 98, 140), t), cwin = fogc(rgb(255, 226, 120), t), cdk = fogc(rgb(40, 52, 86), t);
    canvas.fillRect(sx - w, top, w * 2 * 6 / 10, H, c1); canvas.fillRect(sx - w + w * 2 * 6 / 10, top, w * 2 - w * 2 * 6 / 10, H, c2);
    canvas.fillRect(sx - w - 1, top - 2, w * 2 + 2, 3, fogc(rgb(170, 184, 214), t));
    int rowH = (int)(5 * s); if (rowH < 3) rowH = 3;
    int ww = w / 3 > 1 ? w / 3 : 1;
    for (int y = top + rowH, row = 0; y < bot - 2 && row < 14; y += rowH, row++) for (int c = 0; c < 3; c++) { int x = sx - w + w / 4 + c * (w * 2 / 3 - ww / 2); canvas.fillRect(x, y, ww, rowH > 4 ? 2 : 1, ((row + c * 2 + (int)h) % 3) ? cwin : cdk); }
    canvas.drawLine(sx, top - 2, sx, top - 5 - w / 2, fogc(rgb(200, 200, 210), t));
  } else if (var == 2) {                                                                                      // silo / cheminee rayee
    uint16_t cl = fogc(rgb(236, 236, 236), t), cm = fogc(rgb(196, 200, 206), t), cd = fogc(rgb(140, 146, 160), t), cr = fogc(rgb(214, 70, 60), t);
    int w3 = w * 2 / 3; if (w3 < 1) w3 = 1;
    canvas.fillRect(sx - w, top, w3, H, cl); canvas.fillRect(sx - w + w3, top, w3, H, cm); canvas.fillRect(sx - w + 2 * w3, top, w * 2 - 2 * w3, H, cd);
    for (int i = 0; i < 3; i++) canvas.fillRect(sx - w, top + H * (1 + i * 3) / 10, w * 2, H / 10 + 1, cr);
    canvas.fillEllipse(sx, top, w, w / 2 + 1, cl); canvas.drawEllipse(sx, top, w, w / 2 + 1, cd);
  } else {                                                                                                     // eolienne
    uint16_t cp = fogc(rgb(240, 242, 246), t), cd = fogc(rgb(170, 178, 196), t);
    int pw = w / 3 > 1 ? w / 3 : 1;
    canvas.fillRect(sx - pw, top, pw * 2, H, cp); canvas.drawFastVLine(sx + pw, top, H, cd);
    canvas.fillTriangle(sx - w / 2, bot, sx + w / 2, bot, sx, bot - H / 8, cd);
    float ang = animT * 0.09f + h; int bl = (int)(8 * s); if (bl < 2) bl = 2;
    for (int i = 0; i < 3; i++) { float a2 = ang + i * 2.094f; canvas.drawLine(sx, top, sx + (int)(cosf(a2) * bl), top + (int)(sinf(a2) * bl), cp); if (bl > 6) canvas.drawLine(sx + 1, top, sx + 1 + (int)(cosf(a2) * bl), top + (int)(sinf(a2) * bl), cd); }
    canvas.fillCircle(sx, top, pw + 1, cd);
  }
}

static void drawOrb(int sx, int sy, float s) {
  int r = (int)(5 * s); if (r < 2) return;
  int pl = (int)(sinf(animT * 0.2f) * (r / 4 + 1));
  canvas.fillCircle(sx, sy, r + 3 + pl, rgb(24, 70, 140)); canvas.fillCircle(sx, sy, r + 1 + pl / 2, rgb(40, 130, 210));
  canvas.fillCircle(sx, sy, r, rgb(90, 220, 255)); canvas.fillCircle(sx, sy, r * 6 / 10, rgb(190, 245, 255));
  canvas.fillCircle(sx - r / 3, sy - r / 3, r / 4 + 1, TFT_WHITE);
  if (r > 5) { canvas.drawFastHLine(sx - r - 4, sy, 3, TFT_WHITE); canvas.drawFastHLine(sx + r + 2, sy, 3, TFT_WHITE); canvas.drawFastVLine(sx, sy - r - 4, 3, TFT_WHITE); canvas.drawFastVLine(sx, sy + r + 2, 3, TFT_WHITE); }
}

// ------------------------------------------------------------------ monde : ciel, montagnes, terrain, decor
static uint16_t skyCol(int y) { int t = y * 256 / SKYH; if (t > 256) t = 256; return t < 150 ? mixc(SKY0, SKY1, t * 256 / 150) : mixc(SKY1, HAZE, (t - 150) * 256 / 106); }

static void renderSky(int hz) {
  for (int y = 0; y < hz; y++) canvas.drawFastHLine(0, y, SCR_W, skyCol(y));
  int sunX = 176 - (int)(camX * 0.25f), sunY = hz - 20;                                                       // soleil + halo
  canvas.fillCircle(sunX, sunY, 24, mixc(skyCol(sunY), rgb(255, 236, 190), 60)); canvas.fillCircle(sunX, sunY, 18, mixc(skyCol(sunY), rgb(255, 236, 190), 120));
  canvas.fillCircle(sunX, sunY, 12, rgb(255, 244, 200)); canvas.fillCircle(sunX, sunY, 8, rgb(255, 252, 235));
  for (int i = 0; i < 7; i++) {                                                                               // nuages a 3 profondeurs
    cloudX[i] += 0.08f + cloudPar[i] * 0.2f; if (cloudX[i] > SCR_W + 80) cloudX[i] -= SCR_W + 160;
    float fx = fmodf(cloudX[i] - camX * cloudPar[i] + 4 * (SCR_W + 160), SCR_W + 160) - 80;
    int cx = (int)fx, cy = (int)cloudY[i] + (hz - HZ0) / 2, w = 12 + (int)(cloudPar[i] * 40);
    uint16_t cw = rgb(255, 252, 248), cs = mixc(rgb(200, 212, 238), skyCol(cy), 60 - (int)(cloudPar[i] * 40));
    canvas.fillEllipse(cx, cy + 3, w, 4, cs); canvas.fillCircle(cx - w / 2, cy + 1, w / 3 + 1, cw); canvas.fillCircle(cx + w / 3, cy, w / 3 + 2, cw); canvas.fillCircle(cx, cy - 3, w / 3, cw);
    canvas.fillEllipse(cx, cy + 2, w - 1, 3, cw);
  }
}

static float ridge(float u) { return 1.0f - fabsf(sinf(u)); }
static void renderMountains(int hz) {
  struct Ly { float par, fq, h, amp; uint16_t lit, dark, snowL, snowD; float snowLine; };
  const Ly L3[3] = {
    {0.35f, 0.022f, 16, 15, mixc(rgb(110, 130, 190), HAZE, 150), mixc(rgb(80, 96, 160), HAZE, 150), rgb(252, 252, 255), rgb(206, 218, 246), 20},
    {0.6f, 0.034f, 9, 12, mixc(rgb(110, 98, 160), HAZE, 80), mixc(rgb(74, 66, 128), HAZE, 80), rgb(244, 244, 252), rgb(196, 204, 236), 17},
    {1.0f, 0.05f, 5, 7, rgb(70, 150, 76), rgb(44, 112, 60), 0, 0, 999}};
  for (int l = 0; l < 3; l++) {
    const Ly &y = L3[l]; float off = camX * y.par + l * 91.f; int prev = 0;
    for (int x = 0; x < SCR_W; x += 2) {
      float u = (x + off) * y.fq;
      int h = (int)(y.h + y.amp * (l < 2 ? (0.65f * ridge(u) + 0.35f * ridge(u * 2.3f + 1.7f)) : (0.5f + 0.5f * sinf(u) * 0.8f + 0.2f * sinf(u * 2.7f + 1.f))));
      if (h < 1) h = 1;
      bool lit = h >= prev; prev = h;
      canvas.fillRect(x, hz - h, 2, h, lit ? y.lit : y.dark);
      if (h > y.snowLine) canvas.fillRect(x, hz - h, 2, h - (int)y.snowLine, lit ? y.snowL : y.snowD);
    }
  }
  canvas.fillRect(0, hz - 2, SCR_W, 2, mixc(HAZE, rgb(70, 150, 76), 70));
}

static float riverN(float wx, float wz) {
  float ax = fabsf(wx); if (ax < 70 || ax > 150) return 9;
  float rc = 108.f + 26.f * sinf(wz * 0.0075f + (wx < 0 ? 2.1f : 0.f));
  return fabsf(ax - rc) / 7.0f;
}
static float lakeN(float wx, float wz) {
  const float LS = 220;
  int lx = (int)floorf(wx / LS), lz = (int)floorf(wz / LS);
  uint32_t h = hash2(lx + 91, lz + 17); if (h % 4) return 9;
  float cx = (lx + 0.3f + ((h >> 8) & 127) / 320.f) * LS, cz = (lz + 0.3f + ((h >> 16) & 127) / 320.f) * LS;
  float rx = 30 + ((h >> 4) & 15), rz = 40 + ((h >> 12) & 15);
  if (fabsf(cx) - rx < 34) return 9;                                                                          // jamais sur la piste
  float dx = (wx - cx) / rx, dzz = (wz - cz) / rz; return sqrtf(dx * dx + dzz * dzz);
}
static uint16_t groundAt(float wx, float wz, int stripe, bool nearRow) {
  float ax = fabsf(wx);
  if (ax < 11.f) {                                                                                            // piste
    if (ax < 7.2f) return (ax < 0.9f && (((int)floorf(wz / 16.f)) & 1)) ? YELC : ROADC;
    if (ax < 8.4f) return LINEC;
    return GRAVEL;
  }
  if (ax > 60.f) { float rn = riverN(wx, wz); if (rn < 1.f) return nearRow && ((((int)floorf(wz / 6.f)) + ((int)floorf(wx / 8.f))) & 1) ? W2 : W1; if (rn < 1.35f) return SAND; }
  if (ax > 20.f) { float ln = lakeN(wx, wz); if (ln < 1.f) return nearRow && ((((int)floorf(wz / 6.f)) + ((int)floorf(wx / 8.f))) & 1) ? W2 : W1; if (ln < 1.18f) return SAND; }
  int ix = (int)floorf(wx / CS), iz = (int)floorf(wz / CS); uint32_t h = hash2(ix, iz);
  if (nearRow) { float fx = wx - ix * CS, fz = wz - iz * CS; if (((h >> 20) & 1) && fx < 1.1f) return HEDGE; if (((h >> 21) & 1) && fz < 1.1f) return HEDGE; }
  return FC[h & 7][(h >> 8) & 3][stripe & 1];
}
static void renderGround(int hz) {
  for (int Y = hz; Y < SCR_H; Y++) {
    if (Y == hz) { canvas.drawFastHLine(0, Y, SCR_W, HAZE); continue; }
    if (Y < 0) continue;
    float z = alt * F / (Y - hz);
    int t = (int)((z - 60) * 256 / (FAR_Z - 60)); if (t < 0) t = 0;
    if (t >= 252) { canvas.drawFastHLine(0, Y, SCR_W, HAZE); continue; }
    float zw = z + camZ; bool nr = z < 150;
    int stripe = nr ? (((int)floorf(zw / 7.f)) & 1) : 0;
    int ch = z < 50 ? 4 : (z < 150 ? 8 : 16);
    uint16_t runC = 0; int runX = 0;
    for (int px = 0; px < SCR_W; px += ch) {
      uint16_t c = groundAt(camX + (px + ch * 0.5f - SCR_W / 2) * z / F, zw, stripe, nr);
      if (px == 0) { runC = c; runX = 0; }
      else if (c != runC) { canvas.fillRect(runX, Y, px - runX, 1, t ? mixc(runC, HAZE, t) : runC); runC = c; runX = px; }
    }
    canvas.fillRect(runX, Y, SCR_W - runX, 1, t ? mixc(runC, HAZE, t) : runC);
  }
}

// decor deterministe (arbres, pins, maisons, moulin, rochers) place par cellules de profondeur
static void drawScenery(int type, int sx, int by, float s, int t, uint32_t h) {
  if (type < 7) {                                                                                             // arbre rond
    int w = (int)(1.6f * s); if (w < 1) w = 1; int th = (int)(6 * s), r = (int)(5.5f * s); if (r < 1) r = 1;
    canvas.fillRect(sx - w / 2, by - th, w, th, fogc(rgb(104, 70, 40), t));
    uint16_t g = (h >> 5) & 1 ? rgb(40, 140, 60) : rgb(60, 156, 54);
    canvas.fillCircle(sx, by - th - r / 2, r, fogc(mixc(g, TFT_BLACK, 60), t));
    canvas.fillCircle(sx - r / 4, by - th - r / 2 - r / 5, r * 8 / 10, fogc(g, t));
    if (r > 3) canvas.fillCircle(sx - r / 3, by - th - r * 8 / 10, r / 3, fogc(mixc(g, TFT_WHITE, 70), t));
  } else if (type < 13) {                                                                                      // pin
    int H = (int)(22 * s), W = (int)(8 * s); if (H < 2) H = 2; if (W < 2) W = 2;
    canvas.fillRect(sx - 1, by - H / 6, 2, H / 6, fogc(rgb(96, 64, 38), t));
    uint16_t d = fogc(rgb(22, 88, 52), t), m = fogc(rgb(34, 116, 62), t);
    for (int i = 0; i < 3; i++) { int y0 = by - H / 6 - (H * 5 / 6) * (i + 1) / 3, y1 = by - H / 6 - (H * 5 / 6) * i / 3 + H / 10, w2 = W * (3 - i) / 3 + W / 3; canvas.fillTriangle(sx - w2 / 2, y1, sx + w2 / 2, y1, sx, y0, i & 1 ? m : d); canvas.fillTriangle(sx, y1, sx + w2 / 2, y1, sx, y0, d); }
  } else if (type < 15) {                                                                                      // maison
    int bw = (int)(10 * s), bh = (int)(6 * s), rh = (int)(5 * s); if (bw < 2) bw = 2; if (bh < 2) bh = 2;
    canvas.fillRect(sx - bw / 2, by - bh, bw, bh, fogc(rgb(240, 230, 208), t)); canvas.fillRect(sx + bw / 4, by - bh, bw - bw / 2 - bw / 4 + 1, bh, fogc(rgb(200, 188, 166), t));
    canvas.fillTriangle(sx - bw / 2 - 1, by - bh, sx + bw / 2 + 1, by - bh, sx, by - bh - rh, fogc(rgb(196, 70, 52), t));
    if (bw > 6) { canvas.fillRect(sx - bw / 6, by - bh * 6 / 10, bw / 5 + 1, bh * 6 / 10, fogc(rgb(110, 70, 44), t)); canvas.fillRect(sx - bw * 4 / 10, by - bh * 7 / 10, bw / 6 + 1, bh / 4 + 1, fogc(rgb(120, 180, 230), t)); }
  } else if (type == 15) {                                                                                     // moulin
    int H = (int)(14 * s), W = (int)(5 * s); if (H < 3) H = 3;
    canvas.fillTriangle(sx - W, by, sx + W, by, sx - W / 2, by - H, fogc(rgb(228, 214, 190), t)); canvas.fillTriangle(sx + W, by, sx + W / 2, by - H, sx - W / 2, by - H, fogc(rgb(228, 214, 190), t));
    canvas.fillTriangle(sx + W / 3, by, sx + W, by, sx + W / 2, by - H, fogc(rgb(190, 176, 152), t));
    float ang = animT * 0.05f + (h & 7); int bl = (int)(9 * s); if (bl < 2) bl = 2;
    for (int i = 0; i < 4; i++) { float a2 = ang + i * 1.5708f; int ex = sx + (int)(cosf(a2) * bl), ey = by - H + (int)(sinf(a2) * bl); canvas.drawLine(sx, by - H, ex, ey, fogc(rgb(246, 244, 240), t)); if (bl > 6) canvas.drawLine(sx + 1, by - H, ex + 1, ey, fogc(rgb(150, 120, 90), t)); }
  } else {                                                                                                      // rocher
    int rx = (int)(4 * s), ry = (int)(2.6f * s); if (rx < 1) rx = 1; if (ry < 1) ry = 1;
    canvas.fillEllipse(sx, by - ry / 2, rx, ry, fogc(rgb(122, 120, 126), t)); canvas.fillEllipse(sx - rx / 4, by - ry / 2 - 1, rx * 6 / 10, ry * 6 / 10, fogc(rgb(170, 168, 172), t));
  }
}
static void renderScenery(int hz) {
  int kFar = (int)floorf((camZ + FAR_Z) / SC), kNear = (int)floorf((camZ + NEAR_Z * 0.8f) / SC);
  for (int k = kFar; k >= kNear; k--) for (int sl = 0; sl < 2; sl++) {
    uint32_t h = hash2(k * 3 + sl, 911 + sl);
    if ((h & 7) > 4) continue;
    float wz = k * SC + ((h >> 3) & 15) * 1.8f; float z = wz - camZ; if (z < NEAR_Z * 0.8f || z > FAR_Z) continue;
    float off = 15.f + ((h >> 8) & 63) * 1.3f, wx = ((h >> 14) & 1) ? off : -off;
    if (riverN(wx, wz) < 1.45f || lakeN(wx, wz) < 1.3f) continue;
    float s = F / z; int sx = SCR_W / 2 + (int)((wx - camX) * s); if (sx < -60 || sx > SCR_W + 60) continue;
    drawScenery((int)((h >> 16) % 20), sx, hz + (int)(alt * s), s, fogT(z), h);
  }
}

static void renderAltimeter(float nextY) {
  const int x = SCR_W - 9, y0 = 24, y1 = 108;
  canvas.fillRect(x - 2, y0 - 3, 8, y1 - y0 + 6, rgb(12, 16, 44)); canvas.drawRect(x - 2, y0 - 3, 8, y1 - y0 + 6, rgb(70, 80, 140));
  canvas.fillRect(x - 1, y1 - 7, 6, 8, rgb(150, 36, 40));                                                      // zone dangereuse (sol)
  for (int a = 10; a <= 70; a += 10) { int yy = y1 - (int)((a - 2.5f) / 69.5f * (y1 - y0)); canvas.drawFastHLine(x, yy, 3, rgb(120, 130, 190)); }
  if (nextY >= 0) { int yy = y1 - (int)((nextY - 2.5f) / 69.5f * (y1 - y0)); canvas.fillRect(x - 1, yy - 1, 6, 3, rgb(120, 240, 255)); }
  int my = y1 - (int)((alt - 2.5f) / 69.5f * (y1 - y0)); canvas.fillTriangle(x - 6, my - 3, x - 6, my + 3, x - 1, my, TFT_WHITE);
}

static void render() {
  int hz = curHZ;
  renderSky(hz);
  renderMountains(hz);
  renderGround(hz);
  renderScenery(hz);
  // objets tries : les plus lointains d'abord
  int order[16], n = 0;
  for (int i = 0; i < 16; i++) if (objs[i].on && objs[i].z > NEAR_Z * 0.6f) order[n++] = i;
  for (int i = 1; i < n; i++) { int k = order[i], j = i - 1; while (j >= 0 && objs[order[j]].z < objs[k].z) { order[j + 1] = order[j]; j--; } order[j + 1] = k; }
  float nextZ = 9999, nextX = 0, nextY = -1; for (int i = 0; i < n; i++) { const Obj &o = objs[order[i]]; if (o.type == 0 && !o.done && o.z < nextZ) { nextZ = o.z; nextX = o.x; nextY = o.y; } }
  for (int k = 0; k < n; k++) {
    const Obj &o = objs[order[k]];
    float s = F / o.z; int sx = SCR_W / 2 + (int)((o.x - camX) * s);
    if (sx < -90 || sx > SCR_W + 90) continue;
    if (o.type == 0) drawRing(sx, hz + (int)((alt - o.y) * s), s, o.z, o.z == nextZ, hz + alt * s);
    else if (o.type == 1) drawTower(sx, s, o.z, o.y, o.var);
    else drawOrb(sx, hz + (int)((alt - o.y) * s), s);
  }
  int px, py; planePos(px, py);
  // ombre de l'avion
  { float gz = 16; int gy = hz + (int)(alt * F / gz); if (gy < SCR_H - 2 && state != TITLE) { int sw = 15 - (int)(alt * 0.12f); if (sw < 6) sw = 6; canvas.fillEllipse(SCR_W / 2 + (int)(vx * 4), gy, sw, 3, mixc(rgb(50, 120, 60), TFT_BLACK, 150)); } }
  if (boosting) for (int i = 0; i < 12; i++) { float ang = i * 0.5236f + (animT % 5) * 0.09f; int x0 = SCR_W / 2 + (int)(cosf(ang) * 84), y0 = hz + 22 + (int)(sinf(ang) * 46); canvas.drawLine(x0, y0, x0 + (int)(cosf(ang) * 34), y0 + (int)(sinf(ang) * 20), rgb(255, 255, 255)); }
  for (auto &p : pts) if (p.life > 0) {                                                                       // trainees
    int r = p.kind == 2 ? 2 + p.life / 9 : 1 + p.life / 8;
    uint16_t c = p.kind == 0 ? mixc(TFT_WHITE, HAZE, (16 - p.life) * 14) : (p.kind == 1 ? mixc(rgb(255, 190, 70), rgb(120, 30, 10), (16 - p.life) * 14) : mixc(rgb(60, 60, 66), rgb(150, 150, 156), (26 - p.life) * 8));
    canvas.fillCircle((int)p.x, (int)p.y, r, c);
  }
  float vroll = sinf(roll) * 1.15f;
  if (state == OVER && endT < 14) { int r = 6 + endT * 3; canvas.fillCircle(px, py + 2, r, rgb(255, 150, 40)); canvas.fillCircle(px, py + 2, r * 2 / 3, rgb(255, 220, 90)); canvas.fillCircle(px, py + 2, r / 3, TFT_WHITE); }
  else drawPlane(px, py + (state == OVER ? 0 : 0), vroll, boosting, 1.0f, invul > 0 && (animT & 4));
  // guidage vers le prochain anneau
  if (state == PLAY && nextY >= 0 && nextZ < 300) {
    float dx = nextX - camX, dy = nextY - alt, d = sqrtf(dx * dx + dy * dy);
    if (d > 7) { float ux = dx / d, uy = -dy / d, vx2 = -uy, vy2 = ux; int cx = px + (int)(ux * 24), cy = py - 4 + (int)(uy * 20); uint16_t c = (animT >> 3) & 1 ? rgb(255, 255, 255) : rgb(120, 240, 255);
      canvas.fillTriangle(cx + (int)(ux * 5), cy + (int)(uy * 5), cx - (int)(ux * 3 + vx2 * 4), cy - (int)(uy * 3 + vy2 * 4), cx - (int)(ux * 3 - vx2 * 4), cy - (int)(uy * 3 - vy2 * 4), c); }
  }
  // infos
  for (auto &p : pops) if (p.t > 0) ui::popText(SCR_W / 2, (int)p.y, p.s, p.col);
  char b[32];
  if (state == PLAY || state == PAUSED || state == OVER) {
    snprintf(b, sizeof b, "%lu", (unsigned long)score); txtO(b, 5, 2, &fonts::Font4, TFT_WHITE, rgb(20, 20, 70));
    if (combo >= 2) { snprintf(b, sizeof b, "x%d", combo); txtO(b, 5, 28, &fonts::Font2, rgb(255, 210, 50), rgb(20, 20, 70)); }
    for (int i = 0; i < lives; i++) { int hx = SCR_W - 16 - i * 16, hy = 3; canvas.fillTriangle(hx, hy, hx - 6, hy + 10, hx + 6, hy + 10, rgb(20, 24, 60)); canvas.fillTriangle(hx, hy + 1, hx - 5, hy + 9, hx + 5, hy + 9, TFT_WHITE); canvas.fillTriangle(hx - 2, hy + 4, hx + 2, hy + 4, hx, hy, rgb(228, 62, 62)); canvas.fillRect(hx - 6, hy + 8, 13, 2, rgb(150, 162, 196)); }
    renderAltimeter(nextY);
    // jauge de turbo (segmentee) a gauche, vitesse a droite
    canvas.fillRect(3, SCR_H - 13, 74, 11, rgb(14, 16, 44)); canvas.drawRect(3, SCR_H - 13, 74, 11, rgb(70, 80, 140));
    canvas.fillTriangle(8, SCR_H - 12, 5, SCR_H - 6, 8, SCR_H - 6, boostLock ? rgb(200, 70, 60) : rgb(255, 214, 70)); canvas.fillTriangle(8, SCR_H - 5, 11, SCR_H - 11, 8, SCR_H - 11, boostLock ? rgb(200, 70, 60) : rgb(255, 214, 70));
    int seg = (int)(boost * 10 / 100.0f + 0.5f); for (int i = 0; i < 10; i++) canvas.fillRect(15 + i * 6, SCR_H - 11, 5, 7, i < seg ? (boostLock ? rgb(200, 70, 60) : (boosting ? rgb(255, 238, 120) : mixc(rgb(255, 150, 40), rgb(255, 230, 90), i * 25))) : rgb(34, 38, 80));
    int kmh = (int)(speed * 55); snprintf(b, sizeof b, "%d", kmh); txtO(b, SCR_W - 24, SCR_H - 14, &fonts::Font2, TFT_WHITE, rgb(20, 20, 70), textdatum_t::top_right); txtO("km/h", SCR_W - 4, SCR_H - 10, &fonts::Font0, rgb(190, 200, 255), rgb(20, 20, 70), textdatum_t::top_right);
    float sp = clampf(speed / (3.6f * 1.75f), 0, 1); int ss = (int)(sp * 8 + 0.5f); for (int i = 0; i < 8; i++) { int hh = 2 + i; canvas.fillRect(SCR_W - 100 + i * 5, SCR_H - 4 - hh, 4, hh, i < ss ? mixc(rgb(90, 220, 255), rgb(255, 90, 70), i * 36) : rgb(34, 38, 80)); }
  }
  if (state == TITLE) {
    canvas.fillRoundRect(40, 14, 160, 52, 8, rgb(255, 255, 255)); canvas.drawRoundRect(40, 14, 160, 52, 8, rgb(30, 50, 140)); canvas.drawRoundRect(42, 16, 156, 48, 6, rgb(150, 180, 240));
    txt(L("FLIGHT","AVION"), SCR_W / 2, 18, &fonts::Font4, rgb(30, 50, 140), textdatum_t::top_center, 1.3f);
    if (best > 0) { snprintf(b, sizeof b, L("best %lu","record %lu"), (unsigned long)best); txt(b, SCR_W / 2, 52, &fonts::Font0, rgb(200, 90, 30), textdatum_t::top_center); }
  }
  if (state == PAUSED) ui::pause();
  if (state == OVER) ui::gameOver("CRASH !", score, best, (score) > 0 && (uint32_t)(score) >= (uint32_t)(best));
}

void loop() {
  uint32_t now = micros();
  if (last == 0) last = now;
  acc += now - last; last = now; if (acc > 100000) acc = 100000;
  bool a = M5.BtnA.wasPressed(), p = M5.BtnPWR.wasPressed();
  switch (state) {
    case TITLE: if (a) { reset(); state = PLAY; sfx(660, 40); } break;
    case PLAY: if (p) { state = PAUSED; sfx(440, 50); } break;
    case PAUSED: if (p || a) { state = PLAY; sfx(660, 50); } break;
    case OVER: if (a && endT > 40) { reset(); state = PLAY; sfx(660, 40); } break;
  }
  while (acc >= STEP_US) { step(); acc -= STEP_US; }
  render();
  bgm::run(14, state == PLAY);
  present(0, 0);
}

void icon(int x, int y) {
  canvas.setClipRect(x, y, 64, 64);
  for (int j = 0; j < 34; j++) canvas.drawFastHLine(x, y + j, 64, skyCol(j * SKYH / 34));
  canvas.fillCircle(x + 46, y + 22, 11, mixc(skyCol(22), rgb(255, 236, 190), 110)); canvas.fillCircle(x + 46, y + 22, 7, rgb(255, 248, 215));
  canvas.fillTriangle(x - 4, y + 34, x + 12, y + 14, x + 28, y + 34, rgb(104, 94, 156)); canvas.fillTriangle(x + 8, y + 22, x + 12, y + 14, x + 16, y + 22, rgb(246, 246, 252));
  canvas.fillTriangle(x + 18, y + 34, x + 38, y + 18, x + 62, y + 34, rgb(126, 112, 172)); canvas.fillTriangle(x + 33, y + 23, x + 38, y + 18, x + 43, y + 23, rgb(246, 246, 252));
  canvas.fillRect(x, y + 34, 64, 30, FC[0][0][0] ? FC[0][0][0] : rgb(88, 168, 64));
  canvas.fillTriangle(x, y + 36, x + 22, y + 36, x - 6, y + 64, rgb(198, 180, 74)); canvas.fillTriangle(x + 44, y + 36, x + 64, y + 36, x + 70, y + 64, rgb(150, 112, 72));
  canvas.fillTriangle(x + 22, y + 36, x + 34, y + 36, x + 46, y + 64, rgb(66, 142, 54)); canvas.fillTriangle(x + 22, y + 36, x + 46, y + 64, x + 8, y + 64, rgb(112, 184, 74));
  canvas.fillTriangle(x + 27, y + 34, x + 37, y + 34, x + 52, y + 64, ROADC); canvas.fillTriangle(x + 27, y + 34, x + 52, y + 64, x + 14, y + 64, ROADC);
  canvas.drawLine(x + 32, y + 34, x + 33, y + 64, YELC);
  canvas.fillTriangle(x + 8, y + 50, x + 12, y + 50, x + 10, y + 41, rgb(30, 100, 56));
  for (int k = 0; k < 4; k++) canvas.drawCircle(x + 38, y + 25, 12 - k, k < 1 ? rgb(176, 108, 20) : (k < 3 ? rgb(255, 200, 52) : rgb(255, 242, 160)));
  drawPlane(x + 28, y + 46, 0.3f, true, 0.8f);
  canvas.clearClipRect();
}

}  // namespace sky
