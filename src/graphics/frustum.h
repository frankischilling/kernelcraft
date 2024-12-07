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
#include "../world/cube.h"

// Frustum structure to hold the six planes
typedef struct {
  Plane planes[6]; // Six planes (right, left, top, bottom, near, far), each with ABCD coefficients
} Frustum;

// Function declarations
void frustum_update(Frustum* frustum, const Mat4 projection, const Mat4 view);
bool is_block_occluded(Vec3i* pos, float size, const Camera* camera);
bool frustum_cube_visible(const Frustum* frustum, Vec3* pos, float size, const Camera* camera);
bool frustum_block_visible(const Frustum* frustum, Vec3* pos, Vec3* sizes, const Camera* camera);
bool is_face_visible(Vec3i* posi, int face, const Camera* camera);

#endif