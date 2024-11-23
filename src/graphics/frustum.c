/**
 * @file graphics/frustum.c
 * @brief Frustum culling implementation
 * @author frankischilling
 * @date 2024-11-23
 */
#include <math.h>
#include "frustum.h"
#include "../world/block_types.h"
#include "../world/world.h"  // For getBlockType function
#include "../math/math.h"

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

bool is_face_visible(float x, float y, float z, int face, const Camera* camera) {
    Vec3 blockCenter = {x, y, z};
    Vec3 toCam;
    vec3_subtract(toCam, camera->position, blockCenter);
    vec3_normalize(toCam, toCam);
    
    Vec3 faceNormal = {0.0f, 0.0f, 0.0f};
    switch(face) {
        case 0: faceNormal[0] = 1.0f; break;  // Right face
        case 1: faceNormal[0] = -1.0f; break; // Left face
        case 2: faceNormal[1] = 1.0f; break;  // Top face
        case 3: faceNormal[1] = -1.0f; break; // Bottom face
        case 4: faceNormal[2] = 1.0f; break;  // Front face
        case 5: faceNormal[2] = -1.0f; break; // Back face
    }
    
    return vec3_dot(toCam, faceNormal) > 0.0f;
}

bool is_block_occluded(float x, float y, float z, float size, const Camera* camera) {
    // Get the block type of current block
    BlockType current = getBlockType(x, y, z);
    if (current == BLOCK_AIR) {
        return true;
    }

    // Check if any face of the block is visible
    bool visible = false;
    
    // Check all six faces
    const Vec3 faceOffsets[] = {
        { size, 0, 0},  // Right
        {-size, 0, 0},  // Left
        {0,  size, 0},  // Top
        {0, -size, 0},  // Bottom
        {0, 0,  size},  // Front
        {0, 0, -size}   // Back
    };

    for (int i = 0; i < 6; i++) {
        float checkX = x + faceOffsets[i][0];
        float checkY = y + faceOffsets[i][1];
        float checkZ = z + faceOffsets[i][2];
        
        BlockType neighbor = getBlockType(checkX, checkY, checkZ);
        if (neighbor == BLOCK_AIR) {
            // If any face is exposed to air, the block is visible
            return false;
        }
    }

    // If we're very close to the block, don't cull it
    float dx = camera->position[0] - x;
    float dy = camera->position[1] - y;
    float dz = camera->position[2] - z;
    float distSq = dx*dx + dy*dy + dz*dz;
    if (distSq < size * size * 4.0f) {  // Adjust this value as needed
        return false;
    }

    return true;  // Block is completely surrounded by other blocks
}

BlockVisibility frustum_check_cube(const Frustum* frustum, float x, float y, float z, float size, const Camera* camera) {
    // First check distance to camera for depth-based culling
    float dx = x - camera->position[0];
    float dy = y - camera->position[1];
    float dz = z - camera->position[2];
    float distSq = dx * dx + dy * dy + dz * dz;

    // If the block is too far, consider it hidden
    if (distSq > 10000.0f) { // Adjust this value based on your needs
        return BLOCK_HIDDEN;
    }

    // Then check frustum culling
    for (int i = 0; i < 6; i++) {
        float d = frustum->planes[i][0] * x + 
                 frustum->planes[i][1] * y + 
                 frustum->planes[i][2] * z + 
                 frustum->planes[i][3];
        
        float r = size * 0.5f * (fabsf(frustum->planes[i][0]) + 
                                fabsf(frustum->planes[i][1]) + 
                                fabsf(frustum->planes[i][2]));
        
        if (d < -r) {
            return BLOCK_HIDDEN;
        }
    }

    // Check if any face is visible to the camera
    bool any_face_visible = false;
    for (int face = 0; face < 6; face++) {
        if (is_face_visible(x, y, z, face, camera)) {
            any_face_visible = true;
            break;
        }
    }

    if (!any_face_visible) {
        return BLOCK_HIDDEN;
    }

    // Check occlusion LAST
    if (is_block_occluded(x, y, z, size, camera)) {
        return BLOCK_HIDDEN;
    }

    return BLOCK_PARTIALLY_VISIBLE;
}