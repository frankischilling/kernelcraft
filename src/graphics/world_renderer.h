#ifndef WORLD_RENDERER_H
#define WORLD_RENDERER_H

#include <GL/glew.h>
#include "camera.h"
#include "../world/day_night.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
  bool success;
  int surfaceBlocks; // Blocks represented by submitted meshes, not pixel visibility.
  int chunksConsidered, chunksRendered, chunksRebuilt;
  int chunksOccluded;                        // Bounds proven hidden in this frame's software depth buffer.
  int terrainDrawCalls;                      // Excludes the chunk grid and overlays.
  size_t submittedQuads, submittedTriangles; // Merged rectangles, not unit block faces.
  double meshUpdateMilliseconds;             // CPU mesh construction and GL submission; no GPU wait.
} RenderResult;

// These functions require a current GL context. The caller owns the shader.
bool initWorld(GLuint shaderProgram);
// Updates uniforms only; the clock never invalidates chunk meshes or culling.
void setWorldDayNight(const DayNightState* state);
// Solid terrain uses current-view conservative occlusion; wireframe bypasses it.
// The caller clears depth and draws opaque terrain with ordinary depth testing.
// Reuse the CPU draw list only while the camera, projection, viewport, world and
// mode are unchanged. Changing any of them recomputes it immediately, without
// GPU queries or readback. Polygon modes are
// restored before returning, so selection and HUD drawing remain independent.
RenderResult renderWorld(const Camera* camera, const Mat4 view, const Mat4 projection, bool wireframe);
void cleanupWorld(void);

#endif
