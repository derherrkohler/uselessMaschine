/*
  =====================================================================
   Useless Machine mit Charakter
   ESP32-C3 SuperMini / ESP32-S3 SuperMini – Arduino-Core 2.x oder 3.x

   Dateien:
     config.h    – Pins, Servo-Kalibrierung, Verhalten, Deep Sleep
     bytecode.h  – Mini-Skriptsprache (Opcodes + Makros)
     motion.h    – Schalter, Servo (LEDC), Bewegungs-Engine, Interpreter
     scripts.h   – Gesten, Stimmungen, 50 Persönlichkeiten

   Serieller Monitor (115200 Baud, "Neue Zeile"): '?' zeigt die Befehle.
  =====================================================================
*/
#include "config.h"
#include "bytecode.h"
#include "motion.h"
#include "scripts.h"

#include <esp_sleep.h>
#include <esp_system.h>
#include <driver/gpio.h>
#include <soc/soc_caps.h>
#if SOC_PM_SUPPORT_EXT0_WAKEUP
  #include <driver/rtc_io.h>
#endif

Motion M;

// Überlebt Deep Sleep (nicht aber Stromlos/Reset)
RTC_DATA_ATTR uint8_t  annoy = 0;          // 0..100 wie genervt die Maschine ist
RTC_DATA_ATTR uint32_t totalRuns = 0;
RTC_DATA_ATTR uint8_t  recent[5] = {255, 255, 255, 255, 255};
RTC_DATA_ATTR uint8_t  recentPos = 0;

bool     haveLastRun = false;
uint32_t lastRunEnd = 0;
uint32_t lastActivity = 0;
uint32_t peekAt = 0;
bool     justWoke = false;
bool     blockedUntilOff = false;   // nach fehlgeschlagenem Klick: erst wieder, wenn Schalter aus
bool     calibration = false;
bool     pendingRetrigger = false;

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

// Tagesform: frisch aufgewacht, genervt, erneut eingeschaltet ...
static void applyDynamics(Plan& p, bool retrigger) {
  if (justWoke && !retrigger && random(100) < 50) {
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
  if (failed) Serial.println(F("  !! Schalter ließ sich nicht umlegen -> SERVO_US_PUSH / Mechanik prüfen"));

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
    justWoke = false;
    totalRuns++;

    Outcome o = perform(p, test);

    lastRunEnd = millis();
    haveLastRun = true;
    lastActivity = lastRunEnd;
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
//  Deep Sleep
// ---------------------------------------------------------------------
static void goToSleep() {
  if (!esp_sleep_is_valid_wakeup_gpio((gpio_num_t)PIN_SWITCH)) {
    Serial.println(F("[sleep] PIN_SWITCH kann nicht aus Deep Sleep wecken – Sleep deaktiviert."));
    lastActivity = millis();
    return;
  }
  Serial.println(F("[sleep] Gute Nacht. Wecken per Schalter."));
  Serial.flush();

  M.servo.detach();                      // Pin LOW -> keine Phantom-Pulse
  gpio_hold_en((gpio_num_t)PIN_SERVO);   // GPIO4 ist RTC-GPIO: Hold gilt auch im Deep Sleep
#ifdef PIN_RGB_LED
  // Datenleitung fest auf LOW, sonst können Störimpulse die LED im Schlaf einschalten
  pinMode(PIN_RGB_LED, OUTPUT);
  digitalWrite(PIN_RGB_LED, LOW);
  gpio_hold_en((gpio_num_t)PIN_RGB_LED);
#endif
#if !SOC_GPIO_SUPPORT_HOLD_SINGLE_IO_IN_DSLP
  gpio_deep_sleep_hold_en();             // nötig, damit digitale GPIOs (z. B. 48) im Deep Sleep gehalten werden
#endif

#if SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP    // ESP32-C3
  esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_SWITCH,
      SWITCH_ON_LEVEL == LOW ? ESP_GPIO_WAKEUP_GPIO_LOW : ESP_GPIO_WAKEUP_GPIO_HIGH);
#elif SOC_PM_SUPPORT_EXT0_WAKEUP         // ESP32-S3
  if (SWITCH_USE_INTERNAL_PULLUP) {
    rtc_gpio_pullup_en((gpio_num_t)PIN_SWITCH);
    rtc_gpio_pulldown_dis((gpio_num_t)PIN_SWITCH);
  }
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_SWITCH, SWITCH_ON_LEVEL == LOW ? 0 : 1);
#endif
  esp_deep_sleep_start();
}

// ---------------------------------------------------------------------
//  Serielle Befehle (Kalibrierung & Test)
// ---------------------------------------------------------------------
static void printHelp() {
  Serial.println(F(
      "\nBefehle:\n"
      "  c        Kalibriermodus an/aus (Schalter & Sleep ignoriert)\n"
      "  u1500    Servo direkt auf 1500 µs (aktiviert Kalibriermodus)\n"
      "  + / -    +/-10 µs  (z. B. +25)\n"
      "  h d t p  fahre zu HOME / DECKEL / TOUCH / PUSH (kalibrierte Werte)\n"
      "  g42      fahre zu Position 42 %\n"
      "  r        Testlauf zufällige Persönlichkeit (ohne Schalter)\n"
      "  n7       Testlauf Persönlichkeit Nr. 7\n"
      "  l        Persönlichkeiten auflisten\n"
      "  s        Status\n"));
}

static void goPos(float target) {
  M.servo.attach();
  M.setGuard(Guard::None);
  M.mood = MOODS[mNormal];
  M.moveTo(target, 60, E_SMOOTH, 100);
  Serial.printf("pos %.0f %% = %.0f µs\n", M.pos, M.servo.us());
}

static void runCommand(char* cmd) {
  lastActivity = millis();
  int arg = atoi(cmd + 1);
  switch (cmd[0]) {
    case '?': printHelp(); break;
    case 'c':
      calibration = !calibration;
      M.servo.attach();
      Serial.printf("Kalibriermodus %s\n", calibration ? "AN" : "AUS");
      break;
    case 'u':
      calibration = true;
      M.servo.attach();
      M.servo.writeUs(arg);
      Serial.printf("%.0f µs  (danach 'h' benutzen, bevor normale Bewegungen laufen)\n", M.servo.us());
      break;
    case '+':
    case '-': {
      calibration = true;
      int step = arg ? arg : 10;
      M.servo.attach();
      M.servo.writeUs(M.servo.us() + (cmd[0] == '+' ? step : -step));
      Serial.printf("%.0f µs\n", M.servo.us());
      break;
    }
    case 'h': goPos(0); break;
    case 'd': goPos(P_LID); break;
    case 't': goPos(P_TOUCH); break;
    case 'p': goPos(100); delay(300); goPos(P_CLOSE); break;
    case 'g': goPos(constrain(arg, 0, 100)); break;
    case 'r': handleTrigger(true); break;
    case 'n':
      if (arg >= 0 && arg < PERSONA_COUNT) handleTrigger(true, arg);
      else Serial.printf("0..%u\n", PERSONA_COUNT - 1);
      break;
    case 'l':
      for (uint8_t i = 0; i < PERSONA_COUNT; i++) Serial.printf("  %2u  %s\n", i, PERSONAS[i].name);
      break;
    case 's':
      Serial.printf("Schalter %s | pos %.1f %% | %.0f µs | genervt %u | Laeufe %lu | Kalibrierung %s\n",
                    M.sw.isOn() ? "AN" : "aus", M.pos, M.servo.us(), annoy, (unsigned long)totalRuns,
                    calibration ? "AN" : "aus");
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
  // Servo-Pin so früh wie möglich definiert auf LOW (gegen Zucken).
  // Erst Pegel setzen, dann den Deep-Sleep-Hold lösen.
  pinMode(PIN_SERVO, OUTPUT);
  digitalWrite(PIN_SERVO, LOW);
#if !SOC_GPIO_SUPPORT_HOLD_SINGLE_IO_IN_DSLP
  gpio_deep_sleep_hold_dis();
#endif
  gpio_hold_dis((gpio_num_t)PIN_SERVO);
#ifdef PIN_RGB_LED
  gpio_hold_dis((gpio_num_t)PIN_RGB_LED);
  rgbLedOff();
#endif
#if SOC_PM_SUPPORT_EXT0_WAKEUP
  rtc_gpio_deinit((gpio_num_t)PIN_SWITCH);  // nach ext0-Wakeup wieder normaler GPIO
#endif

  Serial.begin(115200);

  esp_sleep_wakeup_cause_t wake = esp_sleep_get_wakeup_cause();
  justWoke = (wake == ESP_SLEEP_WAKEUP_GPIO || wake == ESP_SLEEP_WAKEUP_EXT0);
  if (!justWoke) {
    annoy = 0;
  } else {
    annoy /= 2;  // Schlaf beruhigt
  }
  if (esp_reset_reason() == ESP_RST_BROWNOUT) {
    Serial.println(F("!! BROWNOUT-Reset: Versorgung bricht beim Servo-Anlauf ein. "
                     "Elko/Diode/Batterien prüfen oder SPEED_LIMIT_PCT_S setzen."));
  }

  if (justWoke) Serial.println(F("[wake] vom Schalter geweckt"));

  M.begin();
  lastActivity = millis();

  Serial.printf("\nUseless Machine bereit (%s). %u Persönlichkeiten. '?' für Hilfe.\n",
                CONFIG_IDF_TARGET, PERSONA_COUNT);
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
    lastActivity = millis();
  }

  // Ruhender Arm: PWM aus -> kein Brummen, weniger Strom
  if (M.servo.isAttached() && M.pos < 0.5f && millis() - M.lastMotion > SERVO_DETACH_IDLE_MS) {
    M.servo.detach();
  }

#if ENABLE_DEEP_SLEEP
  if (!on && !blockedUntilOff && !peekAt && millis() - lastActivity > IDLE_SLEEP_MS) goToSleep();
#endif

  delay(5);
}
