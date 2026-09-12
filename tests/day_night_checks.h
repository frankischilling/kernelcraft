#include "world/day_night.h"

static void test_day_night(void) {
  DayNightClock a, b;
  initDayNight(&a);
  initDayNight(&b);
  advanceDayNight(&a, 0, true);
  advanceDayNight(&b, 0, true);
  for (int i = 0; i < 60; i++)
    advanceDayNight(&a, 1.0 / 60.0, true);
  for (int i = 0; i < 20; i++)
    advanceDayNight(&b, 0.05, true);
  CHECK(a.tick == DAY_NIGHT_START_TICK + 20 && a.tick == b.tick);
  advanceDayNight(&a, 0.013, true);
  double beforePause = dayNightPhase(&a);
  advanceDayNight(&a, 3600, false);
  unsigned paused = a.tick;
  advanceDayNight(&a, 3600, true);
  CHECK(a.tick == paused && dayNightPhase(&a) == beforePause);
  advanceDayNight(&a, 3600, true);
  CHECK(a.tick == paused + 2);
  paused = a.tick;
  advanceDayNight(&a, NAN, true);
  advanceDayNight(&a, INFINITY, true);
  advanceDayNight(&a, -1, true);
  CHECK(a.tick == paused);
  a.tick = DAY_NIGHT_TICKS_PER_DAY - 1;
  a.remainder = 0;
  advanceDayNight(&a, 0.1, true);
  CHECK(a.tick == 1 && fabs(dayNightPhase(&a) - 1) < 0.001);
  // A full cycle wraps without drift at either frame rate.
  initDayNight(&a);
  advanceDayNight(&a, 0, true);
  for (int i = 0; i < 72000; i++)
    advanceDayNight(&a, 1.0 / 60.0, true);
  CHECK(a.tick == DAY_NIGHT_START_TICK);

  DayNightState noon = sampleDayNight(0.25), midnight = sampleDayNight(0.75);
  DayNightState dawn = sampleDayNight(0), dusk = sampleDayNight(0.5);
  CHECK(noon.day == 1 && noon.stars == 0 && noon.sunDirection.y > 0.99f);
  CHECK(midnight.night == 1 && midnight.stars == 1 && midnight.moonDirection.y > 0.99f);
  CHECK(dawn.twilight == 1 && dusk.twilight == 1);
  CHECK(noon.skyFill.x > midnight.skyFill.x * 3 && midnight.groundFill.x > 0);
  for (int i = -1000; i <= 1000; i++) {
    DayNightState state = sampleDayNight(i / 1000.0);
    DayNightState next = sampleDayNight(i / 1000.0 + 0.000001);
    CHECK(fabsf(state.day + state.twilight + state.night - 1) < 0.00001f);
    CHECK(state.day >= 0 && state.twilight >= 0 && state.night >= 0);
    CHECK(fabsf(state.sunDirection.y + state.moonDirection.y) < 0.00001f);
    CHECK(fabsf(vec3_dot(&state.sunDirection, &state.sunDirection) - 1) < 0.00001f);
    CHECK(fabsf(next.day - state.day) < 0.001f && fabsf(next.night - state.night) < 0.001f);
    CHECK(fabsf(next.lightColor.x - state.lightColor.x) < 0.001f);
    CHECK(state.stars == 0 || state.sunDirection.y < 0);
  }
  CHECK(isfinite(sampleDayNight(NAN).sunDirection.y));
  // A new moon must stop emitting direct light, while ambient fill stays readable.
  DayNightState newMoon = sampleDayNight(4.75);
  CHECK(newMoon.lightColor.x == 0 && newMoon.lightColor.y == 0 && newMoon.lightColor.z == 0);
  CHECK(newMoon.skyFill.x == midnight.skyFill.x && newMoon.groundFill.x == midnight.groundFill.x);
  // Completing four days reaches new moon; another four returns to full.
  initDayNight(&a);
  advanceDayNight(&a, 0, true);
  for (int day = 1; day <= 8; day++) {
    a.tick = 23999;
    a.remainder = 0;
    advanceDayNight(&a, 0.05, true);
    CHECK(a.tick == 0 && dayNightPhase(&a) == day % 8);
  }
  const float light[] = {1, 0.85f, 0.5f, 0.15f, 0, 0.15f, 0.5f, 0.85f};
  for (int day = 0; day < 8; day++) {
    DayNightState state = sampleDayNight(day + 0.75);
    CHECK((int)state.moonPhase == day && state.moonIllumination == light[day]);
    CHECK(fabsf(state.lightColor.z - 0.16f * light[day]) < 0.00001f);
    CHECK(sampleDayNight(day + 0.25).lightColor.x == noon.lightColor.x);
    CHECK(sampleDayNight(day + 8.75).moonPhase == state.moonPhase);
    CHECK(sampleDayNight(day - 7.25).moonPhase == state.moonPhase);
    for (int tick = 0; tick < 12000; tick++)
      advanceDayNight(&a, 0.1, true);
    CHECK(a.tick == 0 && a.moonPhase == (day + 1) % 8);
    double before = dayNightPhase(&a);
    advanceDayNight(&a, 3600, false);
    advanceDayNight(&a, 3600, true);
    CHECK(dayNightPhase(&a) == before);
  }
  CHECK(sampleDayNight(-1e-20).moonPhase == MOON_FULL);
  CHECK(sampleDayNight(INFINITY).moonPhase == MOON_FULL);
  CHECK(isfinite(sampleDayNight(1e300).moonIllumination));
  puts("Day/night timing, wraparound, pauses, palettes, and orbit checks finished");
}
