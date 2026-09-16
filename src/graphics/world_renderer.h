#ifndef WORLD_RENDERER_H
#define WORLD_RENDERER_H

#include <GL/glew.h>
#include "camera.h"
#include "../world/day_night.h"
#include "../world/render_distance.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
  bool success;
  int surfaceBlocks; // Blocks represented by submitted meshes, not pixel visibility.
  int chunksConsidered, chunksRendered, chunksRebuilt;
  int chunksOccluded;                        // Bounds proven hidden in this frame's software depth buffer.
  int terrainDrawCalls;                      // Excludes the chunk grid and overlays.
  int shadowDrawCalls;                       // Casters for newly cached light directions or edits; usually zero.
  int visibilityChunksScanned;               // Chunk slots visited while rebuilding visibility; zero on a cache hit.
  int visibilityCandidates;                  // Distance/frustum/mesh survivors before conservative occlusion.
  size_t submittedQuads, submittedTriangles; // Merged rectangles, not unit block faces.
  size_t indexBytesRetained;                 // Current GPU element-buffer data stores, including reusable empty chunks.
  size_t indexBytesUploaded;                 // Element-buffer bytes submitted by dirty chunk rebuilds this frame.
  double meshUpdateMilliseconds;             // CPU mesh construction and GL submission; no GPU wait.
} RenderResult;

// The finite world remains fully loaded. Invalid values leave the setting and
// current visibility cache unchanged. These configuration calls do not require
// an OpenGL context and the setting survives cleanupWorld/initWorld cycles.
bool setWorldRenderDistance(int chunks);
int getWorldRenderDistance(void);
// Rendering functions require a current GL context. The caller owns the shader.
bool initWorld(GLuint shaderProgram);
// Borrowed nearest-sampled cutout image for surface overlays; valid until cleanup.
GLuint worldLeafTexture(void);
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
