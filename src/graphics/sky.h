#ifndef SKY_H
#define SKY_H

#include <GL/glew.h>
#include "camera.h"
#include "../world/day_night.h"

typedef struct {
  GLuint program, vao, textures[4 + MOON_PHASE_COUNT];
  GLint front, right, up, scale, weights, sun, moon, stars, moonIllumination;
} SkyRenderer;

// Initialize a zeroed renderer, preserving program/texture bindings, and destroy
// it with the context current.
bool initSky(SkyRenderer* sky);
void cleanupSky(SkyRenderer* sky);
// Draw after opaque terrain, before selection/clouds/HUD. The depth buffer must
// be cleared to 1. No depth writes; camera translation has no effect.
void renderSky(const SkyRenderer* sky, const Camera* camera, float aspect, const DayNightState* state);

#endif
