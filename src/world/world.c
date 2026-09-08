/**
 * @file world/world.c
 * @brief World generation and rendering.
 * @author frankischilling, VladimirJanus
 * @version 0.1
 * @date 2024-11-19
 *
 */
#include "world.h"
#include <stdlib.h>

static Chunk* chunks[CHUNKS_PER_AXIS][CHUNKS_PER_AXIS];

bool initChunks(void) {
  cleanupChunks();
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
      int heights[CHUNK_SIZE][CHUNK_SIZE];
      for (int i = 0; i < CHUNK_SIZE; i++) {
        for (int k = 0; k < CHUNK_SIZE; k++) {
          heights[i][k] = (int)floorf(getTerrainHeight((chunk->position.a * CHUNK_SIZE + i) * CUBE_SIZE, (chunk->position.b * CHUNK_SIZE + k) * CUBE_SIZE));
        }
      }
      for (int i = 0; i < CHUNK_SIZE; i++) {
        for (int j = 0; j < CHUNK_HEIGHT; j++) {
          for (int k = 0; k < CHUNK_SIZE; k++) {
            int height = heights[i][k];
            if (j < height - DIRT_LAYERS)
              chunk->blocks[i][j][k].id = BLOCK_STONE;
            else if (j < height)
              chunk->blocks[i][j][k].id = BLOCK_DIRT;
            else if (j == height)
              chunk->blocks[i][j][k].id = BLOCK_GRASS;
          }
        }
      }
    }
  }
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
static float getBiomeBlendFactor(float x, float z) {
  // Use a different noise frequency for biome transitions
  float biomeNoise = perlin(x * 0.02f, 0, z * 0.02f);
  return smoothstep(0.4f, 0.6f, biomeNoise);
}
BiomeParameters getInterpolatedBiomeParameters(float x, float z) {
  float blendFactor = getBiomeBlendFactor(x, z);
  BiomeParameters result;

  result.frequency = lerp(biomeParameters[BIOME_PLAINS].frequency, biomeParameters[BIOME_HILLS].frequency, blendFactor);
  result.amplitude = lerp(biomeParameters[BIOME_PLAINS].amplitude, biomeParameters[BIOME_HILLS].amplitude, blendFactor);
  result.persistence = lerp(biomeParameters[BIOME_PLAINS].persistence, biomeParameters[BIOME_HILLS].persistence, blendFactor);
  result.heightScale = lerp(biomeParameters[BIOME_PLAINS].heightScale, biomeParameters[BIOME_HILLS].heightScale, blendFactor);

  return result;
}
// Add this function to get height based on biome
float getTerrainHeight(float x, float z) {
  BiomeParameters params = getInterpolatedBiomeParameters(x, z);
  float height = 0.0f;
  float amplitude = params.amplitude;
  float frequency = params.frequency;

  // Use more octaves for more detailed terrain
  for (int i = 0; i < 4; i++) {
    height += perlin(x * frequency, 0, z * frequency) * amplitude;
    amplitude *= params.persistence;
    frequency *= 2.0f;
  }

  return height * params.heightScale;
}
const char* getCurrentBiomeText(float x, float z) {
  float blendFactor = getBiomeBlendFactor(x, z);
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
  bool exposureChanged = blockIsSolid(old->id) != blockIsSolid(id);
  int x = pos->x + WORLD_SIZE / 2, z = pos->z + WORLD_SIZE / 2;
  int cx = x / CHUNK_SIZE, cz = z / CHUNK_SIZE;
  int lx = x % CHUNK_SIZE, lz = z % CHUNK_SIZE;
  Chunk* chunk = chunks[cx][cz];
  chunk->blocks[lx][pos->y][lz].id = (uint8_t)id;
  chunk->dirty = true;
  // A material-only change cannot expose a neighbor's face.
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
