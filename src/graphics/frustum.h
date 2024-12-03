/**
 * @file graphics/frustum.h
 * @brief Frustum culling implementation
 * @author frankischilling
 * @date 2024-11-23
 */
#ifndef FRUSTUM_H
#define FRUSTUM_H

#include <GL/glew.h>
#include <stdbool.h>
#include "../graphics/camera.h"
#include "../math/math.h"
#include "../world/block.h"

// Frustum structure to hold the six planes
typedef struct {
  float planes[6][4]; // Six planes (left, right, top, bottom, near, far), each with ABCD coefficients
} Frustum;

// Visibility status for blocks
typedef enum {
  BLOCK_HIDDEN = 0,
  BLOCK_VISIBLE = 1,
} BlockVisibility;

// Function declarations
void frustum_update(Frustum* frustum, const Mat4 projection, const Mat4 view);
bool is_block_occluded(Vec3* pos, float size, const Camera* camera);
BlockVisibility frustum_check_cube(const Frustum* frustum, Vec3* pos, float size, const Camera* camera);
bool is_face_visible(Vec3* pos, int face, const Camera* camera);

#endif