// ============================================================================
//  BGM - musiques de fond ORIGINALES (une par jeu, ~40 a 70 s, structure A-B-A).
//  Mono : une note a la fois, sur un canal a part du buzzer, a volume reduit.
//  Pseudo-polyphonie : sur les notes longues, la melodie laisse place a une note de basse
//  (syntaxe "E5:1/C4" = melodie E5, puis basse C4 sur la fin de la note).
//  Elle se tait pendant les effets sonores (sfx) et reprend en suivant le temps.
//  Reglage : Settings > "Game music" (g_music).
// ============================================================================
#include "common.h"

namespace bgm {

struct Loop { int bpm; const char* seq; };
static const Loop LOOPS[] = {
  /* 0 FLAP */ {138,
                        "G5:.5 F5:1/C4 E5:.5 E5:1/C4 C5:1/C4 B4:.5 A4:.5 B4:.5 B4:.5 B4:.5 A4:.5 B4:.5 C5:.5 C5:1/A3 D5:1/A3 E5:1/A3 G5:1/A3 "
                        "A5:1/F4 B5:1/F4 A5:2/F4 G5:.5 A5:1/C4 B5:.5 G5:1/C4 E5:1/C4 D5:.5 C5:.5 B4:.5 A4:.5 B4:.5 D5:.5 F5:.5 G5:.5 F5:1/F4 "
                        "G5:1/F4 A5:1/F4 B5:1/F4 B5:1/G3 C6:1/G3 C6:2/G3 E6:1/A3 C6:.5 D6:.5 C6:.5 B5:.5 D6:1/A3 C6:.5 E6:1/F4 E6:.5 C6:1/F4 "
                        "D6:1/F4 C6:1/C4 D6:.5 E6:.5 E6:.5 E6:.5 E6:1/C4 D6:1/G3 C6:1/G3 B5:2/G3 A5:1/A3 C6:.5 E6:.5 E6:.5 E6:.5 E6:1/A3 C6:.5 "
                        "E6:1/F4 E6:.5 C6:1/F4 E6:1/F4 D6:1/G3 E6:.5 E6:.5 D6:.5 E6:.5 E6:1/G3 E6:1/C4 E6:1/C4 D6:2/C4 G5:.5 F5:1/C4 E5:.5 "
                        "E5:1/C4 C5:1/C4 B4:.5 A4:.5 B4:.5 B4:.5 B4:.5 A4:.5 B4:.5 C5:.5 C5:1/A3 D5:1/A3 E5:1/A3 G5:1/A3 A5:1/F4 B5:1/F4 "
                        "A5:2/F4 G5:.5 A5:1/C4 B5:.5 G5:1/C4 E5:1/C4 D5:.5 C5:.5 B4:.5 A4:.5 B4:.5 D5:.5 F5:.5 G5:.5 F5:1/F4 G5:1/F4 A5:1/F4 "
                        "B5:1/F4 B5:1/G3 C6:1/G3 C6:2/C4"},
  /* 1 SNAKE */ {118,
                        "E5:.5 F#5:1/A3 G5:.5 A5:1/A3 G5:1/A3 G5:1/G3 R:1 G5:1/G3 A5:1/G3 A5:1/F#4 B5:.5 C6:.5 C6:.5 C6:.5 A5:1/F#4 G5:1/G3 "
                        "F#5:1/G3 F#5:2/G3 E5:.5 F#5:1/A3 A5:.5 A5:1/A3 C6:1/A3 A5:1/D4 G5:1/D4 F#5:1/D4 G5:1/D4 G5:1/E4 F#5:.5 R:.5 E5:.5 "
                        "D5:.5 R:1 E5:1/A3 C5:1/A3 C5:2/A3 D5:1/D4 R:.5 B4:.5 D5:.5 C5:.5 D5:1/D4 E5:.5 F#5:1/A3 G5:.5 E5:1/A3 R:1 D5:1/D4 R:.5 "
                        "E5:.5 F#5:.5 G5:.5 R:1 G5:1/E4 B5:1/E4 B5:2/E4 A5:1/A3 F#5:.5 A5:.5 A5:.5 G5:.5 F#5:1/A3 G5:.5 E5:1/G3 R:.5 D5:1/G3 "
                        "C5:1/G3 C5:1/F#4 C5:.5 A4:.5 A4:.5 R:.5 B4:1/F#4 B4:1/E4 D5:1/E4 E5:2/E4 E5:.5 F#5:1/A3 G5:.5 A5:1/A3 G5:1/A3 G5:1/G3 "
                        "R:1 G5:1/G3 A5:1/G3 A5:1/F#4 B5:.5 C6:.5 C6:.5 C6:.5 A5:1/F#4 G5:1/G3 F#5:1/G3 F#5:2/G3 E5:.5 F#5:1/A3 A5:.5 A5:1/A3 "
                        "C6:1/A3 A5:1/D4 G5:1/D4 F#5:1/D4 G5:1/D4 G5:1/E4 F#5:.5 R:.5 E5:.5 D5:.5 R:1 E5:1/A3 C5:1/A3 E5:2/A3"},
  /* 2 TETRIS */ {126,
                        "F5:1/D4 G5:.5 A#5:.5 A5:1/D4 A#5:1/D4 C6:1.5/C4 D6:.5 C6:1.5/C4 D6:.5 D6:1/A#3 C6:1/A#3 D6:1/A#3 D6:1/A#3 C6:1/A3 "
                        "D6:1/A3 C6:2/A3 D6:1/D4 C6:.5 C6:.5 A5:1/D4 G5:1/D4 G5:1.5/G3 F5:.5 G5:1.5/G3 E5:.5 E5:1/A3 D5:1/A3 E5:1/A3 D5:1/A3 "
                        "D5:1/D4 E5:1/D4 F5:2/D4 A5:.5 G5:.5 F5:.5 E5:.5 F5:.5 D5:.5 A#4:.5 G4:.5 G4:1/C4 G4:.5 G4:.5 G4:1/C4 A4:1/C4 A4:.5 "
                        "C5:.5 D5:.5 E5:.5 F5:.5 G5:.5 A5:.5 C6:.5 C6:1/A3 D6:1/A3 C6:2/A3 A#5:.5 D6:.5 D6:.5 D6:.5 D6:.5 A#5:.5 C6:.5 A#5:.5 "
                        "A#5:1/G3 C6:.5 D6:.5 D6:1/G3 D6:1/G3 C6:.5 D6:.5 D6:.5 A#5:.5 A5:.5 A#5:.5 A5:.5 G5:.5 F5:1/D4 D5:1/D4 E5:2/D4 F5:1/D4 "
                        "G5:.5 A#5:.5 A5:1/D4 A#5:1/D4 C6:1.5/C4 D6:.5 C6:1.5/C4 D6:.5 D6:1/A#3 C6:1/A#3 D6:1/A#3 D6:1/A#3 C6:1/A3 D6:1/A3 "
                        "C6:2/A3 D6:1/D4 C6:.5 C6:.5 A5:1/D4 G5:1/D4 G5:1.5/G3 F5:.5 G5:1.5/G3 E5:.5 E5:1/A3 D5:1/A3 E5:1/A3 D5:1/A3 D5:1/D4 "
                        "E5:1/D4 F5:2/D4"},
  /* 3 STACK */ {100,
                        "G5:2/C4 G5:1/C4 F5:1/C4 E5:1/A3 D5:1/A3 E5:1/A3 C5:1/A3 C5:2/F4 C5:2/F4 D5:1/G3 C5:1/G3 D5:2/G3 E5:2/C4 E5:1/C4 "
                        "D5:1/C4 E5:1/A3 C5:1/A3 C5:1/A3 B4:1/A3 C5:2/F4 C5:2/F4 D5:1/G3 C5:1/G3 C5:2/G3 F5:2/D4 F5:2/D4 G5:2/G3 G5:1/G3 "
                        "B5:1/G3 C6:2/C4 C6:2/C4 C6:1/A3 D6:1/A3 C6:2/A3 C6:2/F4 C6:2/F4 B5:2/G3 B5:1/G3 A5:1/G3 G5:2/C4 G5:2/C4 G5:1/C4 "
                        "F5:1/C4 G5:2/C4 G5:2/C4 G5:1/C4 F5:1/C4 E5:1/A3 D5:1/A3 E5:1/A3 C5:1/A3 C5:2/F4 C5:2/F4 D5:1/G3 C5:1/G3 D5:2/G3 "
                        "E5:2/C4 E5:1/C4 D5:1/C4 E5:1/A3 C5:1/A3 C5:1/A3 B4:1/A3 C5:2/F4 C5:2/F4 D5:1/G3 C5:1/G3 G5:2/C4"},
  /* 4 FISH */ {150,
                        "B5:.25 A5:.25 G5:.25 F#5:.25 E5:.25 G5:.25 A5:.25 B5:.25 B5:.25 A5:.25 B5:.25 C6:.25 D6:.25 E6:.25 F#6:.25 G6:.25 "
                        "G6:.5 A6:.5 A6:.5 G6:.5 G6:.5 G6:.5 F#6:.5 D6:.5 C6:.5 D6:1/A3 C6:.5 C6:1/A3 D6:1/A3 D6:.5 C6:.5 D6:1/D4 C6:2/D4 "
                        "B5:.25 G5:.25 B5:.25 C6:.25 B5:.25 D6:.25 D6:.25 C6:.25 B5:.25 A5:.25 C6:.25 D6:.25 E6:.25 D6:.25 F#6:.25 E6:.25 E6:.5 "
                        "C6:.5 D6:.5 C6:.5 B5:.5 C6:.5 C6:.5 B5:.5 C6:.5 D6:1/C4 E6:.5 E6:1/C4 C6:1/C4 D6:.5 E6:.5 G6:1/D4 G6:2/D4 F#6:.5 "
                        "A6:1/B3 F#6:.5 F#6:1/B3 E6:1/B3 E6:.25 C6:.25 B5:.25 A5:.25 A5:.25 G5:.25 A5:.25 B5:.25 B5:.25 A5:.25 G5:.25 A5:.25 "
                        "G5:.25 A5:.25 G5:.25 F#5:.25 E5:.5 D5:1/A3 C5:.5 C5:1/A3 C5:1/A3 D5:.5 E5:.5 G5:1/D4 A5:2/D4 B5:.5 C6:1/G3 E6:.5 "
                        "D6:1/G3 E6:1/G3 E6:.25 G6:.25 A6:.25 F#6:.25 A6:.25 G6:.25 F#6:.25 D6:.25 C6:.25 B5:.25 C6:.25 A5:.25 B5:.25 A5:.25 "
                        "G5:.25 A5:.25 A5:.5 B5:1/D4 D6:.5 D6:1/D4 F#6:1/D4 D6:.5 F#6:.5 E6:1/G3 D6:2/G3 B5:.25 A5:.25 G5:.25 F#5:.25 E5:.25 "
                        "G5:.25 A5:.25 B5:.25 B5:.25 A5:.25 B5:.25 C6:.25 D6:.25 E6:.25 F#6:.25 G6:.25 G6:.5 A6:.5 A6:.5 G6:.5 G6:.5 G6:.5 "
                        "F#6:.5 D6:.5 C6:.5 D6:1/A3 C6:.5 C6:1/A3 D6:1/A3 D6:.5 C6:.5 D6:1/D4 C6:2/D4 B5:.25 G5:.25 B5:.25 C6:.25 B5:.25 D6:.25 "
                        "D6:.25 C6:.25 B5:.25 A5:.25 C6:.25 D6:.25 E6:.25 D6:.25 F#6:.25 E6:.25 E6:.5 C6:.5 D6:.5 C6:.5 B5:.5 C6:.5 C6:.5 B5:.5 "
                        "C6:.5 D6:1/C4 E6:.5 E6:1/C4 C6:1/C4 D6:.5 E6:.5 G6:1/D4 D6:2/G3"},
  /* 5 RIVER */ {112,
                        "F5:.5 A5:.5 B5:.5 A5:.5 A5:.5 G5:.5 A5:.5 G5:.5 G5:1.5/G3 F5:.5 G5:1.5/G3 F5:.5 F5:1/D4 E5:1/D4 F5:1/D4 D5:1/D4 "
                        "E5:1/A3 C5:1/A3 C5:2/A3 D5:.5 E5:.5 F5:.5 G5:.5 F5:.5 G5:.5 A5:.5 G5:.5 G5:1.5/G3 A5:.5 G5:1.5/G3 G5:.5 A5:1/A3 "
                        "B5:1/A3 A5:1/A3 G5:1/A3 F5:1/D4 G5:1/D4 F5:2/D4 F5:1/B3 G5:1/B3 F5:1/B3 D5:1/B3 F5:.5 G5:.5 A5:.5 C6:.5 C6:.5 A5:.5 "
                        "G5:.5 E5:.5 F5:1/B3 G5:1/B3 F5:1/B3 E5:1/B3 D5:1/G3 F5:1/G3 F5:2/G3 F5:1/D4 D5:1/D4 D5:1/D4 E5:1/D4 E5:.5 F5:.5 G5:.5 "
                        "A5:.5 A5:.5 C6:.5 C6:.5 B5:.5 B5:1/G3 C6:1/G3 B5:1/G3 C6:1/G3 A5:1/D4 G5:1/D4 A5:2/D4 F5:.5 A5:.5 B5:.5 A5:.5 A5:.5 "
                        "G5:.5 A5:.5 G5:.5 G5:1.5/G3 F5:.5 G5:1.5/G3 F5:.5 F5:1/D4 E5:1/D4 F5:1/D4 D5:1/D4 E5:1/A3 C5:1/A3 C5:2/A3 D5:.5 E5:.5 "
                        "F5:.5 G5:.5 F5:.5 G5:.5 A5:.5 G5:.5 G5:1.5/G3 A5:.5 G5:1.5/G3 G5:.5 A5:1/A3 B5:1/A3 A5:1/A3 G5:1/A3 F5:1/D4 G5:1/D4 "
                        "A5:2/D4"},
  /* 6 LANDER */ {84,
                        "E5:2/A3 E5:1/A3 F5:1/A3 F5:2/F4 F5:2/F4 E5:1/C4 D5:1/C4 E5:1/C4 D5:1/C4 D5:1/G3 C5:1/G3 D5:2/G3 E5:2/A3 E5:1/A3 "
                        "G5:1/A3 F5:2/F4 F5:2/F4 F5:1/D4 G5:1/D4 F5:1/D4 G5:1/D4 G5:1/E4 A5:1/E4 A5:2/E4 D6:1/D4 E6:1/D4 D6:1/D4 C6:1/D4 "
                        "C6:2/F4 C6:1/F4 B5:1/F4 A5:1/A3 F5:1/A3 E5:1/A3 E5:1/A3 E5:1/E4 D5:1/E4 D5:2/E4 D5:1/D4 C5:1/D4 D5:1/D4 B4:1/D4 "
                        "B4:2/G3 B4:1/G3 A4:1/G3 B4:1/E4 D5:1/E4 E5:1/E4 G5:1/E4 A5:1/A3 G5:1/A3 G5:2/A3 E5:2/A3 E5:1/A3 F5:1/A3 F5:2/F4 "
                        "F5:2/F4 E5:1/C4 D5:1/C4 E5:1/C4 D5:1/C4 D5:1/G3 C5:1/G3 D5:2/G3 E5:2/A3 E5:1/A3 G5:1/A3 F5:2/F4 F5:2/F4 F5:1/D4 "
                        "G5:1/D4 F5:1/D4 G5:1/D4 G5:1/E4 A5:1/E4 A5:2/A3"},
  /* 7 DINO */ {148,
                        "E5:.5 F#5:1/E4 E5:.5 E5:1/E4 C5:1/E4 D5:.5 C5:.5 R:.5 B4:.5 D5:.5 E5:.5 F#5:.5 G5:.5 G5:1/C4 A5:.5 B5:.5 G5:.5 A5:.5 "
                        "C6:1/C4 D6:1/D4 D6:1/D4 C6:2/D4 B5:.5 R:1 C6:.5 B5:1/E4 C6:1/E4 D6:.5 D6:.5 C6:.5 D6:.5 D6:.5 C6:.5 B5:.5 C6:.5 "
                        "C6:1/A3 B5:.5 C6:.5 C6:.5 D6:.5 B5:1/A3 B5:1/B3 C6:1/B3 B5:2/B3 C6:1/C4 D6:.5 C6:.5 C6:.5 B5:.5 A5:1/C4 A5:.5 C6:1/A3 "
                        "D6:.5 C6:1/A3 B5:1/A3 B5:1/E4 G5:.5 F#5:.5 E5:.5 D5:.5 F#5:1/E4 F#5:1/B3 E5:1/B3 F#5:2/B3 E5:1/C4 G5:.5 R:.5 G5:.5 "
                        "B5:.5 R:1 A5:.5 F#5:1/D4 E5:.5 F#5:1/D4 R:1 F#5:1/B3 E5:.5 C5:.5 B4:.5 G4:.5 E4:1/B3 E4:1/E4 E4:1/E4 F#4:2/E4 E5:.5 "
                        "F#5:1/E4 E5:.5 E5:1/E4 C5:1/E4 D5:.5 C5:.5 R:.5 B4:.5 D5:.5 E5:.5 F#5:.5 G5:.5 G5:1/C4 A5:.5 B5:.5 G5:.5 A5:.5 C6:1/C4 "
                        "D6:1/D4 D6:1/D4 C6:2/D4 B5:.5 R:1 C6:.5 B5:1/E4 C6:1/E4 D6:.5 D6:.5 C6:.5 D6:.5 D6:.5 C6:.5 B5:.5 C6:.5 C6:1/A3 B5:.5 "
                        "C6:.5 C6:.5 D6:.5 B5:1/A3 B5:1/B3 C6:1/B3 G4:2/E4"},
  /* 8 METEORS */ {132,
                        "F#5:.5 D5:.5 C#5:.5 B4:.5 B4:.5 A4:.5 G4:.5 F#4:.5 G4:.5 A4:.25 C#5:.25 A4:.5 G4:.25 A4:.25 B4:.5 C#5:.25 A4:.25 G4:.5 "
                        "F#4:.25 G4:.25 G4:.5 A4:1/E4 B4:.5 B4:1/E4 D5:1/E4 E5:1/A3 D5:1/A3 E5:2/A3 F#5:.5 D5:.5 E5:.5 C#5:.5 D5:.5 B4:.5 D5:.5 "
                        "F#5:.5 G5:.5 B5:.25 D6:.25 E6:.5 C#6:.25 E6:.25 D6:.5 C#6:.25 B5:.25 A5:.5 B5:.25 C#6:.25 B5:.5 A5:1/E4 F#5:.5 E5:1/E4 "
                        "C#5:1/E4 C#5:1/F#4 A4:1/F#4 B4:2/F#4 D5:.5 E5:1/D4 D5:.5 D5:1/D4 C#5:1/D4 C#5:.5 D5:.5 C#5:.5 A4:.5 A4:.5 G4:.5 A4:.5 "
                        "C#5:.5 D5:.5 E5:1/G3 C#5:.5 D5:1/G3 E5:1/G3 F#5:1/F#4 A5:1/F#4 A5:2/F#4 B5:.5 D6:1/B3 E6:.5 D6:1/B3 D6:1/B3 B5:.5 "
                        "G5:.5 F#5:.5 E5:.5 E5:.5 D5:.5 C#5:.5 A4:.5 A4:.5 G4:1/F#4 A4:.5 A4:1/F#4 A4:1/F#4 B4:1/B3 C#5:1/B3 C#5:2/B3 F#5:.5 "
                        "D5:.5 C#5:.5 B4:.5 B4:.5 A4:.5 G4:.5 F#4:.5 G4:.5 A4:.25 C#5:.25 A4:.5 G4:.25 A4:.25 B4:.5 C#5:.25 A4:.25 G4:.5 "
                        "F#4:.25 G4:.25 G4:.5 A4:1/E4 B4:.5 B4:1/E4 D5:1/E4 E5:1/A3 D5:1/A3 E5:2/A3 F#5:.5 D5:.5 E5:.5 C#5:.5 D5:.5 B4:.5 D5:.5 "
                        "F#5:.5 G5:.5 B5:.25 D6:.25 E6:.5 C#6:.25 E6:.25 D6:.5 C#6:.25 B5:.25 A5:.5 B5:.25 C#6:.25 B5:.5 A5:1/E4 F#5:.5 E5:1/E4 "
                        "C#5:1/E4 C#5:1/F#4 A4:1/F#4 D5:2/B3"},
  /* 9 MAZE */ {96,
                        "F5:1/F4 G5:1/F4 F5:1/F4 R:1 E5:1.5/A3 F5:.5 E5:1.5/A3 G5:.5 F5:2/A#3 F5:1/A#3 R:1 E5:1/C4 F5:1/C4 E5:2/C4 F5:1/F4 "
                        "E5:1/F4 F5:1/F4 D5:1/F4 E5:1.5/A3 E5:.5 E5:1.5/A3 F5:.5 F5:2/A#3 F5:1/A#3 A5:1/A#3 G5:1/C4 A5:1/C4 A5:2/C4 D6:2/D4 "
                        "D6:1/D4 D6:1/D4 D6:1/A#3 A#5:1/A#3 A#5:1/A#3 D6:1/A#3 D6:2/G3 D6:1/G3 C6:1/G3 C6:1/C4 A#5:1/C4 A#5:2/C4 A5:2/D4 "
                        "A5:1/D4 A#5:1/D4 A#5:1/A#3 C6:1/A#3 A#5:1/A#3 C6:1/A#3 C6:2/C4 C6:1/C4 D6:1/C4 C6:1/F4 A#5:1/F4 C6:2/F4 F5:1/F4 "
                        "G5:1/F4 F5:1/F4 R:1 E5:1.5/A3 F5:.5 E5:1.5/A3 G5:.5 F5:2/A#3 F5:1/A#3 R:1 E5:1/C4 F5:1/C4 E5:2/C4 F5:1/F4 E5:1/F4 "
                        "F5:1/F4 D5:1/F4 E5:1.5/A3 E5:.5 E5:1.5/A3 F5:.5 F5:2/A#3 F5:1/A#3 A5:1/A#3 G5:1/C4 A5:1/C4 C6:2/F4"},
  /* 10 BREAKOUT */ {140,
                        "E5:.5 C5:1/C4 B4:.5 C5:1/C4 B4:1/C4 C5:.5 A4:.5 B4:.5 A4:.5 A4:.5 G4:.5 A4:.5 B4:.5 B4:1/G3 G4:.5 G4:.5 G4:1/G3 "
                        "A4:1/G3 C5:1/C4 D5:1/C4 E5:2/C4 E5:.5 F5:1/C4 D5:.5 E5:1/C4 F5:1/C4 F5:.5 G5:.5 A5:.5 B5:.5 A5:.5 B5:.5 C6:.5 B5:.5 "
                        "B5:1/G3 G5:.5 E5:.5 D5:1/G3 E5:1/G3 E5:1/C4 F5:1/C4 G5:2/C4 A5:1/A3 G5:.5 E5:.5 E5:1/A3 D5:1/A3 D5:.5 B4:1/G3 A4:.5 "
                        "B4:1/G3 G4:1/G3 A4:1/F4 B4:.5 C5:.5 C5:1/F4 D5:1/F4 D5:1/G3 C5:1/G3 D5:2/G3 E5:1/A3 G5:.5 A5:.5 A5:1/A3 C6:1/A3 C6:.5 "
                        "D6:1/F4 E6:.5 C6:1/F4 D6:1/F4 D6:1/G3 B5:.5 A5:.5 G5:1/G3 E5:1/G3 E5:1/C4 F5:1/C4 G5:2/C4 E5:.5 C5:1/C4 B4:.5 C5:1/C4 "
                        "B4:1/C4 C5:.5 A4:.5 B4:.5 A4:.5 A4:.5 G4:.5 A4:.5 B4:.5 B4:1/G3 G4:.5 G4:.5 G4:1/G3 A4:1/G3 C5:1/C4 D5:1/C4 E5:2/C4 "
                        "E5:.5 F5:1/C4 D5:.5 E5:1/C4 F5:1/C4 F5:.5 G5:.5 A5:.5 B5:.5 A5:.5 B5:.5 C6:.5 B5:.5 B5:1/G3 G5:.5 E5:.5 D5:1/G3 "
                        "E5:1/G3 E5:1/C4 F5:1/C4 G5:2/C4"},
  /* 11 INVADERS */ {112,
                        "C5:1/A3 D5:.5 F5:.5 E5:1/A3 F5:1/A3 E5:1/A3 F5:1/A3 E5:1/A3 F5:1/A3 F5:1/F4 G5:.5 A5:.5 A5:.5 B5:.5 G5:1/F4 G5:1/G3 "
                        "F5:1/G3 F5:2/G3 E5:1/A3 F5:.5 G5:.5 E5:1/A3 F5:1/A3 F5:1/D4 G5:1/D4 F5:1/D4 G5:1/D4 G5:1/E4 F5:.5 G5:.5 G5:.5 G5:.5 "
                        "F5:1/E4 E5:1/A3 E5:1/A3 E5:2/A3 F5:1/F4 F5:.5 D5:.5 C5:.5 A4:.5 G4:1/F4 G4:1/G3 F4:.5 E4:.5 G4:1/G3 B4:1/G3 C5:1/A3 "
                        "D5:.5 D5:.5 C5:.5 D5:.5 E5:1/A3 D5:1/D4 C5:1/D4 C5:2/D4 C5:1/F4 D5:.5 C5:.5 C5:.5 B4:.5 C5:1/F4 D5:1/G3 B4:.5 B4:.5 "
                        "B4:1/G3 A4:1/G3 B4:1/E4 D5:.5 F5:.5 E5:.5 F5:.5 F5:1/E4 E5:1/A3 C5:1/A3 B4:2/A3 C5:1/A3 D5:.5 F5:.5 E5:1/A3 F5:1/A3 "
                        "E5:1/A3 F5:1/A3 E5:1/A3 F5:1/A3 F5:1/F4 G5:.5 A5:.5 A5:.5 B5:.5 G5:1/F4 G5:1/G3 F5:1/G3 F5:2/G3 E5:1/A3 F5:.5 G5:.5 "
                        "E5:1/A3 F5:1/A3 F5:1/D4 G5:1/D4 F5:1/D4 G5:1/D4 G5:1/E4 F5:.5 G5:.5 G5:.5 G5:.5 F5:1/E4 E5:1/A3 E5:1/A3 C5:2/A3"},
  /* 12 SLALOM */ {160,
                        "B5:.5 A5:.5 G5:.5 B5:.5 B5:.5 G5:.5 F#5:.5 G5:.5 A5:.5 B5:1/D4 C6:.5 D6:1/D4 E6:1/D4 D6:1/G3 E6:.5 F#6:.5 G6:.5 F#6:.5 "
                        "G6:1/G3 F#6:1/D4 A6:1/D4 A6:2/D4 G6:.5 A6:.5 G6:.5 F#6:.5 E6:.5 C6:.5 A5:.5 B5:.5 C6:.5 D6:1/C4 F#6:.5 E6:1/C4 "
                        "F#6:1/C4 F#6:1/D4 G6:.5 A6:.5 A6:.5 G6:.5 A6:1/D4 G6:1/G3 F#6:1/G3 D6:2/G3 E6:1/C4 F#6:.5 E6:.5 E6:.5 D6:.5 B5:1/C4 "
                        "B5:.5 G5:.5 E5:.5 F#5:.5 G5:.5 E5:.5 F#5:.5 E5:.5 F#5:1/D4 A5:.5 B5:.5 A5:.5 C6:.5 B5:1/D4 B5:1/E4 A5:1/E4 B5:2/E4 "
                        "C6:1/C4 D6:.5 E6:.5 E6:.5 G6:.5 F#6:1/C4 D6:.5 E6:.5 G6:.5 A6:.5 G6:.5 A6:.5 A6:.5 G6:.5 F#6:1/D4 E6:.5 C6:.5 D6:.5 "
                        "B5:.5 C6:1/D4 B5:1/G3 C6:1/G3 D6:2/G3 B5:.5 A5:.5 G5:.5 B5:.5 B5:.5 G5:.5 F#5:.5 G5:.5 A5:.5 B5:1/D4 C6:.5 D6:1/D4 "
                        "E6:1/D4 D6:1/G3 E6:.5 F#6:.5 G6:.5 F#6:.5 G6:1/G3 F#6:1/D4 A6:1/D4 A6:2/D4 G6:.5 A6:.5 G6:.5 F#6:.5 E6:.5 C6:.5 A5:.5 "
                        "B5:.5 C6:.5 D6:1/C4 F#6:.5 E6:1/C4 F#6:1/C4 F#6:1/D4 G6:.5 A6:.5 A6:.5 G6:.5 A6:1/D4 G6:1/G3 F#6:1/G3 D6:2/G3"},
  /* 13 RACE */ {172,
                        "E5:.5 F#5:.25 G5:.25 B5:.5 C6:.25 D6:.25 B5:.5 C6:.25 A5:.25 C6:.5 D6:.25 C6:.25 A5:.5 B5:.5 A5:.5 F#5:.5 F#5:.5 "
                        "F#5:.5 G5:.5 A5:.5 G5:1/C4 A5:.5 C6:.5 C6:.5 D6:.5 B5:1/C4 A5:.5 F#5:.5 E5:1/D4 F#5:2/D4 E5:.5 D5:.25 C5:.25 E5:.5 "
                        "G5:.25 F#5:.25 E5:.5 D5:.25 C5:.25 C5:.5 A4:.25 B4:.25 A4:.5 F#4:.5 A4:.5 F#4:.5 F#4:.5 A4:.5 B4:.5 C5:.5 C5:1/C4 "
                        "A4:.5 G4:.5 G4:.5 B4:.5 C5:1/C4 D5:.5 B4:.5 C5:1/B3 B4:2/B3 E5:1/A3 D5:.5 C5:.5 C5:.5 A4:.5 G4:1/A3 A4:.5 F#4:.25 "
                        "G4:.25 A4:.5 C5:.25 D5:.25 D5:.5 F#5:.25 E5:.25 F#5:.5 G5:.25 A5:.25 G5:1/E4 B5:.5 C6:.5 B5:.5 C6:.5 D6:1/E4 D6:.5 "
                        "C6:.5 B5:1/B3 A5:2/B3 A5:1/A3 C6:.5 B5:.5 A5:.5 A5:.5 F#5:1/A3 F#5:.5 G5:.25 F#5:.25 E5:.5 F#5:.25 F#5:.25 F#5:.5 "
                        "E5:.25 F#5:.25 E5:.5 D5:.25 C5:.25 D5:1/B3 D5:.5 E5:.5 D5:.5 B4:.5 G4:1/B3 G4:.5 F#4:.5 G4:1/E4 B4:2/E4 E5:.5 F#5:.25 "
                        "G5:.25 B5:.5 C6:.25 D6:.25 B5:.5 C6:.25 A5:.25 C6:.5 D6:.25 C6:.25 A5:.5 B5:.5 A5:.5 F#5:.5 F#5:.5 F#5:.5 G5:.5 A5:.5 "
                        "G5:1/C4 A5:.5 C6:.5 C6:.5 D6:.5 B5:1/C4 A5:.5 F#5:.5 E5:1/D4 F#5:2/D4 E5:.5 D5:.25 C5:.25 E5:.5 G5:.25 F#5:.25 E5:.5 "
                        "D5:.25 C5:.25 C5:.5 A4:.25 B4:.25 A4:.5 F#4:.5 A4:.5 F#4:.5 F#4:.5 A4:.5 B4:.5 C5:.5 C5:1/C4 A4:.5 G4:.5 G4:.5 B4:.5 "
                        "C5:1/C4 D5:.5 B4:.5 C5:1/B3 B4:2/E4"},
  /* 14 FLIGHT */ {104,
                        "A5:2/D4 A5:1/D4 B5:1/D4 B5:1.5/E4 C#6:.5 B5:1.5/E4 D6:.5 C#6:1/A3 E6:1/A3 E6:1/A3 C#6:1/A3 D6:1/D4 F#6:1/D4 F#6:2/D4 "
                        "F#6:2/D4 F#6:1/D4 D6:1/D4 B5:1.5/E4 G#5:.5 G#5:1.5/E4 A5:.5 A5:1/A3 B5:1/A3 A5:1/A3 C#6:1/A3 D6:1/D4 E6:1/D4 D6:2/D4 "
                        "F#6:1/B3 F#6:1/B3 F#6:1/B3 G#6:1/B3 G#6:2/G#3 G#6:1/G#3 F#6:1/G#3 E6:1/E4 G#6:1/E4 G#6:1/E4 G#6:1/E4 E6:1/A3 C#6:1/A3 "
                        "C#6:2/A3 B5:1/B3 B5:1/B3 B5:1/B3 C#6:1/B3 B5:2/G#3 B5:1/G#3 D6:1/G#3 B5:1/E4 D6:1/E4 B5:1/E4 C#6:1/E4 A5:1/D4 A5:1/D4 "
                        "A5:2/D4 A5:2/D4 A5:1/D4 B5:1/D4 B5:1.5/E4 C#6:.5 B5:1.5/E4 D6:.5 C#6:1/A3 E6:1/A3 E6:1/A3 C#6:1/A3 D6:1/D4 F#6:1/D4 "
                        "F#6:2/D4 F#6:2/D4 F#6:1/D4 D6:1/D4 B5:1.5/E4 G#5:.5 G#5:1.5/E4 A5:.5 A5:1/A3 B5:1/A3 A5:1/A3 C#6:1/A3 D6:1/D4 E6:1/D4 "
                        "A5:2/D4"},
  /* 15 EMBERWOOD village + foret (original) */ {108,
                        "E5:1/C4 G5:.5 E5:.5 D5:1/G3 C5:1/G3 D5:.5 E5:.5 G5:1/A3 A5:1/A3 G5:2/E3 E5:1/F3 F5:.5 E5:.5 D5:1/F3 C5:1/F3 "
                        "D5:.5 E5:.5 C5:1/G3 B4:1/G3 C5:2/C4 E5:1/C4 G5:.5 E5:.5 D5:1/G3 C5:1/G3 D5:.5 E5:.5 G5:1/A3 C6:1/A3 B5:2/E3 "
                        "A5:1/F3 G5:.5 F5:.5 E5:1/G3 D5:1/G3 E5:.5 D5:.5 C5:3/C4 R:1 A5:1/F3 G5:.5 F5:.5 E5:1/C4 C5:1/C4 F5:1/D3 E5:.5 "
                        "D5:.5 C5:2/A3 G5:1/E3 F5:.5 E5:.5 D5:1/G3 B4:1/G3 C5:1/A3 D5:1/A3 E5:2/G3 A5:1/F3 B5:.5 C6:.5 G5:1/C4 E5:1/C4 "
                        "F5:1/D3 G5:.5 A5:.5 G5:2/E3 F5:1/D3 E5:.5 D5:.5 E5:1/C4 C5:1/C4 D5:2/G3 R:2 E5:1/C4 G5:.5 E5:.5 D5:1/G3 "
                        "C5:1/G3 D5:.5 E5:.5 G5:1/A3 A5:1/A3 G5:2/E3 E5:1/F3 F5:.5 E5:.5 D5:1/F3 C5:1/F3 D5:.5 E5:.5 C5:1/G3 B4:1/G3 "
                        "C5:2/C4 E5:1/C4 G5:.5 E5:.5 D5:1/G3 C5:1/G3 D5:.5 E5:.5 G5:1/A3 C6:1/A3 B5:2/E3 A5:1/F3 G5:.5 F5:.5 E5:1/G3 "
                        "D5:1/G3 E5:.5 D5:.5 C5:3/C4 R:1"},
  /* 16 EMBERWOOD crypte (original) */ {80,
                        "A4:2/A3 C5:1/A3 B4:1/A3 A4:2/F3 E4:2/F3 F4:1/D3 E4:1/D3 D4:1/E3 C4:1/E3 B3:3/E3 R:1 A4:2/A3 C5:1/A3 E5:1/A3 "
                        "D5:2/D3 C5:1/D3 B4:1/D3 C5:1/A3 B4:1/A3 G#4:1/E3 B4:1/E3 A4:3/A3 R:1 E5:2/C4 D5:1/C4 C5:1/C4 D5:2/G3 B4:2/G3 "
                        "C5:1/F3 B4:1/F3 A4:1/D3 F4:1/D3 E4:3/E3 R:1 A4:2/A3 C5:1/A3 B4:1/A3 A4:2/F3 E4:2/F3 F4:1/D3 E4:1/E3 G#4:1/E3 "
                        "B4:1/E3 A4:3/A3 R:1"},
  /* 17 EMBERWOOD boss (original) */ {152,
                        "D5:.5 D5:.5 A4:.5 D5:.5 F5:1/D3 E5:.5 D5:.5 C5:.5 C5:.5 G4:.5 C5:.5 E5:1/C3 D5:.5 C5:.5 A#4:.5 A#4:.5 F4:.5 "
                        "A#4:.5 D5:1/A#2 C5:.5 A#4:.5 A4:.5 C#5:.5 E5:.5 G5:.5 A5:2/A2 D5:.5 D5:.5 A4:.5 D5:.5 F5:1/D3 G5:.5 A5:.5 "
                        "A#5:.5 A5:.5 G5:.5 F5:.5 E5:1/C3 G5:1/C3 F5:.5 E5:.5 D5:.5 C5:.5 A#4:1/A#2 D5:1/A#2 A4:.5 C#5:.5 E5:.5 C#5:.5 "
                        "D5:2/D3 F5:1/F3 F5:.5 G5:.5 A5:1/F3 F5:1/F3 G5:1/C3 E5:.5 F5:.5 G5:1/C3 C6:1/C3 A#5:1/G2 A5:.5 G5:.5 F5:1/A#2 "
                        "D5:1/A#2 E5:1/A2 C#5:1/A2 A4:2/A2 D5:.5 D5:.5 A4:.5 D5:.5 F5:1/D3 E5:.5 D5:.5 C5:.5 C5:.5 G4:.5 C5:.5 E5:1/C3 "
                        "D5:.5 C5:.5 A#4:.5 A#4:.5 F4:.5 A#4:.5 D5:1/A#2 C5:.5 A#4:.5 A4:.5 C#5:.5 E5:.5 G5:.5 A5:2/A2 D5:.5 D5:.5 "
                        "A4:.5 D5:.5 F5:1/D3 G5:.5 A5:.5 A#5:.5 A5:.5 G5:.5 F5:.5 E5:1/C3 G5:1/C3 F5:.5 E5:.5 D5:.5 C5:.5 A#4:1/A#2 "
                        "D5:1/A#2 A4:.5 C#5:.5 E5:.5 C#5:.5 D5:2/D3"},
};
static const int NL = sizeof(LOOPS) / sizeof(LOOPS[0]);
struct Note { float freq, bass; int start, dur; };
static Note notes[320]; static int nNotes = 0, totalMs = 1;
static int cur = -1, curId = -1;
static uint32_t t0 = 0, sfxUntil = 0;
static const int CH = 6;

static void load(int id) {
  const char* p = LOOPS[id].seq; float beat = 60000.f / LOOPS[id].bpm; int ms = 0; nNotes = 0;
  static const int base[7] = {9, 11, 0, 2, 4, 5, 7};
  while (*p && nNotes < 320) {
    while (*p == ' ') p++;
    if (!*p) break;
    int midi = -1;
    if (*p == 'R') p++; else {
      int n = base[*p++ - 'A']; if (*p == '#') { n++; p++; }
      midi = 12 * ((*p++ - '0') + 1) + n;
    }
    p++;
    float d = atof(p); while (*p && *p != ' ' && *p != '/') p++;
    [[maybe_unused]] int bmidi = -1;
    if (*p == '/') {   // note de basse
      p++; int n = base[*p++ - 'A']; if (*p == '#') { n++; p++; }
      bmidi = 12 * ((*p++ - '0') + 1) + n;
      while (*p && *p != ' ') p++;
    }
    int dur = (int)(d * beat);
    if (midi >= 0) {   // le buzzer piezo est tres faible sous ~450 Hz : on remonte d'une octave
      float f = 440.f * powf(2.f, (midi - 69) / 12.f); while (f < 440.f) f *= 2.f;
      notes[nNotes++] = {f, 0.f, ms, dur};   // basse volontairement ignoree (inaudible sur le buzzer)
    }
    ms += dur;
  }
  totalMs = ms > 0 ? ms : 1;
}

void stop() { if (curId >= 0) M5.Speaker.stop(CH); curId = -1; cur = -1; }
void onSfx(int ms) { sfxUntil = millis() + ms + 20; if (curId >= 0) { M5.Speaker.stop(CH); cur = -1; } }

void run(int id, bool on) {
  if (!on || !g_sound || !g_music || id < 0 || id >= NL) { if (curId >= 0) stop(); return; }
  if (curId != id) { load(id); curId = id; t0 = millis(); cur = -1; M5.Speaker.setChannelVolume(CH, 200); }
  uint32_t now = millis();
  if (now < sfxUntil) return;
  int t = (int)((now - t0) % totalMs), i = -1;
  for (int k = 0; k < nNotes; k++) if (t >= notes[k].start && t < notes[k].start + notes[k].dur) { i = k; break; }
  if (i < 0) { cur = -1; return; }
  // note longue avec basse : melodie sur ~55 % de la duree, puis basse sur la fin
  int off = t - notes[i].start, sub = (notes[i].bass > 0 && notes[i].dur >= 300 && off * 100 >= notes[i].dur * 55) ? 1 : 0;
  if (i * 2 + sub != cur) {
    cur = i * 2 + sub;
    int end = notes[i].start + notes[i].dur, rem = end - t;
    if (sub) { int d = rem * 9 / 10; if (d < 25) d = 25; M5.Speaker.tone(notes[i].bass, d, CH, true); }
    else {
      int cut = (notes[i].bass > 0 && notes[i].dur >= 300) ? notes[i].start + notes[i].dur * 55 / 100 - t : rem;
      int d = cut * 9 / 10; if (d < 25) d = 25;
      M5.Speaker.tone(notes[i].freq, d, CH, true);
    }
  }
}

}  // namespace bgm
