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

/* U follows the first edge of the existing cube face; V follows its second.
 * Only equal materials on the same oriented plane merge. Lighting is evaluated
 * per fragment from world position and the constant face normal, so it does not
 * add a vertex-lighting constraint to the merge key. */
static void meshRectangles(const Chunk* chunk, const uint8_t exposed[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE], size_t slots[MATERIAL_COUNT], ChunkMesh* output) {
  const int corners[4] = {0, 1, 2, 4};
  const uint32_t outward[6] = {0, 1, 2, 2, 3, 0};
  const uint32_t reversed[6] = {0, 2, 1, 2, 0, 3};
  const int dimensions[3] = {CHUNK_SIZE, CHUNK_HEIGHT, CHUNK_SIZE};
  const int origin[3] = {chunk->position.a * CHUNK_SIZE, 0, chunk->position.b * CHUNK_SIZE};
  for (int face = 0; face < 6; face++) {
    int axis = face == RIGHT || face == LEFT ? 0 : face == TOP || face == BOTTOM ? 1 : 2;
    int u = axis == 0 ? 2 : 0, v = axis == 1 ? 2 : 1;
    int columns = dimensions[u], rows = dimensions[v];
    for (int slice = 0; slice < dimensions[axis]; slice++) {
      uint8_t mask[CHUNK_SIZE * CHUNK_HEIGHT] = {0};
      for (int row = 0; row < rows; row++)
        for (int col = 0; col < columns; col++) {
          int p[3];
          p[axis] = slice;
          p[u] = col;
          p[v] = row;
          if (exposed[p[0]][p[1]][p[2]] & (1u << face))
            mask[row * columns + col] = (uint8_t)(faceMaterial(chunk->blocks[p[0]][p[1]][p[2]].id, face) + 1);
        }
      for (int row = 0; row < rows; row++)
        for (int col = 0; col < columns;) {
          uint8_t key = mask[row * columns + col];
          if (!key) {
            col++;
            continue;
          }
          int width = 1, height = 1;
          while (col + width < columns && mask[row * columns + col + width] == key)
            width++;
          while (row + height < rows) {
            int next = 0;
            while (next < width && mask[(row + height) * columns + col + next] == key)
              next++;
            if (next != width)
              break;
            height++;
          }
          size_t slot = slots[key - 1]++;
          if (output) {
            int p[3], extent[3] = {1, 1, 1};
            p[axis] = slice;
            p[u] = col;
            p[v] = row;
            extent[u] = width;
            extent[v] = height;
            const float* source = getCubeFaceVertices(face);
            for (int corner = 0; corner < 4; corner++) {
              const float* vertex = source + corners[corner] * 8;
              output->vertices[slot * 4 + corner] =
                  (MeshVertex){{(origin[0] + p[0] + (vertex[0] + 0.5f) * extent[0]) * CUBE_SIZE, (p[1] + (vertex[1] + 0.5f) * extent[1]) * CUBE_SIZE,
                                (origin[2] + p[2] + (vertex[2] + 0.5f) * extent[2]) * CUBE_SIZE},
                               {vertex[3], vertex[4], vertex[5]},
                               {vertex[6] * width, vertex[7] * height},
                               (float)(key - 1)};
            }
            const uint32_t* winding = face == RIGHT || face == TOP || face == REAR ? reversed : outward;
            for (int i = 0; i < 6; i++)
              output->indices[slot * 6 + i] = (uint32_t)(slot * 4) + winding[i];
          }
          for (int dy = 0; dy < height; dy++)
            memset(mask + (row + dy) * columns + col, 0, (size_t)width);
          col += width;
        }
    }
  }
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

  // Count first, then allocate exact buffers and repeat the deterministic sweep.
  meshRectangles(chunk, exposed, faceCounts, NULL);
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

  meshRectangles(chunk, exposed, nextFace, mesh);
  return true;
}

void freeChunkMesh(ChunkMesh* mesh) {
  free(mesh->vertices);
  free(mesh->indices);
  memset(mesh, 0, sizeof(*mesh));
}
