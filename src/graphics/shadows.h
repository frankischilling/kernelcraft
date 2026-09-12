#ifndef SHADOWS_H
#define SHADOWS_H

#include <GL/glew.h>
#include "../math/math.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
  GLuint vao;
  GLsizei indices;
} ShadowGeometry;

typedef struct {
  GLuint framebuffer, depth, program;
  GLint transformLocation;
  int size;
  Vec3 center, direction;
  float radius;
  Mat4 transform;
  bool valid;
} ShadowMap;

// A fixed sphere covers every possible caster/receiver, independent of camera
// culling. All resources belong to this object and require a current GL context.
bool initShadowMap(ShadowMap* map, Vec3 center, float radius);
void cleanupShadowMap(ShadowMap* map);
// Reuses an unchanged map. Restores the framebuffer and raster state it uses.
bool updateShadowMap(ShadowMap* map, Vec3 direction, bool edited, const ShadowGeometry* geometry, size_t count, int* drawCalls);

#endif
