#ifndef WORLD_RENDERER_H
#define WORLD_RENDERER_H

#include <GL/glew.h>
#include "camera.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
  bool success;
  int surfaceBlocks; // Blocks represented by submitted meshes, not pixel visibility.
  int chunksConsidered, chunksRendered, chunksRebuilt;
  int terrainDrawCalls;                      // Excludes the chunk grid and overlays.
  size_t submittedQuads, submittedTriangles; // Merged rectangles, not unit block faces.
  double meshUpdateMilliseconds;             // CPU mesh construction and GL submission; no GPU wait.
} RenderResult;

// These functions require a current GL context. The caller owns the shader.
bool initWorld(GLuint shaderProgram);
RenderResult renderWorld(const Camera* camera, const Mat4 view, const Mat4 projection);
void cleanupWorld(void);

#endif
