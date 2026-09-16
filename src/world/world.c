/**
 * @file world/world.c
 * @brief World generation and rendering.
 * @author frankischilling, VladimirJanus
 * @version 0.1
 * @date 2024-11-19
 *
 */
#include "world.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static Chunk* chunks[CHUNKS_PER_AXIS][CHUNKS_PER_AXIS];
static uint32_t currentSeed;
static uint32_t currentGeneratorVersion = WORLD_GENERATOR_VERSION;
static Noise activeNoise;
static Noise activeForestNoise;
static float terrainHeight(const Noise* noise, float x, float z);

#define TREE_SITE_SPACING 8
#define TREE_CANOPY_RADIUS 2
#define TREE_GRASS_RADIUS 3
#define FOREST_NOISE_FREQUENCY 0.0125f
#define FOREST_NOISE_THRESHOLD 0.52f

typedef struct {
  int64_t x;
  int64_t z;
  int surfaceY;
  int trunkHeight;
} TreeSite;

static bool generatorVersionValid(uint32_t version) {
  return version >= 1 && version <= WORLD_GENERATOR_VERSION;
}

static uint32_t mix32(uint32_t value) {
  value ^= value >> 16;
  value *= UINT32_C(0x7feb352d);
  value ^= value >> 15;
  value *= UINT32_C(0x846ca68b);
  value ^= value >> 16;
  return value;
}

static uint32_t coordinateHash(uint32_t seed, int64_t x, int64_t z, uint32_t salt) {
  uint64_t ux = (uint64_t)x;
  uint64_t uz = (uint64_t)z;
  uint32_t hash = mix32(seed ^ salt ^ (uint32_t)ux);
  hash = mix32(hash ^ (uint32_t)(ux >> 32));
  hash = mix32(hash ^ (uint32_t)uz);
  return mix32(hash ^ (uint32_t)(uz >> 32));
}

static int64_t floorDiv64(int64_t value, int64_t divisor) {
  int64_t quotient = value / divisor;
  if (value % divisor < 0)
    quotient--;
  return quotient;
}

static bool forestBiomeAt(const Noise* noise, float x, float z) {
  return perlinWithNoise(noise, x * FOREST_NOISE_FREQUENCY, 0.0f, z * FOREST_NOISE_FREQUENCY) >= FOREST_NOISE_THRESHOLD;
}

static bool treeSiteAt(const Noise* terrainNoise, const Noise* forestNoise, uint32_t seed, int64_t cellX, int64_t cellZ, TreeSite* site) {
  uint32_t xHash = coordinateHash(seed, cellX, cellZ, UINT32_C(0x243f6a88));
  uint32_t zHash = coordinateHash(seed, cellX, cellZ, UINT32_C(0x85a308d3));
  int64_t x = cellX * TREE_SITE_SPACING + 1 + (int64_t)(xHash % 6);
  int64_t z = cellZ * TREE_SITE_SPACING + 1 + (int64_t)(zHash % 6);
  if (x - TREE_CANOPY_RADIUS < -WORLD_SIZE / 2 || x + TREE_CANOPY_RADIUS >= WORLD_SIZE / 2 || z - TREE_CANOPY_RADIUS < -WORLD_SIZE / 2 || z + TREE_CANOPY_RADIUS >= WORLD_SIZE / 2)
    return false;
  if (!forestBiomeAt(forestNoise, (float)x, (float)z))
    return false;
  if (coordinateHash(seed, cellX, cellZ, UINT32_C(0x13198a2e)) % 100 >= 75)
    return false;

  int surfaceY = (int)floorf(terrainHeight(terrainNoise, (float)x * CUBE_SIZE, (float)z * CUBE_SIZE));
  int trunkHeight = 4 + (int)(coordinateHash(seed, cellX, cellZ, UINT32_C(0x03707344)) & 1U);
  if (surfaceY < 0 || surfaceY + trunkHeight + 1 >= CHUNK_HEIGHT)
    return false;
  // Reject cliff-adjacent sites whose crown would be buried. Check the same
  // global footprint for every chunk, preserving the original terrain heights.
  int lowestLeaves = surfaceY + trunkHeight - 2;
  for (int dx = -TREE_CANOPY_RADIUS; dx <= TREE_CANOPY_RADIUS; dx++)
    for (int dz = -TREE_CANOPY_RADIUS; dz <= TREE_CANOPY_RADIUS; dz++) {
      if ((!dx && !dz) || (abs(dx) == TREE_CANOPY_RADIUS && abs(dz) == TREE_CANOPY_RADIUS))
        continue;
      int ground = (int)floorf(terrainHeight(terrainNoise, (float)(x + dx) * CUBE_SIZE, (float)(z + dz) * CUBE_SIZE));
      if (ground >= lowestLeaves)
        return false;
    }
  *site = (TreeSite){x, z, surfaceY, trunkHeight};
  return true;
}

static void placeDecoration(Chunk* chunk, int64_t chunkX, int64_t chunkZ, int64_t x, int y, int64_t z, int id) {
  if (y < 0 || y >= CHUNK_HEIGHT || x < chunkX || x >= chunkX + CHUNK_SIZE || z < chunkZ || z >= chunkZ + CHUNK_SIZE)
    return;
  Block* block = &chunk->blocks[(int)(x - chunkX)][y][(int)(z - chunkZ)];
  if (id == BLOCK_OAK_LOG) {
    if (block->id == BLOCK_AIR || block->id == BLOCK_OAK_LEAVES)
      block->id = BLOCK_OAK_LOG;
  } else if (id == BLOCK_OAK_LEAVES && block->id == BLOCK_AIR) {
    block->id = BLOCK_OAK_LEAVES;
  }
}

static void placeTree(Chunk* chunk, int64_t chunkX, int64_t chunkZ, const TreeSite* site) {
  int crownY = site->surfaceY + site->trunkHeight;
  for (int y = site->surfaceY + 1; y <= crownY; y++)
    placeDecoration(chunk, chunkX, chunkZ, site->x, y, site->z, BLOCK_OAK_LOG);

  for (int dy = -2; dy <= -1; dy++)
    for (int dx = -TREE_CANOPY_RADIUS; dx <= TREE_CANOPY_RADIUS; dx++)
      for (int dz = -TREE_CANOPY_RADIUS; dz <= TREE_CANOPY_RADIUS; dz++) {
        if (abs(dx) == TREE_CANOPY_RADIUS && abs(dz) == TREE_CANOPY_RADIUS)
          continue;
        placeDecoration(chunk, chunkX, chunkZ, site->x + dx, crownY + dy, site->z + dz, BLOCK_OAK_LEAVES);
      }

  for (int dx = -1; dx <= 1; dx++)
    for (int dz = -1; dz <= 1; dz++)
      placeDecoration(chunk, chunkX, chunkZ, site->x + dx, crownY, site->z + dz, BLOCK_OAK_LEAVES);
  for (int dx = -1; dx <= 1; dx++)
    for (int dz = -1; dz <= 1; dz++)
      if (abs(dx) + abs(dz) <= 1)
        placeDecoration(chunk, chunkX, chunkZ, site->x + dx, crownY + 1, site->z + dz, BLOCK_OAK_LEAVES);
}

static void markTreeGrass(uint8_t proximity[CHUNK_SIZE][CHUNK_SIZE], int64_t chunkX, int64_t chunkZ, const TreeSite* site) {
  for (int x = 0; x < CHUNK_SIZE; x++)
    for (int z = 0; z < CHUNK_SIZE; z++) {
      int64_t dx = chunkX + x - site->x;
      int64_t dz = chunkZ + z - site->z;
      int64_t distanceSquared = dx * dx + dz * dz;
      if (distanceSquared <= TREE_GRASS_RADIUS * TREE_GRASS_RADIUS) {
        uint8_t level = distanceSquared <= TREE_CANOPY_RADIUS * TREE_CANOPY_RADIUS ? 2 : 1;
        if (level > proximity[x][z])
          proximity[x][z] = level;
      }
    }
}

static void generateBaseTerrain(Chunk* chunk, const Noise* noise, int surfaceHeights[CHUNK_SIZE][CHUNK_SIZE]) {
  int64_t chunkX = (int64_t)chunk->position.a * CHUNK_SIZE;
  int64_t chunkZ = (int64_t)chunk->position.b * CHUNK_SIZE;
  for (int x = 0; x < CHUNK_SIZE; x++)
    for (int z = 0; z < CHUNK_SIZE; z++) {
      int height = (int)floorf(terrainHeight(noise, (float)(chunkX + x) * CUBE_SIZE, (float)(chunkZ + z) * CUBE_SIZE));
      if (surfaceHeights)
        surfaceHeights[x][z] = height;
      for (int y = 0; y < CHUNK_HEIGHT; y++) {
        if (y < height - DIRT_LAYERS)
          chunk->blocks[x][y][z].id = BLOCK_STONE;
        else if (y < height)
          chunk->blocks[x][y][z].id = BLOCK_DIRT;
        else if (y == height)
          chunk->blocks[x][y][z].id = BLOCK_GRASS;
      }
    }
}

static void decorateForestChunk(Chunk* chunk, const Noise* terrainNoise, const Noise* forestNoise, uint32_t seed, int surfaceHeights[CHUNK_SIZE][CHUNK_SIZE]) {
  int64_t chunkX = (int64_t)chunk->position.a * CHUNK_SIZE;
  int64_t chunkZ = (int64_t)chunk->position.b * CHUNK_SIZE;
  uint8_t proximity[CHUNK_SIZE][CHUNK_SIZE] = {{0}};
  int64_t minCellX = floorDiv64(chunkX - TREE_GRASS_RADIUS, TREE_SITE_SPACING);
  int64_t maxCellX = floorDiv64(chunkX + CHUNK_SIZE - 1 + TREE_GRASS_RADIUS, TREE_SITE_SPACING);
  int64_t minCellZ = floorDiv64(chunkZ - TREE_GRASS_RADIUS, TREE_SITE_SPACING);
  int64_t maxCellZ = floorDiv64(chunkZ + CHUNK_SIZE - 1 + TREE_GRASS_RADIUS, TREE_SITE_SPACING);

  for (int64_t cellX = minCellX; cellX <= maxCellX; cellX++)
    for (int64_t cellZ = minCellZ; cellZ <= maxCellZ; cellZ++) {
      TreeSite site;
      if (!treeSiteAt(terrainNoise, forestNoise, seed, cellX, cellZ, &site))
        continue;
      markTreeGrass(proximity, chunkX, chunkZ, &site);
      placeTree(chunk, chunkX, chunkZ, &site);
    }

  for (int x = 0; x < CHUNK_SIZE; x++)
    for (int z = 0; z < CHUNK_SIZE; z++) {
      int y = surfaceHeights[x][z];
      if (y < 0 || y >= CHUNK_HEIGHT || chunk->blocks[x][y][z].id != BLOCK_GRASS)
        continue;
      int threshold = proximity[x][z] == 2 ? 80 : proximity[x][z] == 1 ? 35 : forestBiomeAt(forestNoise, (float)(chunkX + x), (float)(chunkZ + z)) ? 10 : 2;
      if (coordinateHash(seed, chunkX + x, chunkZ + z, UINT32_C(0xa4093822)) % 100 < (uint32_t)threshold)
        chunk->blocks[x][y][z].id = BLOCK_LEAFY_GRASS;
    }
}

static void generateTerrainChunkWithNoise(Chunk* chunk, uint32_t seed, uint32_t version, const Noise* terrainNoise, const Noise* forestNoise) {
  int surfaceHeights[CHUNK_SIZE][CHUNK_SIZE];
  memset(chunk->blocks, 0, sizeof(chunk->blocks));
  generateBaseTerrain(chunk, terrainNoise, version >= 2 ? surfaceHeights : NULL);
  if (version >= 2) {
    decorateForestChunk(chunk, terrainNoise, forestNoise, seed, surfaceHeights);
    int64_t centerX = (int64_t)chunk->position.a * CHUNK_SIZE + CHUNK_SIZE / 2;
    int64_t centerZ = (int64_t)chunk->position.b * CHUNK_SIZE + CHUNK_SIZE / 2;
    chunk->id = forestBiomeAt(forestNoise, (float)centerX, (float)centerZ) ? BIOME_FOREST : BIOME_PLAINS;
  } else {
    chunk->id = BIOME_PLAINS;
  }
  chunk->dirty = true;
}

uint32_t worldSeed(void) {
  return currentSeed;
}

uint32_t worldGeneratorVersion(void) {
  return currentGeneratorVersion;
}

void generateTerrainChunkVersioned(Chunk* chunk, uint32_t seed, uint32_t version) {
  if (!chunk || !generatorVersionValid(version))
    return;
  Noise terrainNoise;
  Noise forestNoise;
  initNoise(&terrainNoise, seed);
  initNoise(&forestNoise, seed ^ UINT32_C(0x9e3779b9));
  generateTerrainChunkWithNoise(chunk, seed, version, &terrainNoise, &forestNoise);
}

void generateTerrainChunk(Chunk* chunk, uint32_t seed) {
  generateTerrainChunkVersioned(chunk, seed, WORLD_GENERATOR_VERSION);
}

bool initChunks(void) {
  return initChunksSeeded(0);
}

bool initChunksSeeded(uint32_t seed) {
  cleanupChunks();
  Noise terrainNoise;
  Noise forestNoise;
  initNoise(&terrainNoise, seed);
  initNoise(&forestNoise, seed ^ UINT32_C(0x9e3779b9));
  for (int x = 0; x < CHUNKS_PER_AXIS; x++) {
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Chunk* chunk = calloc(1, sizeof(*chunk));
      if (!chunk) {
        cleanupChunks();
        return false;
      }

      chunks[x][z] = chunk;
      chunk->dirty = true;
      chunk->position = (Vec2i){x - CHUNKS_PER_AXIS / 2, z - CHUNKS_PER_AXIS / 2};
      generateTerrainChunkWithNoise(chunk, seed, WORLD_GENERATOR_VERSION, &terrainNoise, &forestNoise);
    }
  }

  currentSeed = seed;
  currentGeneratorVersion = WORLD_GENERATOR_VERSION;
  activeNoise = terrainNoise;
  activeForestNoise = forestNoise;
  return true;
}

void cleanupChunks(void) {
  for (int x = 0; x < CHUNKS_PER_AXIS; x++) {
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      free(chunks[x][z]);
      chunks[x][z] = NULL;
    }
  }
}

static BiomeParameters biomeParameters[] = { // Plains biome - flatter, lower amplitude
    {0.03f, 0.5f, 0.3f, 4.0f},               // Lower frequency and amplitude for flatter terrain
                                             // Hills biome - more varied, higher amplitude
    {0.1f, 1.2f, 0.5f, 12.0f}};

static float getBiomeBlendFactor(const Noise* noise, float x, float z) {
  // Use a different noise frequency for biome transitions
  float biomeNoise = perlinWithNoise(noise, x * 0.02f, 0, z * 0.02f);
  return smoothstep(0.4f, 0.6f, biomeNoise);
}

static BiomeParameters biomeParametersAt(const Noise* noise, float x, float z) {
  float blendFactor = getBiomeBlendFactor(noise, x, z);
  BiomeParameters result;

  result.frequency = lerp(biomeParameters[BIOME_PLAINS].frequency, biomeParameters[BIOME_HILLS].frequency, blendFactor);
  result.amplitude = lerp(biomeParameters[BIOME_PLAINS].amplitude, biomeParameters[BIOME_HILLS].amplitude, blendFactor);
  result.persistence = lerp(biomeParameters[BIOME_PLAINS].persistence, biomeParameters[BIOME_HILLS].persistence, blendFactor);
  result.heightScale = lerp(biomeParameters[BIOME_PLAINS].heightScale, biomeParameters[BIOME_HILLS].heightScale, blendFactor);

  return result;
}

static float terrainHeight(const Noise* noise, float x, float z) {
  BiomeParameters params = biomeParametersAt(noise, x, z);
  float height = 0.0f;
  float amplitude = params.amplitude;
  float frequency = params.frequency;

  // Use more octaves for more detailed terrain
  for (int i = 0; i < 4; i++) {
    height += perlinWithNoise(noise, x * frequency, 0, z * frequency) * amplitude;
    amplitude *= params.persistence;
    frequency *= 2.0f;
  }

  return height * params.heightScale;
}

BiomeParameters getInterpolatedBiomeParameters(float x, float z) {
  return biomeParametersAt(currentSeed ? &activeNoise : NULL, x, z);
}

float getTerrainHeight(float x, float z) {
  return terrainHeight(currentSeed ? &activeNoise : NULL, x, z);
}

const char* getCurrentBiomeText(float x, float z) {
  if (currentGeneratorVersion >= 2 && chunks[0][0] && forestBiomeAt(&activeForestNoise, x, z))
    return "Forest";
  float blendFactor = getBiomeBlendFactor(currentSeed ? &activeNoise : NULL, x, z);
  if (blendFactor < 0.4f) {
    return "Plains";
  } else if (blendFactor > 0.6f) {
    return "Hills";
  } else {
    return "Transition"; // Optional: show when we're between biomes
  }
}

Chunk* getChunk(const Vec2i* pos) {
  if (!pos || pos->a < 0 || pos->a >= CHUNKS_PER_AXIS || pos->b < 0 || pos->b >= CHUNKS_PER_AXIS)
    return NULL;
  return chunks[pos->a][pos->b];
}

const Block* getBlock(const Vec3i* pos) {
  if (!pos || pos->y < 0 || pos->y >= CHUNK_HEIGHT || pos->x < -WORLD_SIZE / 2 || pos->x >= WORLD_SIZE / 2 || pos->z < -WORLD_SIZE / 2 || pos->z >= WORLD_SIZE / 2)
    return NULL;
  // Shift into the finite world's nonnegative block coordinates before dividing.
  int x = pos->x + WORLD_SIZE / 2;
  int z = pos->z + WORLD_SIZE / 2;
  Chunk* chunk = chunks[x / CHUNK_SIZE][z / CHUNK_SIZE];
  return chunk ? &chunk->blocks[x % CHUNK_SIZE][pos->y][z % CHUNK_SIZE] : NULL;
}

static void dirtyNeighbor(int x, int z) {
  Chunk* chunk = getChunk(&(Vec2i){x, z});
  if (chunk)
    chunk->dirty = true;
}

bool setBlock(const Vec3i* pos, int id) {
  const Block* old = getBlock(pos);
  if (!old || !blockIDValid(id))
    return false;
  if (old->id == id)
    return true;
  bool exposureChanged = blockIsSolid(old->id) != blockIsSolid(id) || blockOccludesFaces(old->id) != blockOccludesFaces(id);
  int x = pos->x + WORLD_SIZE / 2, z = pos->z + WORLD_SIZE / 2;
  int cx = x / CHUNK_SIZE, cz = z / CHUNK_SIZE;
  int lx = x % CHUNK_SIZE, lz = z % CHUNK_SIZE;
  Chunk* chunk = chunks[cx][cz];
  chunk->blocks[lx][pos->y][lz].id = (uint8_t)id;
  chunk->dirty = true;
  // Changes between opaque and cutout blocks also change neighbor visibility.
  if (exposureChanged) {
    if (lx == 0)
      dirtyNeighbor(cx - 1, cz);
    if (lx == CHUNK_SIZE - 1)
      dirtyNeighbor(cx + 1, cz);
    if (lz == 0)
      dirtyNeighbor(cx, cz - 1);
    if (lz == CHUNK_SIZE - 1)
      dirtyNeighbor(cx, cz + 1);
  }

  return true;
}

bool copyWorldBlocks(uint8_t* blocks, size_t count) {
  if (!blocks || count != WORLD_BLOCK_COUNT)
    return false;
  size_t offset = 0;
  for (int cx = 0; cx < CHUNKS_PER_AXIS; cx++)
    for (int cz = 0; cz < CHUNKS_PER_AXIS; cz++) {
      const Chunk* chunk = chunks[cx][cz];
      if (!chunk)
        return false;
      for (int x = 0; x < CHUNK_SIZE; x++)
        for (int y = 0; y < CHUNK_HEIGHT; y++)
          for (int z = 0; z < CHUNK_SIZE; z++) {
            uint8_t id = chunk->blocks[x][y][z].id;
            if (!blockIDValid(id))
              return false;
            blocks[offset++] = id;
          }
    }

  return true;
}

bool replaceWorldBlocksVersioned(uint32_t seed, uint32_t version, const uint8_t* blocks, size_t count) {
  if (!generatorVersionValid(version) || !blocks || count != WORLD_BLOCK_COUNT)
    return false;
  for (size_t i = 0; i < count; i++)
    if (!blockIDValid(blocks[i]))
      return false;
  Chunk* next[CHUNKS_PER_AXIS][CHUNKS_PER_AXIS] = {{0}};
  size_t offset = 0;
  for (int cx = 0; cx < CHUNKS_PER_AXIS; cx++)
    for (int cz = 0; cz < CHUNKS_PER_AXIS; cz++) {
      Chunk* chunk = calloc(1, sizeof(*chunk));
      if (!chunk) {
        for (int x = 0; x < CHUNKS_PER_AXIS; x++)
          for (int z = 0; z < CHUNKS_PER_AXIS; z++)
            free(next[x][z]);
        return false;
      }

      next[cx][cz] = chunk;
      chunk->position = (Vec2i){cx - CHUNKS_PER_AXIS / 2, cz - CHUNKS_PER_AXIS / 2};
      chunk->dirty = true;
      for (int x = 0; x < CHUNK_SIZE; x++)
        for (int y = 0; y < CHUNK_HEIGHT; y++)
          for (int z = 0; z < CHUNK_SIZE; z++)
            chunk->blocks[x][y][z].id = blocks[offset++];
    }

  cleanupChunks();
  memcpy(chunks, next, sizeof(chunks));
  currentSeed = seed;
  currentGeneratorVersion = version;
  initNoise(&activeNoise, seed);
  initNoise(&activeForestNoise, seed ^ UINT32_C(0x9e3779b9));
  return true;
}

bool replaceWorldBlocks(uint32_t seed, const uint8_t* blocks, size_t count) {
  return replaceWorldBlocksVersioned(seed, WORLD_GENERATOR_VERSION, blocks, count);
}
