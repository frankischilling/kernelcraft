#ifndef DAY_NIGHT_H
#define DAY_NIGHT_H

#include "../math/math.h"
#include <stdbool.h>

#define DAY_NIGHT_TICKS_PER_SECOND 20
#define DAY_NIGHT_TICKS_PER_DAY 24000
#define DAY_NIGHT_START_TICK 3000

typedef struct {
  unsigned tick;
  double remainder;
  bool active;
} DayNightClock;

typedef struct {
  Vec3 sunDirection, moonDirection;
  Vec3 lightDirection, lightColor, skyFill, groundFill;
  float day, twilight, night, stars;
} DayNightState;

void initDayNight(DayNightClock* clock);
// Pause preserves the current sub-tick phase; the first resumed frame discards
// elapsed pause time. Active frame stalls advance by at most 0.1 seconds.
void advanceDayNight(DayNightClock* clock, double seconds, bool active);
double dayNightPhase(const DayNightClock* clock);
DayNightState sampleDayNight(double phase);

#endif
