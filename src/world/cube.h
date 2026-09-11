/**
 * @file graphics/cube.h
 * @brief Cube module for rendering a cube.
 * @author frankischilling
 * @date 2024-11-19
 */
#ifndef CUBE_H
#define CUBE_H

#include "../math/math.h"
#include <stdbool.h>
#include <stdint.h>

enum BlockID {
  BLOCK_AIR = 0,
  BLOCK_GRASS = 1,
  BLOCK_DIRT = 2,
  BLOCK_STONE = 3,
  BLOCK_COBBLESTONE = 4,
  BLOCK_OAK_PLANKS = 5,
  BLOCK_STONE_BRICKS = 6,
};

// block struct
typedef struct {
  uint8_t id;
} Block;

static inline bool blockIDValid(int id) {
  return id >= BLOCK_AIR && id <= BLOCK_STONE_BRICKS;
}

static inline bool blockIsSolid(int id) {
  return blockIDValid(id) && id != BLOCK_AIR;
}

// Six vertices per face, each with position, normal, and UV coordinates.
const float* getCubeFaceVertices(int face);

#endif // CUBE_H
