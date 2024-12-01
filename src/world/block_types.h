/**
 * @file world/block_types.h
 * @brief Block type definitions for the game world.
 * @author frankischilling
 * @version 0.1
 * @date 2024-11-23
 *
 */
#ifndef BLOCK_TYPES_H
#define BLOCK_TYPES_H

#include "../math/math.h"

// Block types
typedef enum {
  BLOCK_AIR,
  BLOCK_GRASS,
  BLOCK_DIRT,
  BLOCK_STONE,
} BlockType;

// Block colors (RGB)
static const Vec3 blockColors[] = {
    {0.0f, 0.0f, 0.0f}, // AIR (not used)
    {0.4f, 0.6f, 0.3f}, // GRASS
    {0.6f, 0.4f, 0.2f}, // DIRT
    {0.5f, 0.5f, 0.5f}  // STONE
};

#endif // BLOCK_TYPES_H
