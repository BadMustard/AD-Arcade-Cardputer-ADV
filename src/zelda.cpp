// ============================================================================
//  LEGEND OF EMBERWOOD - aventure vue de dessus (heros Kiri, jeu original)
//  Inclinaison = marcher ; A = epee / parler / ouvrir ; B = roulade ; PWR = pause
//  Village -> Foret -> Crypte -> Arene du Golem de braise. Sauvegarde automatique.
//  Art : tools/art/*.py -> src/zelda_art.h (palette 48 couleurs, sprites 4 bits / RLE)
// ============================================================================
#include "common.h"
#include "zelda_art.h"

namespace zelda {
using namespace zart;

static const uint32_t STEP_US = 20000;
static const int OY = 23;                       // hauteur du HUD : la salle commence a y = 23
#define IX(r, t) ((r) * 5 + (t))
enum { RG = 0, RF, RE, RW, RP, RT, RB, RS, RK };   // rampes : herbe feuillage terre bois platre terre-cuite bleu pierre peau
enum { TO = 0, TS, TB, TL, TH };                   // tons : contour ombre base clair reflet
static const uint8_t CWHITE = 45, CBLACK = 46, CGOLD = 47;
enum { O_PROP, O_DECAL, O_FG, O_NPC, O_SIGN, O_CHEST, O_DOOR, O_WARP, O_ENEMY, O_GRASS, O_POT, O_SPIKES, O_BOSS, O_LIGHT, O_GATE };
enum { E_SPORE, E_BOAR, E_BAT, E_BONE, E_SHROUD, E_SPIDER };
enum { Z_VILLAGE, Z_FOREST, Z_CRYPT, Z_ARENA };
// drapeaux sauvegardes
enum { F_SWORD = 0, F_BOSS = 1, F_HEART = 2, F_STARTED = 31 };

// ------------------------------------------------------------------ etat global
enum Mode { TITLE, PLAY, DIALOG, PAUSED, GAMEOVER, ENDING };
static Mode mode = TITLE;
static uint8_t* fb = nullptr;                   // tampon d'indices de palette 240x135
static bool swap16 = true;
static uint32_t last = 0, acc = 0, tick = 0;
static Preferences pr;
static uint32_t flags = 0; static int gems = 0, keys = 0, maxhp = 10, checkpoint = 0, playSecs = 0, stepSec = 0;
static bool hasSave = false; static int finalScore = 0;
static int room = 0, fadeT = 0, fadeDir = 0, nextRoom = -1, nextX = 0, nextY = 0, shakeT = 0, offX = 0, offY = 0;
static uint32_t tod = 0;                        // heure du jour (pas de 20 ms), cycle de 9000 pas
static uint8_t coll[14][30];
static uint16_t palNow[48];
static bool aPress, bPress, pPress, aPend, bPend, pPend;

static inline bool flag(int b) { return (flags >> b) & 1; }
static void setFlag(int b) { flags |= (1u << b); }
static void save() {
  pr.begin("zelda", false);
  pr.putUInt("fl", flags | (1u << F_STARTED)); pr.putInt("gm", gems); pr.putInt("ky", keys); pr.putInt("mh", maxhp);
  pr.putInt("cp", checkpoint); pr.putInt("tm", playSecs); pr.end(); hasSave = true;
}
static void load() {
  pr.begin("zelda", true);
  flags = pr.getUInt("fl", 0); gems = pr.getInt("gm", 0); keys = pr.getInt("ky", 0); maxhp = pr.getInt("mh", 10);
  checkpoint = pr.getInt("cp", 0); playSecs = pr.getInt("tm", 0); pr.end();
  hasSave = flag(F_STARTED);
  if (maxhp < 10 || maxhp > 16) maxhp = 10; if (checkpoint < 0 || checkpoint >= NROOMS) checkpoint = 0;
}

// ------------------------------------------------------------------ sprites -> tampon d'indices
enum { BF_FLIP = 1, BF_FLASH = 2 };
static inline uint8_t flashOf(uint8_t c) { return (c < 45 && c % 5 >= 2) ? CWHITE : c; }
template <typename Fn> static void forPix(uint16_t id, Fn fn) {     // fn(i, j, couleur) pour chaque pixel opaque
  const Spr& s = SPR[id]; const int w = s.w, h = s.h; const uint8_t* d = BLOB + s.off;
  if (s.fmt == 0) {
    const uint8_t* lut = d; const uint8_t* px = d + s.nlut; int n = 0;
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++, n++) {
      uint8_t b = px[n >> 1]; uint8_t v = (n & 1) ? (b & 15) : (b >> 4);
      if (v) fn(i, j, lut[v - 1]);
    }
  } else {
    int n = 0, tot = w * h; const uint8_t* p = d;
    while (n < tot) {
      int run = p[0]; uint8_t c = p[1]; p += 2;
      if (c != 255) for (int k = 0; k < run; k++) { int m = n + k, j = m / w; fn(m - j * w, j, c); }
      n += run;
    }
  }
}
static void blit(uint16_t id, int x, int y, uint8_t fl = 0) {
  const Spr& s = SPR[id]; const int w = s.w, h = s.h;
  x += offX; y += offY;
  if (x >= 240 || y >= 135 || x + w <= 0 || y + h <= 0) return;
  const uint8_t* d = BLOB + s.off; const bool flip = fl & BF_FLIP, fl2 = fl & BF_FLASH;
  if (s.fmt == 0) {
    const uint8_t* lut = d; const uint8_t* px = d + s.nlut;
    for (int j = 0; j < h; j++) {
      int Y = y + j; if (Y < OY || Y >= 135) continue;
      uint8_t* row = fb + Y * 240; int n = j * w;
      for (int i = 0; i < w; i++, n++) {
        uint8_t b = px[n >> 1]; uint8_t v = (n & 1) ? (b & 15) : (b >> 4);
        if (!v) continue;
        int X = x + (flip ? (w - 1 - i) : i); if ((unsigned)X >= 240) continue;
        uint8_t c = lut[v - 1]; row[X] = fl2 ? flashOf(c) : c;
      }
    }
  } else {
    int n = 0, tot = w * h; const uint8_t* p = d;
    while (n < tot) {
      int run = p[0]; uint8_t c = p[1]; p += 2;
      if (c != 255) {
        if (fl2) c = flashOf(c);
        int j = n / w, i = n - j * w;
        for (int k = 0; k < run; k++) {
          int Y = y + j, X = x + (flip ? (w - 1 - i) : i);
          if (Y >= OY && Y < 135 && (unsigned)X < 240) fb[Y * 240 + X] = c;
          if (++i >= w) { i = 0; j++; }
        }
      }
      n += run;
    }
  }
}
static inline void pset(int x, int y, uint8_t c) { x += offX; y += offY; if ((unsigned)x < 240 && y >= OY && y < 135) fb[y * 240 + x] = c; }
static void shade(int cx, int cy, int rx, int ry) {          // ombre portee : un ton plus sombre dans une ellipse
  cx += offX; cy += offY;
  for (int y = cy - ry; y <= cy + ry; y++) {
    if (y < OY || y >= 135) continue;
    for (int x = cx - rx; x <= cx + rx; x++) {
      if ((unsigned)x >= 240) continue;
      float a = (float)(x - cx) / (rx + 0.5f), b = (float)(y - cy) / (ry + 0.5f);
      if (a * a + b * b > 1) continue;
      uint8_t& v = fb[y * 240 + x]; if (v < 45 && v % 5 > 1) v--;
    }
  }
}
// sprites directement sur le canvas (HUD, lueurs, icone) - palette fixe
static inline uint16_t sw(uint16_t c) { return swap16 ? (uint16_t)((c >> 8) | (c << 8)) : c; }
static void blit565(uint16_t id, int x, int y, const uint16_t* P, uint8_t fl = 0) {
  uint16_t* out = (uint16_t*)canvas.getBuffer(); const int w = SPR[id].w;
  forPix(id, [&](int i, int j, uint8_t c) {
    int X = x + ((fl & BF_FLIP) ? w - 1 - i : i), Y = y + j;
    if ((unsigned)X < 240 && (unsigned)Y < 135) out[Y * 240 + X] = sw(P[c]);
  });
}

// ------------------------------------------------------------------ particules
struct Part { float x, y, vx, vy; uint8_t c, life; bool grav; };
static const int NPART = 140; static Part parts[NPART]; static int partN = 0;
static void addPart(float x, float y, float vx, float vy, uint8_t c, int life, bool grav = false) {
  Part* p = nullptr;
  for (int i = 0; i < NPART; i++) if (!parts[i].life) { p = &parts[i]; break; }
  if (!p) p = &parts[rnd(0, NPART - 1)];
  *p = {x, y, vx, vy, c, (uint8_t)life, grav};
}
static void burstSprite(uint16_t id, int x0, int y0, bool flip, int every = 3) {   // la mort : le sprite eclate en particules de ses couleurs
  const Spr& s = SPR[id]; float cx = s.w / 2.0f, cy = s.h / 2.0f;
  forPix(id, [&](int i, int j, uint8_t c) {
    if ((i + j * 2) % every) return;
    int ii = flip ? s.w - 1 - i : i;
    float dx = ii - cx, dy = j - cy, d = sqrtf(dx * dx + dy * dy) + 0.5f, sp = 0.5f + (rnd(0, 100) / 100.0f) * 1.6f;
    addPart(x0 + ii, y0 + j, dx / d * sp, dy / d * sp - 0.6f, c, rnd(16, 34), true);
  });
}
static void puff(float x, float y, uint8_t c1, uint8_t c2, int n) {
  for (int i = 0; i < n; i++) { float a = rnd(0, 628) / 100.0f, s = rnd(30, 150) / 100.0f; addPart(x, y, cosf(a) * s, sinf(a) * s - 0.5f, i & 1 ? c1 : c2, rnd(12, 26), true); }
}

// ------------------------------------------------------------------ dialogues (EN / FR)
static const char* const DLG_EN[] = {
  "",
  "EMBERWOOD VILLAGE\nEAST : THE FOREST",
  "WELCOME, TRAVELLER! THE FOREST\nIS DANGEROUS. TAKE THIS SWORD.|A : SWORD    B : ROLL\nTILT THE CONSOLE TO WALK.\nGOOD LUCK, KIRI!",
  "THE OLD CRYPT IN THE FOREST\nIS SEALED. THEY SAY ITS KEY\nLIES IN THE CLEARING...",
  "I SAW A GIANT MADE OF FIRE!\nIT SLEEPS UNDER THE CRYPT.",
  "MY FORGE WENT COLD WHEN THE\nEMBER GOLEM WOKE UP.|BRING BACK ITS FIRE, KIRI.",
  "HALT! NOBODY ENTERS THE\nFOREST WITHOUT A SWORD.\nGO SEE THE MERCHANT.",
  "BEWARE : THORNBOARS CHARGE\nIN STRAIGHT LINES.\nROLL ASIDE WITH B!",
  "YOU FOUND THE CRYPT KEY!",
  "A HEAVY DOOR, LOCKED.\nIT NEEDS A KEY.",
  "HERE SLEEPS THE EMBER GOLEM.\nSTRIKE IT WHEN IT IS STUNNED\nAFTER ITS SLAM.",
  "YOU FOUND 10 GEMS\nAND YOUR HEALTH IS RESTORED!",
  "HEART CONTAINER!\nMAX HEALTH UP.",
  "MY BEST SWORD IS YOURS.\nCOME BACK SAFE, KIRI!",
  "A SWORD! VERY WELL.\nSTAY ON THE PATH.",
  "THE KEY TURNS...\nTHE CRYPT IS OPEN.",
};
static const char* const DLG_FR[] = {
  "",
  "VILLAGE D'EMBERWOOD\nEST : LA FORET",
  "BIENVENUE, VOYAGEUR ! LA FORET\nEST DANGEREUSE. PRENDS CETTE EPEE.|A : EPEE    B : ROULADE\nINCLINE LA CONSOLE POUR MARCHER.\nBONNE CHANCE, KIRI !",
  "LA VIEILLE CRYPTE DE LA FORET\nEST SCELLEE. ON DIT QUE SA CLE\nEST DANS LA CLAIRIERE...",
  "J'AI VU UN GEANT DE FEU !\nIL DORT SOUS LA CRYPTE.",
  "MA FORGE S'EST ETEINTE QUAND\nLE GOLEM DE BRAISE S'EST REVEILLE.|RAPPORTE SON FEU, KIRI.",
  "HALTE ! PERSONNE N'ENTRE DANS\nLA FORET SANS EPEE.\nVA VOIR LE MARCHAND.",
  "ATTENTION : LES SANGLIERS-RONCES\nFONCENT EN LIGNE DROITE.\nESQUIVE AVEC B !",
  "TU AS TROUVE LA CLE DE LA CRYPTE !",
  "UNE LOURDE PORTE VERROUILLEE.\nIL FAUT UNE CLE.",
  "ICI DORT LE GOLEM DE BRAISE.\nFRAPPE-LE QUAND IL EST SONNE\nAPRES SON COUP AU SOL.",
  "TU AS TROUVE 10 GEMMES\nET TA VIE EST RESTAUREE !",
  "RECEPTACLE DE COEUR !\nVIE MAXIMUM AUGMENTEE.",
  "MA MEILLEURE EPEE EST A TOI.\nREVIENS SAIN ET SAUF, KIRI !",
  "UNE EPEE ! TRES BIEN.\nRESTE SUR LE CHEMIN.",
  "LA CLE TOURNE...\nLA CRYPTE EST OUVERTE.",
};
static const char* const WHO_EN[] = {"MERCHANT", "BLACKSMITH", "OLD MAGDA", "PIP", "GUARD", "SIGN", ""};
static const char* const WHO_FR[] = {"MARCHAND", "FORGERON", "VIEILLE MAGDA", "PIP", "GARDE", "PANNEAU", ""};
static struct { int id, who, page, shown; const char* text; int after; } dlg;
static void say(int id, int who, int after = 0) {
  dlg.id = id; dlg.who = who; dlg.page = 0; dlg.shown = 0; dlg.after = after;
  dlg.text = g_lang ? DLG_FR[id] : DLG_EN[id]; mode = DIALOG; sfx(660, 30);
}
static const char* pageStart(int page) { const char* s = dlg.text; for (int p = 0; p < page && s; p++) { s = strchr(s, '|'); if (s) s++; } return s; }
static int pageLen(const char* s) { const char* e = strchr(s, '|'); return e ? (int)(e - s) : (int)strlen(s); }

// ------------------------------------------------------------------ entites
enum Kind { K_NONE, K_NPC, K_GATE, K_SIGN, K_CHEST, K_DOOR, K_WARP, K_ENEMY, K_GRASS, K_POT, K_SPIKES, K_LIGHT, K_PICK, K_WEB, K_BOULDER, K_ERUPT };
struct Ent {
  uint8_t kind, type, st, dir; float x, y, vx, vy, hx, hy;
  int hp, t, cool, hurt, kb; float kbx, kby; int a, b, c; uint16_t spr; bool flip; uint32_t hitSwing;
};
static const int NENT = 40; static Ent ents[NENT];
static Ent* newEnt(uint8_t kind) { for (int i = 0; i < NENT; i++) if (ents[i].kind == K_NONE) { ents[i] = Ent(); ents[i].kind = kind; return &ents[i]; } return nullptr; }
static uint32_t swingId = 1;

// heros
enum HSt { H_WALK, H_ATK, H_ROLL, H_HURT, H_DEAD, H_WIN };
static struct { float x, y; int dir, st, t, hp, inv, rollCool, slowT, kb; float kbx, kby, rdx, rdy; bool moving, endAfter; int animT; } hero;
static const int DX[4] = {0, 0, 1, -1}, DY[4] = {1, -1, 0, 0};

// boss
static struct { bool on, dead; int st, t, hp, hpMax, inv, phase, pose, attackN; float x, y; int stunT; } gb;
enum { GB_SLEEP, GB_WAKE, GB_IDLE, GB_WALK, GB_WIND, GB_SLAM, GB_STUN, GB_ERUPT, GB_THROW0, GB_THROW1, GB_DYING, GB_RUBBLE };
enum { P_IDLE, P_WALK, P_WINDUP, P_SLAM, P_THROW0, P_THROW1, P_STAGGER, P_HURT };
static struct { bool on; float x, y, r; } shock;

// lumieres de la crypte
struct Light { int16_t x, y; int32_t t2[2][16]; };
static Light lights[12]; static int nLights = 0;
static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
static void addLight(int x, int y, float r, float st) {
  if (nLights >= 12) return;
  Light& l = lights[nLights++]; l.x = x; l.y = y + OY;
  for (int b = 0; b < 16; b++) {
    float bd = (b / 16.0f - 0.5f) * 0.32f, tl = (1.2f - bd) / 2.7f, tm = (0.42f - bd) / 2.7f;
    float dl = r * (1 - tl / st), dm = r * (1 - tm / st);
    l.t2[0][b] = dl > 0 ? (int32_t)(dl * dl) : -1; l.t2[1][b] = dm > 0 ? (int32_t)(dm * dm) : -1;
  }
}

// ------------------------------------------------------------------ collisions
static const Room& curRoom() { return ROOMS[room]; }
static bool dynSolid(int px, int py, const Ent* self) {
  for (int i = 0; i < NENT; i++) {
    const Ent& e = ents[i]; if (&e == self) continue;
    switch (e.kind) {
      case K_NPC: if (fabsf(px - e.x) < 7 && py > e.y - 6 && py < e.y + 2) return true; break;
      case K_GATE: if (!flag(F_SWORD) && px >= 200 && py >= 48 && py < 84) return true; break;
      case K_CHEST: if (px >= e.a && px < e.a + 18 && py >= e.b + 6 && py < e.b + 16) return true; break;
      case K_POT: if (!e.st && px >= e.a + 1 && px < e.a + 13 && py >= e.b + 10 && py < e.b + 18) return true; break;
      case K_DOOR: if (px >= e.a && px < e.a + 32 && py < e.b + 34) return true; break;
      default: break;
    }
  }
  if (gb.on && !gb.dead && gb.st != GB_RUBBLE && fabsf(px - gb.x) < 22 && py > gb.y - 14 && py < gb.y + 2) return true;
  return false;
}
static bool solidAt(int px, int py, bool fly, const Ent* self, bool hero_) {
  if (px < 0 || px >= 240 || py < 0 || py >= 112) {
    if (!hero_) return true;
    int d = px < 0 ? 3 : (px >= 240 ? 1 : (py < 0 ? 0 : 2));
    return curRoom().ex[d] < 0;
  }
  uint8_t c = coll[py >> 3][px >> 3];
  if (c && !(fly && c != 1)) return true;
  return dynSolid(px, py, self);
}
static bool boxFree(float x, float y, int hw, int hh, bool fly, const Ent* self, bool hero_) {
  int x0 = (int)x - hw, x1 = (int)x + hw, y0 = (int)y - hh, y1 = (int)y;
  for (int px = x0; px <= x1; px += (x1 - x0 + 1) / 2 > 0 ? (x1 - x0) / 2 : 1)
    for (int py = y0; py <= y1; py += hh > 0 ? hh : 1) if (solidAt(px, py, fly, self, hero_)) return false;
  return true;
}
static bool moveBox(float& x, float& y, float dx, float dy, int hw, int hh, bool fly, const Ent* self, bool hero_) {
  bool hit = false;
  if (hero_ && !boxFree(x, y, hw, hh, fly, self, true)) {         // deja dans un obstacle : on laisse sortir (anti-blocage)
    x += dx; y += dy; return false;                                 // (les bords avec sortie comptent comme libres)
  }
  if (dx != 0) {
    if (boxFree(x + dx, y, hw, hh, fly, self, hero_)) x += dx;
    else { hit = true;
      if (hero_) { for (int s = 1; s <= 3; s++) { if (boxFree(x + dx, y - s, hw, hh, fly, self, hero_)) { y -= 0.6f; break; } if (boxFree(x + dx, y + s, hw, hh, fly, self, hero_)) { y += 0.6f; break; } } } }
  }
  if (dy != 0) {
    if (boxFree(x, y + dy, hw, hh, fly, self, hero_)) y += dy;
    else { hit = true;
      if (hero_) { for (int s = 1; s <= 3; s++) { if (boxFree(x - s, y + dy, hw, hh, fly, self, hero_)) { x -= 0.6f; break; } if (boxFree(x + s, y + dy, hw, hh, fly, self, hero_)) { x += 0.6f; break; } } } }
  }
  return hit;
}

// ------------------------------------------------------------------ salles
static void spawnEnemy(int type, int x, int y) {
  Ent* e = newEnt(K_ENEMY); if (!e) return;
  e->type = type; e->x = e->hx = x; e->y = e->hy = y; e->t = rnd(0, 60);
  static const int HP[6] = {2, 4, 1, 3, 3, 2};
  e->hp = HP[type] + (g_diff == 2 ? 1 : 0); e->cool = rnd(30, 90);
}
static void loadRoom(int id, float hx, float hy) {
  room = id; const Room& r = ROOMS[id];
  memcpy(coll, r.coll, sizeof coll);
  for (int i = 0; i < NENT; i++) ents[i].kind = K_NONE;
  for (int i = 0; i < NPART; i++) parts[i].life = 0;
  nLights = 0; gb.on = false; shock.on = false;
  for (int i = 0; i < r.nobj; i++) {
    const Obj& o = r.objs[i]; Ent* e = nullptr;
    switch (o.type) {
      case O_NPC: e = newEnt(K_NPC); if (e) { e->x = e->hx = o.x + 8; e->y = e->hy = o.y + 23; e->a = o.a; e->b = o.b; e->spr = o.spr; e->t = rnd(0, 100); } break;
      case O_GATE: e = newEnt(K_GATE); if (e) { e->x = o.x + 8; e->y = o.y + 23; e->a = o.a; e->b = o.b; e->spr = o.spr; } break;
      case O_SIGN: e = newEnt(K_SIGN); if (e) { e->a = o.x; e->b = o.y; e->c = o.b; e->spr = o.spr; e->x = o.x + 8; e->y = o.y + 15; } break;
      case O_CHEST: e = newEnt(K_CHEST); if (e) { e->a = o.x; e->b = o.y; e->c = o.b; e->type = o.a; e->st = flag(o.b); e->x = o.x + 9; e->y = o.y + 15; } break;
      case O_DOOR: e = newEnt(K_DOOR); if (e) { e->a = o.x; e->b = o.y; e->c = o.b; e->type = o.a; e->spr = o.spr; e->st = flag(o.b); e->x = o.x + 16; e->y = o.y + 31; } break;
      case O_WARP: e = newEnt(K_WARP); if (e) { e->a = o.x; e->b = o.y; e->c = o.w; e->hp = o.h; e->type = o.a; e->hx = o.b; e->hy = o.c; } break;
      case O_ENEMY: spawnEnemy(o.a, o.x, o.y); break;
      case O_GRASS: e = newEnt(K_GRASS); if (e) { e->a = o.x; e->b = o.y; e->x = o.x + 8; e->y = o.y + 15; e->spr = o.spr; } break;
      case O_POT: e = newEnt(K_POT); if (e) { e->a = o.x; e->b = o.y; e->x = o.x + 7; e->y = o.y + 17; e->spr = o.spr; } break;
      case O_SPIKES: e = newEnt(K_SPIKES); if (e) { e->a = o.x; e->b = o.y; e->c = o.c; e->spr = o.spr; e->x = o.x + 8; e->y = o.y; } break;
      case O_LIGHT: addLight(o.x, o.y, o.a, 1.0f); break;
      case O_BOSS:
        gb.on = true; gb.dead = flag(F_BOSS); gb.x = 120; gb.y = o.y + 70 + 6; gb.hpMax = gb.hp = difi(18, 24, 30);
        gb.st = gb.dead ? GB_RUBBLE : GB_SLEEP; gb.t = (gb.dead && flag(F_HEART)) ? 999 : 0; gb.phase = 1; gb.inv = 0; gb.attackN = 0; break;
      default: break;
    }
  }
  hero.x = hx; hero.y = hy; hero.st = H_WALK; hero.kb = 0;
  if (!boxFree(hero.x, hero.y, 5, 4, false, nullptr, true)) {        // arrivee dans un arbre / un mur : place libre la plus proche
    bool done = false;
    for (int rr = 2; rr <= 80 && !done; rr += 2)
      for (int a = 0; a < 16 && !done; a++) {
        float nx = hx + cosf(a * 0.3927f) * rr, ny = hy + sinf(a * 0.3927f) * rr;
        if (nx < 6 || nx > 234 || ny < 8 || ny > 110) continue;
        if (boxFree(nx, ny, 5, 4, false, nullptr, true)) { hero.x = nx; hero.y = ny; done = true; }
      }
  }
  if (id == 0 || id == 2 || id == 6 || id == 8) { checkpoint = id; save(); }
  sfx(330, 20);
}
static void goRoom(int id, float x, float y) { nextRoom = id; nextX = x; nextY = y; fadeT = 0; fadeDir = 1; }
static void spawnPoint(int id, float& x, float& y) {
  switch (id) { case 0: x = 120; y = 108; break; case 2: x = 12; y = 70; break; case 6: x = 120; y = 96; break; case 8: x = 20; y = 70; break; default: x = 120; y = 80; }
}

// ------------------------------------------------------------------ degats
static void hurtHero(int n, float fx, float fy) {
  if (hero.inv > 0 || hero.st == H_ROLL || hero.st == H_DEAD || hero.st == H_WIN) return;
  n += (g_diff == 2 && n > 0) ? 1 : 0; if (g_diff == 0 && n > 1) n--;
  hero.hp -= n; hero.inv = 70; sfx(180, 120);
  float dx = hero.x - fx, dy = hero.y - fy, d = sqrtf(dx * dx + dy * dy) + 0.01f;
  hero.kbx = dx / d * 2.6f; hero.kby = dy / d * 2.6f; hero.kb = 9; hero.st = H_HURT; hero.t = 0;
  if (hero.hp <= 0) { hero.hp = 0; hero.st = H_DEAD; hero.t = 0; sfx(140, 400); }
}
static void dropLoot(float x, float y, int chance) {
  if ((int)rnd(0, 99) >= chance) return;
  Ent* e = newEnt(K_PICK); if (!e) return;
  e->x = x; e->y = y; e->type = (hero.hp < maxhp && rnd(0, 2) == 0) ? 1 : 0; e->t = 0; e->vy = -1.6f;
}
static void enemyFoot(int type, int& w, int& fy) {
  static const int FY[6] = {22, 22, 22, 33, 24, 17};
  w = SPR[type == E_SPORE ? SPORE_W : type == E_BOAR ? BOAR_W : type == E_BAT ? BAT_F : type == E_BONE ? BONE_W : type == E_SHROUD ? SHROUD_F : SPIDER_W].w;
  fy = FY[type];
}
static uint16_t enemySpr(const Ent& e) {
  int k = (tick / 7 + e.c) & 3;
  switch (e.type) {
    case E_SPORE: return e.st == 1 ? SPORE_A + (e.t > 14 ? 1 : 0) : SPORE_W + k;
    case E_BOAR: return e.st == 1 ? BOAR_A + ((tick / 4) & 1) : (e.st == 2 ? BOAR_W : BOAR_W + k);
    case E_BAT: return e.st == 1 ? BAT_D : BAT_F + ((tick / 4) & 3);
    case E_BONE: return e.st == 1 ? BONE_A + (e.t > 20 ? 1 : 0) : BONE_W + k;
    case E_SHROUD: return e.st == 1 ? SHROUD_A + (e.t > 10 ? 1 : 0) : SHROUD_F + ((tick / 9) & 3);
    default: return e.st == 1 ? SPIDER_A : SPIDER_W + (e.st == 2 ? ((tick / 3) & 3) : 0);
  }
}
static void killEnemy(Ent& e) {
  int w, fy; enemyFoot(e.type, w, fy);
  burstSprite(enemySpr(e), (int)e.x - w / 2, (int)e.y - fy + (e.type == E_BAT ? -6 : 0), e.flip);
  sfx(520, 40); sfx(300, 90, false);
  dropLoot(e.x, e.y, 45); e.kind = K_NONE;
}
static void hitEnemy(Ent& e, float fx, float fy, int dmg) {
  if (e.hurt > 0) return;
  e.hp -= dmg; e.hurt = 14; sfx(900, 25);
  float dx = e.x - fx, dy = e.y - fy, d = sqrtf(dx * dx + dy * dy) + 0.01f;
  e.kbx = dx / d * 3.0f; e.kby = dy / d * 3.0f; e.kb = 6;
  if (e.type == E_BOAR && e.st == 1) { e.st = 2; e.t = 0; }
  if (e.hp <= 0) killEnemy(e);
}

// ------------------------------------------------------------------ IA des ennemis
static void enemyStep(Ent& e) {
  float dx = hero.x - e.x, dy = hero.y - e.y, dist = sqrtf(dx * dx + dy * dy) + 0.01f;
  const bool fly = (e.type == E_BAT || e.type == E_SHROUD);
  if (e.hurt > 0) e.hurt--;
  if (e.kb > 0) { e.kb--; moveBox(e.x, e.y, e.kbx, e.kby, 5, 4, fly, &e, false); return; }
  e.t++; if (e.cool > 0) e.cool--;
  float mvx = 0, mvy = 0; float spdK = dif(0.8f, 1.0f, 1.2f);
  switch (e.type) {
    case E_SPORE:
      if (e.st == 0) {
        if (e.t % 70 == 0) { e.vx = (rnd(0, 2) - 1) * 0.4f; e.vy = (rnd(0, 2) - 1) * 0.4f; }
        mvx = e.vx * spdK; mvy = e.vy * spdK;
        if (dist < 34 && !e.cool) { e.st = 1; e.t = 0; }
      } else {
        if (e.t > 14 && e.t < 34) {
          if (e.t % 3 == 0) addPart(e.x + rnd(0, 20) - 10, e.y - 18 + rnd(0, 10), (rnd(0, 20) - 10) / 20.0f, -0.3f, e.t & 1 ? CGOLD : IX(RG, TH), 20);
          if (dist < 20) hurtHero(1, e.x, e.y);
        }
        if (e.t > 40) { e.st = 0; e.t = 0; e.cool = 90; }
      }
      break;
    case E_BOAR:
      if (e.st == 0) {
        if (e.t % 90 == 0) { e.vx = rnd(0, 1) ? 0.5f : -0.5f; e.vy = 0; }
        mvx = e.vx * spdK;
        if (!e.cool && dist < 110 && (fabsf(dy) < 9 || fabsf(dx) < 9)) {
          e.st = 1; e.t = 0; sfx(220, 60);
          if (fabsf(dy) < 9) { e.vx = dx > 0 ? 2.3f : -2.3f; e.vy = 0; } else { e.vy = dy > 0 ? 2.3f : -2.3f; e.vx = 0; }
        }
      } else if (e.st == 1) {
        mvx = e.vx * spdK; mvy = e.vy * spdK;
        if (e.t % 4 == 0) addPart(e.x - (e.vx > 0 ? 10 : -10), e.y, 0, -0.3f, IX(RE, TL), 12);
        if (e.t > 80) { e.st = 2; e.t = 0; }
      } else { if (e.t > 45) { e.st = 0; e.t = 0; e.cool = 60; } }
      break;
    case E_BAT:
      if (e.st == 0) {
        mvx = cosf(e.t * 0.05f) * 0.7f + (dx > 0 ? 0.15f : -0.15f); mvy = sinf(e.t * 0.08f) * 0.5f + (dy > 0 ? 0.1f : -0.1f);
        if (!e.cool && dist < 80) { e.st = 1; e.t = 0; e.vx = dx / dist * 2.1f; e.vy = dy / dist * 2.1f; sfx(1400, 30); }
      } else { mvx = e.vx * spdK; mvy = e.vy * spdK; if (e.t > 30) { e.st = 0; e.cool = 110; } }
      break;
    case E_BONE:
      if (e.st == 0) {
        if (dist < 110) { mvx = dx / dist * 0.5f * spdK; mvy = dy / dist * 0.5f * spdK; }
        if (dist < 24 && !e.cool) { e.st = 1; e.t = 0; }
      } else {
        if (e.t == 22) { sfx(300, 40); if (dist < 26) hurtHero(2, e.x, e.y); }
        if (e.t > 34) { e.st = 0; e.cool = 50; }
      }
      break;
    case E_SHROUD:
      if (e.st == 0) {
        if (dist < 140) { mvx = dx / dist * 0.32f * spdK; mvy = dy / dist * 0.32f * spdK; }
        if (dist < 34 && !e.cool) { e.st = 1; e.t = 0; e.vx = dx / dist * 1.4f; e.vy = dy / dist * 1.4f; sfx(240, 160); }
      } else { mvx = e.vx; mvy = e.vy; if (e.t > 24) { e.st = 0; e.cool = 80; } }
      break;
    default:  // E_SPIDER
      if (e.st == 0) {                                        // pause
        if (e.t > 34) { e.st = 2; e.t = 0; float a = atan2f(dy, dx) + (rnd(0, 100) - 50) / 60.0f; e.vx = cosf(a) * 1.6f; e.vy = sinf(a) * 1.6f; }
        if (!e.cool && dist < 90 && dist > 30) {               // jet de toile
          e.st = 1; e.t = 0;
        }
      } else if (e.st == 1) {
        if (e.t == 16) { Ent* w = newEnt(K_WEB); if (w) { w->x = e.x; w->y = e.y - 6; w->vx = dx / dist * 1.7f; w->vy = dy / dist * 1.7f; w->t = 0; } sfx(800, 30); }
        if (e.t > 26) { e.st = 0; e.t = 0; e.cool = 120; }
      } else { mvx = e.vx * spdK; mvy = e.vy * spdK; if (e.t > 18) { e.st = 0; e.t = 0; } }
      break;
  }
  if (mvx || mvy) {
    bool hit = moveBox(e.x, e.y, mvx, mvy, 5, 4, fly, &e, false);
    if (hit) { e.vx = -e.vx; e.vy = -e.vy; if (e.type == E_BOAR && e.st == 1) { e.st = 2; e.t = 0; shakeT = 8; sfx(120, 80); } }
    if (mvx > 0.05f) e.flip = false; else if (mvx < -0.05f) e.flip = true;
  } else if (e.type != E_BAT) e.flip = dx < 0;
  // contact
  float cy = e.type == E_BAT ? e.y - 12 : e.y;
  if (fabsf(hero.x - e.x) < 9 && fabsf(hero.y - cy) < 9) hurtHero(e.type == E_BOAR && e.st == 1 ? 2 : 1, e.x, cy);
}

// ------------------------------------------------------------------ golem de braise
static void bossDraw() {
  if (!gb.on) return;
  int ox = (int)gb.x - 42, oy = (int)gb.y - 76 + OY;
  if (gb.st == GB_RUBBLE) { blit(G_RUBBLE + (gb.t > 50 ? 1 : 0), ox, oy); return; }
  int ph = gb.phase, k = (tick / 8) & 1;
  uint8_t fl = (gb.inv > 12 || (gb.st == GB_DYING && (tick & 4))) ? BF_FLASH : 0;
  int bx = 22, by = 26, tdx = 0, tdy = 0, hdy = 0;
  int Ls[2] = {bx - 8, by + 2}, Rs[2] = {bx + 32, by + 2}, Lf[2] = {bx - 12, by + 26}, Rf[2] = {bx + 34, by + 26};
  int legY[2] = {by + 28, by + 28};
  int p = gb.pose, kk = (tick / 10) & 3;
  if (p == P_IDLE) { int b4[4] = {0, 1, 1, 0}, f4[4] = {0, 1, 2, 1}; tdy = b4[kk]; Lf[1] += f4[kk]; Rf[1] += f4[kk]; }
  else if (p == P_WALK) { int s0[4] = {-3, 0, 0, 0}, s1[4] = {0, 0, -3, 0}, sx[4] = {1, 0, -1, 0}, sy[4] = {0, 1, 0, 1}, sw2[4] = {2, 0, -2, 0};
    legY[0] += s0[kk]; legY[1] += s1[kk]; tdx = sx[kk]; tdy = sy[kk]; Lf[0] += sw2[kk]; Rf[0] -= sw2[kk]; }
  else if (p == P_WINDUP) { int w = gb.t > 25 ? 1 : 0; tdy = -2; hdy = -1; Lf[0] = bx - 2; Lf[1] = by - 22 + w; Rf[0] = bx + 24; Rf[1] = by - 22 + w; }
  else if (p == P_SLAM) { tdy = 4; hdy = 3; Lf[0] = bx + 1; Lf[1] = by + 40; Rf[0] = bx + 19; Rf[1] = by + 41; }
  else if (p == P_THROW0) { Rf[0] = bx + 36; Rf[1] = by - 8; tdx = -1; }
  else if (p == P_THROW1) { Rf[0] = bx + 44; Rf[1] = by + 8; tdx = 2; }
  else if (p == P_STAGGER) { tdy = 5; hdy = 4; Lf[0] = bx - 10; Lf[1] = by + 38; Rf[0] = bx + 32; Rf[1] = by + 38; }
  Ls[0] += tdx; Ls[1] += tdy; Rs[0] += tdx; Rs[1] += tdy;
  int pi = (ph - 1) * 2 + k;
  shade(ox + 42, (int)gb.y + OY - 2, 26, 4);
  blit(G_LEG + pi, ox + bx + 6, oy + legY[0], fl); blit(G_LEG + pi, ox + bx + 21, oy + legY[1], fl | BF_FLIP);
  auto arm = [&](int* sh, int* f, bool right) {
    int ax = ox + sh[0] + 8, ay = oy + sh[1] + 9, bx2 = ox + f[0] + 9, by2 = oy + f[1] + 6;
    int n = (int)(sqrtf((float)((bx2 - ax) * (bx2 - ax) + (by2 - ay) * (by2 - ay)))) + 1;
    for (int pass = 0; pass < 2; pass++)
      for (int i = 0; i <= n; i++) {
        int x = ax + (bx2 - ax) * i / n, y = ay + (by2 - ay) * i / n;
        for (int yy = -3; yy <= 3; yy++) for (int xx = -3; xx <= 3; xx++) {
          int d = xx * xx + yy * yy;
          if (pass == 0 && d <= 12) pset(x + xx, y + yy, IX(RS, TO));
          if (pass == 1 && d <= 6) pset(x + xx, y + yy, fl ? CWHITE : (xx + yy < -1 ? IX(RS, TL) : (xx + yy > 2 ? IX(RS, TS) : IX(RS, TB))));
        }
        if (pass == 1 && ph > 1 && i % 5 == 2) pset(x, y, (tick & 4) ? CGOLD : IX(RT, TH));
      }
    blit(G_SHOULDER + pi, ox + sh[0], oy + sh[1], fl | (right ? BF_FLIP : 0)); blit(G_FIST + pi, ox + f[0], oy + f[1], fl | (right ? BF_FLIP : 0));
  };
  bool front = (p == P_WINDUP || p == P_SLAM);
  if (!front) arm(Ls, Lf, false);
  blit(G_TORSO + pi, ox + bx - 2 + tdx, oy + by + tdy, fl);
  blit(gb.st == GB_SLEEP ? G_HEAD + 6 : G_HEAD + pi, ox + bx + 9 + tdx, oy + by - 11 + tdy + hdy, fl);
  if (front) arm(Ls, Lf, false);
  arm(Rs, Rf, true);
  if (p == P_THROW0) blit(FX_BOULDER + (tick / 4 & 3), ox + Rf[0] + 1, oy + Rf[1] - 13);
  if (ph == 3 && gb.st != GB_DYING) for (int i = 0; i < 4; i++) {
    float a = (i * 90 + tick * 3) * 0.01745f;
    blit(G_FRAG + (i & 1), ox + 42 + (int)(cosf(a) * 34) - 3, oy + by + 12 + (int)(sinf(a) * 10) - 3, fl);
  }
}
static void bossStartAttack() {
  gb.attackN++;
  int ph = gb.phase, choice;
  if (ph == 1) choice = 0; else if (ph == 2) choice = gb.attackN % 2; else choice = gb.attackN % 3;
  if (choice == 0) { gb.st = GB_WIND; gb.pose = P_WINDUP; gb.t = 0; sfx(160, 200); }
  else if (choice == 1) {
    gb.st = GB_ERUPT; gb.pose = P_WINDUP; gb.t = 0;
    int n = ph == 3 ? 3 : 2;
    for (int i = 0; i < n; i++) { Ent* e = newEnt(K_ERUPT); if (e) { e->x = i == 0 ? hero.x : rnd(30, 210); e->y = i == 0 ? hero.y : rnd(40, 100); e->t = -i * 12; } }
    sfx(200, 300);
  } else { gb.st = GB_THROW0; gb.pose = P_THROW0; gb.t = 0; }
}
static void bossStep() {
  if (!gb.on) return;
  if (gb.inv > 0) gb.inv--;
  gb.t++;
  float dx = hero.x - gb.x, dy = hero.y - gb.y, dist = sqrtf(dx * dx + dy * dy) + 0.01f;
  switch (gb.st) {
    case GB_SLEEP: gb.pose = P_IDLE; if (hero.y < 96 && gb.t > 40) { gb.st = GB_WAKE; gb.t = 0; shakeT = 50; sfx(90, 600); } break;
    case GB_WAKE: if (gb.t % 6 == 0) puff(gb.x + rnd(0, 40) - 20, gb.y - rnd(10, 60), IX(RS, TL), IX(RG, TL), 3);
      if (gb.t > 60) { gb.st = GB_IDLE; gb.t = 0; } break;
    case GB_IDLE: gb.pose = P_IDLE; if (gb.t > 36) { gb.t = 0; if (dist > 46 && gb.attackN % 2 == 0) gb.st = GB_WALK; else bossStartAttack(); } break;
    case GB_WALK: {
      gb.pose = P_WALK; float sp = gb.phase == 1 ? 0.32f : (gb.phase == 2 ? 0.45f : 0.55f);
      float nx = gb.x + dx / dist * sp, ny = gb.y + dy / dist * sp * 0.6f;
      if (nx > 50 && nx < 190) gb.x = nx; if (ny > 60 && ny < 96) gb.y = ny;
      if (gb.t % 25 == 0) { shakeT = 3; sfx(110, 40); }
      if (gb.t > 90 || dist < 40) { gb.st = GB_IDLE; gb.t = 30; gb.attackN++; }
    } break;
    case GB_WIND: gb.pose = P_WINDUP; if (gb.t > 44) { gb.st = GB_SLAM; gb.t = 0; gb.pose = P_SLAM; shock.on = true; shock.x = gb.x; shock.y = gb.y + 2; shock.r = 8; shakeT = 16; sfx(70, 300);
        puff(gb.x, gb.y, IX(RS, TL), IX(RT, TH), 14); } break;
    case GB_SLAM: if (gb.t > 14) { gb.st = GB_STUN; gb.t = 0; gb.pose = P_STAGGER; } break;
    case GB_STUN: gb.pose = P_STAGGER; if (gb.t % 10 == 0) addPart(gb.x + rnd(0, 20) - 10, gb.y - 64, 0, -0.4f, CGOLD, 20);
      if (gb.t > (gb.phase == 3 ? 55 : 75)) { gb.st = GB_IDLE; gb.t = 0; } break;
    case GB_ERUPT: gb.pose = gb.t < 20 ? P_WINDUP : P_IDLE; if (gb.t > 90) { gb.st = GB_IDLE; gb.t = 0; } break;
    case GB_THROW0: gb.pose = P_THROW0; if (gb.t > 34) { gb.st = GB_THROW1; gb.t = 0; gb.pose = P_THROW1;
        Ent* e = newEnt(K_BOULDER); if (e) { e->x = gb.x + 26; e->y = gb.y - 40; e->hx = hero.x; e->hy = hero.y; float ddx = e->hx - e->x, ddy = e->hy - e->y, d = sqrtf(ddx * ddx + ddy * ddy) + 0.01f; e->vx = ddx / d * 2.4f; e->vy = ddy / d * 2.4f; e->t = 0; }
        sfx(260, 80); } break;
    case GB_THROW1: if (gb.t > 24) { gb.st = GB_IDLE; gb.t = 0; } break;
    case GB_DYING: gb.pose = P_STAGGER; shakeT = 2;
      if (gb.t % 9 == 0) { puff(gb.x + rnd(0, 50) - 25, gb.y - rnd(10, 70), CGOLD, IX(RT, TH), 8); sfx(200 + rnd(0, 300), 40); }
      if (gb.t > 100) { gb.st = GB_RUBBLE; gb.t = 0; burstSprite(G_TORSO + 4, (int)gb.x - 20, (int)gb.y - 50, false, 2); shakeT = 20; sfx(80, 500); }
      break;
    case GB_RUBBLE:
      if (gb.t == 50 && !flag(F_HEART)) { Ent* e = newEnt(K_PICK); if (e) { e->x = gb.x; e->y = gb.y + 14; e->type = 2; e->t = 0; e->vy = -1; } }
      break;
  }
  if (gb.st != GB_RUBBLE && gb.st != GB_DYING && gb.st != GB_SLEEP) {
    if (fabsf(dx) < 24 && dy > -18 && dy < 6) hurtHero(2, gb.x, gb.y - 10);
    if (gb.phase == 3) for (int i = 0; i < 4; i++) {
      float a = (i * 90 + tick * 3) * 0.01745f, fx = gb.x + cosf(a) * 34, fy = gb.y - 38 + sinf(a) * 10 + 30;
      if (fabsf(hero.x - fx) < 6 && fabsf(hero.y - fy) < 6) hurtHero(1, fx, fy);
    }
  }
  if (shock.on) {
    shock.r += 2.6f;
    float ex = (hero.x - shock.x) / 1.0f, ey = (hero.y - shock.y) / 0.45f, d = sqrtf(ex * ex + ey * ey);
    if (fabsf(d - shock.r) < 7) hurtHero(2, shock.x, shock.y);
    if (shock.r > 86) shock.on = false;
  }
}
static void bossHit(int dmg) {
  if (!gb.on || gb.dead || gb.inv > 0 || gb.st == GB_SLEEP || gb.st == GB_WAKE || gb.st == GB_DYING || gb.st == GB_RUBBLE) return;
  if (gb.st == GB_STUN) dmg *= 2;
  gb.hp -= dmg; gb.inv = 22; sfx(1100, 30); sfx(500, 60, false);
  puff(hero.x + DX[hero.dir] * 14, hero.y - 10 + DY[hero.dir] * 10, CGOLD, IX(RT, TH), 6);
  int np = gb.hp > gb.hpMax * 2 / 3 ? 1 : (gb.hp > gb.hpMax / 3 ? 2 : 3);
  if (np != gb.phase) { gb.phase = np; shakeT = 30; sfx(90, 500); gb.st = GB_IDLE; gb.t = 0; burstSprite(G_TORSO + (np - 2) * 2, (int)gb.x - 20, (int)gb.y - 50, false, 4); }
  if (gb.hp <= 0) { gb.hp = 0; gb.st = GB_DYING; gb.t = 0; gb.dead = true; setFlag(F_BOSS); save(); }
}

// ------------------------------------------------------------------ heros
static bool frontHit(const Ent& e, int x0, int y0, int w, int h) {
  float fx = hero.x + DX[hero.dir] * 12, fy = hero.y - 6 + DY[hero.dir] * 10;
  return fx >= x0 && fx < x0 + w && fy >= y0 && fy < y0 + h;
}
static bool interact() {
  for (int i = 0; i < NENT; i++) {
    Ent& e = ents[i];
    switch (e.kind) {
      case K_NPC:
        if (frontHit(e, (int)e.x - 10, (int)e.y - 24, 20, 30)) {
          if (e.a == 0 && !flag(F_SWORD)) { say(2, 0, 1); return true; }
          say(e.a == 0 ? 13 : e.b, e.a); e.dir = 0; return true;
        } break;
      case K_GATE: if (frontHit(e, (int)e.x - 12, (int)e.y - 26, 24, 32)) { say(flag(F_SWORD) ? 14 : 6, 4); return true; } break;
      case K_SIGN: if (frontHit(e, e.a - 2, e.b - 2, 20, 22)) { say(e.c, 5); return true; } break;
      case K_CHEST:
        if (!e.st && frontHit(e, e.a - 2, e.b - 2, 22, 22)) {
          e.st = 1; setFlag(e.c); sfx(523, 80); sfx(659, 80, false); sfx(784, 160, false);
          if (e.type == 1) { keys++; say(8, 6); } else { gems += 10; hero.hp = maxhp; say(11, 6); }
          puff(e.x, e.y - 10, CGOLD, CWHITE, 10); save(); return true;
        } break;
      case K_DOOR:
        if (frontHit(e, e.a, e.b, 32, 40)) {
          if (e.st) { goRoom(e.type, 120, 96); return true; }
          if (keys > 0) { keys--; e.st = 1; setFlag(e.c); save(); say(15, 6); sfx(392, 100); sfx(523, 200, false); }
          else say(9, 6);
          return true;
        } break;
      default: break;
    }
  }
  return false;
}
static void swordHits() {
  float cx = hero.x + DX[hero.dir] * 13, cy = hero.y - 9 + DY[hero.dir] * 11;
  for (int i = 0; i < NENT; i++) {
    Ent& e = ents[i];
    if (e.kind == K_ENEMY) {
      float ey = e.type == E_BAT ? e.y - 14 : e.y - 8;
      if (fabsf(e.x - cx) < 15 && fabsf(ey - cy) < 14 && e.hitSwing != swingId) { e.hitSwing = swingId; hitEnemy(e, hero.x, hero.y, 1); }
    } else if (e.kind == K_GRASS && fabsf(e.x - cx) < 13 && fabsf(e.y - 6 - cy) < 12) {
      puff(e.x, e.y - 6, IX(RG, TL), IX(RG, TB), 10); sfx(1500, 15); dropLoot(e.x, e.y, 30); e.kind = K_NONE;
    } else if (e.kind == K_POT && !e.st && fabsf(e.x - cx) < 12 && fabsf(e.y - 8 - cy) < 13) {
      burstSprite(e.spr, e.a, e.b, false, 2); sfx(700, 30); sfx(400, 60, false); dropLoot(e.x, e.y, 60); e.st = 1; e.kind = K_NONE;
    }
  }
  if (gb.on && fabsf(gb.x - cx) < 30 && cy > gb.y - 64 && cy < gb.y + 4) bossHit(1);
}
static void heroStep() {
  if (hero.inv > 0) hero.inv--;
  if (hero.rollCool > 0) hero.rollCool--;
  if (hero.slowT > 0) hero.slowT--;
  hero.t++;
  tilt::update();
  float tx = tilt::x(), ty = -tilt::y();
  auto dz = [](float v) { float a = fabsf(v); if (a < 0.10f) return 0.0f; float m = (a - 0.10f) / 0.32f; if (m > 1) m = 1; return v > 0 ? m : -m; };
  float mx = dz(tx), my = dz(ty);
  switch (hero.st) {
    case H_WALK: {
      float sp = 1.25f * (hero.slowT ? 0.5f : 1.0f);
      hero.moving = mx || my;
      if (hero.moving) {
        if (fabsf(mx) > fabsf(my) * 1.1f) hero.dir = mx > 0 ? 2 : 3; else if (fabsf(my) > fabsf(mx) * 1.1f) hero.dir = my > 0 ? 0 : 1;
        moveBox(hero.x, hero.y, mx * sp, my * sp, 5, 4, false, nullptr, true); hero.animT++;
      }
      if (aPress && !interact() && flag(F_SWORD)) { hero.st = H_ATK; hero.t = 0; swingId++; sfx(1200, 20); sfx(800, 30, false); }
      if (bPress && !hero.rollCool) {
        hero.st = H_ROLL; hero.t = 0; float d = sqrtf(mx * mx + my * my);
        if (d > 0.2f) { hero.rdx = mx / d; hero.rdy = my / d; } else { hero.rdx = DX[hero.dir]; hero.rdy = DY[hero.dir]; }
        sfx(500, 40);
      }
    } break;
    case H_ATK: if (hero.t >= 4 && hero.t <= 9) swordHits(); if (hero.t >= 13) hero.st = H_WALK; break;
    case H_ROLL:
      moveBox(hero.x, hero.y, hero.rdx * 2.3f, hero.rdy * 2.3f, 5, 4, false, nullptr, true);
      if (hero.t % 3 == 0) addPart(hero.x, hero.y, -hero.rdx * 0.3f, -0.2f, IX(RE, TL), 10);
      if (hero.t >= 15) { hero.st = H_WALK; hero.rollCool = 16; }
      break;
    case H_HURT:
      if (hero.kb > 0) { hero.kb--; moveBox(hero.x, hero.y, hero.kbx, hero.kby, 5, 4, false, nullptr, true); }
      if (hero.t > 10) hero.st = H_WALK; break;
    case H_DEAD: if (hero.t > 70) mode = GAMEOVER; break;
    case H_WIN: if (hero.t > 150) {
        if (hero.endAfter) {                            // fin : score + medaille (bronze = golem vaincu, puis selon le temps)
          finalScore = 5000 + (playSecs < 1800 ? (1800 - playSecs) * 2 : 0) + gems * 10 + hero.hp * 50;
          trophy::record(trophy::T_EMBER, finalScore);
          Preferences q; q.begin(dns("ember"), false); if (finalScore > q.getInt("best", 0)) q.putInt("best", finalScore); q.end();
          mode = ENDING;
        } else hero.st = H_WALK;
      } break;
  }
  // sorties de salle
  const Room& r = curRoom();
  if (hero.x < 1 && r.ex[3] >= 0) goRoom(r.ex[3], 236, hero.y);
  else if (hero.x > 239 && r.ex[1] >= 0) goRoom(r.ex[1], 4, hero.y);
  else if (hero.y < 5 && r.ex[0] >= 0) goRoom(r.ex[0], hero.x, 108);
  else if (hero.y > 111 && r.ex[2] >= 0) goRoom(r.ex[2], hero.x, 30);
}

// ------------------------------------------------------------------ objets (pas de jeu)
static void entsStep() {
  for (int i = 0; i < NENT; i++) {
    Ent& e = ents[i];
    switch (e.kind) {
      case K_ENEMY: enemyStep(e); break;
      case K_NPC:
        e.t++;
        if (mode == PLAY && e.t % 120 == 0) { int r = rnd(0, 4); e.vx = r == 0 ? 0.3f : (r == 1 ? -0.3f : 0); e.vy = r == 2 ? 0.25f : (r == 3 ? -0.25f : 0); }
        if (e.vx || e.vy) {
          float nx = e.x + e.vx, ny = e.y + e.vy;
          if (fabsf(nx - e.hx) > 14 || fabsf(ny - e.hy) > 8 || (fabsf(hero.x - nx) < 12 && fabsf(hero.y - ny) < 10)) { e.vx = e.vy = 0; }
          else moveBox(e.x, e.y, e.vx, e.vy, 4, 3, false, &e, false);
          e.dir = e.vx > 0 ? 2 : (e.vx < 0 ? 3 : 0);
        }
        break;
      case K_WARP: if (hero.x >= e.a && hero.x < e.a + e.c && hero.y >= e.b && hero.y < e.b + e.hp && nextRoom < 0) goRoom(e.type, e.hx, e.hy); break;
      case K_SPIKES: {
        int f = ((tick / 12) + e.c) & 3; e.st = f;
        if (f == 2 && hero.x >= e.a && hero.x < e.a + 16 && hero.y >= e.b + 2 && hero.y < e.b + 18) hurtHero(1, hero.x - DX[hero.dir] * 4, hero.y - DY[hero.dir] * 4 - 1);
      } break;
      case K_PICK:
        e.t++; if (e.vy < 0 || e.t < 12) { e.y += e.vy; e.vy += 0.2f; if (e.t > 12) e.vy = 0; }
        if (fabsf(hero.x - e.x) < 9 && fabsf(hero.y - e.y) < 9 && e.t > 8) {
          if (e.type == 0) { gems++; sfx(1568, 30); sfx(2093, 50, false); }
          else if (e.type == 1) { hero.hp = hero.hp + 2 > maxhp ? maxhp : hero.hp + 2; sfx(880, 40); sfx(1175, 60, false); }
          else { maxhp = maxhp + 2 > 16 ? 16 : maxhp + 2; hero.hp = maxhp; setFlag(F_HEART); save(); say(12, 6); hero.st = H_WIN; hero.t = 0; hero.endAfter = true; sfx(523, 120); sfx(659, 120, false); sfx(784, 120, false); sfx(1047, 300, false); }
          e.kind = K_NONE;
        } else if (e.type < 2 && e.t > 420) e.kind = K_NONE;
        break;
      case K_WEB:
        e.t++; e.x += e.vx; e.y += e.vy;
        if (fabsf(hero.x - e.x) < 7 && fabsf(hero.y - 6 - e.y) < 8 && hero.st != H_ROLL) { hero.slowT = 100; hurtHero(1, e.x, e.y); e.kind = K_NONE; break; }
        if (e.t > 80 || e.x < 0 || e.x > 240 || e.y < 0 || e.y > 112) e.kind = K_NONE;
        break;
      case K_BOULDER: {
        e.t++; e.x += e.vx; e.y += e.vy;
        if (e.t % 2 == 0) addPart(e.x, e.y - 4, -e.vx * 0.3f, -0.2f, e.t & 2 ? CGOLD : IX(RT, TH), 14);
        float ddx = e.hx - e.x, ddy = e.hy - e.y;
        if (ddx * ddx + ddy * ddy < 9 || e.t > 140) {
          puff(e.x, e.y, IX(RS, TL), CGOLD, 16); shakeT = 8; sfx(120, 120);
          float hx = hero.x - e.x, hy = hero.y - e.y; if (hx * hx + hy * hy < 16 * 16) hurtHero(2, e.x, e.y);
          e.kind = K_NONE;
        }
      } break;
      case K_ERUPT:
        e.t++;
        if (e.t > 40 && e.t < 70) { float hx = hero.x - e.x, hy = hero.y - e.y; if (hx * hx + hy * hy < 11 * 11) hurtHero(2, e.x, e.y); if (e.t % 4 == 0) addPart(e.x + rnd(0, 8) - 4, e.y - 30, 0, -0.6f, CGOLD, 16); }
        if (e.t > 74) e.kind = K_NONE;
        break;
      default: break;
    }
  }
}
static void partsStep() {
  for (int i = 0; i < NPART; i++) {
    Part& p = parts[i]; if (!p.life) continue;
    p.life--; p.x += p.vx; p.y += p.vy; if (p.grav) p.vy += 0.08f; p.vx *= 0.96f;
  }
}

// ------------------------------------------------------------------ rendu
struct DrawItem { int16_t key; uint8_t kind; uint8_t idx; };
static DrawItem items[96]; static int nItems;
static void addItem(int key, uint8_t kind, int idx) { if (nItems < 96) items[nItems++] = {(int16_t)key, kind, (uint8_t)idx}; }
enum { D_OBJ, D_ENT, D_HERO, D_BOSS };
static void drawObj(const Obj& o) {
  int f = o.a > 1 ? (int)((tick / (o.b ? o.b : 4)) % o.a) : 0;
  blit(o.spr + f, o.x, o.y + OY);
}
static void drawHero() {
  int hx = (int)hero.x, hy = (int)hero.y + OY;
  if (hero.inv > 0 && (hero.inv & 2) && hero.st != H_HURT) return;
  shade(hx, hy, 6, 2);
  switch (hero.st) {
    case H_ATK: { int k = hero.t < 4 ? 0 : (hero.t < 9 ? 1 : 2); blit(HERO_ATK + hero.dir * 3 + k, hx - 16, hy - 29); } break;
    case H_ROLL: blit(HERO_ROLL + ((hero.t / 4) & 1), hx - 8, hy - 23, hero.rdx < 0 ? BF_FLIP : 0); break;
    case H_HURT: blit(HERO_HURT, hx - 8, hy - 23, (hero.t & 2) ? BF_FLASH : 0); break;
    case H_DEAD: blit(HERO_HURT, hx - 8, hy - 23 + (hero.t > 20 ? 3 : 0)); break;
    case H_WIN: blit(HERO_VIC + ((hero.t / 10) & 1), hx - 8, hy - 27); break;
    default: blit(HERO_WALK + hero.dir * 4 + (hero.moving ? (hero.animT / 6) & 3 : 0), hx - 8, hy - 23); break;
  }
}
static void drawEnt(const Ent& e) {
  int x = (int)e.x, y = (int)e.y + OY;
  switch (e.kind) {
    case K_NPC: {
      shade(x, y, 6, 2);
      int f = (e.vx || e.vy) ? (tick / 8) & 1 : ((tick / 30) & 1);
      int base = e.dir == 2 ? 2 : (e.dir == 3 ? 4 : 0);
      blit(e.spr + base + f, x - 8, y - 23);
    } break;
    case K_GATE: { int gx = flag(F_SWORD) ? x : x, gy = flag(F_SWORD) ? y - 26 : y; shade(gx, gy, 6, 2); blit(e.spr + (flag(F_SWORD) ? 0 : 4), gx - 8, gy - 23); } break;
    case K_SIGN: blit(e.spr, e.a, e.b + OY); break;
    case K_CHEST: blit(e.st ? CHEST_OPEN : CHEST_SHUT, e.a, e.b + OY); break;
    case K_POT: shade(x, y, 6, 2); blit(e.spr + ((tick / 60) & 1 ? 0 : 0), e.a, e.b + OY); break;
    case K_GRASS: blit(e.spr + ((tick / 18 + e.a) & 1), e.a, e.b + OY); break;
    case K_ENEMY: {
      int w, fy; enemyFoot(e.type, w, fy);
      bool fly = e.type == E_BAT || e.type == E_SHROUD;
      shade(x, y, e.type == E_BOAR ? 10 : 6, 2);
      if (e.type == E_SHROUD && e.st == 0 && ((tick / 3) % 9) == 0) break;              // le linceul vacille
      int lift = fly ? 6 + (int)(2 * sinf(tick * 0.15f + e.c)) : 0;
      blit(enemySpr(e), x - w / 2, y - fy - lift, (e.flip ? BF_FLIP : 0) | (e.hurt > 8 ? BF_FLASH : 0));
    } break;
    case K_PICK: {
      if (e.type < 2 && e.t > 330 && (tick & 4)) break;
      shade(x, y, 4, 1);
      int bob = (int)(sinf(tick * 0.12f) * 1.5f);
      if (e.type == 0) blit(ICON_GEM, x - 4, y - 10 + bob); else if (e.type == 1) blit(HUD_HEART, x - 6, y - 12 + bob); else blit(FX_HEARTC + ((tick / 10) & 1), x - 8, y - 20 + bob);
    } break;
    case K_WEB: blit(WEB + ((e.t / 5) % 3), x - 7, y - 7); break;
    case K_BOULDER: shade(x, (int)e.hy + OY, 6, 2); blit(FX_TRAIL + ((tick / 3) & 1), x - (e.vx > 0 ? 24 : -4), y - 10, e.vx < 0 ? BF_FLIP : 0); blit(FX_BOULDER + ((tick / 3) & 3), x - 8, y - 12); break;
    case K_ERUPT: { int k = e.t < 40 ? 0 : (e.t < 50 ? 1 : (e.t < 64 ? 2 : 3)); if (e.t >= 0) blit(FX_ERUPT + k, x - 8, y - 38); } break;
    default: break;
  }
}
static void buildPalette() {
  const Room& r = curRoom();
  if (r.zone == Z_ARENA) { memcpy(palNow, PAL_ARENA, sizeof palNow); return; }
  // cycle jour / nuit : 0-55 % jour, -> crepuscule -> nuit -> aube
  float ph = (tod % 9000) / 9000.0f; const uint16_t *a = PAL_DAY, *b = PAL_DAY; float t = 0;
  if (ph < 0.55f) { a = b = PAL_DAY; }
  else if (ph < 0.62f) { a = PAL_DAY; b = PAL_DUSK; t = (ph - 0.55f) / 0.07f; }
  else if (ph < 0.68f) { a = PAL_DUSK; b = PAL_NIGHT; t = (ph - 0.62f) / 0.06f; }
  else if (ph < 0.90f) { a = b = PAL_NIGHT; }
  else if (ph < 0.95f) { a = PAL_NIGHT; b = PAL_DUSK; t = (ph - 0.90f) / 0.05f; }
  else { a = PAL_DUSK; b = PAL_DAY; t = (ph - 0.95f) / 0.05f; }
  for (int i = 0; i < 48; i++) palNow[i] = t <= 0 ? a[i] : mixc(a[i], b[i], (int)(t * 256));
}
static bool isNight() { float ph = (tod % 9000) / 9000.0f; return ph > 0.63f && ph < 0.93f; }
static void convert() {
  uint16_t* out = (uint16_t*)canvas.getBuffer();
  int fade = 256; if (fadeDir) fade = 256 - fadeT * 25; if (fade < 0) fade = 0;
  auto mk = [&](const uint16_t* P, uint16_t* lut) { for (int i = 0; i < 48; i++) lut[i] = sw(fade >= 256 ? P[i] : mixc(0, P[i], fade)); };
  if (curRoom().zone == Z_CRYPT) {
    uint16_t lD[48], lM[48], lL[48]; mk(PAL_DARK, lD); mk(PAL_MID, lM); mk(PAL_LIT, lL);
    const uint16_t* luts[3] = {lD, lM, lL};
    addLight((int)hero.x, (int)hero.y - 10, 34, 0.75f);
    for (int i = 0; i < NENT; i++) if (ents[i].kind == K_ENEMY && ents[i].type == E_BONE) addLight((int)ents[i].x + 1, (int)ents[i].y - 30, 22, 0.8f);
    for (int y = OY; y < 135; y++) {
      const uint8_t* src = fb + y * 240; uint16_t* dst = out + y * 240; const uint8_t* by = BAYER[y & 3];
      for (int x = 0; x < 240; x++) {
        int lvl = 0, bi = by[x & 3];
        for (int l = 0; l < nLights && lvl < 2; l++) {
          int dx = x - lights[l].x, dy = y - lights[l].y; if (dx > 60 || dx < -60 || dy > 50 || dy < -50) continue;
          int32_t d2 = dx * dx + ((dy * dy * 23) >> 4);
          if (d2 <= lights[l].t2[0][bi]) lvl = 2; else if (d2 <= lights[l].t2[1][bi]) lvl = 1;
        }
        dst[x] = luts[lvl][src[x]];
      }
    }
    nLights = 0;                                     // les lumieres fixes sont remises a chaque image
    for (int i = 0; i < curRoom().nobj; i++) { const Obj& o = curRoom().objs[i]; if (o.type == O_LIGHT) addLight(o.x, o.y, o.a + (int)(sinf(tick * 0.3f + i) * 1.5f), 1.0f); }
  } else {
    uint16_t lut[48]; mk(palNow, lut);
    for (int p = OY * 240; p < 240 * 135; p++) out[p] = lut[fb[p]];
  }
}
static void drawHud() {
  blit565(HUD_BG, 0, 0, PAL_DAY);
  int hearts = maxhp / 2;
  for (int i = 0; i < hearts; i++) {
    int v = hero.hp - i * 2; int f = v >= 2 ? 0 : (v == 1 ? 2 : 3);
    if (f == 0 && (hero.hp <= 3 ? (tick / 6) % 3 == 0 : (tick / 6) % 12 == 0)) f = 1;
    blit565(HUD_HEART + f, 5 + i * 12, 3, PAL_DAY);
  }
  char b[12];
  snprintf(b, sizeof b, "x%d", gems); txt(b, 17, 14, &fonts::Font0, rgb(255, 248, 232));
  snprintf(b, sizeof b, "x%d", keys); txt(b, 61, 14, &fonts::Font0, rgb(255, 248, 232));
  if (flag(F_SWORD)) blit565(HUD_SWORD, 240 - 52 + 5, 5, PAL_DAY);
  blit565(HUD_BOOT, 240 - 26 + 5, 5, PAL_DAY);
  if (gb.on && !gb.dead && gb.st != GB_SLEEP) {
    blit565(HUD_BOSSBAR, 80, 3, PAL_DAY);
    int w = 98 * gb.hp / gb.hpMax; canvas.fillRect(83 + w, 5, 98 - w, 5, rgb(20, 16, 31));
    txt(L("EMBER GOLEM", "GOLEM DE BRAISE"), 82, 14, &fonts::Font0, rgb(255, 214, 120));
  }
}
static void drawDialog() {
  int x0 = 8, y0 = 135 - 42, w = 224, h = 38;
  uint16_t wd = PAL_DAY[IX(RW, TB)], wdd = PAL_DAY[IX(RW, TO)], wdl = PAL_DAY[IX(RW, TL)], pl = PAL_DAY[IX(RP, TL)], plh = PAL_DAY[IX(RP, TH)], go = PAL_DAY[CGOLD];
  canvas.fillRect(x0 - 1, y0 - 1, w + 2, h + 2, wdd); canvas.fillRect(x0, y0, w, h, wd);
  for (int i = 2; i < w; i += 5) canvas.drawFastVLine(x0 + i, y0 + 1, 2, wdl);
  canvas.fillRect(x0 + 3, y0 + 3, w - 6, h - 6, pl); canvas.drawFastHLine(x0 + 3, y0 + 3, w - 6, plh); canvas.drawFastVLine(x0 + 3, y0 + 3, h - 6, plh);
  for (int c = 0; c < 4; c++) canvas.fillRect(c & 1 ? x0 + w - 3 : x0 + 1, c & 2 ? y0 + h - 3 : y0 + 1, 2, 2, go);
  const char* who = g_lang ? WHO_FR[dlg.who] : WHO_EN[dlg.who];
  if (*who) {
    int tw = strlen(who) * 6 + 8; uint16_t tc = PAL_DAY[IX(RT, TB)];
    canvas.fillRect(x0 + 9, y0 - 9, tw + 2, 11, wdd); canvas.fillRect(x0 + 10, y0 - 8, tw, 10, tc);
    txt(who, x0 + 14, y0 - 6, &fonts::Font0, rgb(255, 244, 220));
  }
  const char* s = pageStart(dlg.page); int n = pageLen(s), shown = dlg.shown < n ? dlg.shown : n;
  char line[48]; int li = 0, ln = 0;
  for (int i = 0; i <= shown; i++) {
    if (i == shown || s[i] == '\n') { line[li] = 0; txt(line, x0 + 8, y0 + 7 + ln * 10, &fonts::Font0, rgb(52, 34, 26)); ln++; li = 0; if (i == shown) break; continue; }
    if (li < 46) line[li++] = s[i];
  }
  if (dlg.shown >= n && (tick / 12) & 1) canvas.fillTriangle(x0 + w - 16, y0 + h - 11, x0 + w - 8, y0 + h - 11, x0 + w - 12, y0 + h - 6, PAL_DAY[IX(RT, TB)]);
}
static void render() {
  if (shakeT > 0) { shakeT--; offX = rnd(0, 4) - 2; offY = rnd(0, 2) - 1; } else { offX = offY = 0; }
  const Room& r = curRoom();
  // tuiles
  for (int ry = 0; ry < 7; ry++) for (int rx = 0; rx < 15; rx++) {
    uint16_t id = r.tiles[ry][rx]; int an = SPR[id].anim;
    if (an > 1) id += (tick / (r.zone == Z_ARENA ? 10 : 8)) % an;
    blit(id, rx * 16, OY + ry * 16);
  }
  // decor au sol
  for (int i = 0; i < r.nobj; i++) if (r.objs[i].type == O_DECAL) drawObj(r.objs[i]);
  for (int i = 0; i < NENT; i++) {
    const Ent& e = ents[i];
    if (e.kind == K_SPIKES) blit(e.spr + e.st, e.a, e.b + OY);
    if (e.kind == K_DOOR) {
      blit(e.spr, e.a, e.b + OY);
      if (e.st) for (int y = 5; y < 32; y++) for (int x = 6; x < 26; x++) { float ex = (x - 15.5f) / 10, ey = (y - 12.0f) / 8; if (y >= 12 || ex * ex + ey * ey <= 1) pset(e.a + x, e.b + OY + y, CBLACK); }
    }
  }
  if (shock.on) {
    for (int a = 0; a < 360; a += 4) {
      float c = cosf(a * 0.01745f), s = sinf(a * 0.01745f);
      for (int k = 0; k < 3; k++) pset((int)(shock.x + c * (shock.r - k)), (int)(shock.y + OY + s * (shock.r - k) * 0.45f), k == 0 ? CWHITE : (k == 1 ? CGOLD : IX(RT, TH)));
    }
  }
  // tri par profondeur
  nItems = 0;
  for (int i = 0; i < r.nobj; i++) if (r.objs[i].type == O_PROP) addItem(r.objs[i].y + SPR[r.objs[i].spr].h, D_OBJ, i);
  for (int i = 0; i < NENT; i++) { const Ent& e = ents[i];
    if (e.kind == K_NPC || e.kind == K_GATE || e.kind == K_SIGN || e.kind == K_CHEST || e.kind == K_POT || e.kind == K_GRASS || e.kind == K_ENEMY || e.kind == K_PICK || e.kind == K_ERUPT)
      addItem((int)e.y + (e.kind == K_GATE && flag(F_SWORD) ? -26 : 0), D_ENT, i);
    else if (e.kind == K_WEB || e.kind == K_BOULDER) addItem(200, D_ENT, i); }
  addItem((int)hero.y, D_HERO, 0);
  if (gb.on) addItem((int)gb.y - (gb.st == GB_RUBBLE ? 10 : 0), D_BOSS, 0);
  for (int i = 1; i < nItems; i++) { DrawItem t = items[i]; int j = i - 1; while (j >= 0 && items[j].key > t.key) { items[j + 1] = items[j]; j--; } items[j + 1] = t; }
  for (int i = 0; i < nItems; i++) {
    const DrawItem& it = items[i];
    if (it.kind == D_OBJ) drawObj(r.objs[it.idx]); else if (it.kind == D_ENT) drawEnt(ents[it.idx]);
    else if (it.kind == D_HERO) drawHero(); else bossDraw();
  }
  for (int i = 0; i < r.nobj; i++) if (r.objs[i].type == O_FG) drawObj(r.objs[i]);
  // feuilles qui tombent (foret, de jour)
  if (r.zone == Z_FOREST && !isNight()) for (int i = 0; i < 5; i++) {
    int lx = (40 + i * 47 + (int)(10 * sinf((tick + i * 40) * 0.03f))) % 240, ly = ((tick / 2 + i * 37) % 130) - 10;
    blit(FX_LEAF + ((tick / 8 + i) & 3), lx, ly + OY);
  }
  if (r.zone == Z_ARENA && (tick & 3) == 0) addPart(rnd(16, 224), 112, (rnd(0, 10) - 5) / 20.0f, -0.6f, rnd(0, 1) ? CGOLD : IX(RT, TH), 40);
  for (int i = 0; i < NPART; i++) { const Part& p = parts[i]; if (p.life) pset((int)p.x, (int)p.y + OY, p.c); }
  convert();
  // lucioles la nuit (emissives, palette du jour)
  if (r.zone == Z_FOREST && isNight()) for (int i = 0; i < 12; i++) {
    int bx = (i * 73 + 20) % 220 + 6, by = (i * 41 + 10) % 100 + 8;
    int x = bx + (int)(6 * sinf((tick + i * 30) * 0.02f)), y = by + (int)(4 * cosf((tick + i * 20) * 0.017f)) + OY;
    int ph = ((tick / 8) + i) % 6; if (ph == 5) continue;
    static const uint8_t FF[5] = {0, 1, 2, 3, 0}; blit565(FX_FIREFLY + FF[ph], x, y, PAL_DAY);
  }
  drawHud();
}

// ------------------------------------------------------------------ ecrans
static void newGame() {
  flags = 0; gems = 0; keys = 0; maxhp = 10; checkpoint = 0; playSecs = 0; tod = 0; save();
}
static void startPlay() {
  float x, y; spawnPoint(checkpoint, x, y);
  hero.hp = maxhp; hero.dir = 0; hero.inv = 0; hero.slowT = 0; hero.rollCool = 0;
  loadRoom(checkpoint, x, y); fadeDir = 0; mode = PLAY;
#ifdef ZTEST
  if (getenv("ZFLAGS")) flags = atoi(getenv("ZFLAGS"));
  if (getenv("ZTOD")) tod = atoi(getenv("ZTOD"));
  if (getenv("ZKEYS")) keys = atoi(getenv("ZKEYS"));
  if (getenv("ZROOM")) loadRoom(atoi(getenv("ZROOM")), getenv("ZX") ? atoi(getenv("ZX")) : 120, getenv("ZY") ? atoi(getenv("ZY")) : 100);
#endif
  if (checkpoint == 0) hero.dir = 1;
}
static void titleScreen() {
  // fond : la salle de la riviere au crepuscule
  room = 3; tod = 5000; for (int i = 0; i < NENT; i++) ents[i].kind = K_NONE; gb.on = false; shock.on = false;
  buildPalette(); hero.y = -100;
  const Room& r = curRoom();
  for (int ry = 0; ry < 7; ry++) for (int rx = 0; rx < 15; rx++) { uint16_t id = r.tiles[ry][rx]; if (SPR[id].anim > 1) id += (tick / 8) % SPR[id].anim; blit(id, rx * 16, OY + ry * 16); }
  for (int i = 0; i < r.nobj; i++) if (r.objs[i].type == O_DECAL || r.objs[i].type == O_PROP) drawObj(r.objs[i]);
  blit(HERO_WALK + 1 * 4, 150, 60 + OY);
  for (int i = 0; i < 6; i++) blit(FX_LEAF + ((tick / 8 + i) & 3), (30 + i * 41 + (int)(9 * sinf((tick + i * 30) * 0.03f))) % 240, ((tick / 2 + i * 31) % 130) - 10 + OY);
  convert();
  uint16_t* out = (uint16_t*)canvas.getBuffer();
  for (int p = 0; p < 240 * OY; p++) out[p] = sw(rgb(20, 16, 31));
  for (int y = 0; y < 135; y++) for (int x = 0; x < 240; x++) if (y < 70 && ((x + y) & 1)) out[y * 240 + x] = sw(rgb(20, 16, 31));
  txtO("LEGEND OF", 120, 8, &fonts::Font2, rgb(255, 214, 120), rgb(40, 20, 10), textdatum_t::top_center);
  txtO("EMBERWOOD", 120, 26, &fonts::Font4, rgb(255, 160, 60), rgb(60, 14, 10), textdatum_t::top_center, 1.5f);
  if ((tick / 20) & 1) txtO(hasSave ? L("A: CONTINUE   B: NEW GAME", "A : CONTINUER   B : NOUVEAU") : L("PRESS A", "APPUIE SUR A"), 120, 118, &fonts::Font0, rgb(255, 248, 232), rgb(20, 16, 31), textdatum_t::top_center);
}
static void overScreen(bool win) {
  canvas.fillRect(30, 40, 180, 56, rgb(20, 16, 31)); canvas.drawRect(30, 40, 180, 56, rgb(242, 184, 58));
  if (!win) {
    txtO(L("YOU FELL...", "TU ES TOMBE..."), 120, 50, &fonts::Font2, rgb(240, 100, 80), 0, textdatum_t::top_center);
    txt(L("A: TRY AGAIN", "A : REESSAYER"), 120, 78, &fonts::Font0, rgb(255, 248, 232), textdatum_t::top_center);
  } else {
    txtO(L("VICTORY!", "VICTOIRE !"), 120, 46, &fonts::Font2, rgb(255, 214, 120), 0, textdatum_t::top_center);
    char b[48]; snprintf(b, sizeof b, L("TIME %d:%02d   GEMS %d", "TEMPS %d:%02d   GEMMES %d"), playSecs / 60, playSecs % 60, gems);
    txt(b, 120, 64, &fonts::Font0, rgb(255, 248, 232), textdatum_t::top_center);
    snprintf(b, sizeof b, "SCORE %d", finalScore); txt(b, 120, 73, &fonts::Font0, rgb(255, 214, 120), textdatum_t::top_center);
    txt(L("EMBERWOOD IS SAVED. A: MENU", "EMBERWOOD EST SAUVE. A : MENU"), 120, 84, &fonts::Font0, rgb(180, 220, 255), textdatum_t::top_center);
  }
}

// ------------------------------------------------------------------ boucle
static void step() {
  tick++;
#ifdef ZTEST
  if (getenv("ZLOG") && tick % 25 == 0) fprintf(stderr, "t=%u mode=%d room=%d hero=%.0f,%.0f dir=%d st=%d hp=%d gems=%d keys=%d fl=%u boss=%d/%d st%d%c", (unsigned)tick, (int)mode, room, hero.x, hero.y, hero.dir, hero.st, hero.hp, gems, keys, (unsigned)flags, gb.hp, gb.hpMax, gb.st, 10);
#endif
  if (mode == TITLE) {
    if (aPress) { if (!hasSave) newGame(); startPlay(); sfx(660, 60); sfx(990, 120, false); }
    else if (bPress && hasSave) { newGame(); startPlay(); sfx(440, 60); }
    return;
  }
  if (mode == PAUSED) { if (pPress || aPress) mode = PLAY; return; }
  if (mode == GAMEOVER) { if (aPress) { hero.hp = maxhp; startPlay(); } return; }
  if (mode == ENDING) { if (aPress) { checkpoint = 0; save(); mode = TITLE; g_quit = true; } return; }
  if (mode == DIALOG) {
    const char* s = pageStart(dlg.page); int n = pageLen(s);
    if (dlg.shown < n) { dlg.shown += 2; if (tick % 3 == 0) sfx(1000 + rnd(0, 300), 6); }
    if (aPress || bPress) {
      if (dlg.shown < n) dlg.shown = n;
      else if (s[n] == '|') { dlg.page++; dlg.shown = 0; }
      else {
        mode = PLAY;
        if (dlg.after == 1) { setFlag(F_SWORD); save(); hero.st = H_WIN; hero.t = 110; hero.endAfter = false; sfx(523, 100); sfx(784, 200, false); }
      }
    }
    entsStep(); partsStep();
    return;
  }
  // PLAY
  if (++stepSec >= 50) { stepSec = 0; playSecs++; }
  tod++;
  if (fadeDir) {
    fadeT++;
    if (fadeDir == 1 && fadeT >= 10) { loadRoom(nextRoom, nextX, nextY); nextRoom = -1; fadeDir = 2; fadeT = 10; }
    else if (fadeDir == 2) { fadeT -= 2; if (fadeT <= 0) { fadeT = 0; fadeDir = 0; } }
    return;
  }
  if (pPress) { mode = PAUSED; sfx(600, 40); return; }
  heroStep(); entsStep(); bossStep(); partsStep();
}

void begin() {
  fb = idxBuffer();
  if (fb) memset(fb, CBLACK, 240 * 135);
  canvas.drawPixel(0, 0, 0x1234); swap16 = ((uint16_t*)canvas.getBuffer())[0] != 0x1234;
  tilt::load(); load(); bgm::stop();
  mode = TITLE; tick = 0; last = 0; acc = 0; aPend = bPend = pPend = false; fadeDir = 0; nextRoom = -1; shakeT = 0;
}
void loop() {
  if (!fb) { canvas.fillScreen(0); txt("NO MEMORY", 120, 60, &fonts::Font2, 0xFFFF, textdatum_t::top_center); present(0, 0); return; }
  uint32_t now = micros();
  if (!last) last = now;
  acc += now - last; last = now; if (acc > 100000) acc = 100000;
  aPend |= M5.BtnA.wasPressed(); bPend |= M5.BtnB.wasPressed(); pPend |= M5.BtnPWR.wasPressed();
  while (acc >= STEP_US) {
    aPress = aPend; bPress = bPend; pPress = pPend; aPend = bPend = pPend = false;
    step(); acc -= STEP_US;
  }
  {                                                     // musique (boucles originales de bgm.cpp)
    int z = mode == TITLE ? Z_FOREST : curRoom().zone;
    int id = z == Z_CRYPT ? 16 : (z == Z_ARENA ? ((gb.on && !gb.dead && gb.st != GB_SLEEP) ? 17 : 16) : 15);
    bgm::run(id, mode == TITLE || mode == PLAY || mode == DIALOG);
  }
  if (mode == TITLE) titleScreen();
  else {
    buildPalette(); render();
    if (mode == DIALOG) drawDialog();
    else if (mode == PAUSED) ui::pause();
    else if (mode == GAMEOVER) overScreen(false);
    else if (mode == ENDING) overScreen(true);
  }
  present(0, 0);
}
void icon(int x, int y) {
  ui::tile(x, y, rgb(46, 92, 56));
  auto big = [&](uint16_t id, int ox, int oy, int sc) {
    forPix(id, [&](int i, int j, uint8_t c) { canvas.fillRect(x + ox + i * sc, y + oy + j * sc, sc, sc, PAL_DAY[c]); });
  };
  big(FX_HEARTC, 42, 4, 1);
  big(HERO_WALK, 16, 10, 2);
}
}  // namespace zelda
