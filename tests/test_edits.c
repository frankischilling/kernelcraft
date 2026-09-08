#include "world/world.h"
#include "world/mesh.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "Edit test: %s (line %d)\n", #condition, __LINE__); \
    exit(EXIT_FAILURE); \
  } \
} while (0)

static void resetWorld(void) {
  for (int x = 0; x < CHUNKS_PER_AXIS; x++)
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Chunk* chunk = getChunk(&(Vec2i){x, z});
      memset(chunk->blocks, 0, sizeof(chunk->blocks));
      chunk->dirty = false;
    }
}

static int dirtyCount(void) {
  int count = 0;
  for (int x = 0; x < CHUNKS_PER_AXIS; x++)
    for (int z = 0; z < CHUNKS_PER_AXIS; z++)
      count += getChunk(&(Vec2i){x, z})->dirty;
  return count;
}

static void testEdits(void) {
  CHECK(dirtyCount() == CHUNKS_PER_AXIS * CHUNKS_PER_AXIS);
  resetWorld();
  Vec3i invalid[] = {{INT_MIN, 0, 0}, {INT_MAX, 0, 0}, {-129, 0, 0}, {128, 0, 0},
                    {0, -1, 0}, {0, 64, 0}, {0, 0, -129}, {0, 0, 128}};
  CHECK(getChunk(NULL) == NULL && getBlock(NULL) == NULL);
  CHECK(!setBlock(NULL, BLOCK_STONE));
  for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
    CHECK(getBlock(&invalid[i]) == NULL);
    CHECK(!setBlock(&invalid[i], BLOCK_STONE));
  }
  Vec3i interior = {1, 20, 1};
  CHECK(!setBlock(&interior, -1) && !setBlock(&interior, 256));
  CHECK(dirtyCount() == 0);
  CHECK(setBlock(&interior, BLOCK_AIR) && dirtyCount() == 0);
  CHECK(setBlock(&interior, BLOCK_GRASS) && dirtyCount() == 1);
  CHECK(getBlock(&interior)->id == BLOCK_GRASS && blockIsSolid(getBlock(&interior)->id));
  CHECK(!blockIsSolid(BLOCK_AIR) && !blockIsSolid(256));
  getChunk(&(Vec2i){8, 8})->dirty = false;
  CHECK(setBlock(&interior, BLOCK_GRASS) && dirtyCount() == 0);

  // A corner affects its own chunk and two face neighbors, never the diagonal.
  const Vec3i corners[] = {{0, 63, 0}, {-1, 0, -1}, {-16, 20, 16}, {15, 20, -17}};
  for (size_t i = 0; i < sizeof(corners) / sizeof(corners[0]); i++) {
    resetWorld();
    CHECK(setBlock(&corners[i], BLOCK_STONE) && dirtyCount() == 3);
    Vec3i pos = corners[i];
    Vec2i owner = blockToChunk(&pos);
    Vec3i local = getLocal(&pos);
    int nx = owner.a + (local.x == 0 ? -1 : 1);
    int nz = owner.b + (local.z == 0 ? -1 : 1);
    for (int x = 0; x < CHUNKS_PER_AXIS; x++)
      for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
        bool expected = (x == owner.a && z == owner.b) || (x == nx && z == owner.b) || (x == owner.a && z == nz);
        CHECK(getChunk(&(Vec2i){x, z})->dirty == expected);
      }
    for (int x = 0; x < CHUNKS_PER_AXIS; x++)
      for (int z = 0; z < CHUNKS_PER_AXIS; z++)
        getChunk(&(Vec2i){x, z})->dirty = false;
    CHECK(setBlock(&corners[i], BLOCK_DIRT) && dirtyCount() == 1);
  }
  resetWorld();
  CHECK(setBlock(&(Vec3i){-128, 0, -128}, BLOCK_STONE) && dirtyCount() == 1);
  resetWorld();
  CHECK(setBlock(&(Vec3i){127, 63, 127}, BLOCK_STONE) && dirtyCount() == 1);

  resetWorld();
  Vec3i left = {-1, 20, 1}, right = {0, 20, 1};
  CHECK(setBlock(&left, BLOCK_STONE) && setBlock(&right, BLOCK_STONE));
  Chunk* neighbor = getChunk(&(Vec2i){7, 8});
  ChunkMesh mesh;
  CHECK(buildChunkMesh(neighbor, &mesh) && mesh.indexCount == 30);
  freeChunkMesh(&mesh);
  neighbor->dirty = false;
  CHECK(setBlock(&right, BLOCK_AIR) && neighbor->dirty);
  CHECK(buildChunkMesh(neighbor, &mesh) && mesh.indexCount == 36);
  freeChunkMesh(&mesh);
}

int main(void) {
  CHECK(initChunks());
  testEdits();
  cleanupChunks();
  CHECK(getBlock(&(Vec3i){0, 0, 0}) == NULL);
  CHECK(!setBlock(&(Vec3i){0, 0, 0}, BLOCK_STONE));
  puts("Block edit tests passed");
  return 0;
}
