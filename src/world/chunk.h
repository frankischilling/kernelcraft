#ifndef CHUNK_H
#define CHUNK_H

#include "cube.h"

#define CHUNK_SIZE 16   // block count
#define CHUNK_HEIGHT 64 // block count
#define CHUNKS_PER_AXIS WORLD_SIZE / CHUNK_SIZE
#define CHUNK_DIMENSIONS                                                                                                                                                           \
  (Vec3) {                                                                                                                                                                         \
    CHUNK_SIZE *CUBE_SIZE, CHUNK_HEIGHT *CUBE_SIZE, CHUNK_SIZE *CUBE_SIZE,                                                                                                         \
  }

typedef enum {
  BIOME_PLAINS,
  BIOME_HILLS,
} BiomeID;

typedef struct {
  Block blocks[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE];
  Vec2i position; // Chunk coordinates
  BiomeID id;
} Chunk;

Vec3 chunkToWorld(Vec2i* chunkPos);
Vec3 getChunkCenter(Vec2i* chunkPos);
Vec3 blockToWorld(Vec3i* blockPos);
Vec2i worldToChunk(Vec3* worldPos);
Vec2i blockToChunk(Vec3i* blockPos);
Vec3i getLocal(Vec3i* blockPos);
Vec3i worldToBlock(Vec3* worldPos);

#endif // CHUNK_H