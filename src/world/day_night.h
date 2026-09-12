#ifndef DAY_NIGHT_H
#define DAY_NIGHT_H

#include "../math/math.h"
#include <stdbool.h>

#define DAY_NIGHT_TICKS_PER_SECOND 20
#define DAY_NIGHT_TICKS_PER_DAY 24000
#define DAY_NIGHT_START_TICK 3000

// One supplied image per game day, starting with full moon on each launch.
typedef enum {
  MOON_FULL,
  MOON_WANING_GIBBOUS,
  MOON_LAST_QUARTER,
  MOON_WANING_CRESCENT,
  MOON_NEW,
  MOON_WAXING_CRESCENT,
  MOON_FIRST_QUARTER,
  MOON_WAXING_GIBBOUS,
  MOON_PHASE_COUNT
} MoonPhase;

typedef struct {
  unsigned tick;
  MoonPhase moonPhase;
  double remainder;
  bool active;
} DayNightClock;

typedef struct {
  Vec3 sunDirection, moonDirection;
  Vec3 lightDirection, lightColor, skyFill, groundFill;
  float day, twilight, night, stars;
  MoonPhase moonPhase;
  float moonIllumination;
} DayNightState;

void initDayNight(DayNightClock* clock);
// Pause preserves the current sub-tick phase; the first resumed frame discards
// elapsed pause time. Active frame stalls advance by at most 0.1 seconds.
void advanceDayNight(DayNightClock* clock, double seconds, bool active);
// Days within the eight-day lunar cycle; the fractional part is solar time.
double dayNightPhase(const DayNightClock* clock);
DayNightState sampleDayNight(double phase);
const char* moonPhaseName(MoonPhase phase);

#endif
