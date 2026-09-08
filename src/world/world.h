/**
 * @file world/world.h
 * @brief CPU world data and terrain generation.
 * @author frankischilling, VladimirJanus
 * @version 0.1
 * @date 2024-11-19
 *
 */
#ifndef WORLD_H
#define WORLD_H

#include "../math/math.h"
#include "chunk.h"
#include "cube.h"
#include <stddef.h>

#define WORLD_GENERATOR_VERSION 1

#define DIRT_LAYERS 3 // Number of dirt layers below the surface

typedef struct {
  float frequency;
  float amplitude;
  float persistence;
  float heightScale;
} BiomeParameters;
BiomeParameters getInterpolatedBiomeParameters(float x, float z);
float getTerrainHeight(float x, float z);
const char* getCurrentBiomeText(float x, float z);
// World owns chunks until cleanup, reinitialization, or successful replacement.
bool initChunks(void);
bool initChunksSeeded(uint32_t seed);
uint32_t worldSeed(void);
// Fills a chunk at its signed chunk position, independently of the live world.
void generateTerrainChunk(Chunk* chunk, uint32_t seed);
void cleanupChunks(void);
#define WORLD_BLOCK_COUNT ((size_t)WORLD_SIZE * WORLD_SIZE * CHUNK_HEIGHT)
// Packed IDs: chunk X/Z then local X/Y/Z, one byte each. Exact fixed size only.
bool copyWorldBlocks(uint8_t* blocks, size_t count);
// Allocate and validate before publishing. Failure preserves the live world.
bool replaceWorldBlocks(uint32_t seed, const uint8_t* blocks, size_t count);

// Chunk indices are array coordinates [0, CHUNKS_PER_AXIS), not signed world coordinates.
// Direct chunk access is for generation, meshing, and fixtures; gameplay edits use setBlock.
Chunk* getChunk(const Vec2i* chunkPos);
// NULL for a missing world, null position, or coordinates outside the finite world.
const Block* getBlock(const Vec3i* pos);
// Returns false for invalid coordinates/IDs. A successful no-op does not dirty meshes.
bool setBlock(const Vec3i* pos, int id);
#endif // WORLD_H
