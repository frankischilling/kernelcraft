#ifndef CLOUDS_H
#define CLOUDS_H

#include <GL/glew.h>
#include "camera.h"
#include "../world/day_night.h"

typedef struct {
  GLuint program, vao;
  GLint front, right, up, scale, origin, projection, weights;
  double offset;
  bool active;
} CloudRenderer;

bool initClouds(CloudRenderer* clouds);
void cleanupClouds(CloudRenderer* clouds);
void advanceClouds(CloudRenderer* clouds, double seconds, bool active);
// Draw after terrain and selection, before the HUD. Uses terrain depth without writing it.
void renderClouds(const CloudRenderer* clouds, const Camera* camera, float aspect, const Mat4 projection, const DayNightState* state);

#endif
