#pragma once
// =====================================================================
//  calibration.h – Kalibrierung prüfen und Schaltpunkt messen
//  (ohne NVS/Serielles Menü, damit es auf dem Host testbar ist)
// =====================================================================
#include "config.h"
#include "motion.h"

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

// µs -> Position 0..100 % (Umkehrung von Motion::posToUs, >100 % hinter PUSH)
static float usToPos(float us) {
  const float h = calib.home, l = calib.lid, t = calib.touch, p = calib.push;
  const float dir = p > h ? 1.0f : -1.0f;
  const float d = (us - h) * dir;  // Abstand ab HOME Richtung Schalter
  if (d <= (l - h) * dir) return P_LID * (us - h) / (l - h);
  if (d <= (t - h) * dir) return P_LID + (P_TOUCH - P_LID) * (us - l) / (t - l);
  return P_TOUCH + (100.0f - P_TOUCH) * (us - t) / (p - t);
}

// Ergebnis der Messfahrt
enum class SweepResult : uint8_t {
  Flipped,     // Schalter fiel während der Messstrecke, Messwert ausgewertet
  NotReached,  // Messstrecke ohne Umschalten zu Ende gefahren
  SwitchOff    // Schalter war vor Beginn der Messstrecke schon aus (von Hand)
};

// Fährt bis kurz vor den Hebel und dann langsam weiter, bis der Schalter umfällt.
// Bei gültigem Messwert werden TOUCH und PUSH in calib ersetzt.
// flipUs bekommt den gemessenen Umschaltpunkt (0 = keiner).
static SweepResult measureSwitchPoint(Motion& M, float& flipUs) {
  const float dir = calib.push > calib.home ? 1.0f : -1.0f;
  const float perDeg = SERVO_US_PER_DEG;
  float start = calib.touch - dir * FIRST_RUN_SWEEP_START_DEG * perDeg;
  if ((start - calib.lid) * dir < 0.0f) start = calib.lid;
  const float limit =
      constrain(calib.push + dir * FIRST_RUN_SWEEP_EXTRA_DEG * perDeg, (float)SERVO_US_MIN, (float)SERVO_US_MAX);
  flipUs = 0;

  // 1) zügig bis kurz vor den Hebel – über die Bewegungs-Engine, damit
  //    SPEED_LIMIT_PCT_S gilt. Schaltet der Mensch dabei aus: abbrechen.
  M.pos = usToPos(M.servo.us());
  M.setGuard(Guard::AbortIfOff);
  const bool arrived = M.moveTo(usToPos(start), 1e6f, E_LIN, 100.0f) && M.waitMs(80);
  M.setGuard(Guard::None);
  M.sw.update();
  // auch ein noch nicht entprelltes AUS zählt, sonst würde es in der Messstrecke als Schaltpunkt erkannt
  if (!arrived || !M.sw.isOn() || !M.sw.rawOn()) {
    M.lastMotion = millis();
    return SweepResult::SwitchOff;  // nur ein Wechsel während der Messstrecke zählt
  }

  // 2) langsam weiter, bis der Schalter umfällt
  float sweepUsPerS = FIRST_RUN_SWEEP_US_PER_S;
  if (SPEED_LIMIT_PCT_S > 0.0f) {
    const float limUsPerS = SPEED_LIMIT_PCT_S / 100.0f * fabs((float)calib.push - (float)calib.home);
    if (limUsPerS < sweepUsPerS) sweepUsPerS = limUsPerS;
  }
  const float durMs = fabs(limit - start) / sweepUsPerS * 1000.0f;
  const uint32_t s0 = millis();
  for (;;) {
    float t = durMs > 0.0f ? (millis() - s0) / durMs : 1.0f;
    if (t > 1.0f) t = 1.0f;
    float us = start + (limit - start) * t;
    M.servo.writeUs(us);
    M.sw.update();
    if (!M.sw.isOn()) { flipUs = us; break; }
    if (t >= 1.0f) break;
    delay(10);
  }
  M.lastMotion = millis();

  if (flipUs <= 0.0f) return SweepResult::NotReached;

  Calib c = calib;
  c.push = (uint16_t)(flipUs + dir * AUTO_PUSH_EXTRA_DEG * perDeg);
  c.touch = (uint16_t)(flipUs - dir * AUTO_TOUCH_BEFORE_DEG * perDeg);
  if ((c.touch - (float)c.lid) * dir <= 0.0f) c.touch = (uint16_t)((c.lid + flipUs) / 2.0f);
  Serial.printf("  Schalter fiel bei %.0f µs\n", flipUs);
  if (calibValid(c)) {
    calib = c;
    Serial.printf("  Aktiv:  HOME %u | DECKEL %u | TOUCH %u | PUSH %u µs\n", calib.home, calib.lid, calib.touch,
                  calib.push);
  } else {
    Serial.println(F("  Messwert unplausibel – gespeicherte Werte bleiben"));
  }
  M.pos = usToPos(flipUs);
  M.servo.writeUs(Motion::posToUs(M.pos));
  return SweepResult::Flipped;
}
