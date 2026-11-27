// ============================================================================
//  MUSIQUE - le buzzer joue des airs connus (domaine public).
//  B = morceau suivant, HAUT = precedent, M5 = jouer / arreter.
// ============================================================================
#include "common.h"
// Ajout de musiques par Wi-Fi : desactive par defaut (ajouter -DWIFI_MUSIC dans build_flags pour l'activer)
#ifdef WIFI_MUSIC
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#endif
#include "songfmt.h"

namespace music {

struct Tune { const char* name; const char* en; int bpm; const char* seq; };
// notation : NOTE[#]OCTAVE:DUREE (en temps) ; R = silence
static const Tune TUNES[] = {
  {"VIVE LE VENT", "JINGLE BELLS", 150,
   "E5:1 E5:1 E5:2 E5:1 E5:1 E5:2 E5:1 G5:1 C5:1.5 D5:.5 E5:4 F5:1 F5:1 F5:1.5 F5:.5 F5:1 E5:1 E5:1 E5:.5 E5:.5 E5:1 D5:1 D5:1 E5:1 D5:2 G5:2 "
   "E5:1 E5:1 E5:2 E5:1 E5:1 E5:2 E5:1 G5:1 C5:1.5 D5:.5 E5:4 F5:1 F5:1 F5:1.5 F5:.5 F5:1 E5:1 E5:1 E5:.5 E5:.5 G5:1 G5:1 F5:1 D5:1 C5:4"},
  {"ODE A LA JOIE", "ODE TO JOY", 132,
   "E5:1 E5:1 F5:1 G5:1 G5:1 F5:1 E5:1 D5:1 C5:1 C5:1 D5:1 E5:1 E5:1.5 D5:.5 D5:2 "
   "E5:1 E5:1 F5:1 G5:1 G5:1 F5:1 E5:1 D5:1 C5:1 C5:1 D5:1 E5:1 D5:1.5 C5:.5 C5:2 "
   "D5:1 D5:1 E5:1 C5:1 D5:1 E5:.5 F5:.5 E5:1 C5:1 D5:1 E5:.5 F5:.5 E5:1 D5:1 C5:1 D5:1 G4:2"},
  {"FRERE JACQUES", "ARE YOU SLEEPING", 130,
   "C5:1 D5:1 E5:1 C5:1 C5:1 D5:1 E5:1 C5:1 E5:1 F5:1 G5:2 E5:1 F5:1 G5:2 "
   "G5:.5 A5:.5 G5:.5 F5:.5 E5:1 C5:1 G5:.5 A5:.5 G5:.5 F5:.5 E5:1 C5:1 C5:1 G4:1 C5:2 C5:1 G4:1 C5:2"},
  {"AU CLAIR DE LA LUNE", "AU CLAIR DE LA LUNE", 120,
   "C5:1 C5:1 C5:1 D5:1 E5:2 D5:2 C5:1 E5:1 D5:1 D5:1 C5:4 "
   "C5:1 C5:1 C5:1 D5:1 E5:2 D5:2 C5:1 E5:1 D5:1 D5:1 C5:4 "
   "D5:1 D5:1 D5:1 D5:1 A4:2 A4:2 D5:1 C5:1 B4:1 A4:1 G4:4 "
   "C5:1 C5:1 C5:1 D5:1 E5:2 D5:2 C5:1 E5:1 D5:1 D5:1 C5:4"},
  {"JOYEUX ANNIVERSAIRE", "HAPPY BIRTHDAY", 120,
   "G4:.75 G4:.25 A4:1 G4:1 C5:1 B4:2 G4:.75 G4:.25 A4:1 G4:1 D5:1 C5:2 "
   "G4:.75 G4:.25 G5:1 E5:1 C5:1 B4:1 A4:1 F5:.75 F5:.25 E5:1 C5:1 D5:1 C5:3"},
  {"AH VOUS DIRAI-JE MAMAN", "TWINKLE TWINKLE", 135,
   "C5:1 C5:1 G5:1 G5:1 A5:1 A5:1 G5:2 F5:1 F5:1 E5:1 E5:1 D5:1 D5:1 C5:2 "
   "G5:1 G5:1 F5:1 F5:1 E5:1 E5:1 D5:2 G5:1 G5:1 F5:1 F5:1 E5:1 E5:1 D5:2 "
   "C5:1 C5:1 G5:1 G5:1 A5:1 A5:1 G5:2 F5:1 F5:1 E5:1 E5:1 D5:1 D5:1 C5:2"},
  {"POUR ELISE", "FUR ELISE", 118,
   "E5:.5 D#5:.5 E5:.5 D#5:.5 E5:.5 B4:.5 D5:.5 C5:.5 A4:1.5 R:.5 C4:.5 E4:.5 A4:.5 B4:1.5 R:.5 E4:.5 G#4:.5 B4:.5 C5:1.5 R:.5 "
   "E4:.5 E5:.5 D#5:.5 E5:.5 D#5:.5 E5:.5 B4:.5 D5:.5 C5:.5 A4:1.5 R:.5 C4:.5 E4:.5 A4:.5 B4:1.5 R:.5 E4:.5 C5:.5 B4:.5 A4:3"},
  {"KOROBEINIKI", "KOROBEINIKI", 140,
   "E5:1 B4:.5 C5:.5 D5:1 C5:.5 B4:.5 A4:1 A4:.5 C5:.5 E5:1 D5:.5 C5:.5 B4:1.5 C5:.5 D5:1 E5:1 C5:1 A4:1 A4:2 "
   "D5:1.5 F5:.5 A5:1 G5:.5 F5:.5 E5:1.5 C5:.5 E5:1 D5:.5 C5:.5 B4:1 B4:.5 C5:.5 D5:1 E5:1 C5:1 A4:1 A4:2"},
  {"MARCHE TURQUE", "TURKISH MARCH", 128,
   "B4:.25 A4:.25 G#4:.25 A4:.25 C5:1 D5:.25 C5:.25 B4:.25 C5:.25 E5:1 F5:.25 E5:.25 D#5:.25 E5:.25 B5:.25 A5:.25 G#5:.25 A5:.25 B5:.25 A5:.25 G#5:.25 A5:.25 C6:1 A5:.5 C6:.5 B5:.5 A5:.5 G#5:.5 A5:.5 E5:2"},
  {"AMAZING GRACE", "AMAZING GRACE", 100,
   "G4:1 C5:2 E5:.5 C5:.5 E5:2 D5:1 C5:2 A4:1 G4:3 G4:1 C5:2 E5:.5 C5:.5 E5:2 D5:1 G5:3 "
   "G5:1 E5:2 G5:.5 G5:.5 G5:2 E5:1 C5:2 A4:1 G4:2 G4:1 C5:2 E5:.5 C5:.5 E5:2 D5:1 C5:3"},
  {"GREENSLEEVES", "GREENSLEEVES", 120,
   "A4:.5 C5:1 D5:.5 E5:.75 F5:.25 E5:.5 D5:1 B4:.5 G4:.75 A4:.25 B4:.5 C5:1 A4:.5 A4:.75 G#4:.25 A4:.5 B4:1 G#4:.5 E4:1 "
   "A4:.5 C5:1 D5:.5 E5:.75 F5:.25 E5:.5 D5:1 B4:.5 G4:.75 A4:.25 B4:.5 C5:.75 B4:.25 A4:.5 G#4:.75 F#4:.25 G#4:.5 A4:2"},
  {"AULD LANG SYNE", "AULD LANG SYNE", 110,
   "G4:1 C5:1.5 C5:.5 C5:1 E5:1 D5:1.5 D5:.5 D5:1 E5:.5 D5:.5 C5:1.5 C5:.5 E5:1 G5:1 A5:3 A5:1 "
   "G5:1.5 E5:.5 E5:1 C5:1 D5:1.5 D5:.5 D5:1 E5:.5 D5:.5 C5:1.5 A4:.5 A4:1 G4:1 C5:4"},
  {"WHEN THE SAINTS", "WHEN THE SAINTS", 132,
   "C5:1 E5:1 F5:1 G5:4 C5:1 E5:1 F5:1 G5:4 C5:1 E5:1 F5:1 G5:2 E5:2 C5:2 E5:2 D5:4 "
   "E5:1 E5:1 D5:1 C5:3 C5:1 E5:1 G5:2 G5:1 G5:1 F5:4 E5:1 F5:1 G5:2 E5:2 C5:2 D5:2 C5:4"},
  {"OH SUSANNA", "OH SUSANNA", 130,
   "C5:.5 D5:.5 E5:1 G5:1 G5:1.5 A5:.5 G5:1 E5:1 C5:1.5 D5:.5 E5:1 E5:1 D5:1 C5:1 D5:3 R:1 "
   "C5:.5 D5:.5 E5:1 G5:1 G5:1.5 A5:.5 G5:1 E5:1 C5:1.5 D5:.5 E5:1 E5:1 D5:1 D5:1 C5:3 R:1 "
   "F5:2 F5:2 A5:1.5 A5:.5 A5:2 G5:1 G5:1 E5:1 C5:1 D5:1 E5:1 D5:1 C5:1 D5:2 C5:3"},
  {"SILENT NIGHT", "SILENT NIGHT", 96,
   "G4:1.5 A4:.5 G4:1 E4:3 G4:1.5 A4:.5 G4:1 E4:3 D5:2 D5:1 B4:3 C5:2 C5:1 G4:3 "
   "A4:2 A4:1 C5:1.5 B4:.5 A4:1 G4:1.5 A4:.5 G4:1 E4:3 A4:2 A4:1 C5:1.5 B4:.5 A4:1 G4:1.5 A4:.5 G4:1 E4:3 "
   "D5:2 D5:1 F5:1.5 D5:.5 B4:1 C5:3 E5:3 C5:1.5 G4:1.5 E4:1.5 G4:1 F4:1.5 D4:1 C4:3"},
  {"WE WISH YOU A MERRY XMAS", "WE WISH YOU A MERRY XMAS", 140,
   "G4:1 C5:1 C5:.5 D5:.5 C5:.5 B4:.5 A4:1 A4:1 A4:1 D5:1 D5:.5 E5:.5 D5:.5 C5:.5 B4:1 G4:1 G4:1 "
   "E5:1 E5:.5 F5:.5 E5:.5 D5:.5 C5:1 A4:1 G4:.5 G4:.5 A4:1 D5:1 B4:1 C5:3"},
  {"BEETHOVEN 5", "BEETHOVEN 5TH", 112,
   "R:.5 G5:.5 G5:.5 G5:.5 D#5:3 R:.5 F5:.5 F5:.5 F5:.5 D5:3 R:.5 G5:.5 G5:.5 G5:.5 D#5:2 G5:.5 G5:.5 G5:.5 D#5:2 "
   "A#5:.5 A#5:.5 A#5:.5 G5:2 A5:.5 A5:.5 A5:.5 F5:2 G5:4"},
  {"MOUNTAIN KING", "MOUNTAIN KING", 140,
   "A4:.5 B4:.5 C5:.5 D5:.5 E5:.5 C5:.5 E5:1 D#5:.5 B4:.5 D#5:1 A4:.5 B4:.5 C5:.5 D5:.5 E5:.5 C5:.5 E5:1 A5:.5 E5:.5 C5:.5 E5:.5 A5:2 "
   "A4:.25 B4:.25 C5:.25 D5:.25 E5:.25 C5:.25 E5:.5 D#5:.25 B4:.25 D#5:.5 A4:.25 B4:.25 C5:.25 D5:.25 E5:.25 C5:.25 E5:.5 A5:.25 E5:.25 C5:.25 E5:.25 A5:1"},
  {"CANON", "CANON", 100,
   "E5:2 D5:2 C5:2 B4:2 A4:2 G4:2 A4:2 B4:2 "
   "C5:1 E5:1 D5:1 B4:1 C5:1 A4:1 B4:1 G4:1 A4:1 F4:1 G4:1 B4:1 C5:1 E5:1 D5:1 G5:1 "
   "E5:2 D5:2 C5:2 B4:2 A4:2 G4:2 A4:2 B4:2 C5:4"},
  {"YANKEE DOODLE", "YANKEE DOODLE", 140,
   "C5:1 C5:1 D5:1 E5:1 C5:1 E5:1 D5:1 G4:1 C5:1 C5:1 D5:1 E5:1 C5:2 B4:2 "
   "C5:1 C5:1 D5:1 E5:1 F5:1 E5:1 D5:1 C5:1 B4:1 G4:1 A4:1 B4:1 C5:2 C5:2"},
  {"LONDON BRIDGE", "LONDON BRIDGE", 130,
   "G5:1.5 A5:.5 G5:1 F5:1 E5:1 F5:1 G5:2 D5:1 E5:1 F5:2 E5:1 F5:1 G5:2 "
   "G5:1.5 A5:.5 G5:1 F5:1 E5:1 F5:1 G5:2 D5:2 G5:1 E5:1 C5:3"},
  {"CAMPTOWN RACES", "CAMPTOWN RACES", 150,
   "G5:1 G5:1 E5:1 G5:1 A5:1 G5:1 E5:2 D5:1 D5:1 C5:1 D5:1 E5:1 E5:1 D5:2 "
   "G5:1 G5:1 E5:1 G5:1 A5:1 G5:1 E5:2 D5:1 E5:1 D5:1 C5:1 C5:3"},
  {"ENTRY OF GLADIATORS", "ENTRY OF GLADIATORS", 150,
   "D5:.5 D#5:.5 E5:.5 C6:1 E5:.5 C6:1 E5:.5 C6:2 R:.5 C6:.5 D6:.5 D#6:.5 E6:.5 C6:.5 D6:.5 E6:1 B5:.5 D6:1 C6:2 "
   "D5:.5 D#5:.5 E5:.5 C6:1 E5:.5 C6:1 E5:.5 C6:2"},
  {"EINE KLEINE", "EINE KLEINE NACHTMUSIK", 128,
   "G5:1 R:.5 D5:.5 G5:1 R:.5 D5:.5 G5:.5 D5:.5 G5:.5 B5:.5 D6:2 C6:1 R:.5 A5:.5 C6:1 R:.5 A5:.5 C6:.5 A5:.5 F#5:.5 A5:.5 D5:2"},
  {"ROW ROW ROW", "ROW ROW ROW YOUR BOAT", 130,
   "C5:1 C5:1 C5:.75 D5:.25 E5:1 E5:.75 D5:.25 E5:.75 F5:.25 G5:2 C6:.33 C6:.33 C6:.33 G5:.33 G5:.33 G5:.33 E5:.33 E5:.33 E5:.33 C5:.33 C5:.33 C5:.33 G5:.75 F5:.25 E5:.75 D5:.25 C5:2"},
};
static const int NT = sizeof(TUNES) / sizeof(TUNES[0]);

struct Note { float freq; int start, dur; int midi; };         // en millisecondes
static const int NNOTES = 400;
static Note notes[NNOTES]; static int nNotes = 0, totalMs = 0, minMidi = 60, maxMidi = 72;
static int sel = 0; static bool playing = false; static uint32_t t0 = 0; static int curNote = -1;
static int animT = 0;
struct Spark { float x, y, vx, vy; int t; uint16_t c; }; static Spark sp[30];

static int midiOf(const char* s, int &adv) {
  static const int base[7] = {9, 11, 0, 2, 4, 5, 7};      // A B C D E F G
  int i = 0, n = base[s[i++] - 'A'];
  if (s[i] == '#') { n++; i++; }
  int oct = s[i++] - '0'; adv = i;
  return 12 * (oct + 1) + n;
}
// ----------------------------------------------------------------------------- morceaux importes (stockes dans la flash)
static const int MAXC = 24;
struct Custom { int slot; char title[26]; int bpm; };
static Custom cs[MAXC]; static int nCs = 0;
static bool inAdd = false;
static char seqBuf[songfmt::SEQCAP + 8];
#ifdef WIFI_MUSIC
static bool fsOk = false;
static bool inAdd = false;           // inAdd : ecran "ajouter" (l'entree apres le dernier morceau)
static void slotPath(int s, char* b) { snprintf(b, 16, "/m%d.txt", s); }
static bool fsInit() { if (!fsOk) fsOk = LittleFS.begin(true, "/littlefs", 8, "spiffs"); return fsOk; }
static void loadIndex() {
  nCs = 0; if (!fsInit()) return;
  for (int s = 0; s < MAXC; s++) {
    char p[16]; slotPath(s, p); if (!LittleFS.exists(p)) continue;
    File f = LittleFS.open(p, "r"); if (!f) continue;
    String t = f.readStringUntil('\n'); String b = f.readStringUntil('\n'); f.close();
    Custom &c = cs[nCs++]; c.slot = s; c.bpm = b.toInt() > 0 ? b.toInt() : 120;
    strncpy(c.title, t.c_str(), 24); c.title[24] = 0;
  }
  if (inAdd) sel = NT + nCs;                              // l'ecran "ajouter" reste l'ecran "ajouter" quand la liste change
}
static bool loadSeq(int slot) {
  char p[16]; slotPath(slot, p); File f = LittleFS.open(p, "r"); if (!f) return false;
  f.readStringUntil('\n'); f.readStringUntil('\n');
  size_t n = f.readBytes(seqBuf, songfmt::SEQCAP); seqBuf[n] = 0; f.close();
  return true;
}
static int saveSong(const songfmt::Result& r) {
  if (!fsInit()) return -1;
  for (int s = 0; s < MAXC; s++) {
    char p[16]; slotPath(s, p); if (LittleFS.exists(p)) continue;
    File f = LittleFS.open(p, "w"); if (!f) return -1;
    f.printf("%s\n%d\n%s\n", r.title, r.bpm, r.seq); f.close(); return s;
  }
  return -1;
}

#else
static void loadIndex() {}
static bool loadSeq(int) { return false; }
#endif

static int nSongs() { return NT + nCs; }                 // l'entree "ajouter" vient apres
static bool addMode() { return inAdd; }
static const char* titleOf(int k) {
  if (k < NT) return g_lang ? TUNES[k].name : TUNES[k].en;
  if (k < nSongs()) return cs[k - NT].title;
  return L("+ ADD MUSIC", "+ AJOUTER");
}

static void parseSeq(const char* seq, int bpm) {
  float beat = 60000.f / bpm; int ms = 0;
  nNotes = 0; minMidi = 200; maxMidi = 0;
  const char* p = seq;
  while (*p && nNotes < NNOTES) {
    while (*p == ' ') p++;
    if (!*p) break;
    int midi = -1, adv = 0;
    if (*p == 'R') { p++; } else { midi = midiOf(p, adv); p += adv; }
    p++;                                                    // ':'
    float d = atof(p); while (*p && *p != ' ') p++;
    int dur = (int)(d * beat);
    if (midi >= 0) { notes[nNotes++] = {440.f * powf(2.f, (midi - 69) / 12.f), ms, dur, midi}; if (midi < minMidi) minMidi = midi; if (midi > maxMidi) maxMidi = midi; }
    ms += dur;
  }
  if (minMidi > maxMidi) { minMidi = 60; maxMidi = 72; }
  totalMs = ms > 0 ? ms : 1; if (maxMidi - minMidi < 8) maxMidi = minMidi + 8;
}
static void parse(int k) {
  if (k < NT) { parseSeq(TUNES[k].seq, TUNES[k].bpm); return; }
  if (k < nSongs() && loadSeq(cs[k - NT].slot)) { parseSeq(seqBuf, cs[k - NT].bpm); return; }
  nNotes = 0; totalMs = 1; minMidi = 60; maxMidi = 72;
}

#ifdef WIFI_MUSIC
// ----------------------------------------------------------------------------- Wi-Fi : ajout de musiques depuis un telephone / PC
static WebServer* web = nullptr; static DNSServer* dns = nullptr;
static char lastMsg[40] = ""; static uint32_t lastMsgT = 0;
static const char* AP_SSID = "AD-Arcade"; static const char* AP_PASS = "adarcade";
static songfmt::Result R;

static const char PAGE[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>AD Arcade - Add music</title><style>
body{margin:0;background:#12102a;color:#e8e8ff;font:16px system-ui,sans-serif;padding:14px;max-width:640px;margin:auto}
h1{color:#ffd250;margin:6px 0 2px}h2{margin:0 0 8px;font-size:18px;color:#ffd250}small{color:#9aa}
.c{background:#1d1a40;border:1px solid #3a3670;border-radius:12px;padding:14px;margin:12px 0}
input,textarea{width:100%;box-sizing:border-box;background:#0d0b20;color:#fff;border:1px solid #4a4690;border-radius:8px;padding:9px;font:15px monospace;margin:4px 0 10px}
button{background:#ffd250;color:#201400;border:0;border-radius:8px;padding:11px 18px;font-weight:700;font-size:16px}
button.d{background:#ff6070;color:#fff;padding:5px 10px;font-size:13px;margin-left:8px}
li{margin:6px 0}code{background:#0d0b20;padding:2px 6px;border-radius:5px;color:#9fe}pre{white-space:pre-wrap;background:#0d0b20;padding:8px;border-radius:8px;color:#9fe;font-size:13px}
#m{font-weight:700;min-height:20px}.ok{color:#6f9}.er{color:#f77}
</style></head><body>
<h1>AD Arcade</h1><small>Add your own music to the M5 (buzzer)</small>
<div class=c><h2>Your songs</h2><ul id=l><li>loading...</li></ul></div>
<div class=c><h2>Add a song</h2>
<label>Title</label><input id=t maxlength=24 placeholder="MY SONG">
<label>Notes (paste RTTTL or simple notes)</label><textarea id=d rows=6 placeholder="Twinkle:d=4,o=5,b=120:c,c,g,g,a,a,2g"></textarea>
<label>...or load a text file (.txt / .rtttl)</label><input type=file id=f accept=".txt,.rtttl,.ring,text/plain">
<button id=b>Add to AD Arcade</button><p id=m></p></div>
<div class=c><h2>What can I import?</h2>
<p><b>1. RTTTL</b> (old phone ringtones, found all over the web). One line:</p>
<pre>Twinkle:d=4,o=5,b=120:c,c,g,g,a,a,2g,f,f,e,e,d,d,2c</pre>
<p><code>d</code> = default length, <code>o</code> = default octave, <code>b</code> = tempo. Notes: <code>8c</code>, <code>e</code>, <code>4a#</code>, <code>p</code> = pause, <code>.</code> = dotted.</p>
<p><b>2. Simple notes</b>: <code>NOTE+OCTAVE:BEATS</code>, separated by spaces. <code>#</code> = sharp, <code>b</code> = flat, <code>R</code> = rest. Optional tempo first with <code>BPM=120</code>.</p>
<pre>BPM=120 C5:1 C5:1 G5:1 G5:1 A5:1 A5:1 G5:2 R:1 F#5:.5 Bb4:1/2</pre>
<p>Limits: 24 songs, 380 notes each. The buzzer plays one note at a time, so give it a single melody (no chords). Use music you are allowed to use.</p></div>
<script>
const $=i=>document.getElementById(i),m=$('m');
async function ls(){const r=await fetch('/list');const a=await r.json();const u=$('l');u.innerHTML='';
if(!a.length){u.innerHTML='<li>No custom songs yet.</li>';return}
a.forEach(s=>{const li=document.createElement('li');li.textContent=s.t;const b=document.createElement('button');b.className='d';b.textContent='delete';
b.onclick=async()=>{await fetch('/del',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'i='+s.i});ls()};li.appendChild(b);u.appendChild(li)})}
$('f').onchange=e=>{const f=e.target.files[0];if(!f)return;const r=new FileReader();r.onload=()=>{$('d').value=r.result;if(!$('t').value)$('t').value=f.name.replace(/\.[^.]*$/,'')};r.readAsText(f)};
$('b').onclick=async()=>{m.className='';m.textContent='Sending...';
try{const r=await fetch('/add',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({t:$('t').value,d:$('d').value})});
const j=await r.json();m.className=j.ok?'ok':'er';m.textContent=j.msg;if(j.ok){$('d').value='';$('t').value='';ls()}}catch(e){m.className='er';m.textContent='Connection lost, try again'}};
ls();
</script></body></html>)HTML";

static void netStop() {
  if (web) { web->stop(); delete web; web = nullptr; }
  if (dns) { dns->stop(); delete dns; dns = nullptr; }
  WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF);
}
static void setMsg(const char* s) { snprintf(lastMsg, sizeof lastMsg, "%s", s); lastMsgT = millis(); }
static void netStart() {
  if (web) return;
  fsInit();
  WiFi.mode(WIFI_AP); WiFi.softAP(AP_SSID, AP_PASS);
  dns = new DNSServer(); dns->start(53, "*", WiFi.softAPIP());
  web = new WebServer(80);
  web->on("/", HTTP_GET, []() { web->send_P(200, "text/html", PAGE); });
  web->on("/list", HTTP_GET, []() {
    String j = "[";
    for (int i = 0; i < nCs; i++) { if (i) j += ","; j += "{\"i\":"; j += cs[i].slot; j += ",\"t\":\""; j += cs[i].title; j += "\"}"; }
    j += "]"; web->send(200, "application/json", j);
  });
  web->on("/add", HTTP_POST, []() {
    if (!songfmt::convert(web->arg("d").c_str(), web->arg("t").c_str(), R)) { web->send(200, "application/json", String("{\"ok\":false,\"msg\":\"") + R.err + "\"}"); return; }
    if (nCs >= MAXC) { web->send(200, "application/json", "{\"ok\":false,\"msg\":\"Song list is full (24 max)\"}"); return; }
    int s = saveSong(R);
    if (s < 0) { web->send(200, "application/json", "{\"ok\":false,\"msg\":\"Could not save (memory full?)\"}"); return; }
    loadIndex(); char b[40]; snprintf(b, sizeof b, "+ %s", R.title); setMsg(b);
    char o[96]; snprintf(o, sizeof o, "{\"ok\":true,\"msg\":\"Added: %s (%d notes)\"}", R.title, R.notes); web->send(200, "application/json", o);
  });
  web->on("/del", HTTP_POST, []() {
    int s = web->arg("i").toInt(); char p[16]; slotPath(s, p); LittleFS.remove(p); loadIndex(); setMsg("deleted"); web->send(200, "text/plain", "ok");
  });
  web->onNotFound([]() { web->sendHeader("Location", "http://192.168.4.1/", true); web->send(302, "text/plain", ""); });
  web->begin();
}
void end() { if (web) netStop(); }

#else
static void netStart() {}
static void netStop() {}
void end() {}
static const char* AP_SSID = ""; static const char* AP_PASS = ""; static char lastMsg[2] = ""; static uint32_t lastMsgT = 0;
#endif

void begin() {
  loadIndex();
  Preferences pr; pr.begin("music", false); sel = pr.getInt("sel", 0); if (sel < 0 || sel >= nSongs()) sel = 0;
  inAdd = false; parse(sel); playing = false; curNote = -1; for (auto &s : sp) s.t = 0;
}
static void startPlay() { playing = true; t0 = millis(); curNote = -1; }
static void stopPlay() { playing = false; M5.Speaker.stop(); }

static uint16_t hueCol(int midi) {
  static const uint16_t pal[12] = {P_R, P_G, P_S, P_B, P_L, P_G, P_S, P_R, P_B, P_G, P_S, P_L};
  return pal[midi % 12];
}
static int yOf(int midi) { return 112 - (midi - minMidi) * 84 / (maxMidi - minMidi); }

static void step() {
  animT++;
  for (auto &s : sp) if (s.t > 0) { s.t--; s.x += s.vx; s.y += s.vy; s.vy += 0.08f; }
  if (!playing) return;
  int now = (int)(millis() - t0);
  if (now >= totalMs + 300) { stopPlay(); return; }
  for (int i = 0; i < nNotes; i++) {
    if (now >= notes[i].start && now < notes[i].start + notes[i].dur) {
      if (i != curNote) {
        curNote = i;
        sfx((int)notes[i].freq, notes[i].dur * 9 / 10 > 30 ? notes[i].dur * 9 / 10 : 30, true);
        for (int k = 0; k < 6; k++) for (auto &s : sp) if (s.t <= 0) { s = {80, (float)yOf(notes[i].midi) + 4, rnd(0, 20) / 10.f - 0.6f, -rnd(5, 20) / 10.f, 24, hueCol(notes[i].midi)}; break; }
      }
      break;
    }
  }
}

static void renderAdd() {
  ui::bg((int)(millis() / 60));
  ui::header(titleOf(sel), P_S);
  ui::window(4, 20, 232, 96, P_G, L("ADD SONGS", "AJOUTER"));
  int wx = 214, wy = 44; int ph = (millis() / 300) % 4;                                   // icone Wi-Fi animee
  canvas.fillRect(wx - 2, wy + 18, 5, 5, P_K); canvas.fillRect(wx - 1, wy + 19, 3, 3, P_S);
  for (int k = 0; k < 3; k++) { uint16_t c = k < ph ? P_S : P_L; for (int w = 0; w < 3; w++) canvas.drawArc(wx, wy + 18, 8 + k * 7 + w, 8 + k * 7 + w, 225, 315, w == 0 || w == 2 ? P_K : c); }
  txt(L("1. Connect to Wi-Fi", "1. Connecte-toi au Wi-Fi"), 10, 42, &fonts::Font0, P_M);
  txt(AP_SSID, 10, 51, &fonts::Font2, P_R);
  char b[40]; snprintf(b, sizeof b, L("password: %s", "mot de passe : %s"), AP_PASS); txt(b, 10, 68, &fonts::Font0, P_K);
  txt(L("2. Open in your browser", "2. Ouvre dans ton navigateur"), 10, 82, &fonts::Font0, P_M);
  txt("192.168.4.1", 10, 92, &fonts::Font2, P_R);
  if (lastMsg[0] && millis() - lastMsgT < 5000) ui::footer(lastMsg);
  else { snprintf(b, sizeof b, L("%d custom songs   B = back", "%d morceaux perso   B = retour"), nCs); ui::footer(b); }
}

static void render() {
  if (addMode()) { renderAdd(); return; }
  ui::bg((int)(millis() / 60));
  int now = playing ? (int)(millis() - t0) : 0;
  canvas.fillRect(3, 20, SCR_W - 6, 99, P_K); canvas.fillRect(4, 21, SCR_W - 8, 97, P_D); canvas.fillRect(4, 21, SCR_W - 8, 1, P_M);
  for (int i = 0; i < 5; i++) canvas.drawFastHLine(4, 30 + i * 18, SCR_W - 8, P_M);
  const int HX = 80; const float PXMS = 0.09f;
  canvas.setClipRect(4, 21, SCR_W - 8, 97);
  for (int i = 0; i < nNotes; i++) {
    float x0 = HX + (notes[i].start - now) * PXMS, w = notes[i].dur * PXMS - 2;
    if (x0 > SCR_W || x0 + w < 0) continue;
    int y = yOf(notes[i].midi); uint16_t c = hueCol(notes[i].midi);
    bool active = playing && now >= notes[i].start && now < notes[i].start + notes[i].dur;
    bool past = playing && now >= notes[i].start + notes[i].dur;
    if (w < 3) w = 3;
    if (active) canvas.fillRect((int)x0 - 3, y - 3, (int)w + 6, 13, P_B);
    canvas.fillRect((int)x0 - 1, y - 1, (int)w + 2, 9, P_K); canvas.fillRect((int)x0, y, (int)w, 7, past ? P_M : c);
    if (!past) { canvas.drawFastHLine((int)x0, y, (int)w, P_B); canvas.drawFastHLine((int)x0, y + 6, (int)w, P_D); }
  }
  canvas.fillRect(HX - 1, 22, 3, 95, P_K); canvas.drawFastVLine(HX, 22, 95, P_B);
  for (auto &s : sp) if (s.t > 0) canvas.fillRect((int)s.x, (int)s.y, 2, 2, s.c);
  canvas.clearClipRect();
  canvas.fillTriangle(HX - 6, 21, HX + 6, 21, HX, 29, P_K); canvas.fillTriangle(HX - 4, 21, HX + 4, 21, HX, 27, P_G);
  ui::header(titleOf(sel), P_S);
  canvas.fillTriangle(6, 10, 14, 4, 14, 16, P_K); canvas.fillTriangle(8, 10, 13, 6, 13, 14, P_B);
  canvas.fillTriangle(SCR_W - 6, 10, SCR_W - 14, 4, SCR_W - 14, 16, P_K); canvas.fillTriangle(SCR_W - 8, 10, SCR_W - 13, 6, SCR_W - 13, 14, P_B);
  // barre de lecture
  canvas.fillRect(0, 119, SCR_W, 16, P_K); canvas.fillRect(0, 119, SCR_W, 1, P_G);
  int prog = playing ? (int)((SCR_W - 4) * clampf(now / (float)totalMs, 0, 1)) : 0;
  canvas.fillRect(2, 130, SCR_W - 4, 3, P_M); canvas.fillRect(2, 130, prog, 3, P_G);
  if (playing) { canvas.fillRect(SCR_W / 2 - 6, 122, 4, 6, P_B); canvas.fillRect(SCR_W / 2 + 2, 122, 4, 6, P_B); }
  else canvas.fillTriangle(SCR_W / 2 - 4, 121, SCR_W / 2 - 4, 128, SCR_W / 2 + 4, 124, P_S);
  if (playing && curNote >= 0) for (int i = 0; i < 6; i++) { int h = 2 + (int)((sinf(animT * 0.5f + i * 1.3f) + 1.0f) * 3); canvas.fillRect(8 + i * 5, 129 - h, 3, h, hueCol(notes[curNote].midi)); }
}

void loop() {
  bool a = M5.BtnA.wasPressed(), p = M5.BtnPWR.wasPressed(), b = M5.BtnB.wasPressed();
#ifdef WIFI_MUSIC
  int total = nSongs() + 1;                                // + l'entree "ajouter"
#else
  int total = nSongs();
#endif
  if (b || p || (a && addMode())) {
    bool back = p || (a && addMode());
    stopPlay(); if (addMode()) netStop();
    sel = (sel + (back ? total - 1 : 1)) % total; sfx(back ? 1000 : 1200, 20);
    inAdd = (sel == nSongs());
    if (inAdd) { nNotes = 0; totalMs = 1; netStart(); lastMsg[0] = 0; }
    else { parse(sel); Preferences pr; pr.begin("music", false); pr.putInt("sel", sel); }
  } else if (a) { if (playing) stopPlay(); else startPlay(); }
#ifdef WIFI_MUSIC
  if (web) { web->handleClient(); dns->processNextRequest(); }
#endif
  step();
  render();
  present(0, 0);
  delay(8);
}

void icon(int x, int y) {
  ui::tile(x, y, P_D);
  for (int i = 0; i < 5; i++) canvas.fillRect(x + 4, y + 14 + i * 8, 56, 1, P_L);
  pxDisc(x + 21, y + 46, 6, P_B, P_B, P_L); pxDisc(x + 45, y + 40, 6, P_B, P_B, P_L);
  canvas.fillRect(x + 25, y + 12, 4, 35, P_K); canvas.fillRect(x + 26, y + 13, 2, 33, P_B);
  canvas.fillRect(x + 49, y + 6, 4, 35, P_K); canvas.fillRect(x + 50, y + 7, 2, 33, P_B);
  canvas.fillTriangle(x + 25, y + 10, x + 53, y + 3, x + 53, y + 13, P_K); canvas.fillTriangle(x + 28, y + 11, x + 50, y + 6, x + 50, y + 11, P_G);
  canvas.fillRect(x + 6, y + 6, 4, 4, P_S); canvas.fillRect(x + 7, y + 7, 2, 2, P_B); canvas.fillRect(x + 8, y + 54, 4, 4, P_R); canvas.fillRect(x + 52, y + 52, 4, 4, P_G);
}

}  // namespace music
