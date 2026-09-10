#ifndef OCCLUSION_H
#define OCCLUSION_H

#include "mesh.h"

enum { OCCLUSION_WIDTH = 128, OCCLUSION_HEIGHT = 72, OCCLUSION_QUADS = 8 };

typedef struct {
  Vec3 corners[4];
  float area;
} OcclusionQuad;

typedef struct {
  OcclusionQuad quads[OCCLUSION_QUADS];
  int count;
} MeshOccluders;

typedef struct {
  Mat4 transform;
  double marginX, marginY;
  float depth[OCCLUSION_HEIGHT][OCCLUSION_WIDTH];
} OcclusionBuffer;

// CPU-only, fixed storage. Only actual opaque mesh rectangles can occlude.
// Keeping a bounded subset loses culling opportunities, never visible geometry.
void buildMeshOccluders(const ChunkMesh* mesh, MeshOccluders* occluders);
void occlusionClear(OcclusionBuffer* buffer, const Mat4 transform, int width, int height);
void occlusionRasterizeQuad(OcclusionBuffer* buffer, const Vec3 corners[4]);
// Returns true only if the entire projected bound is behind fully covered cells.
// Near-plane crossings and nonfinite projections remain visible.
bool occlusionBoundsHidden(const OcclusionBuffer* buffer, Vec3 min, Vec3 max);

#endif
