#pragma once
// Minimaler Arduino-Ersatz für Host-Tests (simulierte Zeit, Schalter, Servo-PWM).
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <vector>

#define CONFIG_IDF_TARGET_ESP32S3 1
#define ESP_ARDUINO_VERSION_MAJOR 3
#define HIGH 1
#define LOW 0
#define INPUT 0x01
#define OUTPUT 0x03
#define INPUT_PULLUP 0x05
#define F(s) (s)

using std::fabs;
using std::abs;

template <class T, class L, class H>
inline T constrain(T x, L lo, H hi) { return x < (T)lo ? (T)lo : (x > (T)hi ? (T)hi : x); }

namespace fake {
struct Write { uint32_t t; uint32_t duty; };
inline uint32_t now = 0;
inline int switchLevel = HIGH;           // Rohpegel am Schalter-Pin
inline std::function<void()> onTick;     // nach jeder simulierten ms
inline std::vector<Write> writes;        // alle ledcWrite-Aufrufe
inline bool quiet = true;
inline unsigned long rng = 12345;
inline void reset(uint32_t start) {
  now = start; switchLevel = HIGH; onTick = nullptr; writes.clear(); rng = 12345;
}
}  // namespace fake

inline uint32_t millis() { return fake::now; }
inline void delay(uint32_t ms) {
  for (uint32_t i = 0; i < ms; i++) {
    fake::now++;
    if (fake::onTick) fake::onTick();
  }
}
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return fake::switchLevel; }
inline bool ledcAttach(int, uint32_t, uint8_t) { return true; }
inline bool ledcDetach(int) { return true; }
inline bool ledcWrite(int, uint32_t duty) { fake::writes.push_back({fake::now, duty}); return true; }

inline long random(long n) {
  fake::rng = fake::rng * 1103515245UL + 12345UL;
  return n > 0 ? (long)((fake::rng >> 8) % (unsigned long)n) : 0;
}
inline long random(long lo, long hi) { return hi > lo ? lo + random(hi - lo) : lo; }

struct FakeSerial {
  void printf(const char* fmt, ...) {
    if (fake::quiet) return;
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
  }
  void print(const char* s) { if (!fake::quiet) fputs(s, stdout); }
  void println(const char* s = "") { if (!fake::quiet) puts(s); }
};
inline FakeSerial Serial;
