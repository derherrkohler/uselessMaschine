#pragma once
// =====================================================================
//  scripts.h – Gesten-Bibliothek, Stimmungen und 50 Persönlichkeiten
//
//  Eine Vorstellung besteht aus 5 Phasen:
//    R  Reaktion   – wie lange/wie reagiert die Maschine aufs Einschalten
//    A  Anfahrt    – Weg von der Box bis kurz vor den Schalter
//    N  Nähe       – das Theater direkt vor dem Schalter
//    K  Klick      – wie der Schalter umgelegt wird
//    Z  Zurück     – Rückzug in die Box
//  Kombinationen: 9 × 13 × 15 × 8 × 11 = 154.440, × 10 Stimmungen,
//  plus Zufallsvariation bei jedem Tempo, jeder Pause, jeder Position.
// =====================================================================
#include "bytecode.h"

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// ---------------------------------------------------------------------
//  STIMMUNGEN                  tempo patience nerves sloppy
// ---------------------------------------------------------------------
enum MoodId : uint8_t { mNormal, mTired, mHectic, mAnxious, mAnnoyed, mBored, mCareful, mPlayful, mProud, mAngry, MOOD_COUNT };
const Mood MOODS[] = {
  {"Normal",      100, 100, 100, 2},
  {"Muede",        55, 160,  60, 3},
  {"Hektisch",    170,  40, 170, 3},
  {"Aengstlich",   80, 140, 190, 2},
  {"Genervt",     140,  60, 120, 2},
  {"Gelangweilt",  70, 180,  50, 4},
  {"Vorsichtig",   60, 130,  90, 1},
  {"Verspielt",   120,  90, 130, 4},
  {"Stolz",       100,  80,  70, 1},
  {"Wuetend",     190,  30, 200, 2},
};

// ---------------------------------------------------------------------
//  R – REAKTION
// ---------------------------------------------------------------------
const uint8_t R_INSTANT[] = { WAIT(0, 60), END };
const uint8_t R_SHORT[]   = { WAIT(200, 700), END };
const uint8_t R_LONG[]    = { NAP(1500, 4000), END };
// Hinweis: Unterhalb von P_LID ist der Arm unsichtbar. Gesten "in der Box"
// arbeiten deshalb mit dem Deckel (anheben, klappern, zuknallen).
const uint8_t R_IGNORE[]  = { NAP(4000, 9000), CHANCE(50), MOVE(P_LID + 4, S_SLOW, E_SMOOTH), NAP(500, 2000), END };
const uint8_t R_WAKEUP[]  = { MOVE(P_LID + 5, S_SLOW, E_SMOOTH), JITTER(2, 400), HOME(S_MED, E_SMOOTH), NAP(800, 2000), END };
const uint8_t R_STARTLE[] = { MOVE(P_LID + 12, S_MAX, E_LIN), WAIT(80, 160), HOME(S_FAST, E_OUT), WAIT(500, 1500), END };
const uint8_t R_DOUBLE[]  = { LOOP(2, 2), MOVE(P_LID + 6, S_MAX, E_LIN), WAIT(40, 80), MOVE(P_LID - 6, S_MAX, E_LIN),
                              WAIT(400, 900), NEXT, END };
const uint8_t R_PEEK[]    = { MOVE(P_PEEK, S_SLOW, E_SMOOTH), NAP(1000, 2500), CHANCE(40), HOME(S_FAST, E_OUT),
                              WAIT(300, 1200), END };
const uint8_t R_RATTLE[]  = { LOOP(3, 6), MOVE(P_LID + 8, S_MAX, E_LIN), WAIT(30, 60), MOVE(P_LID - 6, S_MAX, E_LIN),
                              WAIT(60, 160), NEXT, WAIT(300, 800), END };

enum ReactId : uint8_t { rInstant, rShort, rLong, rIgnore, rWakeup, rStartle, rDouble, rPeek, rRattle, R_COUNT };
const uint8_t* const REACT[] = { R_INSTANT, R_SHORT, R_LONG, R_IGNORE, R_WAKEUP, R_STARTLE, R_DOUBLE, R_PEEK, R_RATTLE };

// ---------------------------------------------------------------------
//  A – ANFAHRT
// ---------------------------------------------------------------------
const uint8_t A_DIRECT[]    = { MOVE(P_CLOSE, S_FAST, E_OUT), END };
const uint8_t A_STROLL[]    = { MOVE(P_NEAR, S_EASY, E_SMOOTH), END };
const uint8_t A_SNEAK[]     = { MOVE(P_NEAR, S_CRAWL, E_LIN), END };
const uint8_t A_ROCKET[]    = { MOVE(60, S_MAX, E_LIN), MOVE(P_CLOSE, S_SLOW, E_OUT), END };
const uint8_t A_RUNUPS[]    = { LOOP(2, 3), MOVER(35, 60, S_FAST, E_OUT), WAIT(150, 500), REL(-20, S_MED, E_SMOOTH),
                                WAIT(100, 400), NEXT, MOVE(P_NEAR, S_MED, E_SMOOTH), END };
const uint8_t A_STOPGO[]    = { LOOP(3, 5), REL(15, S_MED, E_SMOOTH), WAIT(150, 700), NEXT, MOVE(P_NEAR, S_EASY, E_SMOOTH), END };
const uint8_t A_TIPTOE[]    = { LOOP(4, 5), REL(14, S_SLOW, E_SMOOTH), WAIT(300, 900), REL(-4, S_SLOW, E_SMOOTH), NEXT,
                                MOVE(P_NEAR, S_SLOW, E_SMOOTH), END };
const uint8_t A_BACKFORTH[] = { MOVE(70, S_FAST, E_OUT), WAIT(200, 500), MOVE(25, S_MED, E_SMOOTH), WAIT(400, 1200),
                                MOVE(P_CLOSE, S_FAST, E_OUT), END };
const uint8_t A_SHIVER[]    = { LOOP(4, 4), REL(19, S_MED, E_SMOOTH), JITTER(2, 240), NEXT, END };
const uint8_t A_PONDER[]    = { MOVE(P_HALF, S_EASY, E_SMOOTH), NAP(1000, 2500), CHANCE(50), WIGGLE(2, 1, S_MED),
                                MOVE(P_NEAR, S_EASY, E_SMOOTH), END };
const uint8_t A_SPEEDUP[]   = { MOVE(P_CLOSE, S_MED, E_IN), END };
const uint8_t A_STUTTER[]   = { LOOP(6, 7), REL(12, S_MAX, E_LIN), WAIT(20, 100), NEXT, END };
const uint8_t A_WOBBLY[]    = { MOVE(P_HALF, S_MED, E_SMOOTH), WIGGLE(5, 3, S_FAST), MOVE(P_NEAR, S_MED, E_SMOOTH), END };

enum ApproachId : uint8_t { aDirect, aStroll, aSneak, aRocket, aRunups, aStopGo, aTiptoe, aBackForth, aShiver, aPonder,
                            aSpeedUp, aStutter, aWobbly, A_COUNT };
const uint8_t* const APPROACH[] = { A_DIRECT, A_STROLL, A_SNEAK, A_ROCKET, A_RUNUPS, A_STOPGO, A_TIPTOE, A_BACKFORTH,
                                    A_SHIVER, A_PONDER, A_SPEEDUP, A_STUTTER, A_WOBBLY };

// ---------------------------------------------------------------------
//  N – NÄHE (vor dem Schalter)
// ---------------------------------------------------------------------
const uint8_t N_NONE[]      = { WAIT(0, 100), END };
const uint8_t N_HESITATE[]  = { WAIT(600, 1500), END };
const uint8_t N_THINK[]     = { NAP(2000, 5000), END };
const uint8_t N_TREMBLE[]   = { JITTER(3, 1200), END };
const uint8_t N_WIGGLE[]    = { WIGGLE(4, 4, S_FAST), END };
const uint8_t N_RECOIL[]    = { MOVE(P_HALF, S_FAST, E_OUT), WAIT(500, 1500), MOVE(P_CLOSE, S_SLOW, E_SMOOTH), END };
const uint8_t N_TOUCHBACK[] = { MOVE(P_TOUCH, S_SLOW, E_SMOOTH), WAIT(100, 300), MOVE(70, S_MAX, E_LIN), WAIT(400, 1000),
                                MOVE(P_CLOSE, S_EASY, E_SMOOTH), END };
const uint8_t N_SCARED[]    = { MOVE(30, S_MAX, E_LIN), JITTER(2, 400), NAP(1000, 3000), MOVE(P_NEAR, S_SLOW, E_SMOOTH), END };
const uint8_t N_INCH[]      = { LOOP(3, 5), REL(2, S_SLOW, E_LIN), WAIT(300, 800), NEXT, END };
const uint8_t N_TICS[]      = { LOOP(4, 7), REL(-3, S_MAX, E_LIN), WAIT(40, 100), REL(3, S_MAX, E_LIN), WAIT(100, 400), NEXT, END };
const uint8_t N_MUSING[]    = { WAIT(600, 1000), WIGGLE(2, 2, S_MED), NAP(1000, 2500), END };
const uint8_t N_RETRIES[]   = { LOOP(2, 4), MOVE(60, S_FAST, E_OUT), WAIT(100, 300), MOVE(P_CLOSE, S_FAST, E_OUT),
                                WAIT(200, 600), NEXT, END };
const uint8_t N_PANIC[]     = { JITTER(5, 500), WIGGLE(6, 3, S_MAX), END };
const uint8_t N_SIGH[]      = { MOVE(15, S_SLOW, E_SMOOTH), NAP(1500, 3500), MOVE(P_CLOSE, S_FAST, E_OUT), END };
const uint8_t N_CRESCENDO[] = { WIGGLE(1, 2, S_MED), WIGGLE(3, 2, S_FAST), WIGGLE(6, 2, S_MAX), END };

enum NearId : uint8_t { nNone, nHesitate, nThink, nTremble, nWiggle, nRecoil, nTouchBack, nScared, nInch, nTics, nMusing,
                        nRetries, nPanic, nSigh, nCrescendo, N_COUNT };
const uint8_t* const NEARG[] = { N_NONE, N_HESITATE, N_THINK, N_TREMBLE, N_WIGGLE, N_RECOIL, N_TOUCHBACK, N_SCARED, N_INCH,
                                 N_TICS, N_MUSING, N_RETRIES, N_PANIC, N_SIGH, N_CRESCENDO };

// ---------------------------------------------------------------------
//  K – KLICK
// ---------------------------------------------------------------------
const uint8_t K_SNAP[]      = { PUSH(S_MAX, E_LIN), END };
const uint8_t K_FIRM[]      = { PUSH(S_FAST, E_SMOOTH), END };
const uint8_t K_RELUCTANT[] = { MOVE(P_TOUCH, S_SLOW, E_SMOOTH), WAIT(300, 800), PUSH(S_CRAWL, E_LIN), END };
const uint8_t K_LASTCM[]    = { MOVE(P_TOUCH - 4, S_EASY, E_SMOOTH), LOOP(3, 4), REL(2, S_SLOW, E_SMOOTH), WAIT(250, 600), NEXT,
                                PUSH(S_SLOW, E_IN), END };
const uint8_t K_DOUBLETAP[] = { MOVE(P_TOUCH, S_FAST, E_OUT), MOVE(80, S_MAX, E_LIN), WAIT(100, 250), PUSH(S_MAX, E_LIN), END };
const uint8_t K_WINDUP[]    = { MOVE(55, S_FAST, E_OUT), WAIT(300, 700), PUSH(S_MAX, E_LIN), END };
const uint8_t K_SHAKY[]     = { JITTER(2, 400), PUSH(S_MED, E_IN), END };
const uint8_t K_SLOWMO[]    = { MOVE(P_TOUCH, S_CRAWL, E_LIN), NAP(1000, 2000), PUSH(S_MAX, E_LIN), END };

enum KlickId : uint8_t { kSnap, kFirm, kReluctant, kLastCm, kDoubleTap, kWindup, kShaky, kSlowmo, K_COUNT };
const uint8_t* const KLICK[] = { K_SNAP, K_FIRM, K_RELUCTANT, K_LASTCM, K_DOUBLETAP, K_WINDUP, K_SHAKY, K_SLOWMO };

// ---------------------------------------------------------------------
//  Z – ZURÜCK
// ---------------------------------------------------------------------
const uint8_t Z_DASH[]    = { HOME(S_MAX, E_LIN), END };
const uint8_t Z_CALM[]    = { HOME(S_EASY, E_SMOOTH), END };
const uint8_t Z_TRIUMPH[] = { WIGGLE(4, 3, S_FAST), HOME(S_FAST, E_OUT), END };
const uint8_t Z_WARY[]    = { MOVE(70, S_SLOW, E_SMOOTH), NAP(1000, 3000), HOME(S_MED, E_SMOOTH), END };
const uint8_t Z_CHECK[]   = { HOME(S_FAST, E_OUT), NAP(1000, 2500), MOVE(40, S_SLOW, E_SMOOTH), WAIT(600, 1500),
                              HOME(S_FAST, E_OUT), END };
const uint8_t Z_FLEE[]    = { HOME(S_MAX, E_LIN), WAIT(150, 400), LOOP(1, 2), MOVE(P_LID + 5, S_MAX, E_LIN), WAIT(30, 60),
                              HOME(S_MAX, E_LIN), WAIT(100, 250), NEXT, END };
const uint8_t Z_SULK[]    = { HOME(S_CRAWL, E_LIN), END };
const uint8_t Z_STAGES[]  = { LOOP(3, 3), REL(-30, S_MED, E_OUT), WAIT(200, 600), NEXT, HOME(S_SLOW, E_SMOOTH), END };
const uint8_t Z_STUMBLE[] = { MOVE(50, S_MAX, E_LIN), WAIT(100, 200), MOVE(60, S_MED, E_SMOOTH), WAIT(200, 400),
                              HOME(S_FAST, E_OUT), END };
const uint8_t Z_THREAT[]  = { MOVE(60, S_FAST, E_OUT), WIGGLE(8, 2, S_MAX), WAIT(500, 1000), HOME(S_MED, E_SMOOTH), END };
// Knarrende Tür: Deckel halb offen halten und ganz langsam absenken
const uint8_t Z_CREAK[]   = { MOVE(P_PEEK, S_MED, E_OUT), NAP(1000, 2500), HOME(S_CRAWL, E_LIN), END };

enum ZurueckId : uint8_t { zDash, zCalm, zTriumph, zWary, zCheck, zFlee, zSulk, zStages, zStumble, zThreat, zCreak, Z_COUNT };
const uint8_t* const ZURUECK[] = { Z_DASH, Z_CALM, Z_TRIUMPH, Z_WARY, Z_CHECK, Z_FLEE, Z_SULK, Z_STAGES, Z_STUMBLE, Z_THREAT,
                                   Z_CREAK };

// ---------------------------------------------------------------------
//  Sondergesten
// ---------------------------------------------------------------------
// Mensch hat den Schalter selbst ausgemacht, bevor der Arm dran war
const uint8_t G_CONFUSED[] = { MOVE(P_PEEK, S_MED, E_OUT), WIGGLE(3, 2, S_MED), NAP(800, 1600), CHANCE(50), JITTER(2, 300), HOME(S_SLOW, E_SMOOTH), END };
// Einige Sekunden nach einer Aktion nochmal aus der Box linsen
const uint8_t G_PEEK[]     = { MOVE(P_PEEK, S_SLOW, E_SMOOTH), NAP(800, 2500), CHANCE(30), WIGGLE(2, 1, S_MED),
                               HOME(S_MED, E_OUT), END };

// ---------------------------------------------------------------------
//  53 PERSÖNLICHKEITEN – je 6 Bytes + Name
// ---------------------------------------------------------------------
struct Persona {
  const char* name;
  uint8_t mood, react, approach, nearG, klick, back;
};

const Persona PERSONAS[] = {
  {"Der Pflichtbewusste",  mNormal,  rShort,   aDirect,    nNone,      kFirm,      zCalm},
  {"Blitzmerker",          mHectic,  rInstant, aRocket,    nNone,      kSnap,      zDash},
  {"Schlafmuetze",         mTired,   rWakeup,  aSneak,     nHesitate,  kReluctant, zSulk},
  {"Der Zoegerer",         mCareful, rShort,   aStroll,    nHesitate,  kLastCm,    zCalm},
  {"Philosoph",            mBored,   rLong,    aPonder,    nThink,     kFirm,      zWary},
  {"Angsthase",            mAnxious, rStartle, aTiptoe,    nScared,    kShaky,     zFlee},
  {"Nervenbuendel",        mAnxious, rDouble,  aStutter,   nTics,      kShaky,     zFlee},
  {"Drama-Queen",          mPlayful, rShort,   aBackForth, nCrescendo, kSlowmo,    zTriumph},
  {"Genervter Beamter",    mAnnoyed, rLong,    aStroll,    nSigh,      kReluctant, zSulk},
  {"Wutbuerger",           mAngry,   rInstant, aDirect,    nPanic,     kSnap,      zThreat},
  {"Anlaufnehmer",         mNormal,  rShort,   aRunups,    nRetries,   kWindup,    zCalm},
  {"Schleicher",           mCareful, rPeek,    aSneak,     nInch,      kLastCm,    zCheck},
  {"Der Misstrauische",    mCareful, rPeek,    aTiptoe,    nTouchBack, kFirm,      zWary},
  {"Stop-and-Go",          mNormal,  rShort,   aStopGo,    nHesitate,  kDoubleTap, zStages},
  {"Zitteraal",            mAnxious, rShort,   aShiver,    nTremble,   kShaky,     zCalm},
  {"Showmaster",           mProud,   rInstant, aWobbly,    nWiggle,    kSnap,      zTriumph},
  {"Der Ignorant",         mBored,   rIgnore,  aStroll,    nNone,      kFirm,      zCalm},
  {"Last-Minute",          mBored,   rIgnore,  aRocket,    nNone,      kSnap,      zDash},
  {"Vollbremser",          mNormal,  rShort,   aRocket,    nHesitate,  kReluctant, zCalm},
  {"Feigling",             mAnxious, rLong,    aBackForth, nRecoil,    kLastCm,    zFlee},
  {"Gruebler",             mCareful, rShort,   aPonder,    nMusing,    kSlowmo,    zWary},
  {"Hektiker",             mHectic,  rDouble,  aStutter,   nPanic,     kSnap,      zDash},
  {"Der Beleidigte",       mAnnoyed, rLong,    aSneak,     nSigh,      kReluctant, zSulk},
  {"Kontrollfreak",        mCareful, rShort,   aDirect,    nTouchBack, kDoubleTap, zCheck},
  {"Morgenmuffel",         mTired,   rWakeup,  aPonder,    nSigh,      kLastCm,    zSulk},
  {"Taenzer",              mPlayful, rShort,   aWobbly,    nCrescendo, kFirm,      zTriumph},
  {"Sprinter",             mHectic,  rInstant, aSpeedUp,   nNone,      kSnap,      zDash},
  {"Diplomat",             mNormal,  rShort,   aStroll,    nMusing,    kFirm,      zCalm},
  {"Wackeldackel",         mPlayful, rDouble,  aWobbly,    nWiggle,    kShaky,     zTriumph},
  {"Der Unentschlossene",  mCareful, rShort,   aBackForth, nRetries,   kLastCm,    zStages},
  {"Choleriker",           mAngry,   rStartle, aRocket,    nPanic,     kWindup,    zThreat},
  {"Faultier",             mTired,   rLong,    aSneak,     nThink,     kSlowmo,    zSulk},
  {"Nachtwaechter",        mCareful, rPeek,    aStopGo,    nInch,      kFirm,      zCheck},
  {"Tollpatsch",           mPlayful, rStartle, aBackForth, nTics,      kDoubleTap, zStumble},
  {"Pedant",               mProud,   rShort,   aTiptoe,    nInch,      kLastCm,    zStages},
  {"Der Ueberraschte",     mAnxious, rStartle, aDirect,    nScared,    kSnap,      zFlee},
  {"Rebell",               mAnnoyed, rIgnore,  aDirect,    nRecoil,    kSnap,      zThreat},
  {"Zeitlupe",             mTired,   rShort,   aSneak,     nHesitate,  kSlowmo,    zSulk},
  {"Tueftler",             mNormal,  rPeek,    aTiptoe,    nTouchBack, kLastCm,    zCheck},
  {"Prahlhans",            mProud,   rInstant, aSpeedUp,   nWiggle,    kSnap,      zTriumph},
  {"Hasenfuss",            mAnxious, rLong,    aStopGo,    nTremble,   kShaky,     zStages},
  {"Der Geniesser",        mBored,   rShort,   aStroll,    nThink,     kSlowmo,    zCalm},
  {"Stotterer",            mHectic,  rDouble,  aStutter,   nRetries,   kDoubleTap, zStumble},
  {"Drohgebaerde",         mAngry,   rShort,   aWobbly,    nCrescendo, kWindup,    zThreat},
  {"Der Resignierte",      mTired,   rIgnore,  aPonder,    nSigh,      kReluctant, zCalm},
  {"Ueberkorrekt",         mProud,   rShort,   aDirect,    nInch,      kFirm,      zStages},
  {"Der Nachtragende",     mAnnoyed, rShort,   aDirect,    nNone,      kFirm,      zCheck},
  {"Klapperschlange",      mPlayful, rRattle,  aStutter,   nTics,      kSnap,      zFlee},
  {"Knarrende Tuer",       mBored,   rWakeup,  aSneak,     nThink,     kReluctant, zCreak},
  {"Tuersteher",           mAnnoyed, rRattle,  aDirect,    nNone,      kSnap,      zCreak},
  {"Chaot",              RND,      RND,      RND,        RND,        RND,        RND},
  {"Launisch",             RND,      RND,      aBackForth, RND,        RND,        RND},
  {"Wundertuete",          mNormal,  RND,      RND,        RND,        RND,        RND},
};
const uint8_t PERSONA_COUNT = COUNT_OF(PERSONAS);

// Tabellen und Enums müssen zusammenpassen
static_assert(COUNT_OF(MOODS) == MOOD_COUNT, "MOODS passt nicht zu MoodId");
static_assert(COUNT_OF(REACT) == R_COUNT, "REACT passt nicht zu ReactId");
static_assert(COUNT_OF(APPROACH) == A_COUNT, "APPROACH passt nicht zu ApproachId");
static_assert(COUNT_OF(NEARG) == N_COUNT, "NEARG passt nicht zu NearId");
static_assert(COUNT_OF(KLICK) == K_COUNT, "KLICK passt nicht zu KlickId");
static_assert(COUNT_OF(ZURUECK) == Z_COUNT, "ZURUECK passt nicht zu ZurueckId");
static_assert(COUNT_OF(PERSONAS) >= 50, "mindestens 50 Persoenlichkeiten");
