#include "occlusion.h"
#include <math.h>
#include <string.h>

// Expand queries, shrink occluders, and separate depths to absorb rounding in
// the GPU's float transform and subpixel rasterizer. NDC depth increases away
// from the camera under the game's ordinary (non-reversed) depth projection.
static const float depthMargin = 0.00001f;

typedef struct {
  double x, y, z;
} Projected;

static bool project(const OcclusionBuffer* buffer, Vec3 p, Projected* out) {
  const float* m = buffer->transform;
  double w = (double)m[3] * p.x + (double)m[7] * p.y + (double)m[11] * p.z + m[15];
  double z = (double)m[2] * p.x + (double)m[6] * p.y + (double)m[10] * p.z + m[14];
  if (!isfinite(w) || !isfinite(z) || w <= 0.00001 || z <= -w + depthMargin || z >= w)
    return false;
  out->x = (((double)m[0] * p.x + (double)m[4] * p.y + (double)m[8] * p.z + m[12]) / w + 1) * (OCCLUSION_WIDTH * 0.5);
  out->y = (((double)m[1] * p.x + (double)m[5] * p.y + (double)m[9] * p.z + m[13]) / w + 1) * (OCCLUSION_HEIGHT * 0.5);
  out->z = z / w;
  return isfinite(out->x) && isfinite(out->y);
}

void buildMeshOccluders(const ChunkMesh* mesh, MeshOccluders* occluders) {
  occluders->count = 0;
  for (size_t first = 0; first + 3 < mesh->vertexCount; first += 4) {
    Vec3 u, v;
    vec3_subtract(&u, &mesh->vertices[first + 1].position, &mesh->vertices[first].position);
    vec3_subtract(&v, &mesh->vertices[first + 3].position, &mesh->vertices[first].position);
    // Greedy faces are axis-aligned rectangles, so these are their edge lengths.
    float area = (fabsf(u.x) + fabsf(u.y) + fabsf(u.z)) * (fabsf(v.x) + fabsf(v.y) + fabsf(v.z));
    if (area < 4 * CUBE_SIZE * CUBE_SIZE)
      continue;
    int index = occluders->count;
    if (index == OCCLUSION_QUADS) {
      if (area <= occluders->quads[index - 1].area)
        continue;
      index--;
    } else {
      occluders->count++;
    }
    while (index > 0 && area > occluders->quads[index - 1].area) {
      occluders->quads[index] = occluders->quads[index - 1];
      index--;
    }

    for (int corner = 0; corner < 4; corner++)
      occluders->quads[index].corners[corner] = mesh->vertices[first + corner].position;
    occluders->quads[index].area = area;
  }
}

void occlusionClear(OcclusionBuffer* buffer, const Mat4 transform, int width, int height) {
  memcpy(buffer->transform, transform, sizeof(Mat4));
  // One actual viewport pixel absorbs subpixel rasterization error even when
  // the window is smaller than the software buffer. Zero-size views fail open.
  buffer->marginX = (double)OCCLUSION_WIDTH / (width > 0 ? width : 1);
  buffer->marginY = (double)OCCLUSION_HEIGHT / (height > 0 ? height : 1);
  for (int y = 0; y < OCCLUSION_HEIGHT; y++)
    for (int x = 0; x < OCCLUSION_WIDTH; x++)
      buffer->depth[y][x] = 1;
}

void occlusionRasterizeQuad(OcclusionBuffer* buffer, const Vec3 corners[4]) {
  Projected p[4];
  double farthest = -1, minY = OCCLUSION_HEIGHT, maxY = 0;
  for (int i = 0; i < 4; i++) {
    // Skipping a clipped rectangle is conservative and avoids unstable divides
    // at the eye plane. Other complete rectangles may still hide the target.
    if (!project(buffer, corners[i], &p[i]))
      return;
    farthest = fmax(farthest, p[i].z);
    minY = fmin(minY, p[i].y);
    maxY = fmax(maxY, p[i].y);
  }

  double area = 0;
  for (int i = 0; i < 4; i++) {
    int j = (i + 1) % 4;
    area += p[i].x * p[j].y - p[j].x * p[i].y;
  }

  if (!isfinite(area) || fabs(area) < 0.00001 || maxY <= 0 || minY >= OCCLUSION_HEIGHT)
    return;
  double a[4], b[4], c[4];
  double sign = area > 0 ? 1 : -1;
  for (int i = 0; i < 4; i++) {
    int j = (i + 1) % 4;
    a[i] = (p[i].y - p[j].y) * sign;
    b[i] = (p[j].x - p[i].x) * sign;
    c[i] = (p[i].x * p[j].y - p[j].x * p[i].y) * sign;
    // Worst of all four cell corners, plus an inward coverage margin.
    c[i] += fmin(0, a[i]) + fmin(0, b[i]) - buffer->marginX * fabs(a[i]) - buffer->marginY * fabs(b[i]);
  }

  int firstY = (int)fmax(0, floor(minY)), lastY = (int)fmin(OCCLUSION_HEIGHT - 1, floor(maxY));
  float depth = (float)farthest + depthMargin;
  for (int y = firstY; y <= lastY; y++) {
    double left = 0, right = OCCLUSION_WIDTH - 1;
    for (int edge = 0; edge < 4; edge++) {
      double value = b[edge] * y + c[edge];
      if (a[edge] > 0)
        left = fmax(left, -value / a[edge]);
      else if (a[edge] < 0)
        right = fmin(right, -value / a[edge]);
      else if (value < 0) {
        right = -1;
        break;
      }
    }

    if (left > right)
      continue;
    int firstX = (int)ceil(left), lastX = (int)floor(right);
    for (int x = firstX; x <= lastX; x++)
      if (depth < buffer->depth[y][x])
        buffer->depth[y][x] = depth;
  }
}

bool occlusionBoundsHidden(const OcclusionBuffer* buffer, Vec3 min, Vec3 max) {
  double minX = OCCLUSION_WIDTH, minY = OCCLUSION_HEIGHT, maxX = 0, maxY = 0, nearest = 1;
  for (int corner = 0; corner < 8; corner++) {
    Vec3 p = {corner & 1 ? max.x : min.x, corner & 2 ? max.y : min.y, corner & 4 ? max.z : min.z};
    Projected projected;
    if (!project(buffer, p, &projected))
      return false;
    minX = fmin(minX, projected.x);
    minY = fmin(minY, projected.y);
    maxX = fmax(maxX, projected.x);
    maxY = fmax(maxY, projected.y);
    nearest = fmin(nearest, projected.z);
  }

  if (maxX < 0 || maxY < 0 || minX >= OCCLUSION_WIDTH || minY >= OCCLUSION_HEIGHT)
    return false;
  int firstX = (int)fmax(0, floor(minX - buffer->marginX));
  int lastX = (int)fmin(OCCLUSION_WIDTH - 1, floor(maxX + buffer->marginX));
  int firstY = (int)fmax(0, floor(minY - buffer->marginY));
  int lastY = (int)fmin(OCCLUSION_HEIGHT - 1, floor(maxY + buffer->marginY));
  for (int y = firstY; y <= lastY; y++)
    for (int x = firstX; x <= lastX; x++)
      if (buffer->depth[y][x] >= nearest - depthMargin)
        return false;
  return true;
}
