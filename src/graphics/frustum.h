#ifndef FRUSTUM_H
#define FRUSTUM_H

#include <stdbool.h>
#include "../math/math.h"
#include "../graphics/camera.h"

// Frustum structure to hold the six planes
typedef struct {
    float planes[6][4];  // Six planes (left, right, top, bottom, near, far), each with ABCD coefficients
} Frustum;

// Visibility status for blocks
typedef enum {
    BLOCK_HIDDEN,
    BLOCK_VISIBLE,
    BLOCK_PARTIALLY_VISIBLE
} BlockVisibility;

// Function declarations
void frustum_update(Frustum* frustum, const Mat4 projection, const Mat4 view);
BlockVisibility frustum_check_cube(const Frustum* frustum, float x, float y, float z, float size, const Camera* camera);
bool is_face_visible(float x, float y, float z, int face, const Camera* camera);

#endif