#ifndef MESH_H
#define MESH_H

#include "chunk.h"
#include <stddef.h>
#include <stdint.h>

// Existing layer numbers are stable. Leafy ground uses the supplied variant directly.
enum Material {
  MATERIAL_STONE,
  MATERIAL_DIRT,
  MATERIAL_GRASS_TOP,
  MATERIAL_GRASS_SIDE,
  MATERIAL_DIRT_ROCKS,
  MATERIAL_GRASS_TOP_LEAVES,
  MATERIAL_GRASS_BUG,
  MATERIAL_COBBLESTONE,
  MATERIAL_OAK_PLANKS,
  MATERIAL_STONE_BRICKS,
  MATERIAL_OAK_LOG_SIDE,
  MATERIAL_OAK_LOG_TOP,
  MATERIAL_OAK_LEAVES,
  MATERIAL_COUNT
};

typedef struct {
  Vec3 position;
  Vec3 normal;
  float uv[2];
  float material; // Texture-array layer, constant across a rectangle.
} MeshVertex;

typedef struct {
  size_t firstIndex;
  size_t indexCount;
} MeshBatch;

typedef struct {
  MeshVertex* vertices;
  uint32_t* indices;
  size_t vertexCount;
  size_t indexCount;
  MeshBatch batches[MATERIAL_COUNT];
  int surfaceBlocks;
  Vec3 min, max;
} ChunkMesh;

// Greedy rectangles with outward winding and one texture repeat per block.
// Build after all neighboring chunks are populated. The caller owns the result;
// pass an unused output or free its previous buffers before rebuilding.
bool buildChunkMesh(const Chunk* chunk, ChunkMesh* mesh);
void freeChunkMesh(ChunkMesh* mesh);

#endif
