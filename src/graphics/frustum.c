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

  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      clip[i * 4 + j] = 0;
      for (int k = 0; k < 4; ++k) {
        clip[i * 4 + j] += view[i * 4 + k] * projection[k * 4 + j];
      }
    }
  }

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

bool is_face_visible(Vec3i* posi, int face, const Camera* camera) {
  Vec3 pos = (Vec3){posi->x, posi->y, posi->z};
  Vec3 toBlock;
  vec3_subtract(&toBlock, &pos, &camera->position);
  vec3_normalize(&toBlock, &toBlock);

  Vec3 faceNormal = VEC3_ZERO;
  switch (face) {
  case 0:
    faceNormal = (Vec3)VEC3_RIGHT;
    break; // Right face
  case 1:
    faceNormal = (Vec3)VEC3_LEFT;
    break; // Left face
  case 2:
    faceNormal = (Vec3)VEC3_UP;
    break; // Top face
  case 3:
    faceNormal = (Vec3)VEC3_DOWN;
    break; // Bottom face
  case 4:
    faceNormal = (Vec3)VEC3_FRONT;
    break; // Front face
  case 5:
    faceNormal = (Vec3)VEC3_REAR;
    break; // Rear face
  }

  if ((toBlock.x < 0 && toBlock.z > 0) || (toBlock.x > 0 && toBlock.z < 0)) {
    faceNormal = (Vec3){-faceNormal.x, faceNormal.y, -faceNormal.z};
  }
  return vec3_dot(&toBlock, &faceNormal) < 0.0f;
}

bool is_block_occluded(Vec3i* pos, float size, const Camera* camera) {
  // Get the block type of current block
  Block* block = getBlock(pos);
  if (!block) {
    printf("BLOCK IS NULL AT %d, %d, %d this shouldnt happen\n", pos->x, pos->y, pos->z);
  }
  enum BlockID current = block->id;
  if (current == BLOCK_AIR) {
    printf("AIR CHECKED FOR OCCLUSION AT p: %d, %d, %d this shouldnt happen\n", pos->x, pos->y, pos->z);
    return true;
  }

  // Check all six faces

  for (int i = 0; i < 6; i++) {
    Vec3i checkVector;
    vec3i_add(&checkVector, pos, &vec3iFaceMap[i]);
    if (checkVector.y < 0) {
      checkVector.y = 0;
    }
    Block* neighborBlock = getBlock(&checkVector);
    if (!neighborBlock) {
      // If block is next to world border, make it visible
      return false;
    }
    enum BlockID neighborID = block->id;
    if (neighborID == BLOCK_AIR) {
      // If any face is exposed to air, the block is visible
      return false;
    }
  }

  // culling switch when close to the block
  /*Vec3 posv3 = (Vec3){pos->x, pos->y, pos->z};
  if (vec3_distance(&camera->position, &posv3) < 3.0f) {
    return false;
  }*/

  return true; // Block is completely surrounded by other blocks
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
