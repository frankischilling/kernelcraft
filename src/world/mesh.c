#include "mesh.h"
#include "world.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>

static enum Material faceMaterial(uint8_t block, int face) {
  if (block == BLOCK_STONE)
    return MATERIAL_STONE;
  if (block == BLOCK_DIRT || face == BOTTOM)
    return MATERIAL_DIRT;
  return face == TOP ? MATERIAL_GRASS_TOP : MATERIAL_GRASS_SIDE;
}

bool buildChunkMesh(const Chunk* chunk, ChunkMesh* mesh) {
  memset(mesh, 0, sizeof(*mesh));
  uint8_t exposed[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE] = {0};
  size_t faceCounts[MATERIAL_COUNT] = {0};
  int originX = chunk->position.a * CHUNK_SIZE;
  int originZ = chunk->position.b * CHUNK_SIZE;
  mesh->min = (Vec3){FLT_MAX, FLT_MAX, FLT_MAX};
  mesh->max = (Vec3){-FLT_MAX, -FLT_MAX, -FLT_MAX};

  for (int x = 0; x < CHUNK_SIZE; x++) {
    for (int y = 0; y < CHUNK_HEIGHT; y++) {
      for (int z = 0; z < CHUNK_SIZE; z++) {
        uint8_t id = chunk->blocks[x][y][z].id;
        if (!blockIsSolid(id))
          continue;
        for (int face = 0; face < 6; face++) {
          Vec3i pos = {originX + x + vec3iFaceMap[face].x, y + vec3iFaceMap[face].y, originZ + z + vec3iFaceMap[face].z};
          const Block* neighbor = getBlock(&pos);
          if (neighbor && blockIsSolid(neighbor->id))
            continue;
          exposed[x][y][z] |= (uint8_t)(1u << face);
          faceCounts[faceMaterial(id, face)]++;
        }
        if (!exposed[x][y][z])
          continue;
        mesh->surfaceBlocks++;
        mesh->min.x = fminf(mesh->min.x, (originX + x) * CUBE_SIZE);
        mesh->min.y = fminf(mesh->min.y, y * CUBE_SIZE);
        mesh->min.z = fminf(mesh->min.z, (originZ + z) * CUBE_SIZE);
        mesh->max.x = fmaxf(mesh->max.x, (originX + x + 1) * CUBE_SIZE);
        mesh->max.y = fmaxf(mesh->max.y, (y + 1) * CUBE_SIZE);
        mesh->max.z = fmaxf(mesh->max.z, (originZ + z + 1) * CUBE_SIZE);
      }
    }
  }

  size_t nextFace[MATERIAL_COUNT];
  size_t totalFaces = 0;
  for (int material = 0; material < MATERIAL_COUNT; material++) {
    nextFace[material] = totalFaces;
    mesh->batches[material] = (MeshBatch){totalFaces * 6, faceCounts[material] * 6};
    totalFaces += faceCounts[material];
  }
  if (!totalFaces) {
    mesh->min = mesh->max = (Vec3)VEC3_ZERO;
    return true;
  }
  mesh->vertexCount = totalFaces * 4;
  mesh->indexCount = totalFaces * 6;
  mesh->vertices = malloc(mesh->vertexCount * sizeof(*mesh->vertices));
  mesh->indices = malloc(mesh->indexCount * sizeof(*mesh->indices));
  if (!mesh->vertices || !mesh->indices) {
    freeChunkMesh(mesh);
    return false;
  }

  const int corners[4] = {0, 1, 2, 4};
  const uint32_t outward[6] = {0, 1, 2, 2, 3, 0};
  const uint32_t reversed[6] = {0, 2, 1, 2, 0, 3};
  for (int x = 0; x < CHUNK_SIZE; x++) {
    for (int y = 0; y < CHUNK_HEIGHT; y++) {
      for (int z = 0; z < CHUNK_SIZE; z++) {
        for (int face = 0; face < 6; face++) {
          if (!(exposed[x][y][z] & (1u << face)))
            continue;
          size_t slot = nextFace[faceMaterial(chunk->blocks[x][y][z].id, face)]++;
          const float* source = getCubeFaceVertices(face);
          for (int corner = 0; corner < 4; corner++) {
            const float* vertex = source + corners[corner] * 8;
            mesh->vertices[slot * 4 + corner] =
                (MeshVertex){{(originX + x + 0.5f + vertex[0]) * CUBE_SIZE, (y + 0.5f + vertex[1]) * CUBE_SIZE, (originZ + z + 0.5f + vertex[2]) * CUBE_SIZE},
                             {vertex[3], vertex[4], vertex[5]},
                             {vertex[6], vertex[7]}};
          }
          const uint32_t* winding = (face == RIGHT || face == TOP || face == REAR) ? reversed : outward;
          for (int i = 0; i < 6; i++)
            mesh->indices[slot * 6 + i] = (uint32_t)(slot * 4) + winding[i];
        }
      }
    }
  }
  return true;
}

void freeChunkMesh(ChunkMesh* mesh) {
  free(mesh->vertices);
  free(mesh->indices);
  memset(mesh, 0, sizeof(*mesh));
}
