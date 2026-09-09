/**
 * @file graphics/cube.c
 * @brief Cube module for rendering a cube.
 * @author frankischilling
 * @date 2024-11-19
 */
#include "cube.h"
#include <stdio.h>
typedef struct {
  float vertices[48]; // 6 vertices * 8 floats per vertex
} CubeFace;

// Predefined faces with positions, normals, and texture coordinates
static const CubeFace cubeFaces[6] = {
    // Right face (X-positive)
    {{0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,   //
      0.5f, -0.5f, 0.5f,  1.0f, 0.0f, 0.0f, 1.0f, 1.0f,   //
      0.5f, 0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   //
      0.5f, 0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   //
      0.5f, 0.5f,  -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,   //
      0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}}, //

    // Left face (X-negative)
    {{-0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,   //
      -0.5f, -0.5f, 0.5f,  -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,   //
      -0.5f, 0.5f,  0.5f,  -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   //
      -0.5f, 0.5f,  0.5f,  -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   //
      -0.5f, 0.5f,  -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,   //
      -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f}}, //

    // Top face (Y-positive)
    {{-0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,   //
      0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   //
      0.5f,  0.5f, 0.5f,  0.0f, 1.0f, 0.0f, 1.0f, 1.0f,   //
      0.5f,  0.5f, 0.5f,  0.0f, 1.0f, 0.0f, 1.0f, 1.0f,   //
      -0.5f, 0.5f, 0.5f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f,   //
      -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f}}, //

    // Bottom face (Y-negative)
    {{-0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f,   //
      0.5f,  -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f,   //
      0.5f,  -0.5f, 0.5f,  0.0f, -1.0f, 0.0f, 1.0f, 0.0f,   //
      0.5f,  -0.5f, 0.5f,  0.0f, -1.0f, 0.0f, 1.0f, 0.0f,   //
      -0.5f, -0.5f, 0.5f,  0.0f, -1.0f, 0.0f, 0.0f, 0.0f,   //
      -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f}}, //

    // Front face (Z-negative)
    {{-0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,   //
      0.5f,  -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,   //
      0.5f,  0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,   //
      0.5f,  0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,   //
      -0.5f, 0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,   //
      -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}}, //

    // Back face (Z-positive)
    {{-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,   //
      0.5f,  -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,   //
      0.5f,  0.5f,  -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f,   //
      0.5f,  0.5f,  -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f,   //
      -0.5f, 0.5f,  -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,   //
      -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f}}, //
};

const float* getCubeFaceVertices(int face) {
  return face >= 0 && face < 6 ? cubeFaces[face].vertices : NULL;
}

bool blockIDValid(int id) {
  return id >= BLOCK_AIR && id <= BLOCK_COBBLESTONE;
}

bool blockIsSolid(int id) {
  return blockIDValid(id) && id != BLOCK_AIR;
}
