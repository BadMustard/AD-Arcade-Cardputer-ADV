// Conversion de morceaux importes (RTTTL ou notes simples) vers le format interne "NOTE:duree" (duree en temps).
// Fichier sans dependance Arduino : testable sur PC.
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

namespace songfmt {

static const int MAXNOTES = 380;
static const int SEQCAP = 3300;
struct Result { char title[26]; int bpm; char seq[SEQCAP]; int notes; char err[64]; };

static const char* const NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static inline int semi(char c) {
  switch (toupper((unsigned char)c)) { case 'C': return 0; case 'D': return 2; case 'E': return 4; case 'F': return 5; case 'G': return 7; case 'A': return 9; case 'B': return 11; }
  return -1;
}
static inline void fail(Result& r, const char* m) { snprintf(r.err, sizeof r.err, "%s", m); }

static inline bool addNote(Result& r, int midi, float beats) {
  if (r.notes >= MAXNOTES) { fail(r, "Too many notes (max 380)"); return false; }
  if (midi >= 0 && (midi < 12 || midi > 119)) { fail(r, "A note is out of range"); return false; }
  if (beats < 0.0625f) beats = 0.0625f;
  if (beats > 16.f) beats = 16.f;
  char tmp[24];
  if (midi < 0) snprintf(tmp, sizeof tmp, "R:%g", beats); else snprintf(tmp, sizeof tmp, "%s%d:%g", NAMES[midi % 12], midi / 12 - 1, beats);
  size_t L = strlen(r.seq), n = strlen(tmp);
  if (L + n + 2 >= (size_t)SEQCAP) { fail(r, "Song too long"); return false; }
  if (L) r.seq[L++] = ' ';
  memcpy(r.seq + L, tmp, n + 1); r.notes++;
  return true;
}

static inline void cleanTitle(Result& r, const char* in) {
  int n = 0;
  for (const char* p = in; *p && n < 24; p++) {
    char c = (char)toupper((unsigned char)*p);
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '.' || c == '\'' || c == '!') r.title[n++] = c;
  }
  r.title[n] = 0;
  while (n && r.title[n - 1] == ' ') r.title[--n] = 0;
  int s = 0; while (r.title[s] == ' ') s++;
  if (s) memmove(r.title, r.title + s, n - s + 1);
}

// RTTTL :  nom:d=4,o=5,b=120:8c,8d,e,p,2g.
static inline bool fromRtttl(const char* in, Result& r) {
  const char* c1 = strchr(in, ':'); const char* c2 = c1 ? strchr(c1 + 1, ':') : nullptr;
  if (!c1 || !c2) { fail(r, "Not valid RTTTL (name:d=4,o=5,b=120:notes)"); return false; }
  char name[40]; int nl = (int)(c1 - in); if (nl > 39) nl = 39; memcpy(name, in, nl); name[nl] = 0;
  if (!r.title[0]) cleanTitle(r, name);
  int d = 4, o = 5, b = 63;
  for (const char* p = c1 + 1; p < c2;) {
    while (p < c2 && (*p == ' ' || *p == ',')) p++;
    if (p >= c2) break;
    char k = (char)tolower((unsigned char)*p); const char* eq = strchr(p, '=');
    if (!eq || eq > c2) break;
    int v = atoi(eq + 1);
    if (k == 'd') d = v; else if (k == 'o') o = v; else if (k == 'b') b = v;
    p = eq + 1; while (p < c2 && *p != ',') p++;
  }
  if (d != 1 && d != 2 && d != 4 && d != 8 && d != 16 && d != 32) d = 4;
  if (b < 20) b = 20;
  if (b > 400) b = 400;
  if (o < 1 || o > 8) o = 5;
  r.bpm = b;
  for (const char* p = c2 + 1; *p;) {
    while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
    if (!*p) break;
    int dur = 0; if (isdigit((unsigned char)*p)) { dur = atoi(p); while (isdigit((unsigned char)*p)) p++; }
    if (dur < 1 || dur > 64) dur = d;
    char nc = (char)tolower((unsigned char)*p); p++;
    bool rest = nc == 'p'; int sm = rest ? 0 : semi(nc);
    if (!rest && sm < 0) { snprintf(r.err, sizeof r.err, "Unknown note near '%.6s'", p - 1); return false; }
    bool dot = false;
    if (*p == '#') { sm++; p++; }
    if (*p == '.') { dot = true; p++; }
    int oct = o; if (isdigit((unsigned char)*p)) { oct = *p - '0'; p++; }
    if (*p == '.') { dot = true; p++; }
    if (!addNote(r, rest ? -1 : 12 * (oct + 1) + sm, 4.0f / dur * (dot ? 1.5f : 1.0f))) return false;
  }
  if (!r.notes) { fail(r, "No notes found"); return false; }
  return true;
}

// Notes simples :  [BPM=120]  C5:1 D5:1 E5:2 R:1 F#5:.5 Bb4:1/2   (R = silence, octave facultative = 5)
static inline bool fromSimple(const char* in, Result& r) {
  r.bpm = 120;
  for (const char* p = in; *p;) {
    while (*p && (isspace((unsigned char)*p) || *p == ',' || *p == ';')) p++;
    if (!*p) break;
    if (!strncasecmp(p, "bpm", 3) || !strncasecmp(p, "tempo", 5)) {
      p += (tolower((unsigned char)*p) == 'b') ? 3 : 5;
      while (*p == '=' || *p == ':' || *p == ' ') p++;
      int b = atoi(p); if (b >= 20 && b <= 400) r.bpm = b;
      while (isdigit((unsigned char)*p)) p++;
      continue;
    }
    char c = (char)toupper((unsigned char)*p); int midi = -1;
    if (c == 'R' || c == 'P') p++;
    else {
      int sm = semi(c);
      if (sm < 0) { snprintf(r.err, sizeof r.err, "Unknown token near '%.8s'", p); return false; }
      p++;
      if (*p == '#') { sm++; p++; } else if (*p == 'b') { sm--; p++; }
      int oct = 5; if (isdigit((unsigned char)*p)) { oct = *p - '0'; p++; }
      midi = 12 * (oct + 1) + sm;
    }
    float beats = 1;
    if (*p == ':') {
      p++; float a = (float)atof(p);
      while (isdigit((unsigned char)*p) || *p == '.') p++;
      if (*p == '/') { p++; float bb = (float)atof(p); while (isdigit((unsigned char)*p)) p++; if (bb > 0) a /= bb; }
      if (a > 0) beats = a;
    }
    if (*p && !isspace((unsigned char)*p) && *p != ',' && *p != ';') { snprintf(r.err, sizeof r.err, "Unknown token near '%.8s'", p); return false; }
    if (!addNote(r, midi, beats)) return false;
  }
  if (!r.notes) { fail(r, "No notes found"); return false; }
  return true;
}

static inline bool convert(const char* in, const char* titleIn, Result& r) {
  memset(&r, 0, sizeof r);
  cleanTitle(r, titleIn ? titleIn : "");
  const char* c1 = strchr(in, ':'); const char* c2 = c1 ? strchr(c1 + 1, ':') : nullptr; bool rt = false;
  if (c1 && c2) for (const char* p = c1 + 1; p < c2; p++) if (*p == '=') { rt = true; break; }
  bool ok = rt ? fromRtttl(in, r) : fromSimple(in, r);
  if (!ok) { if (!r.err[0]) fail(r, "Could not read this song"); return false; }
  if (!r.title[0]) snprintf(r.title, sizeof r.title, "MY SONG");
  return true;
}

}  // namespace songfmt
