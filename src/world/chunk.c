#include "../math/math.h"
#include "chunk.h"
#include "cube.h"
#include "world.h"
#include <stdio.h>

// Convert chunk coordinates to world space.
Vec3 chunkToWorld(Vec2i* chunkPos) {
  return (Vec3){
      chunkPos->a * CHUNK_SIZE,
      0,
      chunkPos->b * CHUNK_SIZE,
  };
}

// Get the chunk position in world space (centered).
Vec3 getChunkCenter(Vec2i* chunkPos) {
  Vec3 worldCoords = chunkToWorld(chunkPos);
  return (Vec3){
      worldCoords.x + CHUNK_SIZE / 2,
      CHUNK_HEIGHT / 2,
      worldCoords.z + CHUNK_SIZE / 2,
  };
}

// Convert block coordinates to world coordinates, offset by half the cube size.
Vec3 blockToWorld(Vec3i* blockPos) {
  return (Vec3){
      blockPos->x + 0.5f * CUBE_SIZE,
      blockPos->y + 0.5f * CUBE_SIZE,
      blockPos->z + 0.5f * CUBE_SIZE,
  };
}

// Convert world coordinates to block coordinates.
Vec3i worldToBlock(Vec3* worldPos) {
  return (Vec3i){
      (int)(worldPos->x),
      (int)(worldPos->y),
      (int)(worldPos->z),
  };
}

// Convert world coordinates to chunk coordinates.
Vec2i worldToChunk(Vec3* worldPos) {
  return (Vec2i){
      (int)(worldPos->x / CHUNK_SIZE),
      (int)(worldPos->z / CHUNK_SIZE),
  };
}

// Convert block coordinates to chunk coordinates.
Vec2i blockToChunk(Vec3i* blockPos) {
  return (Vec2i){
      (int)(floorf(blockPos->x / (CHUNK_SIZE * CUBE_SIZE)) + CHUNKS_PER_AXIS / 2),
      (int)(floorf(blockPos->z / (CHUNK_SIZE * CUBE_SIZE)) + CHUNKS_PER_AXIS / 2),
  };
}

// Get the local block position within a chunk (adjust for wrapping around).
Vec3i getLocal(Vec3i* blockPos) {
  int localX = (int)((blockPos->x % CHUNK_SIZE) / CUBE_SIZE);
  int localZ = (int)((blockPos->z % CHUNK_SIZE) / CUBE_SIZE);
  if (localX < 0)
    localX += CHUNK_SIZE;
  if (localZ < 0)
    localZ += CHUNK_SIZE;

  if (localX < 0 || localX > 15 || blockPos->y < 0 || localZ < 0 || localZ > 15) {
    printf(" OUT OF BOUNDS p: %d, %d, %d chunk: %d,%d\n", localX, blockPos->y, localZ);
    return (Vec3i)VEC3_ZERO;
  }
  return (Vec3i){localX, blockPos->y, localZ};
}
