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

#include "forest_checks.h"

static uint64_t fingerprint(void) {
  uint64_t hash = UINT64_C(14695981039346656037);
  for (int x = -WORLD_SIZE / 2; x < WORLD_SIZE / 2; x++)
    for (int y = 0; y < CHUNK_HEIGHT; y++)
      for (int z = -WORLD_SIZE / 2; z < WORLD_SIZE / 2; z++)
        hash = (hash ^ getBlock(&(Vec3i){x, y, z})->id) * UINT64_C(1099511628211);
  return hash;
}

int main(void) {
  uint8_t* legacy = generatedWorldBlocks(0, 1);
  CHECK(replaceWorldBlocksVersioned(0, 1, legacy, WORLD_BLOCK_COUNT));
  CHECK(worldSeed() == 0 && worldGeneratorVersion() == 1);
  CHECK(fingerprint() == UINT64_C(512190482430576247));
  for (int x = -WORLD_SIZE / 2; x < WORLD_SIZE / 2; x += CHUNK_SIZE)
    for (int z = -WORLD_SIZE / 2; z < WORLD_SIZE / 2; z += CHUNK_SIZE)
      CHECK(strcmp(getCurrentBiomeText((float)x, (float)z), "Forest") != 0);
  uint64_t legacyFingerprint = fingerprint();
  CHECK(!replaceWorldBlocksVersioned(9, 0, legacy, WORLD_BLOCK_COUNT));
  CHECK(!replaceWorldBlocksVersioned(9, WORLD_GENERATOR_VERSION + 1, legacy, WORLD_BLOCK_COUNT));
  CHECK(worldSeed() == 0 && worldGeneratorVersion() == 1 && fingerprint() == legacyFingerprint);
  free(legacy);

  Chunk untouched = {.position = {-1, -1}};
  memset(untouched.blocks, 0x5a, sizeof(untouched.blocks));
  untouched.dirty = false;
  Chunk before;
  memcpy(&before, &untouched, sizeof(before));
  generateTerrainChunkVersioned(&untouched, 42, 0);
  CHECK(memcmp(&untouched, &before, sizeof(untouched)) == 0);
  generateTerrainChunkVersioned(&untouched, 42, WORLD_GENERATOR_VERSION + 1);
  CHECK(memcmp(&untouched, &before, sizeof(untouched)) == 0);
  generateTerrainChunkVersioned(NULL, 42, WORLD_GENERATOR_VERSION);

  CHECK(initChunksSeeded(0));
  CHECK(worldSeed() == 0 && worldGeneratorVersion() == WORLD_GENERATOR_VERSION);
  CHECK(fingerprint() != UINT64_C(512190482430576247));
  checkForestWorld();
  CHECK(initChunksSeeded(42));
  uint64_t first = fingerprint();
  CHECK(worldSeed() == 42 && worldGeneratorVersion() == WORLD_GENERATOR_VERSION && first != legacyFingerprint);
  CHECK(initChunksSeeded(UINT32_MAX));
  CHECK(worldSeed() == UINT32_MAX && worldGeneratorVersion() == WORLD_GENERATOR_VERSION && fingerprint() != first);
  checkForestWorld();
  CHECK(initChunksSeeded(42));
  CHECK(fingerprint() == first);
  Chunk wrapper = {.position = {-1, 0}}, explicitCurrent = {.position = {-1, 0}};
  generateTerrainChunk(&wrapper, 42);
  generateTerrainChunkVersioned(&explicitCurrent, 42, WORLD_GENERATOR_VERSION);
  CHECK(memcmp(wrapper.blocks, explicitCurrent.blocks, sizeof(wrapper.blocks)) == 0);
  // Recreate every chunk backwards, with unrelated random calls interleaved.
  for (int x = CHUNKS_PER_AXIS - 1; x >= 0; x--)
    for (int z = CHUNKS_PER_AXIS - 1; z >= 0; z--) {
      Chunk actual = {0};
      Chunk legacyChunk = {0};
      actual.position = (Vec2i){x - CHUNKS_PER_AXIS / 2, z - CHUNKS_PER_AXIS / 2};
      legacyChunk.position = actual.position;
      srand((unsigned)(x * 16 + z));
      (void)rand();
      generateTerrainChunkVersioned(&actual, 42, WORLD_GENERATOR_VERSION);
      generateTerrainChunkVersioned(&legacyChunk, 42, 1);
      CHECK(memcmp(actual.blocks, getChunk(&(Vec2i){x, z})->blocks, sizeof(actual.blocks)) == 0);
      checkCurrentGroundMatchesLegacy(&actual, &legacyChunk);
    }

  CHECK(fingerprint() == first);
  CHECK(initChunks());
  CHECK(worldSeed() == 0 && worldGeneratorVersion() == WORLD_GENERATOR_VERSION && fingerprint() != legacyFingerprint);
  cleanupChunks();
  puts("Seed, legacy terrain, forest generation, seams, and generation-order tests passed");
  return 0;
}
