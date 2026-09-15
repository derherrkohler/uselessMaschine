#pragma once
// =====================================================================
//  config.h – alles, was du an deine Hardware anpassen musst
// =====================================================================
#include <Arduino.h>

// ---------------------------------------------------------------------
//  PINS
//  Die Nummern sind GPIO-Nummern des Chips (nicht Pin-Positionen).
//  Auf den SuperMini-Boards steht meist direkt die GPIO-Nummer auf der
//  Platine ("4", "GP4", "IO4" ...). Siehe README für Alternativen.
// ---------------------------------------------------------------------
#if !CONFIG_IDF_TARGET_ESP32S3
  #error "Board auf \"ESP32S3 Dev Module\" stellen – dieses Projekt ist für den ESP32-S3 SuperMini."
#endif

#define PIN_SERVO   5   // Servo-Signal (orange) über 330 Ω
#define PIN_SWITCH  4   // Kippschalter gegen GND
#define PIN_RGB_LED 48  // WS2812 auf dem S3 SuperMini – wird dauerhaft ausgeschaltet

// ---------------------------------------------------------------------
//  SCHALTER
//  Standard: Schalter zieht den GPIO nach GND, wenn er "AN" ist.
//  Der interne Pull-up genügt. Bei Störungen (lange Kabel neben dem Servo):
//  zusätzlich 10 kΩ nach 3V3 und/oder 100 nF nach GND.
// ---------------------------------------------------------------------
#define SWITCH_ON_LEVEL             LOW
#define SWITCH_USE_INTERNAL_PULLUP  true
#define DEBOUNCE_MS                 30

// ---------------------------------------------------------------------
//  SERVO-KALIBRIERUNG
//  Wird mit montiertem Arm im seriellen Monitor eingestellt und im ESP
//  gespeichert (README, Abschnitt 8). Ohne gespeicherte Kalibrierung sendet
//  der ESP KEINE Servo-Pulse – der Arm bleibt liegen.
//  Positionen im Code sind 0..100 %:
//    0  = HOME  : Arm ruht in der Box, kurz vor dem Anschlag     (Befehl 'H')
//    30 = LID   : Arm berührt den Deckel von innen, Deckel noch zu (Befehl 'D')
//    90 = TOUCH : Arm berührt den Hebel, schaltet aber noch NICHT  (Befehl 'T')
//    100= PUSH  : Arm hat den Schalter sicher umgelegt            (Befehl 'P')
//  Die Drehrichtung ergibt sich automatisch aus den Werten.
// ---------------------------------------------------------------------
// Optional: feste Werte (µs) statt gespeicherter Kalibrierung. Gilt nur,
// wenn CALIB_USE_DEFAULTS true ist UND im ESP nichts gespeichert ist.
#define CALIB_USE_DEFAULTS   false
#define CALIB_DEFAULT_HOME   2000
#define CALIB_DEFAULT_LID    1750
#define CALIB_DEFAULT_TOUCH  1150
#define CALIB_DEFAULT_PUSH   1000

#define SERVO_US_MID         1500    // ≈ 90°: erster Puls beim Kalibrieren ('m')
#define SERVO_US_PER_DEG     10.5f   // SG90 ≈ 1900 µs / 180° – nur für die Gradanzeige
#define JOG_US_PER_S         250.0f  // Tempo beim Kalibrieren (≈ 24°/s)

// Auto-Kalibrierung ('a'): Arm fährt von HOME langsam los, bis der Schalter umfällt
#define AUTO_US_PER_S         120.0f  // Suchtempo (≈ 11°/s)
#define AUTO_MAX_DEG          150     // so weit wird maximal gesucht
#define AUTO_PUSH_EXTRA_DEG   4       // PUSH  = Umschaltpunkt + 4°
#define AUTO_TOUCH_BEFORE_DEG 8       // TOUCH = Umschaltpunkt − 8°
#define AUTO_LID_FRACTION     0.3f    // DECKEL ohne Enter: 30 % des Wegs HOME→TOUCH
#define HOME_BACKOFF_US       25      // HOME etwas vom Anschlag weg (≈ 2–3°), gegen Brummen

// Kalibrierfahrt: Die erste Aktion nach dem Einschalten misst den Schaltpunkt neu
// (passt sich an Batteriestand und Mechanik an). PUSH/TOUCH gelten bis zum Ausschalten,
// HOME und DECKEL kommen aus der gespeicherten Kalibrierung.
#define CALIBRATE_ON_FIRST_RUN      true
#define FIRST_RUN_SWEEP_START_DEG   15      // zügig bis so weit vor TOUCH, ab da langsam
#define FIRST_RUN_SWEEP_EXTRA_DEG   30      // so weit hinter dem alten PUSH wird maximal gesucht
#define FIRST_RUN_SWEEP_US_PER_S    200.0f  // Suchtempo (≈ 19°/s)
#define SERVO_US_MIN          500    // harte Sicherheitsgrenzen
#define SERVO_US_MAX         2500

#define SERVO_PWM_BITS     14   // 14 Bit @ 50 Hz ≈ 1,2 µs Auflösung (S3 kann max. 14)
#define SERVO_LEDC_CHANNEL  0   // nur für Arduino-Core 2.x relevant

// Wie schnell der Servo physikalisch maximal ist, in "% des Hubs pro Sekunde".
// SG90 bei 4,5 V mit Deckel-Last ≈ 0,13 s / 60°. Bei ~150° Hub ≈ 300 %/s. Lieber etwas zu niedrig.
#define SERVO_MAX_SPEED_PCT_S   300.0f

// Leerweg HOME..LID (Arm unsichtbar in der Box): langsame Bewegungen fahren
// diesen Abschnitt mindestens mit diesem Tempo, damit kein "totes" Warten entsteht.
// 0 = aus (z. B. Box ohne Deckel).
#define LID_FAST_TRAVEL_PCT_S   120.0f

// Notbremse gegen Brownouts: >0 begrenzt ALLE Bewegungen auf diese Geschwindigkeit
// (keine Vollgas-Sprünge mehr). Z. B. 200.0f, wenn der ESP beim Anfahren resettet.
#define SPEED_LIMIT_PCT_S       0.0f

#define MOTION_STEP_MS          15    // Interpolations-Takt
#define SERVO_DETACH_IDLE_MS   1500   // PWM abschalten, wenn Arm ruht (kein Brummen, weniger Strom)

// ---------------------------------------------------------------------
//  KLICK-LOGIK
// ---------------------------------------------------------------------
#define PUSH_CONFIRM_MS   150   // so lange nach dem Drücken auf "Schalter aus" warten (endet sofort, wenn er fällt)
// "Schaff ich nicht – nochmal, schneller und weiter!": jeder weitere Versuch
// ärgert sich kurz (Schütteln), holt weiter aus und drückt weiter über PUSH hinaus.
#define PUSH_RETRIES        3   // weitere Versuche, falls der Schalter nicht umgefallen ist
#define PUSH_ANGRY_SHAKES   2   // Schüttler nach einem Fehlversuch (0 = aus)
// Mehr Kraft: beim Drücken so viele µs ÜBER den PUSH-Punkt hinaus kommandieren.
// Der Servo drückt proportional zur Abweichung. Wird bei jedem Versuch größer (1×, 2×, 3×).
#define PUSH_OVERDRIVE_US  60
#define PUSH_FURTHER_US    40   // jeder weitere Versuch fährt so viele µs weiter über PUSH hinaus
#define PUSH_OVERDRIVE_MAX_US 300  // Obergrenze für Weiterfahren + Übersteuern zusammen,
                                   // damit der Arm nach dem Umkippen nicht anschlägt
#define PUSH_WINDUP_POS    70   // Anlauf-Position (%) für den 2. und 3. Versuch (kleiner = mehr Schwung, dauert länger)
#define PUSH_WINDUP_STEP   20   // jeder weitere Versuch holt so viel % weiter aus (70 %, dann 50 %)
#define PUSH_WINDUP_PAUSE_MS 60 // kurze Pause am Anlaufpunkt, bevor erneut gedrückt wird
// Hat keiner der Versuche geklappt: so lange warten und dann von selbst neu anfangen
// (0 = erst wieder, wenn der Schalter von Hand ausgeschaltet wurde)
#define BLOCKED_RETRY_MS 4000

// ---------------------------------------------------------------------
//  ZEITBUDGET – harte Obergrenzen je Aktion
//  Gesten werden vorab automatisch gestrafft (kürzere Pausen, schnellere
//  Bewegungen); reicht das nicht, bricht die Phase ab.
//  Ungünstigster Fall: PRE + KLICK + Klick selbst (≤ ~0,65 s) + RETURN ≈ 4 s
// ---------------------------------------------------------------------
#define PRE_BUDGET_MS       1900  // Reaktion + Anfahrt + Theater
#define KLICK_BUDGET_MS      450  // Klick-Geste bis zum Drücken
#define RETURN_MAX_MS       1000  // Rückzug inkl. Ankunft in HOME

// ---------------------------------------------------------------------
//  CHARAKTER
// ---------------------------------------------------------------------

// Gesamtlänge der Vorstellungen. Auch im seriellen Monitor änderbar ('v', 'k'),
// dort gesetzte Werte werden gespeichert und haben Vorrang.
#define GLOBAL_TEMPO_PCT    130     // alle Bewegungen schneller (100 = wie programmiert)
#define GLOBAL_PAUSE_PCT     60     // alle Pausen und Zitterzeiten kürzer (100 = wie programmiert)
#define ANNOY_WINDOW_MS      8000   // erneutes Einschalten innerhalb dieser Zeit nervt
#define ANNOY_STEP           35     // 2. schnelles Umschalten -> sauer (ab ANNOY_GRUMPY_LEVEL)
#define ANNOY_RETRIGGER_STEP 35     // Schalter wieder an, während der Arm noch zurückfährt
#define ANNOY_DECAY_PER_S    2
#define ANNOY_GRUMPY_LEVEL   60
