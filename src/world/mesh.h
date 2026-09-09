#ifndef MESH_H
#define MESH_H

#include "chunk.h"
#include <stddef.h>
#include <stdint.h>

// Layers 4..6 remain reserved for shader-selected dirt and grass variants.
enum Material { MATERIAL_STONE, MATERIAL_DIRT, MATERIAL_GRASS_TOP, MATERIAL_GRASS_SIDE, MATERIAL_COBBLESTONE = 7, MATERIAL_COUNT };

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
