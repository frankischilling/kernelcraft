#include "world/world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c)                                                                                                                                                                   \
  do {                                                                                                                                                                             \
    if (!(c)) {                                                                                                                                                                    \
      fprintf(stderr, "Seed test: %s (line %d)\n", #c, __LINE__);                                                                                                                  \
      exit(EXIT_FAILURE);                                                                                                                                                          \
    }                                                                                                                                                                              \
  } while (0)

static uint64_t fingerprint(void) {
  uint64_t hash = UINT64_C(14695981039346656037);
  for (int x = -WORLD_SIZE / 2; x < WORLD_SIZE / 2; x++)
    for (int y = 0; y < CHUNK_HEIGHT; y++)
      for (int z = -WORLD_SIZE / 2; z < WORLD_SIZE / 2; z++)
        hash = (hash ^ getBlock(&(Vec3i){x, y, z})->id) * UINT64_C(1099511628211);
  return hash;
}

int main(void) {
  CHECK(initChunksSeeded(0));
  CHECK(worldSeed() == 0 && fingerprint() == UINT64_C(512190482430576247));
  CHECK(initChunksSeeded(42));
  uint64_t first = fingerprint();
  CHECK(worldSeed() == 42 && first != UINT64_C(512190482430576247));
  CHECK(initChunksSeeded(UINT32_MAX));
  CHECK(worldSeed() == UINT32_MAX && fingerprint() != first);
  CHECK(initChunksSeeded(42));
  CHECK(fingerprint() == first);
  // Recreate every chunk backwards, with unrelated random calls interleaved.
  for (int x = CHUNKS_PER_AXIS - 1; x >= 0; x--)
    for (int z = CHUNKS_PER_AXIS - 1; z >= 0; z--) {
      Chunk actual = {0};
      actual.position = (Vec2i){x - CHUNKS_PER_AXIS / 2, z - CHUNKS_PER_AXIS / 2};
      srand((unsigned)(x * 16 + z));
      (void)rand();
      generateTerrainChunk(&actual, 42);
      CHECK(memcmp(actual.blocks, getChunk(&(Vec2i){x, z})->blocks, sizeof(actual.blocks)) == 0);
    }
  CHECK(fingerprint() == first);
  CHECK(initChunks());
  CHECK(worldSeed() == 0 && fingerprint() == UINT64_C(512190482430576247));
  cleanupChunks();
  puts("Seed, legacy terrain, and generation-order tests passed");
  return 0;
}
