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
#define SERVO_DETACH_IDLE_MS    600   // PWM abschalten, wenn Arm ruht (kein Brummen, weniger Strom)

// ---------------------------------------------------------------------
//  KLICK-LOGIK
// ---------------------------------------------------------------------
#define PUSH_CONFIRM_MS   250   // so lange nach dem Drücken auf "Schalter aus" warten
#define PUSH_RETRIES        2   // weitere Versuche, falls der Schalter nicht umgefallen ist
// Mehr Kraft: beim Drücken so viele µs ÜBER den PUSH-Punkt hinaus kommandieren.
// Der Servo drückt proportional zur Abweichung. Wird bei jedem Versuch größer (1×, 2×, 3×).
#define PUSH_OVERDRIVE_US  60
#define PUSH_WINDUP_POS    50   // Anlauf-Position (%) für den 2. und 3. Versuch

// ---------------------------------------------------------------------
//  CHARAKTER
// ---------------------------------------------------------------------
#define PERSONA_CHANCE_PCT   70     // Rest: freie Zufallskombination ("Freestyle")

// Gesamtlänge der Vorstellungen. Auch im seriellen Monitor änderbar ('v', 'k'),
// dort gesetzte Werte werden gespeichert und haben Vorrang.
#define GLOBAL_TEMPO_PCT    130     // alle Bewegungen schneller (100 = wie programmiert)
#define GLOBAL_PAUSE_PCT     60     // alle Pausen und Zitterzeiten kürzer (100 = wie programmiert)
#define ANNOY_WINDOW_MS      8000   // erneutes Einschalten innerhalb dieser Zeit nervt
#define ANNOY_STEP           25
#define ANNOY_RETRIGGER_STEP 35     // Schalter wieder an, während der Arm noch zurückfährt
#define ANNOY_DECAY_PER_S    2
#define ANNOY_GRUMPY_LEVEL   60
#define PEEK_CHANCE_PCT      20     // nach einer Aktion später nochmal "nachgucken"
#define PEEK_MIN_MS          4000
#define PEEK_MAX_MS          15000
