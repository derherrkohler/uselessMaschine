#pragma once
// =====================================================================
//  timing.h – überlaufsichere Zeitvergleiche für millis()
//  millis() läuft nach etwa 49,7 Tagen über. Vergleiche daher nie absolut
//  (now >= at), sondern über die vorzeichenbehaftete Differenz. Das gilt,
//  solange beide Zeitpunkte weniger als ~24,8 Tage auseinanderliegen.
// =====================================================================
#include <stdint.h>

// true, wenn now den Zeitpunkt at erreicht oder überschritten hat
static inline bool timeReached(uint32_t now, uint32_t at) { return (int32_t)(now - at) >= 0; }

// verbleibende ms bis at (0, wenn schon erreicht)
static inline uint32_t timeLeft(uint32_t now, uint32_t at) { return timeReached(now, at) ? 0 : at - now; }
