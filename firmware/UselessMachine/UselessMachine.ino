/*
  =====================================================================
   Useless Machine mit Charakter
   ESP32-S3 SuperMini – Arduino-Core 2.x oder 3.x

   Dateien:
     config.h    – Pins, Verhalten, optionale Kalibrier-Vorgaben
     bytecode.h  – Mini-Skriptsprache (Opcodes + Makros)
     motion.h    – Schalter, Servo (LEDC), Bewegungs-Engine, Interpreter
     scripts.h   – Gesten, Stimmungen, Persönlichkeiten

   Ein/Aus über den Hauptschalter der Box (kein Deep Sleep).
   Kalibrierung wird im ESP gespeichert (Preferences/NVS).
   Serieller Monitor (115200 Baud, "Neue Zeile"): '?' zeigt die Befehle.
  =====================================================================
*/
#include "config.h"
#include "bytecode.h"
#include "motion.h"
#include "scripts.h"

#include <esp_system.h>
#include <Preferences.h>

#define PREFS_NS "useless"

Motion M;

uint8_t  annoy = 0;                 // 0..100 wie genervt die Maschine ist
uint32_t totalRuns = 0;
uint8_t  recent[5] = {255, 255, 255, 255, 255};
uint8_t  recentPos = 0;

bool     haveLastRun = false;
uint32_t lastRunEnd = 0;
uint32_t peekAt = 0;
bool     justPoweredOn = true;      // erste Vorstellung nach dem Einschalten: oft verschlafen
bool     blockedUntilOff = false;   // nach fehlgeschlagenem Klick: erst wieder, wenn Schalter aus
bool     calibration = false;       // Kalibriermodus: Schalter wird ignoriert
bool     calibrated = false;        // gültige Kalibrierung vorhanden
bool     pendingRetrigger = false;
Calib    draft = {0, 0, 0, 0};      // Kalibrierung in Arbeit (0 = noch nicht gesetzt)

// ---------------------------------------------------------------------
//  Planung einer Vorstellung
// ---------------------------------------------------------------------
struct Plan {
  const char* name;
  uint8_t moodId;
  Mood mood;
  uint8_t r, a, n, k, z;
};

enum class Outcome : uint8_t { Done, UserUndid, Retrigger, PushFailed };

static uint8_t pickOr(uint8_t v, uint8_t count) { return v == RND ? (uint8_t)random(count) : v; }
static bool isGrumpy(uint8_t m) { return m == mAnnoyed || m == mAngry || m == mHectic; }

static bool recentlyUsed(uint8_t idx) {
  for (uint8_t i = 0; i < COUNT_OF(recent); i++)
    if (recent[i] == idx) return true;
  return false;
}

static int pickPersona(bool grumpy) {
  for (int tries = 0; tries < 60; tries++) {
    int i = random(PERSONA_COUNT);
    if (recentlyUsed(i)) continue;
    if (grumpy && tries < 45 && !(PERSONAS[i].mood != RND && isGrumpy(PERSONAS[i].mood))) continue;
    return i;
  }
  return random(PERSONA_COUNT);
}

static Plan planFromPersona(int i) {
  const Persona& ps = PERSONAS[i];
  Plan p;
  p.name = ps.name;
  p.moodId = pickOr(ps.mood, MOOD_COUNT);
  p.r = pickOr(ps.react, R_COUNT);
  p.a = pickOr(ps.approach, A_COUNT);
  p.n = pickOr(ps.nearG, N_COUNT);
  p.k = pickOr(ps.klick, K_COUNT);
  p.z = pickOr(ps.back, Z_COUNT);
  p.mood = MOODS[p.moodId];
  recent[recentPos] = i;
  recentPos = (recentPos + 1) % COUNT_OF(recent);
  return p;
}

static Plan planFreestyle() {
  Plan p;
  p.name = "Freestyle";
  p.moodId = random(MOOD_COUNT);
  p.r = random(R_COUNT);
  p.a = random(A_COUNT);
  p.n = random(N_COUNT);
  p.k = random(K_COUNT);
  p.z = random(Z_COUNT);
  p.mood = MOODS[p.moodId];
  return p;
}

static Plan choosePlan(bool retrigger) {
  bool grumpy = annoy >= ANNOY_GRUMPY_LEVEL || (retrigger && annoy >= ANNOY_GRUMPY_LEVEL / 2);
  if (grumpy || random(100) < PERSONA_CHANCE_PCT) return planFromPersona(pickPersona(grumpy));
  return planFreestyle();
}

// Tagesform: frisch eingeschaltet, genervt, erneut eingeschaltet ...
static void applyDynamics(Plan& p, bool retrigger) {
  if (justPoweredOn && !retrigger && random(100) < 50) {
    p.moodId = mTired;
    p.mood = MOODS[mTired];
    p.r = rWakeup;
  }
  if (annoy > 0) {
    int tempo = p.mood.tempo + annoy / 2;
    int patience = p.mood.patience * (100 - annoy / 2) / 100;
    int nerves = p.mood.nerves + annoy / 3;
    p.mood.tempo = tempo > 255 ? 255 : tempo;
    p.mood.patience = patience < 20 ? 20 : patience;
    p.mood.nerves = nerves > 255 ? 255 : nerves;
  }
  if (retrigger) p.r = random(100) < 70 ? rInstant : rStartle;
}

static void updateAnnoyance(bool retrigger) {
  uint32_t now = millis();
  int a = annoy;
  if (retrigger) {
    a += ANNOY_RETRIGGER_STEP;
  } else if (haveLastRun) {
    uint32_t since = now - lastRunEnd;
    if (since < ANNOY_WINDOW_MS) {
      a += ANNOY_STEP;
    } else {
      uint32_t decay = since / 1000 * ANNOY_DECAY_PER_S;
      a = decay >= (uint32_t)a ? 0 : a - (int)decay;
    }
  }
  annoy = a > 100 ? 100 : a;
}

// ---------------------------------------------------------------------
//  Eine Vorstellung
// ---------------------------------------------------------------------
static Outcome perform(const Plan& p, bool test) {
  Serial.printf("[#%lu] %-20s | %-11s | R%u A%u N%u K%u Z%u | genervt %u%%\n", (unsigned long)totalRuns, p.name,
                p.mood.name, p.r, p.a, p.n, p.k, p.z, annoy);

  M.servo.attach();
  M.mood = p.mood;
  M.pushFailed = false;
  M.testMode = test;

  // 1-3: Reaktion, Anfahrt, Theater. Macht der Mensch den Schalter selbst aus -> abbrechen.
  M.setGuard(test ? Guard::None : Guard::AbortIfOff);
  if (!(M.run(REACT[p.r]) && M.run(APPROACH[p.a]) && M.run(NEARG[p.n]))) {
    Serial.println(F("  -> Nanu? Schalter ist schon aus."));
    M.setGuard(Guard::AbortIfOn);
    if (!M.run(G_CONFUSED) || !M.moveTo(0, M.speedOf(S_SLOW), E_SMOOTH)) return Outcome::Retrigger;
    return Outcome::UserUndid;
  }

  // 4: Klick – hier unterbricht nichts
  M.setGuard(Guard::None);
  M.run(KLICK[p.k]);
  if (M.sw.isOn() && !test && !M.pushFailed) {
    Serial.println(F("  -> Schalter noch an: Sicherheits-Klick"));
    M.push(1e6f, E_LIN);
  }
  bool failed = M.pushFailed;
  if (failed) Serial.println(F("  !! Schalter ließ sich nicht umlegen -> PUSH neu kalibrieren (P, w) / Mechanik prüfen"));

  // 5: Rückzug. Schaltet der Mensch wieder ein -> sofort neue Vorstellung.
  M.setGuard((test || failed) ? Guard::None : Guard::AbortIfOn);
  if (!M.run(ZURUECK[p.z]) || !M.moveTo(0, M.speedOf(S_MED), E_SMOOTH)) return Outcome::Retrigger;
  M.setGuard(Guard::None);
  M.testMode = false;
  return failed ? Outcome::PushFailed : Outcome::Done;
}

static void handleTrigger(bool test = false, int forcedPersona = -1) {
  bool retrig = pendingRetrigger;
  pendingRetrigger = false;
  do {
    updateAnnoyance(retrig);
    Plan p = forcedPersona >= 0 ? planFromPersona(forcedPersona) : choosePlan(retrig);
    forcedPersona = -1;
    applyDynamics(p, retrig);
    justPoweredOn = false;
    totalRuns++;

    Outcome o = perform(p, test);

    lastRunEnd = millis();
    haveLastRun = true;
    retrig = (o == Outcome::Retrigger);
    if (retrig) Serial.println(F("  -> Schon wieder?!"));
    if (o == Outcome::PushFailed) blockedUntilOff = true;
  } while (retrig);

  M.testMode = false;
  M.setGuard(Guard::None);
  peekAt = (random(100) < PEEK_CHANCE_PCT) ? millis() + random(PEEK_MIN_MS, PEEK_MAX_MS) : 0;
}

// ---------------------------------------------------------------------
//  Onboard-RGB-LED (WS2812) aus
// ---------------------------------------------------------------------
#ifdef PIN_RGB_LED
static void rgbLedOff() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  rgbLedWrite(PIN_RGB_LED, 0, 0, 0);
#else
  neopixelWrite(PIN_RGB_LED, 0, 0, 0);
#endif
}
#endif

// ---------------------------------------------------------------------
//  Kalibrierung: prüfen, laden, speichern
// ---------------------------------------------------------------------
static bool calibValid(const Calib& c, bool verbose = false) {
  const uint16_t v[4] = {c.home, c.lid, c.touch, c.push};
  const char* names[4] = {"HOME (H)", "DECKEL (D)", "TOUCH (T)", "PUSH (P)"};
  bool ok = true;
  for (int i = 0; i < 4; i++) {
    if (v[i] < SERVO_US_MIN || v[i] > SERVO_US_MAX) {
      if (verbose) Serial.printf("  %s fehlt oder ungültig\n", names[i]);
      ok = false;
    }
  }
  if (!ok) return false;
  bool up = c.push > c.home;
  for (int i = 1; i < 4; i++) {
    if (up ? v[i] <= v[i - 1] : v[i] >= v[i - 1]) {
      if (verbose) Serial.println(F("  Reihenfolge stimmt nicht: HOME -> DECKEL -> TOUCH -> PUSH müssen in eine Richtung laufen"));
      return false;
    }
  }
  if (abs((int)c.push - (int)c.home) < 200) {
    if (verbose) Serial.println(F("  HOME und PUSH liegen zu nah beieinander"));
    return false;
  }
  return true;
}

static bool loadCalib() {
  Preferences prefs;
  if (!prefs.begin(PREFS_NS, true)) return false;  // noch nie gespeichert
  Calib c;
  bool ok = prefs.getBytesLength("calib") == sizeof(c) && prefs.getBytes("calib", &c, sizeof(c)) == sizeof(c) &&
            calibValid(c);
  prefs.end();
  if (ok) calib = c;
  return ok;
}

static bool saveCalib(const Calib& c) {
  Preferences prefs;
  if (!prefs.begin(PREFS_NS, false)) return false;
  bool ok = prefs.putBytes("calib", &c, sizeof(c)) == sizeof(c);
  prefs.end();
  return ok;
}

static void clearCalib() {
  Preferences prefs;
  if (prefs.begin(PREFS_NS, false)) {
    prefs.remove("calib");
    prefs.end();
  }
}

// Servo langsam auf einen µs-Wert fahren. Beim allerersten Attach springt der
// Servo auf die zuletzt gesetzte Sollposition (ohne Kalibrierung: Mitte).
static void rampUs(float target) {
  target = constrain(target, (float)SERVO_US_MIN, (float)SERVO_US_MAX);
  M.servo.attach();
  float from = M.servo.us();
  float durMs = fabs(target - from) / JOG_US_PER_S * 1000.0f;
  uint32_t t0 = millis();
  for (;;) {
    float t = durMs > 0 ? (millis() - t0) / durMs : 1.0f;
    if (t > 1.0f) t = 1.0f;
    M.servo.writeUs(from + (target - from) * t);
    if (t >= 1.0f) break;
    delay(15);
  }
  M.lastMotion = millis();
}

static float degFromHome(float us) {
  uint16_t home = draft.home ? draft.home : (calibrated ? calib.home : 0);
  return home ? fabs(us - home) / SERVO_US_PER_DEG : -1.0f;
}

static void printUs() {
  float d = degFromHome(M.servo.us());
  if (d >= 0) Serial.printf("%.0f µs  (≈ %.0f° ab HOME)\n", M.servo.us(), d);
  else Serial.printf("%.0f µs\n", M.servo.us());
}

static void printCalib(const char* title, const Calib& c) {
  Serial.printf("%s  HOME %u | DECKEL %u | TOUCH %u | PUSH %u µs", title, c.home, c.lid, c.touch, c.push);
  if (c.home && c.push) Serial.printf("  (PUSH ≈ %.0f° ab HOME)", fabs((float)c.push - c.home) / SERVO_US_PER_DEG);
  Serial.println();
}

// ---------------------------------------------------------------------
//  Serielle Befehle (Kalibrierung & Test)
// ---------------------------------------------------------------------
static void printHelp() {
  Serial.println(F(
      "\nKalibrieren (Arm bleibt montiert, alles fährt langsam):\n"
      "  m        auf die Mitte (1500 µs ≈ 90°) – immer der erste Schritt\n"
      "  + / -    10 µs (≈ 1°) weiter, z. B. +50 / -50\n"
      "  u1500    langsam auf 1500 µs\n"
      "  H D T P  aktuelle Stellung als HOME / DECKEL / TOUCH / PUSH merken\n"
      "  w        Kalibrierung speichern (bleibt auch nach neuem Flashen)\n"
      "  x        gespeicherte Kalibrierung löschen\n"
      "Nach dem Speichern:\n"
      "  c        Kalibriermodus an/aus (aus = Maschine reagiert auf den Schalter)\n"
      "  h d t p  langsam zu HOME / DECKEL / TOUCH / PUSH\n"
      "  g42      langsam zu Position 42 %\n"
      "  r / n7   Testlauf zufällig / Persönlichkeit Nr. 7\n"
      "  l        Persönlichkeiten auflisten\n"
      "  s        Status\n"));
}

static bool needCalibrated() {
  if (calibrated) return true;
  Serial.println(F("Noch nicht kalibriert: erst m, H D T P setzen, dann w."));
  return false;
}

// Langsam auf eine %-Position (setzt die Engine-Position mit)
static void goPos(float target) {
  rampUs(Motion::posToUs(target));
  M.pos = target;
  Serial.printf("pos %.0f %% = ", target);
  printUs();
}

static void setDraft(char which) {
  uint16_t us = (uint16_t)(M.servo.us() + 0.5f);
  switch (which) {
    case 'H': draft.home = us; Serial.print(F("HOME   = ")); break;
    case 'D': draft.lid = us; Serial.print(F("DECKEL = ")); break;
    case 'T': draft.touch = us; Serial.print(F("TOUCH  = ")); break;
    case 'P': draft.push = us; Serial.print(F("PUSH   = ")); break;
  }
  printUs();
  printCalib("Entwurf:", draft);
}

static void runCommand(char* cmd) {
  int arg = atoi(cmd + 1);
  switch (cmd[0]) {
    case '?': printHelp(); break;

    // --- Kalibrieren ---
    case 'm':
      calibration = true;
      Serial.println(F("Mitte (≈ 90°) ..."));
      rampUs(SERVO_US_MID);
      printUs();
      break;
    case 'u':
      calibration = true;
      rampUs(arg);
      printUs();
      break;
    case '+':
    case '-': {
      calibration = true;
      int step = arg ? arg : 10;
      rampUs(M.servo.us() + (cmd[0] == '+' ? step : -step));
      printUs();
      break;
    }
    case 'H': case 'D': case 'T': case 'P':
      if (!M.servo.isAttached()) { Serial.println(F("Servo noch aus – erst m.")); break; }
      setDraft(cmd[0]);
      break;
    case 'w':
      if (!calibValid(draft, true)) { Serial.println(F("Nicht gespeichert.")); break; }
      if (!saveCalib(draft)) { Serial.println(F("Speichern fehlgeschlagen!")); break; }
      calib = draft;
      calibrated = true;
      printCalib("Gespeichert:", calib);
      goPos(0);
      Serial.println(F("Mit h d t p prüfen, dann c (Kalibriermodus aus)."));
      break;
    case 'x':
      clearCalib();
      calibrated = false;
      calibration = true;
      draft = {0, 0, 0, 0};
      Serial.println(F("Kalibrierung gelöscht. Neu beginnen mit m."));
      break;

    // --- Nach dem Kalibrieren ---
    case 'c':
      if (!needCalibrated()) break;
      calibration = !calibration;
      if (!calibration) goPos(0);
      Serial.printf("Kalibriermodus %s\n", calibration ? "AN" : "AUS");
      break;
    case 'h': if (needCalibrated()) goPos(0); break;
    case 'd': if (needCalibrated()) goPos(P_LID); break;
    case 't': if (needCalibrated()) goPos(P_TOUCH); break;
    case 'p':
      if (!needCalibrated()) break;
      goPos(100);
      delay(300);
      goPos(P_CLOSE);
      break;
    case 'g': if (needCalibrated()) goPos(constrain(arg, 0, 100)); break;
    case 'r':
      if (!needCalibrated()) break;
      goPos(0);
      handleTrigger(true);
      break;
    case 'n':
      if (!needCalibrated()) break;
      if (arg < 0 || arg >= PERSONA_COUNT) { Serial.printf("0..%u\n", PERSONA_COUNT - 1); break; }
      goPos(0);
      handleTrigger(true, arg);
      break;
    case 'l':
      for (uint8_t i = 0; i < PERSONA_COUNT; i++) Serial.printf("  %2u  %s\n", i, PERSONAS[i].name);
      break;
    case 's':
      Serial.printf("Schalter %s | Servo %s | ", M.sw.isOn() ? "AN" : "aus", M.servo.isAttached() ? "an" : "aus");
      printUs();
      Serial.printf("kalibriert %s | Kalibriermodus %s | genervt %u | Laeufe %lu\n", calibrated ? "ja" : "NEIN",
                    calibration ? "AN" : "aus", annoy, (unsigned long)totalRuns);
      if (calibrated) printCalib("Gespeichert:", calib);
      printCalib("Entwurf:    ", draft);
      break;
    default: Serial.println(F("? für Hilfe")); break;
  }
}

static void handleSerial() {
  static char buf[24];
  static uint8_t len = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (len) { buf[len] = 0; runCommand(buf); len = 0; }
    } else if (len < sizeof(buf) - 1) {
      buf[len++] = c;
    }
  }
}

// ---------------------------------------------------------------------
void setup() {
  // Servo-Pin so früh wie möglich definiert auf LOW (gegen Zucken)
  pinMode(PIN_SERVO, OUTPUT);
  digitalWrite(PIN_SERVO, LOW);
#ifdef PIN_RGB_LED
  rgbLedOff();
#endif

  Serial.begin(115200);

  if (esp_reset_reason() == ESP_RST_BROWNOUT) {
    Serial.println(F("!! BROWNOUT-Reset: Versorgung bricht beim Servo-Anlauf ein. "
                     "Elko/Batterien prüfen oder SPEED_LIMIT_PCT_S setzen."));
  }

  calibrated = loadCalib();
#if CALIB_USE_DEFAULTS
  if (!calibrated) {
    Calib d = {CALIB_DEFAULT_HOME, CALIB_DEFAULT_LID, CALIB_DEFAULT_TOUCH, CALIB_DEFAULT_PUSH};
    if (calibValid(d)) { calib = d; calibrated = true; }
  }
#endif

  if (calibrated) {
    draft = calib;
    M.begin();  // Arm liegt in HOME -> Puls auf HOME bewegt ihn nicht
  } else {
    M.sw.begin();  // KEINE Servo-Pulse, Arm bleibt liegen
    calibration = true;
  }

  Serial.printf("\nUseless Machine bereit (%s). %u Persönlichkeiten. '?' für Hilfe.\n",
                CONFIG_IDF_TARGET, PERSONA_COUNT);
  if (calibrated) {
    printCalib("Kalibrierung:", calib);
  } else {
    Serial.println(F("!! Noch nicht kalibriert: Servo bleibt aus, Schalter wird ignoriert. Start mit 'm'."));
  }
}

void loop() {
  M.sw.update();
  handleSerial();

  if (calibration) { delay(5); return; }

  bool on = M.sw.isOn();
  if (blockedUntilOff) {
    if (!on) blockedUntilOff = false;
  } else if (on) {
    handleTrigger();
    return;
  }

  // Später nochmal aus der Box linsen
  if (peekAt && !on && millis() >= peekAt) {
    peekAt = 0;
    M.servo.attach();
    M.mood = MOODS[random(MOOD_COUNT)];
    M.setGuard(Guard::AbortIfOn);
    if (!M.run(G_PEEK)) pendingRetrigger = true;   // erwischt!
    M.setGuard(Guard::None);
  }

  // Ruhender Arm: PWM aus -> kein Brummen, weniger Strom
  if (M.servo.isAttached() && M.pos < 0.5f && millis() - M.lastMotion > SERVO_DETACH_IDLE_MS) {
    M.servo.detach();
  }

  delay(5);
}
