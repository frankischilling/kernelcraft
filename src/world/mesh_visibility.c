#include "mesh_visibility.h"
#include <math.h>
#include <stdlib.h>

struct MeshVisibilityNode {
  Vec3 min, max;
  uint32_t left, right;
};

static const float planeTolerance = 0.0001f;

static void extendBounds(MeshVisibilityNode* node, const Vec3* position) {
  node->min.x = fminf(node->min.x, position->x);
  node->min.y = fminf(node->min.y, position->y);
  node->min.z = fminf(node->min.z, position->z);
  node->max.x = fmaxf(node->max.x, position->x);
  node->max.y = fmaxf(node->max.y, position->y);
  node->max.z = fmaxf(node->max.z, position->z);
}

static int compareX(const void* a, const void* b) {
  const MeshVisibilityNode *left = a, *right = b;
  float difference = left->min.x + left->max.x - right->min.x - right->max.x;
  return (difference > 0) - (difference < 0);
}

static int compareY(const void* a, const void* b) {
  const MeshVisibilityNode *left = a, *right = b;
  float difference = left->min.y + left->max.y - right->min.y - right->max.y;
  return (difference > 0) - (difference < 0);
}

static int compareZ(const void* a, const void* b) {
  const MeshVisibilityNode *left = a, *right = b;
  float difference = left->min.z + left->max.z - right->min.z - right->max.z;
  return (difference > 0) - (difference < 0);
}

static uint32_t buildNode(MeshVisibility* visibility, size_t first, size_t count) {
  if (count == 1)
    return (uint32_t)first;
  MeshVisibilityNode bounds = visibility->nodes[first];
  for (size_t i = first + 1; i < first + count; i++) {
    extendBounds(&bounds, &visibility->nodes[i].min);
    extendBounds(&bounds, &visibility->nodes[i].max);
  }
  Vec3 size;
  vec3_subtract(&size, &bounds.max, &bounds.min);
  int (*compare)(const void*, const void*) = size.x >= size.y && size.x >= size.z ? compareX : size.y >= size.z ? compareY : compareZ;
  qsort(visibility->nodes + first, count, sizeof(*visibility->nodes), compare);
  size_t half = count / 2;
  bounds.left = buildNode(visibility, first, half);
  bounds.right = buildNode(visibility, first + half, count - half);
  uint32_t index = (uint32_t)visibility->nodeCount++;
  visibility->nodes[index] = bounds;
  return index;
}

bool buildMeshVisibility(const ChunkMesh* mesh, MeshVisibility* visibility) {
  *visibility = (MeshVisibility){0};
  if (mesh->vertexCount % 4)
    return false;
  size_t quads = mesh->vertexCount / 4;
  if (!quads)
    return true;
  if (!mesh->vertices || quads > UINT32_MAX / 2 || quads > SIZE_MAX / sizeof(*visibility->nodes) / 2)
    return false;
  visibility->nodes = malloc((quads * 2 - 1) * sizeof(*visibility->nodes));
  if (!visibility->nodes)
    return false;
  visibility->nodeCount = quads;
  for (size_t i = 0; i < quads; i++) {
    MeshVisibilityNode* node = &visibility->nodes[i];
    node->min = node->max = mesh->vertices[i * 4].position;
    node->left = node->right = UINT32_MAX;
    for (size_t vertex = 1; vertex < 4; vertex++)
      extendBounds(node, &mesh->vertices[i * 4 + vertex].position);
  }
  // Leaves occupy the first quads slots; parents follow their children and
  // the root is last. Median splits bound traversal depth even on flat meshes.
  buildNode(visibility, 0, quads);
  return true;
}

void freeMeshVisibility(MeshVisibility* visibility) {
  free(visibility->nodes);
  *visibility = (MeshVisibility){0};
}

static float planeDistance(const float plane[4], Vec3 position) {
  return plane[0] * position.x + plane[1] * position.y + plane[2] * position.z + plane[3] + planeTolerance;
}

static bool rectangleIntersects(const MeshVisibilityNode* node, const float planes[6][4]) {
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

static bool nodeIntersects(const MeshVisibility* visibility, uint32_t index, const float planes[6][4]) {
  const MeshVisibilityNode* node = &visibility->nodes[index];
  bool entirelyInside = true;
  for (int i = 0; i < 6; i++) {
    Vec3 far = {planes[i][0] >= 0 ? node->max.x : node->min.x, planes[i][1] >= 0 ? node->max.y : node->min.y, planes[i][2] >= 0 ? node->max.z : node->min.z};
    if (planeDistance(planes[i], far) < 0)
      return false;
    Vec3 near = {planes[i][0] >= 0 ? node->min.x : node->max.x, planes[i][1] >= 0 ? node->min.y : node->max.y, planes[i][2] >= 0 ? node->min.z : node->max.z};
    entirelyInside &= planeDistance(planes[i], near) >= 0;
  }
  if (entirelyInside)
    return true;
  if (node->left == UINT32_MAX)
    return rectangleIntersects(node, planes);
  return nodeIntersects(visibility, node->left, planes) || nodeIntersects(visibility, node->right, planes);
}

bool meshVisibilityIntersects(const MeshVisibility* visibility, const float planes[6][4]) {
  return visibility->nodeCount && nodeIntersects(visibility, (uint32_t)(visibility->nodeCount - 1), planes);
}
