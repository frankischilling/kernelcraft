/**
 * @file graphics/frustum.c
 * @brief Frustum culling implementation
 * @author frankischilling
 * @date 2024-11-23
 */
#include "frustum.h"
#include "../world/world.h" // For getBlock function
#include <math.h>
#include <stdio.h>

void frustum_update(Frustum* frustum, const Mat4 projection, const Mat4 view) {
  // Combine projection and view matrices
  Mat4 clip;

  mat4_multiply(clip, projection, view);

  // Right plane
  frustum->planes[0][0] = clip[3] - clip[0];
  frustum->planes[0][1] = clip[7] - clip[4];
  frustum->planes[0][2] = clip[11] - clip[8];
  frustum->planes[0][3] = clip[15] - clip[12];
  plane_normalize(frustum->planes[0]);

  // Left plane
  frustum->planes[1][0] = clip[3] + clip[0];
  frustum->planes[1][1] = clip[7] + clip[4];
  frustum->planes[1][2] = clip[11] + clip[8];
  frustum->planes[1][3] = clip[15] + clip[12];
  plane_normalize(frustum->planes[1]);

  // Top plane
  frustum->planes[2][0] = clip[3] - clip[1];
  frustum->planes[2][1] = clip[7] - clip[5];
  frustum->planes[2][2] = clip[11] - clip[9];
  frustum->planes[2][3] = clip[15] - clip[13];
  plane_normalize(frustum->planes[2]);

  // Bottom plane
  frustum->planes[3][0] = clip[3] + clip[1];
  frustum->planes[3][1] = clip[7] + clip[5];
  frustum->planes[3][2] = clip[11] + clip[9];
  frustum->planes[3][3] = clip[15] + clip[13];
  plane_normalize(frustum->planes[3]);

  // Front plane
  frustum->planes[4][0] = clip[3] + clip[2];
  frustum->planes[4][1] = clip[7] + clip[6];
  frustum->planes[4][2] = clip[11] + clip[10];
  frustum->planes[4][3] = clip[15] + clip[14];
  plane_normalize(frustum->planes[4]);

  // Back plane
  frustum->planes[5][0] = clip[3] - clip[2];
  frustum->planes[5][1] = clip[7] - clip[6];
  frustum->planes[5][2] = clip[11] - clip[10];
  frustum->planes[5][3] = clip[15] - clip[14];
  plane_normalize(frustum->planes[5]);
}

bool is_face_visible(Vec3i* pos, int face, const Camera* camera) {
  // Compare against the face plane; no normalization or quadrant correction is needed.
  switch (face) {
  case RIGHT:
    return camera->position.x > (pos->x + 1) * CUBE_SIZE;
  case LEFT:
    return camera->position.x < pos->x * CUBE_SIZE;
  case TOP:
    return camera->position.y > (pos->y + 1) * CUBE_SIZE;
  case BOTTOM:
    return camera->position.y < pos->y * CUBE_SIZE;
  case FRONT:
    return camera->position.z > (pos->z + 1) * CUBE_SIZE;
  case REAR:
    return camera->position.z < pos->z * CUBE_SIZE;
  default:
    return false;
  }
}

bool is_block_occluded(Vec3i* pos, float size, const Camera* camera) {
  (void)size;
  (void)camera;
  Block* block = getBlock(pos);
  if (!block || block->id == BLOCK_AIR)
    return true;
  for (int face = 0; face < 6; face++) {
    Vec3i neighborPos;
    vec3i_add(&neighborPos, pos, &vec3iFaceMap[face]);
    Block* neighbor = getBlock(&neighborPos);
    if (!neighbor || neighbor->id == BLOCK_AIR)
      return false;
  }
  return true;
}

bool frustum_cube_visible(const Frustum* frustum, Vec3* pos, float size, const Camera* camera) {
  for (int i = 0; i < 6; i++) {
    float d = frustum->planes[i][0] * pos->x + frustum->planes[i][1] * pos->y + frustum->planes[i][2] * pos->z + frustum->planes[i][3];

    float r = size * 0.5f * (fabsf(frustum->planes[i][0]) + fabsf(frustum->planes[i][1]) + fabsf(frustum->planes[i][2]));

    if (d < -r) {
      return false;
    }
  }

  return true;
}

bool frustum_block_visible(const Frustum* frustum, Vec3* pos, Vec3* sizes, const Camera* camera) {
  for (int i = 0; i < 6; i++) {
    float d = frustum->planes[i][0] * pos->x + frustum->planes[i][1] * pos->y + frustum->planes[i][2] * pos->z + frustum->planes[i][3];

    float r = 0.5f * (sizes->x * fabsf(frustum->planes[i][0]) + sizes->y * fabsf(frustum->planes[i][1]) + sizes->z * fabsf(frustum->planes[i][2]));

    if (d < -r) {
      return false;
    }
  }

  return true;
}
