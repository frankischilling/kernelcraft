#include "mesh_visibility.h"
#include <stdlib.h>

static const float planeTolerance = 0.0001f;

static void extendBounds(MeshSurfaceBounds* bounds, const Vec3* position) {
  if (position->x < bounds->min.x)
    bounds->min.x = position->x;
  if (position->y < bounds->min.y)
    bounds->min.y = position->y;
  if (position->z < bounds->min.z)
    bounds->min.z = position->z;
  if (position->x > bounds->max.x)
    bounds->max.x = position->x;
  if (position->y > bounds->max.y)
    bounds->max.y = position->y;
  if (position->z > bounds->max.z)
    bounds->max.z = position->z;
}

bool buildMeshVisibility(const ChunkMesh* mesh, MeshVisibility* visibility) {
  *visibility = (MeshVisibility){0};
  if (mesh->vertexCount % 4)
    return false;
  size_t quads = mesh->vertexCount / 4;
  if (!quads)
    return true;
  if (!mesh->vertices || quads > SIZE_MAX / sizeof(*visibility->surfaces))
    return false;
  visibility->surfaces = malloc(quads * sizeof(*visibility->surfaces));
  if (!visibility->surfaces)
    return false;
  visibility->surfaceCount = quads;
  visibility->bounds.min = visibility->bounds.max = mesh->vertices[0].position;
  for (size_t i = 0; i < quads; i++) {
    MeshSurfaceBounds* surface = &visibility->surfaces[i];
    surface->min = surface->max = mesh->vertices[i * 4].position;
    for (size_t vertex = 1; vertex < 4; vertex++)
      extendBounds(surface, &mesh->vertices[i * 4 + vertex].position);
    extendBounds(&visibility->bounds, &surface->min);
    extendBounds(&visibility->bounds, &surface->max);
  }

  return true;
}

void freeMeshVisibility(MeshVisibility* visibility) {
  free(visibility->surfaces);
  *visibility = (MeshVisibility){0};
}

static float planeDistance(const float plane[4], Vec3 position) {
  return plane[0] * position.x + plane[1] * position.y + plane[2] * position.z + plane[3] + planeTolerance;
}

static bool rectangleIntersects(const MeshSurfaceBounds* node, const float planes[6][4]) {
  // Clip the actual rectangle, since testing its box against individual planes
  // can still accept rectangles outside a frustum corner.
  Vec3 polygons[2][16];
  Vec3 a = node->min, b = node->min, c = node->max, d = node->max;
  if (a.x == c.x) {
    b.y = c.y;
    d.y = a.y;
  } else {
    b.x = c.x;
    d.x = a.x;
  }

  polygons[0][0] = a;
  polygons[0][1] = b;
  polygons[0][2] = c;
  polygons[0][3] = d;
  int count = 4, input = 0;
  for (int plane = 0; plane < 6; plane++) {
    int outputCount = 0;
    Vec3 previous = polygons[input][count - 1];
    float previousDistance = planeDistance(planes[plane], previous);
    for (int i = 0; i < count; i++) {
      Vec3 current = polygons[input][i];
      float distance = planeDistance(planes[plane], current);
      // Four vertices plus six clipping planes need at most ten vertices.
      // Stay conservative if that invariant changes.
      if (outputCount > 13)
        return true;
      if ((previousDistance >= 0) != (distance >= 0)) {
        float t = previousDistance / (previousDistance - distance);
        polygons[1 - input][outputCount++] =
            (Vec3){previous.x + t * (current.x - previous.x), previous.y + t * (current.y - previous.y), previous.z + t * (current.z - previous.z)};
      }

      if (distance >= 0)
        polygons[1 - input][outputCount++] = current;
      previous = current;
      previousDistance = distance;
    }

    if (!outputCount)
      return false;
    input = 1 - input;
    count = outputCount;
  }

  return true;
}

// -1 outside, 0 intersecting a plane, 1 entirely inside.
static int classifyBounds(const MeshSurfaceBounds* node, const float planes[6][4]) {
  bool entirelyInside = true;
  for (int i = 0; i < 6; i++) {
    Vec3 far = {planes[i][0] >= 0 ? node->max.x : node->min.x, planes[i][1] >= 0 ? node->max.y : node->min.y, planes[i][2] >= 0 ? node->max.z : node->min.z};
    if (planeDistance(planes[i], far) < 0)
      return -1;
    Vec3 near = {planes[i][0] >= 0 ? node->min.x : node->max.x, planes[i][1] >= 0 ? node->min.y : node->max.y, planes[i][2] >= 0 ? node->min.z : node->max.z};
    entirelyInside &= planeDistance(planes[i], near) >= 0;
  }

  return entirelyInside ? 1 : 0;
}

bool meshVisibilityIntersects(const MeshVisibility* visibility, const float planes[6][4]) {
  if (!visibility->surfaceCount)
    return false;
  int bounds = classifyBounds(&visibility->bounds, planes);
  if (bounds)
    return bounds > 0;
  // Most visible chunks accept the first surface. Only partial-frustum chunks
  // need this refinement; fully inside/outside meshes return above.
  for (size_t i = 0; i < visibility->surfaceCount; i++) {
    const MeshSurfaceBounds* surface = &visibility->surfaces[i];
    int intersection = classifyBounds(surface, planes);
    if (intersection > 0 || (intersection == 0 && rectangleIntersects(surface, planes)))
      return true;
  }

  return false;
}
