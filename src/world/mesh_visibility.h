#ifndef MESH_VISIBILITY_H
#define MESH_VISIBILITY_H

#include "mesh.h"

typedef struct {
  Vec3 min, max;
} MeshSurfaceBounds;

typedef struct {
  MeshSurfaceBounds* surfaces;
  size_t surfaceCount;
  MeshSurfaceBounds bounds;
} MeshVisibility;

// CPU sidecar for the axis-aligned rectangles emitted by buildChunkMesh.
// Built in linear time, with no sorting or recursive tree. Pass an unused
// output; free it before rebuilding. No mesh pointers are kept.
bool buildMeshVisibility(const ChunkMesh* mesh, MeshVisibility* visibility);
void freeMeshVisibility(MeshVisibility* visibility);
// Inward-facing, normalized world-space frustum planes. Boundary contact is
// conservatively visible, including surfaces crossing the near plane.
bool meshVisibilityIntersects(const MeshVisibility* visibility, const float planes[6][4]);

#endif
