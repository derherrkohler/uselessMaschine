#pragma once
// =====================================================================
//  bytecode.h – die kleine Bewegungs-Skriptsprache
//
//  Ein Skript ist ein const uint8_t[] (liegt automatisch im Flash).
//  Jeder Befehl = 1 Byte Opcode + 0..4 Byte Argumente.
//
//  Positionen : 0..100 %  (0 = Ruhe, 90 = Hebel berührt, 100 = geschaltet)
//               Gesten werden automatisch auf P_GESTURE_MAX begrenzt,
//               nur PUSH darf bis 100 fahren.
//  Tempo      : % pro Sekunde (Spitzengeschwindigkeit), 255 = Vollgas
//  Zeiten     : WAIT in 20-ms-Schritten (max 5100 ms)
//               NAP  in 100-ms-Schritten (max 25500 ms)
//  Alle Tempi, Zeiten, Amplituden werden zur Laufzeit durch die
//  "Stimmung" (Mood) skaliert und leicht zufällig variiert.
// =====================================================================
#include <Arduino.h>

enum : uint8_t {
  OP_END = 0,  //                                   Skriptende
  OP_MOVE,     // pos, speed, ease                  absolut fahren
  OP_MOVER,    // posMin, posMax, speed, ease       zufälliges Ziel
  OP_REL,      // delta(int8), speed, ease          relativ fahren
  OP_WAIT,     // min, max  (×20 ms)                warten
  OP_NAP,      // min, max  (×100 ms)               lange warten
  OP_WIGGLE,   // amp, count, speed                 um aktuelle Pos. wackeln
  OP_JITTER,   // amp, dauer (×20 ms)               zufälliges Zittern
  OP_LOOP,     // min, max                          Block zufällig oft wiederholen
  OP_NEXT,     //                                   Blockende
  OP_CHANCE,   // prozent                           nächsten Befehl nur mit p % ausführen
  OP_PUSH,     // speed, ease                       Schalter umlegen (mit Kontrolle)
  OP_HOME,     // speed, ease                       zurück auf 0
  OP__COUNT
};

// Länge jedes Befehls inkl. Opcode (Reihenfolge wie enum!)
static const uint8_t OP_LEN[OP__COUNT] = { 1, 4, 5, 4, 3, 3, 4, 3, 3, 1, 2, 3, 3 };

// ---- Positionen -----------------------------------------------------
#define P_HOME         0
#define P_LID         30   // Arm berührt Deckel von innen (darunter: unsichtbar)
#define P_PEEK        45   // Deckel einen Spalt offen
#define P_HALF        50
#define P_NEAR        76
#define P_CLOSE       85
#define P_TOUCH       90
#define P_GESTURE_MAX 94

// ---- Geschwindigkeiten (%/s) -----------------------------------------
#define S_CRAWL   10
#define S_SLOW    25
#define S_EASY    45
#define S_MED     80
#define S_FAST   160
#define S_MAX    255

// ---- Beschleunigungsprofile ------------------------------------------
#define E_LIN     0   // gleichmäßig
#define E_SMOOTH  1   // sanft an, sanft ab
#define E_OUT     2   // schnell los, deutlich abbremsen
#define E_IN      3   // langsam los, beschleunigen

// ---- Makros zum Schreiben von Skripten --------------------------------
// (Keine Casts bei Zeiten: der Compiler meldet Fehler, wenn ein Wert nicht in 1 Byte passt.)
#define END              OP_END
#define MOVE(p, s, e)    OP_MOVE, (p), (s), (e)
#define MOVER(a, b, s, e) OP_MOVER, (a), (b), (s), (e)
#define REL(d, s, e)     OP_REL, (uint8_t)(int8_t)(d), (s), (e)
#define WAIT(a, b)       OP_WAIT, ((a) / 20), ((b) / 20)
#define NAP(a, b)        OP_NAP, ((a) / 100), ((b) / 100)
#define WIGGLE(amp, n, s) OP_WIGGLE, (amp), (n), (s)
#define JITTER(amp, ms)  OP_JITTER, (amp), ((ms) / 20)
#define LOOP(a, b)       OP_LOOP, (a), (b)
#define NEXT             OP_NEXT
#define CHANCE(pct)      OP_CHANCE, (pct)
#define PUSH(s, e)       OP_PUSH, (s), (e)
#define HOME(s, e)       OP_HOME, (s), (e)

#define RND 0xFF   // in Persönlichkeits-Rezepten: "zufällig wählen"

// ---- Stimmung --------------------------------------------------------
struct Mood {
  const char* name;
  uint8_t tempo;     // % – Geschwindigkeitsfaktor
  uint8_t patience;  // % – Faktor für Pausen
  uint8_t nerves;    // % – Faktor für Wackel-/Zitteramplitude
  uint8_t sloppy;    // ± Positionsstreuung in %-Punkten
};
