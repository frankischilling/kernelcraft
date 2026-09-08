#ifndef MESH_H
#define MESH_H

#include "chunk.h"
#include <stddef.h>
#include <stdint.h>

enum Material { MATERIAL_STONE, MATERIAL_DIRT, MATERIAL_GRASS_TOP, MATERIAL_GRASS_SIDE, MATERIAL_COUNT };

typedef struct {
  Vec3 position;
  Vec3 normal;
  float uv[2];
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

// Build after all neighboring chunks are populated. The caller owns the result.
bool buildChunkMesh(const Chunk* chunk, ChunkMesh* mesh);
void freeChunkMesh(ChunkMesh* mesh);

#endif
