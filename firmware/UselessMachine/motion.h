#pragma once
// =====================================================================
//  motion.h – Schalter entprellen, Servo ansteuern, Skripte ausführen
// =====================================================================
#include "config.h"
#include "bytecode.h"

// ---------------------------------------------------------------------
//  Entprellter Schalter
// ---------------------------------------------------------------------
class DebouncedSwitch {
 public:
  void begin() {
    pinMode(PIN_SWITCH, SWITCH_USE_INTERNAL_PULLUP ? INPUT_PULLUP : INPUT);
    delay(2);
    stable_ = lastRaw_ = raw();
    lastChange_ = millis();
  }
  void update() {
    bool r = raw();
    uint32_t now = millis();
    if (r != lastRaw_) {
      lastRaw_ = r;
      lastChange_ = now;
    } else if (r != stable_ && now - lastChange_ >= DEBOUNCE_MS) {
      stable_ = r;
    }
  }
  bool isOn() const { return stable_; }

 private:
  bool raw() const { return digitalRead(PIN_SWITCH) == SWITCH_ON_LEVEL; }
  bool stable_ = false, lastRaw_ = false;
  uint32_t lastChange_ = 0;
};

// ---------------------------------------------------------------------
//  Servo über LEDC (ohne externe Library, µs-genau)
// ---------------------------------------------------------------------
class ServoOut {
 public:
  void attach() {
    if (attached_) return;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(PIN_SERVO, 50, SERVO_PWM_BITS);
#else
    ledcSetup(SERVO_LEDC_CHANNEL, 50, SERVO_PWM_BITS);
    ledcAttachPin(PIN_SERVO, SERVO_LEDC_CHANNEL);
#endif
    attached_ = true;
    writeUs(us_);
  }

  // PWM aus, Pin fest auf LOW -> Servo bekommt keine Pulse mehr
  void detach() {
    if (attached_) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
      ledcDetach(PIN_SERVO);
#else
      ledcDetachPin(PIN_SERVO);
#endif
    }
    attached_ = false;
    pinMode(PIN_SERVO, OUTPUT);
    digitalWrite(PIN_SERVO, LOW);
  }

  void writeUs(float us) {
    us_ = constrain(us, (float)SERVO_US_MIN, (float)SERVO_US_MAX);
    if (!attached_) return;
    const float maxDuty = (float)((1UL << SERVO_PWM_BITS) - 1);
    uint32_t duty = (uint32_t)(us_ * maxDuty / 20000.0f + 0.5f);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(PIN_SERVO, duty);
#else
    ledcWrite(SERVO_LEDC_CHANNEL, duty);
#endif
  }

  float us() const { return us_; }
  bool isAttached() const { return attached_; }

 private:
  bool attached_ = false;
  float us_ = SERVO_US_HOME;
};

// ---------------------------------------------------------------------
//  Bewegungs-Engine + Skript-Interpreter
// ---------------------------------------------------------------------
enum class Guard : uint8_t {
  None,        // nichts unterbricht
  AbortIfOff,  // Schalter wurde (vom Menschen) schon ausgemacht -> abbrechen
  AbortIfOn    // Schalter wieder an, während wir zurückfahren -> abbrechen
};

class Motion {
 public:
  DebouncedSwitch sw;
  ServoOut servo;
  Mood mood = {"Normal", 100, 100, 100, 2};
  float pos = 0;              // aktuelle Soll-Position 0..100
  bool pushFailed = false;    // Schalter ließ sich nicht umlegen
  bool testMode = false;      // Testlauf ohne echten Schalter
  uint32_t lastMotion = 0;

  void begin() {
    sw.begin();
    servo.writeUs(posToUs(0));
    servo.attach();
    pos = 0;
    lastMotion = millis();
  }

  void setGuard(Guard g) { guard_ = g; }

  static float posToUs(float p) {
    if (p <= P_LID)
      return SERVO_US_HOME + (SERVO_US_LID - SERVO_US_HOME) * (p / (float)P_LID);
    if (p <= P_TOUCH)
      return SERVO_US_LID + (SERVO_US_TOUCH - SERVO_US_LID) * ((p - P_LID) / (float)(P_TOUCH - P_LID));
    return SERVO_US_TOUCH + (SERVO_US_PUSH - SERVO_US_TOUCH) * ((p - P_TOUCH) / (100.0f - P_TOUCH));
  }

  // Wartet und überwacht dabei den Schalter. false = Abbruch durch Guard.
  bool waitMs(uint32_t ms) {
    uint32_t start = millis();
    for (;;) {
      sw.update();
      if (guard_ == Guard::AbortIfOff && !sw.isOn()) return false;
      if (guard_ == Guard::AbortIfOn && sw.isOn()) return false;
      uint32_t el = millis() - start;
      if (el >= ms) return true;
      uint32_t rest = ms - el;
      delay(rest < 5 ? rest : 5);
    }
  }

  // Mood-skalierte Geschwindigkeit aus Skriptwert
  float speedOf(uint8_t s) const {
    if (s >= S_MAX) return 1e6f;
    float v = s * mood.tempo / 100.0f * rndf(0.85f, 1.15f);
    return v < 3.0f ? 3.0f : v;
  }

  bool moveTo(float target, float speed, uint8_t ease, float maxPos = P_GESTURE_MAX) {
    target = constrain(target, 0.0f, maxPos);
    float dist = fabs(target - pos);
    if (dist < 0.2f) return waitMs(0);

    // Leerweg unter dem Deckel ist unsichtbar -> zügig durchfahren
    const float dz = P_LID - 3.0f;
    if (LID_FAST_TRAVEL_PCT_S > 0.0f && speed < LID_FAST_TRAVEL_PCT_S) {
      if (pos < dz - 0.5f && target > dz) {
        if (!moveTo(dz, LID_FAST_TRAVEL_PCT_S, E_SMOOTH, maxPos)) return false;
      } else if (pos > dz && target < dz - 0.5f) {
        if (!moveTo(dz, speed, ease, maxPos)) return false;
        return moveTo(target, LID_FAST_TRAVEL_PCT_S, E_OUT, maxPos);
      }
    }

    float vmax = SERVO_MAX_SPEED_PCT_S;
    bool limited = SPEED_LIMIT_PCT_S > 0.0f;
    if (limited && SPEED_LIMIT_PCT_S < vmax) vmax = SPEED_LIMIT_PCT_S;

    // Vollgas: Ziel direkt setzen, Servo macht den Rest
    if (speed >= vmax && !limited) {
      servo.writeUs(posToUs(target));
      pos = target;
      lastMotion = millis();
      return waitMs((uint32_t)(dist / SERVO_MAX_SPEED_PCT_S * 1000.0f) + 10);
    }
    if (speed > vmax) speed = vmax;

    // speed = Spitzengeschwindigkeit -> Dauer abhängig vom Profil verlängern
    float durMs = dist / speed * 1000.0f * peakFactor(ease);
    float start = pos;
    uint32_t t0 = millis();
    for (;;) {
      float t = (millis() - t0) / durMs;
      if (t > 1.0f) t = 1.0f;
      pos = start + (target - start) * applyEase(t, ease);
      servo.writeUs(posToUs(pos));
      lastMotion = millis();
      if (t >= 1.0f) return true;
      if (!waitMs(MOTION_STEP_MS)) return false;
    }
  }

  bool wiggle(uint8_t amp, uint8_t count, uint8_t s) {
    float a = constrain(amp * mood.nerves / 100.0f, 1.0f, 20.0f);
    float base = pos;
    float v = speedOf(s);
    uint8_t n = count + ((mood.nerves > 140 && random(2)) ? 1 : 0);
    for (uint8_t i = 0; i < n; i++) {
      if (!moveTo(base + a, v, E_SMOOTH)) return false;
      if (!moveTo(base - a, v, E_SMOOTH)) return false;
    }
    return moveTo(base, v, E_SMOOTH);
  }

  bool jitter(uint8_t amp, uint32_t ms) {
    float a = constrain(amp * mood.nerves / 100.0f, 0.5f, 12.0f);
    float base = pos;
    uint32_t t0 = millis();
    while (millis() - t0 < ms) {
      float p = constrain(base + rndf(-a, a), 0.0f, (float)P_GESTURE_MAX);
      servo.writeUs(posToUs(p));
      lastMotion = millis();
      if (!waitMs(random(25, 70))) { pos = p; return false; }
    }
    servo.writeUs(posToUs(base));
    pos = base;
    return waitMs(20);
  }

  // Schalter umlegen – mit Erfolgskontrolle und Wiederholung.
  // Danach wird immer vom Anschlag weggefahren (kein Blockieren = kein Stromspitzen-Dauerfeuer).
  bool push(float v, uint8_t ease) {
    bool ok = false;
    if (!sw.isOn() && !testMode) {
      ok = true;  // schon aus (z. B. beim Heranfahren umgefallen)
    } else {
      for (uint8_t attempt = 0; attempt <= PUSH_RETRIES; attempt++) {
        if (!moveTo(100, attempt ? 1e6f : v, attempt ? E_LIN : ease, 100)) return false;
        uint32_t t0 = millis();
        while (millis() - t0 < PUSH_CONFIRM_MS && (sw.isOn() || testMode)) {
          if (!waitMs(5)) return false;
          if (testMode && millis() - t0 > 120) break;
        }
        if (!sw.isOn() || testMode) { ok = true; break; }
        Serial.printf("  [push] Schalter noch an – Versuch %u\n", attempt + 2);
        if (attempt < PUSH_RETRIES) {
          if (!moveTo(P_CLOSE - 10, 1e6f, E_LIN)) return false;
          if (!waitMs(150)) return false;
        }
      }
    }
    if (!ok) pushFailed = true;
    return moveTo(P_CLOSE, 1e6f, E_LIN);
  }

  // ------------------------------------------------------------------
  //  Interpreter. false = abgebrochen (Guard hat ausgelöst)
  // ------------------------------------------------------------------
  bool run(const uint8_t* c) {
    struct Frame { uint16_t pc; uint8_t left; } stack[4];
    uint8_t sp = 0;
    uint16_t pc = 0;
    for (uint16_t steps = 0; steps < 2000; steps++) {
      uint8_t op = c[pc];
      if (op >= OP__COUNT) {
        Serial.printf("  [vm] ungültiger Opcode %u @%u\n", op, pc);
        return true;
      }
      const uint8_t* a = c + pc + 1;
      uint16_t next = pc + OP_LEN[op];
      switch (op) {
        case OP_END:
          return true;
        case OP_MOVE:
          if (!moveTo(sloppy(a[0]), speedOf(a[1]), a[2])) return false;
          break;
        case OP_MOVER:
          if (!moveTo(sloppy(rndRange(a[0], a[1])), speedOf(a[2]), a[3])) return false;
          break;
        case OP_REL:
          if (!moveTo(pos + (int8_t)a[0], speedOf(a[1]), a[2])) return false;
          break;
        case OP_WAIT:
          if (!waitMs(scaleWait(rndRange(a[0], a[1]) * 20UL))) return false;
          break;
        case OP_NAP:
          if (!waitMs(scaleWait(rndRange(a[0], a[1]) * 100UL))) return false;
          break;
        case OP_WIGGLE:
          if (!wiggle(a[0], a[1], a[2])) return false;
          break;
        case OP_JITTER:
          if (!jitter(a[0], a[1] * 20UL)) return false;
          break;
        case OP_LOOP: {
          uint8_t n = rndRange(a[0], a[1]);
          if (n == 0 || sp >= 4) next = skipInstr(c, pc);
          else stack[sp++] = {next, n};
          break;
        }
        case OP_NEXT:
          if (sp) {
            if (--stack[sp - 1].left) next = stack[sp - 1].pc;
            else sp--;
          }
          break;
        case OP_CHANCE:
          if ((uint8_t)random(100) >= a[0]) next = skipInstr(c, next);
          break;
        case OP_PUSH:
          if (!push(speedOf(a[0]), a[1])) return false;
          break;
        case OP_HOME:
          if (!moveTo(0, speedOf(a[0]), a[1])) return false;
          break;
      }
      pc = next;
    }
    Serial.println(F("  [vm] Schrittlimit erreicht (Endlosschleife?)"));
    return true;
  }

  static float rndf(float lo, float hi) { return lo + (hi - lo) * (random(10001) / 10000.0f); }

 private:
  Guard guard_ = Guard::None;

  static uint8_t rndRange(uint8_t lo, uint8_t hi) {
    if (hi < lo) { uint8_t t = lo; lo = hi; hi = t; }
    return lo + random(hi - lo + 1);
  }
  float sloppy(uint8_t p) const { return p + (int)random(-(int)mood.sloppy, (int)mood.sloppy + 1); }
  uint32_t scaleWait(uint32_t ms) const { return (uint32_t)(ms * mood.patience / 100.0f * rndf(0.9f, 1.1f)); }

  static float applyEase(float t, uint8_t e) {
    switch (e) {
      case E_SMOOTH: return t * t * (3.0f - 2.0f * t);
      case E_OUT: { float u = 1.0f - t; return 1.0f - u * u * u; }
      case E_IN: return t * t;
      default: return t;
    }
  }
  // Verhältnis Spitzen- zu Durchschnittsgeschwindigkeit des Profils
  static float peakFactor(uint8_t e) {
    switch (e) {
      case E_SMOOTH: return 1.5f;
      case E_OUT: return 3.0f;
      case E_IN: return 2.0f;
      default: return 1.0f;
    }
  }
  // Überspringt einen Befehl; bei LOOP den ganzen Block bis zum passenden NEXT
  static uint16_t skipInstr(const uint8_t* c, uint16_t pc) {
    uint8_t op = c[pc];
    if (op == OP_END || op >= OP__COUNT) return pc;
    if (op != OP_LOOP) return pc + OP_LEN[op];
    int depth = 0;
    do {
      op = c[pc];
      if (op == OP_END || op >= OP__COUNT) return pc;
      if (op == OP_LOOP) depth++;
      else if (op == OP_NEXT) depth--;
      pc += OP_LEN[op];
    } while (depth > 0);
    return pc;
  }
};
