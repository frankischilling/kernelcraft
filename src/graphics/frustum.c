/**
 * @file frustum.c
 * @brief Frustum culling implementation
 */
#include <math.h>
#include "frustum.h"

static void normalize_plane(float plane[4]) {
    float magnitude = sqrtf(plane[0] * plane[0] + plane[1] * plane[1] + plane[2] * plane[2]);
    if (magnitude != 0.0f) {
        plane[0] /= magnitude;
        plane[1] /= magnitude;
        plane[2] /= magnitude;
        plane[3] /= magnitude;
    }
}

void frustum_update(Frustum* frustum, const Mat4 projection, const Mat4 view) {
    // Combine projection and view matrices
    Mat4 clip;
    
    // First row
    clip[0]  = view[0] * projection[0] + view[1] * projection[4] + view[2] * projection[8] + view[3] * projection[12];
    clip[1]  = view[0] * projection[1] + view[1] * projection[5] + view[2] * projection[9] + view[3] * projection[13];
    clip[2]  = view[0] * projection[2] + view[1] * projection[6] + view[2] * projection[10] + view[3] * projection[14];
    clip[3]  = view[0] * projection[3] + view[1] * projection[7] + view[2] * projection[11] + view[3] * projection[15];

    // Second row
    clip[4]  = view[4] * projection[0] + view[5] * projection[4] + view[6] * projection[8] + view[7] * projection[12];
    clip[5]  = view[4] * projection[1] + view[5] * projection[5] + view[6] * projection[9] + view[7] * projection[13];
    clip[6]  = view[4] * projection[2] + view[5] * projection[6] + view[6] * projection[10] + view[7] * projection[14];
    clip[7]  = view[4] * projection[3] + view[5] * projection[7] + view[6] * projection[11] + view[7] * projection[15];

    // Third row
    clip[8]  = view[8] * projection[0] + view[9] * projection[4] + view[10] * projection[8] + view[11] * projection[12];
    clip[9]  = view[8] * projection[1] + view[9] * projection[5] + view[10] * projection[9] + view[11] * projection[13];
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
    normalize_plane(frustum->planes[0]);

    // Right plane
    frustum->planes[1][0] = clip[3] - clip[0];
    frustum->planes[1][1] = clip[7] - clip[4];
    frustum->planes[1][2] = clip[11] - clip[8];
    frustum->planes[1][3] = clip[15] - clip[12];
    normalize_plane(frustum->planes[1]);

    // Bottom plane
    frustum->planes[2][0] = clip[3] + clip[1];
    frustum->planes[2][1] = clip[7] + clip[5];
    frustum->planes[2][2] = clip[11] + clip[9];
    frustum->planes[2][3] = clip[15] + clip[13];
    normalize_plane(frustum->planes[2]);

    // Top plane
    frustum->planes[3][0] = clip[3] - clip[1];
    frustum->planes[3][1] = clip[7] - clip[5];
    frustum->planes[3][2] = clip[11] - clip[9];
    frustum->planes[3][3] = clip[15] - clip[13];
    normalize_plane(frustum->planes[3]);

    // Near plane
    frustum->planes[4][0] = clip[3] + clip[2];
    frustum->planes[4][1] = clip[7] + clip[6];
    frustum->planes[4][2] = clip[11] + clip[10];
    frustum->planes[4][3] = clip[15] + clip[14];
    normalize_plane(frustum->planes[4]);

    // Far plane
    frustum->planes[5][0] = clip[3] - clip[2];
    frustum->planes[5][1] = clip[7] - clip[6];
    frustum->planes[5][2] = clip[11] - clip[10];
    frustum->planes[5][3] = clip[15] - clip[14];
    normalize_plane(frustum->planes[5]);
}

bool frustum_check_cube(const Frustum* frustum, float x, float y, float z, float size) {
    // Check each plane of the frustum
    for (int i = 0; i < 6; i++) {
        float d = frustum->planes[i][0] * x + 
                 frustum->planes[i][1] * y + 
                 frustum->planes[i][2] * z + 
                 frustum->planes[i][3];
        
        // Calculate the radius of the cube for this orientation
        float r = size * 0.5f * (fabsf(frustum->planes[i][0]) + 
                                fabsf(frustum->planes[i][1]) + 
                                fabsf(frustum->planes[i][2]));
        
        // If the distance is negative and greater than the radius,
        // the cube is outside the frustum
        if (d < -r) {
            return false;
        }
    }
    return true;
}