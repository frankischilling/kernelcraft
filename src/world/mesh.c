#include "mesh.h"
#include "world.h"
#include <stdlib.h>
#include <string.h>

static enum Material faceMaterial(uint8_t block, int face) {
  if (block == BLOCK_OAK_PLANKS)
    return MATERIAL_OAK_PLANKS;
  if (block == BLOCK_STONE_BRICKS)
    return MATERIAL_STONE_BRICKS;
  if (block == BLOCK_COBBLESTONE)
    return MATERIAL_COBBLESTONE;
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
enum { MESH_SLICES = CHUNK_HEIGHT > CHUNK_SIZE ? CHUNK_HEIGHT : CHUNK_SIZE };

typedef struct {
  uint8_t face, slice, col, row, width, height, material;
} MeshRectangle;

_Static_assert(CHUNK_SIZE <= UINT8_MAX && CHUNK_HEIGHT <= UINT8_MAX && MATERIAL_COUNT <= UINT8_MAX, "Rectangle fields must fit in a byte");

static size_t meshRectangles(const Chunk* chunk, const uint8_t exposed[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE], const bool slices[6][MESH_SLICES], size_t counts[MATERIAL_COUNT],
                             MeshRectangle* rectangles) {
  size_t count = 0;
  const int dimensions[3] = {CHUNK_SIZE, CHUNK_HEIGHT, CHUNK_SIZE};
  for (int face = 0; face < 6; face++) {
    int axis = face == RIGHT || face == LEFT ? 0 : face == TOP || face == BOTTOM ? 1 : 2;
    int u = axis == 0 ? 2 : 0, v = axis == 1 ? 2 : 1;
    int columns = dimensions[u], rows = dimensions[v];
    for (int slice = 0; slice < dimensions[axis]; slice++) {
      if (!slices[face][slice])
        continue;
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

          counts[key - 1]++;
          rectangles[count++] = (MeshRectangle){(uint8_t)face, (uint8_t)slice, (uint8_t)col, (uint8_t)row, (uint8_t)width, (uint8_t)height, (uint8_t)(key - 1)};

          for (int dy = 0; dy < height; dy++)
            memset(mask + (row + dy) * columns + col, 0, (size_t)width);
          col += width;
        }
    }
  }
  return count;
}

static void emitRectangles(const Chunk* chunk, const MeshRectangle* rectangles, size_t count, size_t slots[MATERIAL_COUNT], ChunkMesh* output) {
  const int corners[4] = {0, 1, 2, 4};
  const uint32_t outward[6] = {0, 1, 2, 2, 3, 0};
  const uint32_t reversed[6] = {0, 2, 1, 2, 0, 3};
  const int origin[3] = {chunk->position.a * CHUNK_SIZE, 0, chunk->position.b * CHUNK_SIZE};
  for (size_t rectangle = 0; rectangle < count; rectangle++) {
    const MeshRectangle* rect = &rectangles[rectangle];
    int face = rect->face;
    int axis = face == RIGHT || face == LEFT ? 0 : face == TOP || face == BOTTOM ? 1 : 2;
    int u = axis == 0 ? 2 : 0, v = axis == 1 ? 2 : 1;
    size_t slot = slots[rect->material]++;
    int p[3], extent[3] = {1, 1, 1};
    p[axis] = rect->slice;
    p[u] = rect->col;
    p[v] = rect->row;
    extent[u] = rect->width;
    extent[v] = rect->height;
    const float* source = getCubeFaceVertices(face);
    for (int corner = 0; corner < 4; corner++) {
      const float* vertex = source + corners[corner] * 8;
      output->vertices[slot * 4 + corner] = (MeshVertex){{(origin[0] + p[0] + (vertex[0] + 0.5f) * extent[0]) * CUBE_SIZE, (p[1] + (vertex[1] + 0.5f) * extent[1]) * CUBE_SIZE,
                                                          (origin[2] + p[2] + (vertex[2] + 0.5f) * extent[2]) * CUBE_SIZE},
                                                         {vertex[3], vertex[4], vertex[5]},
                                                         {vertex[6] * rect->width, vertex[7] * rect->height},
                                                         (float)rect->material};
    }
    const uint32_t* winding = face == RIGHT || face == TOP || face == REAR ? reversed : outward;
    for (int i = 0; i < 6; i++)
      output->indices[slot * 6 + i] = (uint32_t)(slot * 4) + winding[i];
  }
}

// Keep the hot mesh builder's entry alignment independent of other code size.
// Native paired tests exposed substantial edit-cost sensitivity to text layout.
// This is only a placement hint; other compilers retain the same C11 algorithm.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((aligned(64)))
#endif
bool buildChunkMesh(const Chunk* chunk, ChunkMesh* mesh) {
  memset(mesh, 0, sizeof(*mesh));
  uint8_t exposed[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE] = {0};
  bool slices[6][MESH_SLICES] = {{false}};
  // Cache solidity once. The one-cell border preserves cross-chunk/world edges
  // without repeating world coordinate lookup for every interior neighbor.
  bool solid[CHUNK_SIZE + 2][CHUNK_HEIGHT + 2][CHUNK_SIZE + 2] = {{{false}}};
  size_t faceCounts[MATERIAL_COUNT] = {0};
  size_t exposedFaces = 0;
  int originX = chunk->position.a * CHUNK_SIZE;
  int originZ = chunk->position.b * CHUNK_SIZE;
  Vec3i min = {CHUNK_SIZE, CHUNK_HEIGHT, CHUNK_SIZE}, max = {0, 0, 0};

  bool anySolid = false;
  for (int x = 0; x < CHUNK_SIZE; x++)
    for (int y = 0; y < CHUNK_HEIGHT; y++)
      for (int z = 0; z < CHUNK_SIZE; z++) {
        solid[x + 1][y + 1][z + 1] = blockIsSolid(chunk->blocks[x][y][z].id);
        anySolid |= solid[x + 1][y + 1][z + 1];
      }
  if (!anySolid) {
    mesh->min = mesh->max = (Vec3)VEC3_ZERO;
    return true;
  }
  for (int y = 0; y < CHUNK_HEIGHT; y++)
    for (int edge = 0; edge < CHUNK_SIZE; edge++) {
      const Block* left = getBlock(&(Vec3i){originX - 1, y, originZ + edge});
      const Block* right = getBlock(&(Vec3i){originX + CHUNK_SIZE, y, originZ + edge});
      const Block* back = getBlock(&(Vec3i){originX + edge, y, originZ - 1});
      const Block* front = getBlock(&(Vec3i){originX + edge, y, originZ + CHUNK_SIZE});
      solid[0][y + 1][edge + 1] = left && blockIsSolid(left->id);
      solid[CHUNK_SIZE + 1][y + 1][edge + 1] = right && blockIsSolid(right->id);
      solid[edge + 1][y + 1][0] = back && blockIsSolid(back->id);
      solid[edge + 1][y + 1][CHUNK_SIZE + 1] = front && blockIsSolid(front->id);
    }

  for (int x = 0; x < CHUNK_SIZE; x++) {
    for (int y = 0; y < CHUNK_HEIGHT; y++) {
      for (int z = 0; z < CHUNK_SIZE; z++) {
        if (!solid[x + 1][y + 1][z + 1])
          continue;
        for (int face = 0; face < 6; face++) {
          if (solid[x + 1 + vec3iFaceMap[face].x][y + 1 + vec3iFaceMap[face].y][z + 1 + vec3iFaceMap[face].z])
            continue;
          exposedFaces++;
          exposed[x][y][z] |= (uint8_t)(1u << face);
          int slice = face == RIGHT || face == LEFT ? x : face == TOP || face == BOTTOM ? y : z;
          slices[face][slice] = true;
        }

        if (!exposed[x][y][z])
          continue;
        mesh->surfaceBlocks++;
        if (x < min.x)
          min.x = x;
        if (y < min.y)
          min.y = y;
        if (z < min.z)
          min.z = z;
        if (x + 1 > max.x)
          max.x = x + 1;
        if (y + 1 > max.y)
          max.y = y + 1;
        if (z + 1 > max.z)
          max.z = z + 1;
      }
    }
  }

  if (!exposedFaces) {
    mesh->min = mesh->max = (Vec3)VEC3_ZERO;
    return true;
  }

  // Surface bounds are integral block coordinates; scale only the final box.
  mesh->min = (Vec3){(originX + min.x) * CUBE_SIZE, min.y * CUBE_SIZE, (originZ + min.z) * CUBE_SIZE};
  mesh->max = (Vec3){(originX + max.x) * CUBE_SIZE, max.y * CUBE_SIZE, (originZ + max.z) * CUBE_SIZE};

  // Each rectangle covers at least one exposed unit face. Bound temporary
  // storage by that count, then retain the sweep order within each material.
  if (exposedFaces > SIZE_MAX / sizeof(MeshRectangle)) {
    freeChunkMesh(mesh);
    return false;
  }
  MeshRectangle* rectangles = malloc(exposedFaces * sizeof(*rectangles));
  if (!rectangles) {
    freeChunkMesh(mesh);
    return false;
  }
  size_t rectangleCount = meshRectangles(chunk, exposed, slices, faceCounts, rectangles);
  size_t nextFace[MATERIAL_COUNT];
  size_t totalFaces = 0;
  for (int material = 0; material < MATERIAL_COUNT; material++) {
    nextFace[material] = totalFaces;
    mesh->batches[material] = (MeshBatch){totalFaces * 6, faceCounts[material] * 6};
    totalFaces += faceCounts[material];
  }

  if (totalFaces > UINT32_MAX / 4 || totalFaces > SIZE_MAX / (4 * sizeof(MeshVertex)) || totalFaces > SIZE_MAX / (6 * sizeof(uint32_t))) {
    free(rectangles);
    freeChunkMesh(mesh);
    return false;
  }

  mesh->vertexCount = totalFaces * 4;
  mesh->indexCount = totalFaces * 6;
  mesh->vertices = malloc(mesh->vertexCount * sizeof(*mesh->vertices));
  mesh->indices = malloc(mesh->indexCount * sizeof(*mesh->indices));
  if (!mesh->vertices || !mesh->indices) {
    free(rectangles);
    freeChunkMesh(mesh);
    return false;
  }

  emitRectangles(chunk, rectangles, rectangleCount, nextFace, mesh);
  free(rectangles);
  return true;
}

void freeChunkMesh(ChunkMesh* mesh) {
  free(mesh->vertices);
  free(mesh->indices);
  memset(mesh, 0, sizeof(*mesh));
}
