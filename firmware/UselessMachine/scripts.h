#pragma once
// =====================================================================
//  scripts.h – die Aktionen der Useless Machine
//
//  Jede Aktion besteht aus drei kurzen Skripten:
//    Anfahrt  – bis vor den Schalter (legt ihn noch nicht um)
//    Klick    – Schalter umlegen (PUSH, mit Erfolgskontrolle)
//    Zurück   – in die Box (höchstens RETURN_MAX_MS)
//  Normal wird zufällig eine der ACTIONS gewählt, nie zweimal dieselbe
//  hintereinander. Wer schnell hintereinander schaltet, bekommt SAUER.
// =====================================================================
#include "bytecode.h"

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// ---------------------------------------------------------------------
//  STIMMUNGEN                  tempo patience nerves sloppy
// ---------------------------------------------------------------------
enum MoodId : uint8_t { mNormal, mAngry, MOOD_COUNT };
const Mood MOODS[] = {
  {"Normal", 100, 100, 100, 1},
  {"Sauer",  160,  40, 180, 1},
};

// ---------------------------------------------------------------------
//  ANFAHRT
// ---------------------------------------------------------------------
// langsam anschleichen, kurz innehalten
const uint8_t A_SNEAK[]   = { MOVE(P_NEAR, S_SLOW, E_SMOOTH), WAIT(200, 400), END };
// gar nicht erst anfahren
const uint8_t A_NONE[]    = { END };
// vorsichtig heran, 2- bis 3-mal vor und zurück
const uint8_t A_CAREFUL[] = { MOVE(P_NEAR, S_MED, E_SMOOTH),
                              LOOP(2, 3), REL(-12, S_MED, E_SMOOTH), WAIT(60, 160),
                                          REL(12, S_MED, E_SMOOTH), WAIT(60, 160), NEXT,
                              END };

// ---------------------------------------------------------------------
//  KLICK
// ---------------------------------------------------------------------
const uint8_t K_ZACK[]    = { PUSH(S_MAX, E_LIN), END };

// ---------------------------------------------------------------------
//  ZURÜCK
// ---------------------------------------------------------------------
const uint8_t Z_NORMAL[]  = { HOME(S_FAST, E_OUT), END };
// drohend schütteln, dann mit Vollgas zurück
const uint8_t Z_ANGRY[]   = { WIGGLE(6, 2, S_MAX), HOME(S_MAX, E_LIN), END };

// Mensch hat den Schalter selbst ausgemacht, bevor der Arm dran war
const uint8_t G_UNDO[]    = { HOME(S_MED, E_SMOOTH), END };

// ---------------------------------------------------------------------
//  AKTIONEN
// ---------------------------------------------------------------------
struct Action {
  const char* name;
  uint8_t mood;
  const uint8_t* approach;
  const uint8_t* klick;
  const uint8_t* back;
};

const Action ACTIONS[] = {
  {"Anschleichen und plopp", mNormal, A_SNEAK,   K_ZACK, Z_NORMAL},
  {"Zack",                   mNormal, A_NONE,    K_ZACK, Z_NORMAL},
  {"Vorsichtig und zack",    mNormal, A_CAREFUL, K_ZACK, Z_NORMAL},
};
const uint8_t ACTION_COUNT = COUNT_OF(ACTIONS);

// Schnell hintereinander geschaltet
const Action ANGRY_ACTION = {"Sauer!", mAngry, A_NONE, K_ZACK, Z_ANGRY};

static_assert(COUNT_OF(MOODS) == MOOD_COUNT, "MOODS passt nicht zu MoodId");
