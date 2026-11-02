// Host-Tests für Bewegungs-Engine und Kalibrierfahrt (simulierte Zeit und Hardware).
// Bauen und ausführen: test/run_host_tests.sh
#include "Arduino.h"
#include "config.h"
#include "bytecode.h"
#include "motion.h"
#include "calibration.h"

static int failures = 0, checks = 0;
#define CHECK(cond)                                                          \
  do {                                                                       \
    checks++;                                                                \
    if (!(cond)) {                                                           \
      failures++;                                                            \
      std::printf("  FEHLER %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
    }                                                                        \
  } while (0)

static const Calib DEFAULT_CALIB = {CALIB_DEFAULT_HOME, CALIB_DEFAULT_LID, CALIB_DEFAULT_TOUCH, CALIB_DEFAULT_PUSH};

static float dutyToUs(uint32_t duty) { return duty * 20000.0f / (float)((1UL << SERVO_PWM_BITS) - 1); }

static bool sameCalib(const Calib& a, const Calib& b) {
  return a.home == b.home && a.lid == b.lid && a.touch == b.touch && a.push == b.push;
}

// Frische Maschine: Schalter AN, Arm in HOME, Zeit bei startMs
static void setup(Motion& M, uint32_t startMs, bool switchOn = true) {
  fake::reset(startMs);
  fake::switchLevel = switchOn ? SWITCH_ON_LEVEL : !SWITCH_ON_LEVEL;
  calib = DEFAULT_CALIB;
  M.begin();
  M.setGuard(Guard::None);
}

// ---------------------------------------------------------------------
//  #3 Deadline und millis()-Überlauf
// ---------------------------------------------------------------------
static void test_deadline_across_wrap_not_expired() {
  Motion M;
  setup(M, 0);
  fake::now = 0xFFFFFF00u;
  M.setDeadline(millis() + 1000);  // liegt nach dem Überlauf (numerisch klein)
  const uint32_t t = millis();
  bool ok = M.waitMs(100);
  CHECK(ok);
  CHECK(!M.timedOut);
  CHECK(millis() - t >= 100);
}

static void test_deadline_exactly_zero_expires() {
  Motion M;
  setup(M, 0);
  fake::now = 0xFFFFFFFFu - 499;
  M.setDeadline(millis() + 500);  // ergibt genau 0
  const uint32_t t = millis();
  bool ok = M.waitMs(2000);
  CHECK(!ok);
  CHECK(M.timedOut);
  const uint32_t el = millis() - t;
  CHECK(el >= 500 && el <= 510);
}

static void test_deadline_passed_across_wrap() {
  Motion M;
  setup(M, 0);
  fake::now = 5;
  M.setDeadline(0xFFFFFFF0u);  // 21 ms in der Vergangenheit
  const uint32_t t = millis();
  bool ok = M.waitMs(100);
  CHECK(!ok);
  CHECK(M.timedOut);
  CHECK(millis() - t < 10);
}

static void test_deadline_normal() {
  Motion M;
  setup(M, 1000);
  M.setDeadline(millis() + 50);
  CHECK(!M.waitMs(200));
  CHECK(M.timedOut);
  M.setDeadline(millis() + 500);
  CHECK(M.waitMs(100));
}

// push() darf bei genug Restzeit nicht auf Vollgas umschalten, auch über den Überlauf hinweg
static void test_push_keeps_speed_across_wrap() {
  Motion M;
  setup(M, 0);
  fake::now = 0xFFFFFF00u;
  M.testMode = true;
  M.pos = 85;
  M.servo.writeUs(Motion::posToUs(85));
  M.setDeadline(millis() + 5000);
  const size_t first = fake::writes.size();
  const uint32_t t0 = millis();
  const float pushUs = Motion::posToUs(100);
  M.push(50.0f, E_LIN);  // 15 % bei 50 %/s ≈ 300 ms
  uint32_t reached = 0;
  bool found = false;
  for (size_t i = first; i < fake::writes.size(); i++) {
    if (fabs(dutyToUs(fake::writes[i].duty) - pushUs) < 2.0f) {
      reached = fake::writes[i].t - t0;
      found = true;
      break;
    }
  }
  CHECK(found);
  CHECK(reached >= 250);
  CHECK(M.deadlineActive());  // Zeitgrenze nach dem Klick wiederhergestellt
}

static void test_timing_helpers() {
  CHECK(timeReached(100, 100));
  CHECK(timeReached(101, 100));
  CHECK(!timeReached(99, 100));
  CHECK(!timeReached(0xFFFFFF00u, 0x00000010u));
  CHECK(timeReached(0x00000010u, 0xFFFFFF00u));
  CHECK(timeLeft(0xFFFFFF00u, 0x00000010u) == 0x110u);
  CHECK(timeLeft(0x00000010u, 0xFFFFFF00u) == 0);
  CHECK(timeLeft(5, 5) == 0);
}

static void test_clear_deadline() {
  Motion M;
  setup(M, 1000);
  M.setDeadline(millis() + 10);
  M.clearDeadline();
  CHECK(!M.deadlineActive());
  CHECK(M.waitMs(100));
  CHECK(!M.timedOut);
}

// ---------------------------------------------------------------------
//  Kalibrierfahrt
// ---------------------------------------------------------------------
// Mechanik: Schalter fällt, sobald der Arm 1100 µs erreicht (zwischen TOUCH 1150 und PUSH 1000)
static const float FLIP_US = 1100.0f;
static void physicalSwitch(Motion& M) {
  fake::onTick = [&M]() {
    if (M.servo.us() <= FLIP_US) fake::switchLevel = !SWITCH_ON_LEVEL;
  };
}

static void test_calib_measures_flip_point() {
  Motion M;
  setup(M, 1000);
  physicalSwitch(M);
  float flip = 0;
  SweepResult r = measureSwitchPoint(M, flip);
  CHECK(r == SweepResult::Flipped);
  CHECK(flip <= FLIP_US && flip >= FLIP_US - 15.0f);
  CHECK(calib.push == (uint16_t)(flip - AUTO_PUSH_EXTRA_DEG * SERVO_US_PER_DEG));
  CHECK(calib.home == DEFAULT_CALIB.home && calib.lid == DEFAULT_CALIB.lid);
}

// #2: Mensch schaltet während der schnellen Anfahrt aus -> Kalibrierung bleibt
static void test_calib_user_switches_off_during_approach() {
  Motion M;
  setup(M, 1000);
  const uint32_t t0 = millis();
  fake::onTick = [t0]() {
    if (millis() - t0 >= 40) fake::switchLevel = !SWITCH_ON_LEVEL;
  };
  float flip = 0;
  SweepResult r = measureSwitchPoint(M, flip);
  CHECK(r == SweepResult::SwitchOff);
  CHECK(flip == 0.0f);
  CHECK(sameCalib(calib, DEFAULT_CALIB));
}

// #2: Schalter fällt (von Hand) kurz bevor die Messstrecke beginnt
static void test_calib_user_switches_off_just_before_sweep() {
  Motion M;
  setup(M, 1000);
  // Anfahrt 2000 -> ~1307 µs dauert bei vollem Tempo gut 300 ms
  const uint32_t t0 = millis();
  fake::onTick = [t0]() {
    if (millis() - t0 >= 250) fake::switchLevel = !SWITCH_ON_LEVEL;
  };
  float flip = 0;
  SweepResult r = measureSwitchPoint(M, flip);
  CHECK(r == SweepResult::SwitchOff);
  CHECK(sameCalib(calib, DEFAULT_CALIB));
}

// #2 (Review): Ausschalten kurz vor Messbeginn, Entprellung noch nicht abgeschlossen
static void test_calib_user_switches_off_within_debounce() {
  Motion M;
  setup(M, 1000);
  const float startUs = CALIB_DEFAULT_TOUCH + FIRST_RUN_SWEEP_START_DEG * SERVO_US_PER_DEG;  // Richtung abwärts
  static uint32_t arrivedAt;
  arrivedAt = 0;
  fake::onTick = [&M, startUs]() {
    if (!arrivedAt && M.servo.us() <= startUs + 0.5f) arrivedAt = millis();
    // Anfahrt endet 80 ms nach Ankunft; 70 ms danach ausschalten -> weniger als DEBOUNCE_MS vor Messbeginn
    if (arrivedAt && millis() - arrivedAt >= 70) fake::switchLevel = !SWITCH_ON_LEVEL;
  };
  float flip = 0;
  SweepResult r = measureSwitchPoint(M, flip);
  CHECK(arrivedAt != 0);
  CHECK(r == SweepResult::SwitchOff);
  CHECK(sameCalib(calib, DEFAULT_CALIB));
}

static void test_calib_not_reached() {
  Motion M;
  setup(M, 1000);
  float flip = 0;
  SweepResult r = measureSwitchPoint(M, flip);  // Schalter fällt nie
  CHECK(r == SweepResult::NotReached);
  CHECK(sameCalib(calib, DEFAULT_CALIB));
}

// #1: Mit SPEED_LIMIT_PCT_S > 0 hält auch die Kalibrierfahrt das Tempolimit ein
static void test_calib_respects_speed_limit() {
  if (!(SPEED_LIMIT_PCT_S > 0.0f)) {
    std::printf("  (Tempolimit-Test übersprungen: SPEED_LIMIT_PCT_S = 0)\n");
    return;
  }
  Motion M;
  setup(M, 1000);
  physicalSwitch(M);
  const float vmaxPct = SPEED_LIMIT_PCT_S < SERVO_MAX_SPEED_PCT_S ? SPEED_LIMIT_PCT_S : SERVO_MAX_SPEED_PCT_S;
  const float usPerMs = vmaxPct / 100.0f * fabs((float)calib.push - (float)calib.home) / 1000.0f;
  fake::writes.clear();
  fake::writes.push_back({millis(), (uint32_t)(M.servo.us() * ((1UL << SERVO_PWM_BITS) - 1) / 20000.0f + 0.5f)});
  float flip = 0;
  measureSwitchPoint(M, flip);
  float worst = 0;
  for (size_t i = 1; i < fake::writes.size(); i++) {
    const float dUs = fabs(dutyToUs(fake::writes[i].duty) - dutyToUs(fake::writes[i - 1].duty));
    const float allowed = usPerMs * (fake::writes[i].t - fake::writes[i - 1].t) + 3.0f;
    if (dUs - allowed > worst) worst = dUs - allowed;
  }
  if (worst > 0) std::printf("  Tempolimit um %.0f µs überschritten\n", worst);
  CHECK(worst <= 0.0f);
}

int main() {
  std::printf("Host-Tests (SPEED_LIMIT_PCT_S = %.0f)\n", (double)SPEED_LIMIT_PCT_S);
  test_deadline_across_wrap_not_expired();
  test_deadline_exactly_zero_expires();
  test_deadline_passed_across_wrap();
  test_deadline_normal();
  test_push_keeps_speed_across_wrap();
  test_timing_helpers();
  test_clear_deadline();
  test_calib_measures_flip_point();
  test_calib_user_switches_off_during_approach();
  test_calib_user_switches_off_just_before_sweep();
  test_calib_user_switches_off_within_debounce();
  test_calib_not_reached();
  test_calib_respects_speed_limit();
  std::printf("%d Prüfungen, %d Fehler\n", checks, failures);
  return failures ? 1 : 0;
}
