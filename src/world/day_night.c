#include "day_night.h"

void initDayNight(DayNightClock* clock) {
  *clock = (DayNightClock){.tick = DAY_NIGHT_START_TICK};
}

void advanceDayNight(DayNightClock* clock, double seconds, bool active) {
  bool resumed = active && !clock->active;
  clock->active = active;
  if (!active || resumed) {
    return;
  }
  if (!isfinite(seconds) || seconds <= 0)
    return;
  clock->remainder += fmin(seconds, 0.1) * DAY_NIGHT_TICKS_PER_SECOND;
  unsigned ticks = (unsigned)floor(clock->remainder + 1e-10);
  clock->remainder = fmax(0, clock->remainder - ticks);
  clock->tick = (clock->tick + ticks) % DAY_NIGHT_TICKS_PER_DAY;
}

double dayNightPhase(const DayNightClock* clock) {
  return (clock->tick + clock->remainder) / DAY_NIGHT_TICKS_PER_DAY;
}

static Vec3 blendFill(Vec3 day, Vec3 dusk, Vec3 night, DayNightState state) {
  return (Vec3){day.x * state.day + dusk.x * state.twilight + night.x * state.night, day.y * state.day + dusk.y * state.twilight + night.y * state.night,
                day.z * state.day + dusk.z * state.twilight + night.z * state.night};
}

DayNightState sampleDayNight(double phase) {
  if (!isfinite(phase))
    phase = (double)DAY_NIGHT_START_TICK / DAY_NIGHT_TICKS_PER_DAY;
  phase -= floor(phase);
  double angle = phase * 6.283185307179586;
  DayNightState state = {0};
  state.sunDirection = (Vec3){(float)cos(angle), (float)sin(angle), 0};
  vec3_scale(&state.moonDirection, &state.sunDirection, -1);
  float altitude = state.sunDirection.y;
  state.day = smoothstep(0, 0.35f, altitude);
  state.night = smoothstep(0, 0.35f, -altitude);
  state.twilight = 1 - state.day - state.night;
  state.stars = state.night;
  state.lightDirection = altitude >= 0 ? state.sunDirection : state.moonDirection;
  // Both lights go to zero at the horizon, so switching the key direction
  // between opposite bodies never causes a discontinuity in face brightness.
  float sun = smoothstep(0, 0.2f, altitude);
  float moon = smoothstep(0, 0.2f, -altitude);
  state.lightColor = (Vec3){0.62f * sun + 0.055f * moon, 0.60f * sun + 0.06f * moon, 0.56f * sun + 0.09f * moon};
  state.skyFill = blendFill((Vec3){0.36f, 0.39f, 0.44f}, (Vec3){0.24f, 0.13f, 0.16f}, (Vec3){0.045f, 0.035f, 0.075f}, state);
  state.groundFill = blendFill((Vec3){0.18f, 0.16f, 0.14f}, (Vec3){0.10f, 0.055f, 0.07f}, (Vec3){0.022f, 0.018f, 0.035f}, state);
  return state;
}
