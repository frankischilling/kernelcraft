/**
 * @file graphics/frustum.c
 * @brief Frustum culling implementation
 * @author frankischilling
 * @date 2024-11-23
 */
#include "frustum.h"
#include "../math/math.h"
#include "../world/block.h"
#include "../world/world.h" // For getBlock function
#include <math.h>
#include <stdio.h>

void frustum_update(Frustum* frustum, const Mat4 projection, const Mat4 view) {
  // Combine projection and view matrices
  Mat4 clip;

  // First row
  clip[0] = view[0] * projection[0] + view[1] * projection[4] + view[2] * projection[8] + view[3] * projection[12];
  clip[1] = view[0] * projection[1] + view[1] * projection[5] + view[2] * projection[9] + view[3] * projection[13];
  clip[2] = view[0] * projection[2] + view[1] * projection[6] + view[2] * projection[10] + view[3] * projection[14];
  clip[3] = view[0] * projection[3] + view[1] * projection[7] + view[2] * projection[11] + view[3] * projection[15];

  // Second row
  clip[4] = view[4] * projection[0] + view[5] * projection[4] + view[6] * projection[8] + view[7] * projection[12];
  clip[5] = view[4] * projection[1] + view[5] * projection[5] + view[6] * projection[9] + view[7] * projection[13];
  clip[6] = view[4] * projection[2] + view[5] * projection[6] + view[6] * projection[10] + view[7] * projection[14];
  clip[7] = view[4] * projection[3] + view[5] * projection[7] + view[6] * projection[11] + view[7] * projection[15];

  // Third row
  clip[8] = view[8] * projection[0] + view[9] * projection[4] + view[10] * projection[8] + view[11] * projection[12];
  clip[9] = view[8] * projection[1] + view[9] * projection[5] + view[10] * projection[9] + view[11] * projection[13];
  clip[10] = view[8] * projection[2] + view[9] * projection[6] + view[10] * projection[10] + view[11] * projection[14];
  clip[11] = view[8] * projection[3] + view[9] * projection[7] + view[10] * projection[11] + view[11] * projection[15];

  // Fourth row
  clip[12] = view[12] * projection[0] + view[13] * projection[4] + view[14] * projection[8] + view[15] * projection[12];
  clip[13] = view[12] * projection[1] + view[13] * projection[5] + view[14] * projection[9] + view[15] * projection[13];
  clip[14] = view[12] * projection[2] + view[13] * projection[6] + view[14] * projection[10] + view[15] * projection[14];
  clip[15] = view[12] * projection[3] + view[13] * projection[7] + view[14] * projection[11] + view[15] * projection[15];

  // Left plane
  frustum->planes[0][0] = clip[3] + clip[0];
  frustum->planes[0][1] = clip[7] + clip[4];
  frustum->planes[0][2] = clip[11] + clip[8];
  frustum->planes[0][3] = clip[15] + clip[12];
  plane_normalize(frustum->planes[0]);

  // Right plane
  frustum->planes[1][0] = clip[3] - clip[0];
  frustum->planes[1][1] = clip[7] - clip[4];
  frustum->planes[1][2] = clip[11] - clip[8];
  frustum->planes[1][3] = clip[15] - clip[12];
  plane_normalize(frustum->planes[1]);

  // Bottom plane
  frustum->planes[2][0] = clip[3] + clip[1];
  frustum->planes[2][1] = clip[7] + clip[5];
  frustum->planes[2][2] = clip[11] + clip[9];
  frustum->planes[2][3] = clip[15] + clip[13];
  plane_normalize(frustum->planes[2]);

  // Top plane
  frustum->planes[3][0] = clip[3] - clip[1];
  frustum->planes[3][1] = clip[7] - clip[5];
  frustum->planes[3][2] = clip[11] - clip[9];
  frustum->planes[3][3] = clip[15] - clip[13];
  plane_normalize(frustum->planes[3]);

  // Near plane
  frustum->planes[4][0] = clip[3] + clip[2];
  frustum->planes[4][1] = clip[7] + clip[6];
  frustum->planes[4][2] = clip[11] + clip[10];
  frustum->planes[4][3] = clip[15] + clip[14];
  plane_normalize(frustum->planes[4]);

  // Far plane
  frustum->planes[5][0] = clip[3] - clip[2];
  frustum->planes[5][1] = clip[7] - clip[6];
  frustum->planes[5][2] = clip[11] - clip[10];
  frustum->planes[5][3] = clip[15] - clip[14];
  plane_normalize(frustum->planes[5]);
}

bool is_face_visible(Vec3* pos, int face, const Camera* camera) {
  Vec3 toCam;
  vec3_subtract(&toCam, &camera->position, pos);
  vec3_normalize(&toCam, &toCam);

  Vec3 faceNormal = VEC3_ZERO;
  switch (face) {
  case 0:
    faceNormal.x = 1.0f;
    break; // Right face
  case 1:
    faceNormal.x = -1.0f;
    break; // Left face
  case 2:
    faceNormal.y = 1.0f;
    break; // Top face
  case 3:
    faceNormal.y = -1.0f;
    break; // Bottom face
  case 4:
    faceNormal.z = 1.0f;
    break; // Front face
  case 5:
    faceNormal.z = -1.0f;
    break; // Back face
  }

  return vec3_dot(&toCam, &faceNormal) > 0.0f;
}

bool is_block_occluded(Vec3i* pos, float size, const Camera* camera) {
  // Get the block type of current block
  Block* block = getBlock(pos);
  if (!block) {
    printf("BLOCK IS NULL AT %d, %d, %d this shouldnt happen\n", pos->x, pos->y, pos->z);
  }
  BlockID current = block->id;
  if (current == BLOCK_AIR) {
    printf("AIR CHECKED FOR OCCLUSION AT p: %d, %d, %d this shouldnt happen\n", pos->x, pos->y, pos->z);
    return true;
  }

  // Check if any face of the block is visible
  // bool visible = false; // Remove this unused variable

  // Check all six faces
  const Vec3i faceOffsets[] = {
      {size, 0, 0},  // Right
      {-size, 0, 0}, // Left
      {0, size, 0},  // Top
      {0, -size, 0}, // Bottom
      {0, 0, size},  // Front
      {0, 0, -size}  // Back
  };

  for (int i = 0; i < 6; i++) {
    Vec3i checkVector;
    vec3i_add(&checkVector, pos, &faceOffsets[i]);
    if (checkVector.y < 0) {
      checkVector.y = 0;
    }
    Block* block = getBlock(&checkVector);
    if (!block) {
      return false;
    }
    BlockID neighbor = block->id;
    if (neighbor == BLOCK_AIR) {
      // If any face is exposed to air, the block is visible
      return false;
    }
  }

  // If we're very close to the block, don't cull it
  /*Vec3 posv3 = (Vec3){pos->x, pos->y, pos->z};
  if (vec3_distance(&camera->position, &posv3) < 3.0f) { // Adjust this value as needed
    return false;
  }*/

  return true; // Block is completely surrounded by other blocks
}

BlockVisibility frustum_check_cube(const Frustum* frustum, Vec3* pos, float size, const Camera* camera) {
  for (int i = 0; i < 6; i++) {
    float d = frustum->planes[i][0] * pos->x + frustum->planes[i][1] * pos->y + frustum->planes[i][2] * pos->z + frustum->planes[i][3];

    float r = size * 0.5f * (fabsf(frustum->planes[i][0]) + fabsf(frustum->planes[i][1]) + fabsf(frustum->planes[i][2]));

    if (d < -r) {
      return BLOCK_HIDDEN;
    }
  }

  return BLOCK_VISIBLE;
}

BlockVisibility frustum_check_block(const Frustum* frustum, Vec3* pos, Vec3* sizes, const Camera* camera) {
  for (int i = 0; i < 6; i++) {
    float d = frustum->planes[i][0] * pos->x + frustum->planes[i][1] * pos->y + frustum->planes[i][2] * pos->z + frustum->planes[i][3];

    float r = 0.5f * (sizes->x * fabsf(frustum->planes[i][0]) + sizes->y * fabsf(frustum->planes[i][1]) + sizes->z * fabsf(frustum->planes[i][2]));

    if (d < -r) {
      return BLOCK_HIDDEN;
    }
  }

  return BLOCK_VISIBLE;
}
