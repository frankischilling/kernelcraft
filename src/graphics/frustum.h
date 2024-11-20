#ifndef FRUSTUM_H
#define FRUSTUM_H

#include <stdbool.h>
#include "../math/math.h"

// Frustum structure to hold the six planes
typedef struct {
    float planes[6][4];  // Six planes (left, right, top, bottom, near, far), each with ABCD coefficients
} Frustum;

// Function declarations
void frustum_update(Frustum* frustum, const Mat4 projection, const Mat4 view);
bool frustum_check_cube(const Frustum* frustum, float x, float y, float z, float size);

#endif