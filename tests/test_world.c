#include "world/world.h"
#include "graphics/frustum.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef KERNELCRAFT_BASELINE
#include "world/mesh.h"
#endif

static int failures;
#define CHECK(condition)                                                                                                                                                           \
  do {                                                                                                                                                                             \
    if (!(condition)) {                                                                                                                                                            \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                                                                                                              \
      failures++;                                                                                                                                                                  \
    }                                                                                                                                                                              \
  } while (0)

#ifndef KERNELCRAFT_BASELINE
#include "occlusion_cpu_checks.h"
#endif

static void writeBlock(Vec3i* pos, int id) {
#ifdef KERNELCRAFT_BASELINE
  getBlock(pos)->id = id;
#else
  CHECK(setBlock(pos, id));
#endif
}

static void test_coordinates(void) {
  Vec3 position = {-0.1f, -0.1f, -16.1f};
  Vec3i block = worldToBlock(&position);
  CHECK(block.x == -1 && block.y == -1 && block.z == -17);
  Vec2i chunk = worldToChunk(&position);
  CHECK(chunk.a == -1 && chunk.b == -2);
  Vec3i negative = {-17, 0, -1};
  Vec3i local = getLocal(&negative);
  CHECK(local.x == 15 && local.z == 15);
  Vec3i upper = {0, CHUNK_HEIGHT, 0};
  CHECK(getBlock(&upper) == NULL);
  Vec3i outside[] = {{-129, 0, 0}, {128, 0, 0}, {0, -1, 0}, {0, 0, 128}, {0, 0, -129}};
  for (size_t i = 0; i < sizeof(outside) / sizeof(outside[0]); i++)
    CHECK(getBlock(&outside[i]) == NULL);
}

static void test_occlusion(void) {
  Vec3i center = {0, 20, 0};
  writeBlock(&center, BLOCK_STONE);
  for (int face = 0; face < 6; face++) {
    Vec3i neighbor = {center.x + vec3iFaceMap[face].x, center.y + vec3iFaceMap[face].y, center.z + vec3iFaceMap[face].z};
    writeBlock(&neighbor, BLOCK_STONE);
  }
  CHECK(is_block_occluded(&center, 1, NULL));
  Vec3i top = {0, 21, 0};
  writeBlock(&top, BLOCK_AIR);
  CHECK(!is_block_occluded(&center, 1, NULL));
}

static void test_frustum(void) {
  Mat4 projection, view;
  Vec3 eye = {10, 20, 30}, target = {10, 20, 29}, up = {0, 1, 0};
  mat4_lookAt(view, &eye, &target, &up);
  mat4_perspective(projection, 90, 1, 1, 10);
  Frustum frustum;
  frustum_update(&frustum, projection, view);
  Vec3 inside = {10, 20, 25}, behind = {10, 20, 32}, far = {10, 20, 18}, side = {20, 20, 25};
  CHECK(frustum_cube_visible(&frustum, &inside, 1, NULL));
  CHECK(!frustum_cube_visible(&frustum, &behind, 1, NULL));
  CHECK(!frustum_cube_visible(&frustum, &far, 1, NULL));
  CHECK(!frustum_cube_visible(&frustum, &side, 1, NULL));
}

#ifndef KERNELCRAFT_BASELINE
static void clear_world(void) {
  for (int x = 0; x < CHUNKS_PER_AXIS; x++) {
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Vec2i pos = {x, z};
      memset(getChunk(&pos)->blocks, 0, sizeof(getChunk(&pos)->blocks));
    }
  }
}

static void check_mesh_geometry(const ChunkMesh* mesh) {
  for (size_t i = 0; i < mesh->indexCount; i += 3) {
    uint32_t a = mesh->indices[i], b = mesh->indices[i + 1], c = mesh->indices[i + 2];
    CHECK(a < mesh->vertexCount && b < mesh->vertexCount && c < mesh->vertexCount);
    if (a >= mesh->vertexCount || b >= mesh->vertexCount || c >= mesh->vertexCount)
      continue;
    const MeshVertex *v0 = &mesh->vertices[a], *v1 = &mesh->vertices[b], *v2 = &mesh->vertices[c];
    Vec3 ab, ac, normal;
    vec3_subtract(&ab, &v1->position, &v0->position);
    vec3_subtract(&ac, &v2->position, &v0->position);
    vec3_cross(&normal, &ab, &ac);
    CHECK(vec3_dot(&normal, &v0->normal) > 0);
  }
  for (size_t i = 0; i < mesh->vertexCount; i++) {
    const MeshVertex* vertex = &mesh->vertices[i];
    CHECK(vertex->position.x >= mesh->min.x && vertex->position.x <= mesh->max.x);
    CHECK(vertex->position.y >= mesh->min.y && vertex->position.y <= mesh->max.y);
    CHECK(vertex->position.z >= mesh->min.z && vertex->position.z <= mesh->max.z);
    CHECK(isfinite(vertex->uv[0]) && isfinite(vertex->uv[1]));
  }
}

/* Expand every rectangle back into unit faces and compare with block queries.
 * This detects holes, duplicate faces, internal faces, and wrong materials
 * without depending on
 * the mesher's choice of rectangles. */
static size_t check_mesh_coverage(const Chunk* chunk, const ChunkMesh* mesh) {
  unsigned char seen[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE][6] = {0};
  size_t area = 0, nextIndex = 0;
  CHECK(mesh->vertexCount * 6 == mesh->indexCount * 4);
  for (int material = 0; material < MATERIAL_COUNT; material++) {
    MeshBatch batch = mesh->batches[material];
    CHECK(batch.firstIndex == nextIndex && batch.indexCount % 6 == 0);
    nextIndex += batch.indexCount;
    CHECK(nextIndex <= mesh->indexCount);
    if (nextIndex > mesh->indexCount)
      return 0;
    for (size_t index = batch.firstIndex; index < nextIndex; index += 6) {
      size_t slot = index / 6;
      const MeshVertex* vertices = mesh->vertices + slot * 4;
      Vec3 n = vertices[0].normal;
      int face = n.x == 1 ? RIGHT : n.x == -1 ? LEFT : n.y == 1 ? TOP : n.y == -1 ? BOTTOM : n.z == 1 ? FRONT : REAR;
      Vec3i direction = vec3iFaceMap[face];
      CHECK(n.x == direction.x && n.y == direction.y && n.z == direction.z);
      float low[3] = {INFINITY, INFINITY, INFINITY}, high[3] = {-INFINITY, -INFINITY, -INFINITY};
      for (int c = 0; c < 4; c++) {
        CHECK(vertices[c].material == material);
        const float p[] = {vertices[c].position.x / CUBE_SIZE, vertices[c].position.y / CUBE_SIZE, vertices[c].position.z / CUBE_SIZE};
        CHECK(vertices[c].normal.x == n.x && vertices[c].normal.y == n.y && vertices[c].normal.z == n.z);
        for (int axis = 0; axis < 3; axis++) {
          CHECK(isfinite(p[axis]) && p[axis] == floorf(p[axis]));
          low[axis] = fminf(low[axis], p[axis]);
          high[axis] = fmaxf(high[axis], p[axis]);
        }
      }
      int axis = n.x ? 0 : n.y ? 1 : 2;
      int u = axis == 0 ? 2 : 0, v = axis == 1 ? 2 : 1;
      CHECK(low[axis] == high[axis] && high[u] > low[u] && high[v] > low[v]);
      unsigned references[4] = {0};
      for (int i = 0; i < 6; i++) {
        uint32_t vertex = mesh->indices[index + i];
        CHECK(vertex >= slot * 4 && vertex < (slot + 1) * 4);
        if (vertex >= slot * 4 && vertex < (slot + 1) * 4)
          references[vertex - slot * 4]++;
      }
      int diagonal[2] = {0}, shared = 0;
      for (int c = 0; c < 4; c++) {
        CHECK(references[c] == 1 || references[c] == 2);
        if (references[c] == 2 && shared < 2)
          diagonal[shared++] = c;
      }
      CHECK(shared == 2);
      Vec3 d;
      vec3_subtract(&d, &vertices[diagonal[0]].position, &vertices[diagonal[1]].position);
      float components[] = {d.x, d.y, d.z};
      CHECK(fabsf(components[u]) == (high[u] - low[u]) * CUBE_SIZE && fabsf(components[v]) == (high[v] - low[v]) * CUBE_SIZE);
      /* Existing cube mapping: U increases along z on X faces, x otherwise;
       * V increases along z on tops and decreases along the other V axes. */
      for (int c = 0; c < 4; c++) {
        float p[] = {vertices[c].position.x / CUBE_SIZE, vertices[c].position.y / CUBE_SIZE, vertices[c].position.z / CUBE_SIZE};
        CHECK(vertices[c].uv[0] == p[u] - low[u]);
        CHECK(vertices[c].uv[1] == (face == TOP ? p[v] - low[v] : high[v] - p[v]));
      }
      for (int a = (int)low[u]; a < (int)high[u]; a++)
        for (int b = (int)low[v]; b < (int)high[v]; b++) {
          int cell[3] = {(int)low[0], (int)low[1], (int)low[2]};
          cell[u] = a;
          cell[v] = b;
          if (n.x + n.y + n.z > 0)
            cell[axis]--;
          Vec3i pos = {cell[0], cell[1], cell[2]};
          int x = pos.x - chunk->position.a * CHUNK_SIZE, y = pos.y, z = pos.z - chunk->position.b * CHUNK_SIZE;
          CHECK(x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_HEIGHT && z >= 0 && z < CHUNK_SIZE);
          if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE)
            continue;
          CHECK(seen[x][y][z][face]++ == 0);
          int id = chunk->blocks[x][y][z].id;
          int expected = id == 5                              ? 8
                         : id == 6                            ? 9
                         : id == BLOCK_COBBLESTONE            ? MATERIAL_COBBLESTONE
                         : id == BLOCK_STONE                  ? MATERIAL_STONE
                         : id == BLOCK_DIRT || face == BOTTOM ? MATERIAL_DIRT
                         : face == TOP                        ? MATERIAL_GRASS_TOP
                                                              : MATERIAL_GRASS_SIDE;
          CHECK(blockIsSolid(id) && material == expected);
          Vec3i neighbor = {pos.x + direction.x, pos.y + direction.y, pos.z + direction.z};
          const Block* block = getBlock(&neighbor);
          CHECK(!block || !blockIsSolid(block->id));
          area++;
        }
    }
  }
  CHECK(nextIndex == mesh->indexCount);
  for (int x = 0; x < CHUNK_SIZE; x++)
    for (int y = 0; y < CHUNK_HEIGHT; y++)
      for (int z = 0; z < CHUNK_SIZE; z++)
        for (int face = 0; face < 6; face++) {
          Vec3i d = vec3iFaceMap[face];
          Vec3i pos = {chunk->position.a * CHUNK_SIZE + x + d.x, y + d.y, chunk->position.b * CHUNK_SIZE + z + d.z};
          const Block* neighbor = getBlock(&pos);
          bool exposed = blockIsSolid(chunk->blocks[x][y][z].id) && (!neighbor || !blockIsSolid(neighbor->id));
          CHECK(seen[x][y][z][face] == (unsigned)exposed);
        }
  return area;
}

static void test_mesh(void) {
  clear_world();
  Vec2i index = {8, 8};
  Chunk* chunk = getChunk(&index);
  ChunkMesh mesh;
  CHECK(buildChunkMesh(chunk, &mesh));
  CHECK(mesh.indexCount == 0 && mesh.vertexCount == 0 && mesh.surfaceBlocks == 0);
  freeChunkMesh(&mesh);

  chunk->blocks[0][0][0].id = BLOCK_GRASS;
  CHECK(buildChunkMesh(chunk, &mesh));
  CHECK(mesh.indexCount == 36 && mesh.vertexCount == 24 && mesh.surfaceBlocks == 1);
  CHECK(mesh.batches[MATERIAL_GRASS_TOP].indexCount == 6);
  CHECK(mesh.batches[MATERIAL_DIRT].indexCount == 6);
  CHECK(mesh.batches[MATERIAL_GRASS_SIDE].indexCount == 24);
  CHECK(mesh.min.x == 0 && mesh.max.x == 1 && mesh.min.y == 0 && mesh.max.y == 1);
  check_mesh_geometry(&mesh);
  freeChunkMesh(&mesh);

  chunk->blocks[1][0][0].id = BLOCK_STONE;
  CHECK(buildChunkMesh(chunk, &mesh));
  CHECK(mesh.indexCount == 60 && mesh.surfaceBlocks == 2);
  CHECK(mesh.batches[MATERIAL_STONE].indexCount == 30);
  check_mesh_geometry(&mesh);
  freeChunkMesh(&mesh);

  clear_world();
  Vec3i seamLeft = {-1, 63, 0}, seamRight = {0, 63, 0};
  writeBlock(&seamLeft, BLOCK_STONE);
  writeBlock(&seamRight, BLOCK_STONE);
  CHECK(buildChunkMesh(chunk, &mesh));
  CHECK(mesh.indexCount == 30 && mesh.surfaceBlocks == 1);
  CHECK(mesh.min.y == 63 && mesh.max.y == 64);
  check_mesh_geometry(&mesh);
  freeChunkMesh(&mesh);
  index.a = 7;
  CHECK(buildChunkMesh(getChunk(&index), &mesh));
  CHECK(mesh.indexCount == 30 && mesh.min.x == -1 && mesh.max.x == 0);
  check_mesh_geometry(&mesh);
  freeChunkMesh(&mesh);

  clear_world();
  memset(chunk->blocks, BLOCK_STONE, sizeof(chunk->blocks));
  CHECK(buildChunkMesh(chunk, &mesh));
  /* Six outside rectangles: 2 * (16*16 + 16*64 + 16*64). */
  CHECK(mesh.indexCount == 36);
  CHECK(mesh.surfaceBlocks == 4232);
  check_mesh_geometry(&mesh);
  CHECK(check_mesh_coverage(chunk, &mesh) == 4608);
  freeChunkMesh(&mesh);

  clear_world();
  chunk->blocks[0][0][0].id = BLOCK_COBBLESTONE;
  chunk->blocks[1][0][0].id = BLOCK_COBBLESTONE;
  CHECK(buildChunkMesh(chunk, &mesh));
  CHECK(mesh.indexCount == 36 && mesh.batches[MATERIAL_COBBLESTONE].indexCount == 36);
  CHECK(check_mesh_coverage(chunk, &mesh) == 10);
  freeChunkMesh(&mesh);
  chunk->blocks[1][0][0].id = BLOCK_STONE;
  CHECK(buildChunkMesh(chunk, &mesh));
  CHECK(mesh.indexCount == 60 && mesh.batches[MATERIAL_COBBLESTONE].indexCount == 30 && mesh.batches[MATERIAL_STONE].indexCount == 30);
  CHECK(check_mesh_coverage(chunk, &mesh) == 10);
  freeChunkMesh(&mesh);
}

static void test_greedy_shapes(void) {
  for (int id = 5; id <= 6; id++) {
    clear_world();
    Chunk* buildingChunk = getChunk(&(Vec2i){7, 7});
    buildingChunk->blocks[1][20][1].id = (uint8_t)id;
    buildingChunk->blocks[2][20][1].id = (uint8_t)id;
    ChunkMesh buildingMesh;
    CHECK(buildChunkMesh(buildingChunk, &buildingMesh));
    CHECK(buildingMesh.indexCount == 36 && buildingMesh.batches[id + 3].indexCount == 36);
    CHECK(check_mesh_coverage(buildingChunk, &buildingMesh) == 10);
    check_mesh_geometry(&buildingMesh);
    freeChunkMesh(&buildingMesh);
    buildingChunk->blocks[2][20][1].id = (uint8_t)(id == 5 ? 6 : 5);
    CHECK(buildChunkMesh(buildingChunk, &buildingMesh));
    CHECK(buildingMesh.indexCount == 60 && buildingMesh.batches[8].indexCount == 30 && buildingMesh.batches[9].indexCount == 30);
    CHECK(check_mesh_coverage(buildingChunk, &buildingMesh) == 10);
    freeChunkMesh(&buildingMesh);
  }
  clear_world();
  Chunk* chunk = getChunk(&(Vec2i){7, 7});
  for (int x = 2; x < 7; x++)
    for (int y = 3; y < 10; y++)
      for (int z = 4; z < 7; z++)
        chunk->blocks[x][y][z].id = BLOCK_STONE;
  ChunkMesh mesh;
  CHECK(buildChunkMesh(chunk, &mesh));
  CHECK(mesh.indexCount == 36 && mesh.vertexCount == 24);
  CHECK(check_mesh_coverage(chunk, &mesh) == 142);
  check_mesh_geometry(&mesh);
  freeChunkMesh(&mesh);
  /* Holes, stair steps, mixed grass/stone/dirt, and neighboring chunks. */
  for (int pattern = 0; pattern < 4; pattern++) {
    clear_world();
    for (int x = -18; x < 2; x++)
      for (int z = -18; z < 2; z++)
        for (int y = 0; y < 6; y++) {
          int id = pattern == 0 ? BLOCK_GRASS : 1 + (x * x + y + z * z) % 4;
          if ((pattern == 1 && (x + y + z) % 2) || (pattern == 2 && y > (x * x + z * z) % 6) || (pattern == 3 && x % 3 == 0 && z % 3 == 0))
            id = BLOCK_AIR;
          CHECK(setBlock(&(Vec3i){x, y, z}, id));
        }
    for (int x = 6; x <= 8; x++)
      for (int z = 6; z <= 8; z++) {
        Chunk* current = getChunk(&(Vec2i){x, z});
        CHECK(buildChunkMesh(current, &mesh));
        check_mesh_geometry(&mesh);
        check_mesh_coverage(current, &mesh);
        freeChunkMesh(&mesh);
      }
  }
}

static void test_generated_meshes(void) {
  // Fingerprint recorded from the unchanged generator at revision 12de8dd.
  uint64_t terrainHash = UINT64_C(14695981039346656037);
  for (int x = -128; x < 128; x++)
    for (int y = 0; y < 64; y++)
      for (int z = -128; z < 128; z++) {
        Vec3i pos = {x, y, z};
        terrainHash = (terrainHash ^ getBlock(&pos)->id) * UINT64_C(1099511628211);
      }
  CHECK(terrainHash == UINT64_C(512190482430576247));
  size_t faces = 0, quads = 0, bytes = 0;
  for (int x = 0; x < CHUNKS_PER_AXIS; x++) {
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Vec2i index = {x, z};
      Chunk* chunk = getChunk(&index);
      ChunkMesh mesh;
      CHECK(buildChunkMesh(chunk, &mesh));
      check_occluder_mesh(&mesh);
      size_t expectedFaces = 0;
      for (int i = 0; i < CHUNK_SIZE; i++)
        for (int j = 0; j < CHUNK_HEIGHT; j++)
          for (int k = 0; k < CHUNK_SIZE; k++) {
            if (chunk->blocks[i][j][k].id == BLOCK_AIR)
              continue;
            Vec3i neighbors[6] = {{chunk->position.a * 16 + i + 1, j, chunk->position.b * 16 + k}, {chunk->position.a * 16 + i - 1, j, chunk->position.b * 16 + k},
                                  {chunk->position.a * 16 + i, j + 1, chunk->position.b * 16 + k}, {chunk->position.a * 16 + i, j - 1, chunk->position.b * 16 + k},
                                  {chunk->position.a * 16 + i, j, chunk->position.b * 16 + k + 1}, {chunk->position.a * 16 + i, j, chunk->position.b * 16 + k - 1}};
            for (int face = 0; face < 6; face++) {
              const Block* neighbor = getBlock(&neighbors[face]);
              if (!neighbor || neighbor->id == BLOCK_AIR)
                expectedFaces++;
            }
          }
      CHECK(check_mesh_coverage(chunk, &mesh) == expectedFaces);
      CHECK(mesh.indexCount <= expectedFaces * 6);
      check_mesh_geometry(&mesh);
      faces += expectedFaces;
      quads += mesh.indexCount / 6;
      bytes += mesh.vertexCount * sizeof(MeshVertex) + mesh.indexCount * sizeof(uint32_t);
      freeChunkMesh(&mesh);
    }
  }
  printf("Generated world: %zu exposed unit faces, %zu quads, %zu mesh bytes\n", faces, quads, bytes);
}
#endif

int main(void) {
  initChunks();
#ifndef KERNELCRAFT_BASELINE
  test_generated_meshes();
  test_software_occlusion();
  test_mesh_visibility();
#endif
  test_coordinates();
  test_occlusion();
  test_frustum();
#ifndef KERNELCRAFT_BASELINE
  test_mesh();
  test_greedy_shapes();
#endif
  cleanupChunks();
  if (failures) {
    fprintf(stderr, "%d checks failed\n", failures);
    return 1;
  }
  puts("World regression tests passed");
  return 0;
}
