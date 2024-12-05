/**
 * @file graphics/cube.h
 * @brief Cube module for rendering a cube.
 * @author frankischilling
 * @date 2024-11-19
 */
#ifndef CUBE_H
#define CUBE_H

#include <GL/glew.h> // Include GLEW to get OpenGL extensions
#include "../math/math.h"
#include <stdbool.h>

enum BlockID {
  BLOCK_AIR = 0,
  BLOCK_GRASS = 1,
  BLOCK_DIRT = 2,
  BLOCK_STONE = 3,
};

// block struct
typedef struct {
  enum BlockID id;
  bool checkedNeighbours;
  bool neighbour[6];
} Block;

// Block colors (R0.GB)
static const Vec3 blockColors[] = {
    {0.0f, 0.0f, 0.0f}, // AIR (not used)
    {0.4f, 0.6f, 0.3f}, // GRASS
    {0.6f, 0.4f, 0.2f}, // DIRT
    {0.5f, 0.5f, 0.5f}  // STONE
};
void initCube();

#endif // CUBE_H