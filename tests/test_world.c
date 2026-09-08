#include "world/world.h"
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
  Block* block = getBlock(&center);
  block->id = BLOCK_STONE;
  for (int face = 0; face < 6; face++) {
    Vec3i neighbor = {center.x + vec3iFaceMap[face].x, center.y + vec3iFaceMap[face].y, center.z + vec3iFaceMap[face].z};
    getBlock(&neighbor)->id = BLOCK_STONE;
  }
  CHECK(is_block_occluded(&center, 1, NULL));
  Vec3i top = {0, 21, 0};
  getBlock(&top)->id = BLOCK_AIR;
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
    CHECK(vertex->uv[0] >= 0 && vertex->uv[0] <= 1 && vertex->uv[1] >= 0 && vertex->uv[1] <= 1);
  }
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
  getBlock(&seamLeft)->id = BLOCK_STONE;
  getBlock(&seamRight)->id = BLOCK_STONE;
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
  CHECK(mesh.indexCount == 27648);
  CHECK(mesh.surfaceBlocks == 4232);
  check_mesh_geometry(&mesh);
  freeChunkMesh(&mesh);
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
  size_t faces = 0, bytes = 0;
  for (int x = 0; x < CHUNKS_PER_AXIS; x++) {
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Vec2i index = {x, z};
      Chunk* chunk = getChunk(&index);
      ChunkMesh mesh;
      CHECK(buildChunkMesh(chunk, &mesh));
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
              Block* neighbor = getBlock(&neighbors[face]);
              if (!neighbor || neighbor->id == BLOCK_AIR)
                expectedFaces++;
            }
          }
      CHECK(mesh.indexCount == expectedFaces * 6);
      check_mesh_geometry(&mesh);
      faces += expectedFaces;
      bytes += mesh.vertexCount * sizeof(MeshVertex) + mesh.indexCount * sizeof(uint32_t);
      freeChunkMesh(&mesh);
    }
  }
  printf("Generated world: %zu exposed faces, %zu mesh bytes\n", faces, bytes);
}
#endif

int main(void) {
  initChunks();
#ifndef KERNELCRAFT_BASELINE
  test_generated_meshes();
#endif
  test_coordinates();
  test_occlusion();
  test_frustum();
#ifndef KERNELCRAFT_BASELINE
  test_mesh();
#endif
  cleanupChunks();
  if (failures) {
    fprintf(stderr, "%d checks failed\n", failures);
    return 1;
  }
  puts("World regression tests passed");
  return 0;
}
