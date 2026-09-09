#include "world/world.h"
#include "world/mesh.h"
#include "world/hotbar.h"
#include "world/edit.h"
#include "world/player.h"
#include "utils/raycast.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                                                                                                           \
  do {                                                                                                                                                                             \
    if (!(condition)) {                                                                                                                                                            \
      fprintf(stderr, "Edit test: %s (line %d)\n", #condition, __LINE__);                                                                                                          \
      exit(EXIT_FAILURE);                                                                                                                                                          \
    }                                                                                                                                                                              \
  } while (0)

static void resetWorld(void) {
  for (int x = 0; x < CHUNKS_PER_AXIS; x++)
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Chunk* chunk = getChunk(&(Vec2i){x, z});
      memset(chunk->blocks, 0, sizeof(chunk->blocks));
      chunk->dirty = false;
    }
}

static int dirtyCount(void) {
  int count = 0;
  for (int x = 0; x < CHUNKS_PER_AXIS; x++)
    for (int z = 0; z < CHUNKS_PER_AXIS; z++)
      count += getChunk(&(Vec2i){x, z})->dirty;
  return count;
}

static void testEdits(void) {
  CHECK(dirtyCount() == CHUNKS_PER_AXIS * CHUNKS_PER_AXIS);
  resetWorld();
  // Slot 4 is a distinct, solid, placeable cobblestone block (persisted ID 4).
  CHECK(hotbarBlock(3) == 4);
  CHECK(setBlock(&(Vec3i){-1, 20, -1}, hotbarBlock(3)));
  CHECK(getBlock(&(Vec3i){-1, 20, -1})->id == 4 && blockIsSolid(4));
  CHECK(dirtyCount() == 3);
  for (int id = 5; id <= 6; id++) {
    resetWorld();
    CHECK(hotbarBlock(id - 1) == id);
    CHECK(setBlock(&(Vec3i){-1, 20, -1}, id));
    CHECK(getBlock(&(Vec3i){-1, 20, -1})->id == id && dirtyCount() == 3);
    CHECK(!playerCanOccupy((Vec3){-0.5f, 20, -0.5f}));
    Ray selected = rayCast((Vec3){-0.5f, 20.5f, -3}, (Vec3){0, 0, 1}, 6);
    CHECK(selected.hit && selected.blockCoords.x == -1 && selected.blockCoords.z == -1);
    CHECK(!editTarget((Vec3){-0.5f, 20.5f, -3}, (Vec3){0, 0, 1}, (Vec3){-0.5f, 20, -1.5f}, false, id, true));
    CHECK(editTarget((Vec3){-0.5f, 20.5f, -3}, (Vec3){0, 0, 1}, (Vec3){-0.5f, 20, -3}, false, id, true));
    CHECK(getBlock(&(Vec3i){-1, 20, -2})->id == id);
  }
  CHECK(!blockIDValid(7) && !setBlock(&(Vec3i){0, 20, 0}, 7));
  for (int slot = 6; slot < HOTBAR_SLOT_COUNT; slot++)
    CHECK(hotbarBlock(slot) == BLOCK_AIR);
  resetWorld();
  Vec3i invalid[] = {{INT_MIN, 0, 0}, {INT_MAX, 0, 0}, {-129, 0, 0}, {128, 0, 0}, {0, -1, 0}, {0, 64, 0}, {0, 0, -129}, {0, 0, 128}};
  CHECK(getChunk(NULL) == NULL && getBlock(NULL) == NULL);
  CHECK(!setBlock(NULL, BLOCK_STONE));
  for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
    CHECK(getBlock(&invalid[i]) == NULL);
    CHECK(!setBlock(&invalid[i], BLOCK_STONE));
  }
  Vec3i interior = {1, 20, 1};
  CHECK(!setBlock(&interior, -1) && !setBlock(&interior, 256));
  CHECK(dirtyCount() == 0);
  CHECK(setBlock(&interior, BLOCK_AIR) && dirtyCount() == 0);
  CHECK(setBlock(&interior, BLOCK_GRASS) && dirtyCount() == 1);
  CHECK(getBlock(&interior)->id == BLOCK_GRASS && blockIsSolid(getBlock(&interior)->id));
  CHECK(!blockIsSolid(BLOCK_AIR) && !blockIsSolid(256));
  getChunk(&(Vec2i){8, 8})->dirty = false;
  CHECK(setBlock(&interior, BLOCK_GRASS) && dirtyCount() == 0);

  // A corner affects its own chunk and two face neighbors, never the diagonal.
  const Vec3i corners[] = {{0, 63, 0}, {-1, 0, -1}, {-16, 20, 16}, {15, 20, -17}};
  for (size_t i = 0; i < sizeof(corners) / sizeof(corners[0]); i++) {
    resetWorld();
    CHECK(setBlock(&corners[i], BLOCK_STONE) && dirtyCount() == 3);
    Vec3i pos = corners[i];
    Vec2i owner = blockToChunk(&pos);
    Vec3i local = getLocal(&pos);
    int nx = owner.a + (local.x == 0 ? -1 : 1);
    int nz = owner.b + (local.z == 0 ? -1 : 1);
    for (int x = 0; x < CHUNKS_PER_AXIS; x++)
      for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
        bool expected = (x == owner.a && z == owner.b) || (x == nx && z == owner.b) || (x == owner.a && z == nz);
        CHECK(getChunk(&(Vec2i){x, z})->dirty == expected);
      }
    for (int x = 0; x < CHUNKS_PER_AXIS; x++)
      for (int z = 0; z < CHUNKS_PER_AXIS; z++)
        getChunk(&(Vec2i){x, z})->dirty = false;
    CHECK(setBlock(&corners[i], BLOCK_DIRT) && dirtyCount() == 1);
  }
  resetWorld();
  CHECK(setBlock(&(Vec3i){-128, 0, -128}, BLOCK_STONE) && dirtyCount() == 1);
  resetWorld();
  CHECK(setBlock(&(Vec3i){127, 63, 127}, BLOCK_STONE) && dirtyCount() == 1);

  resetWorld();
  Vec3i left = {-1, 20, 1}, right = {0, 20, 1};
  CHECK(setBlock(&left, BLOCK_STONE) && setBlock(&right, BLOCK_STONE));
  Chunk* neighbor = getChunk(&(Vec2i){7, 8});
  ChunkMesh mesh;
  CHECK(buildChunkMesh(neighbor, &mesh) && mesh.indexCount == 30);
  freeChunkMesh(&mesh);
  neighbor->dirty = false;
  CHECK(setBlock(&right, BLOCK_AIR) && neighbor->dirty);
  CHECK(buildChunkMesh(neighbor, &mesh) && mesh.indexCount == 36);
  freeChunkMesh(&mesh);
}

static void testHandBreaking(void) {
  const Vec3 eye = {-0.5f, 20.5f, -3}, direction = {0, 0, 1};
  const Vec3i target = {-1, 20, -1}, behind = {-1, 20, 0};
  const struct {
    int block;
    double seconds;
  } cases[] = {{BLOCK_DIRT, 0.5}, {BLOCK_GRASS, 0.75}, {BLOCK_STONE, 1.5}, {BLOCK_COBBLESTONE, 2.0}, {5, 1.0}, {6, 2.0}};
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    for (int rate = 30; rate <= 120; rate *= 2) {
      resetWorld();
      CHECK(setBlock(&target, cases[i].block) && setBlock(&behind, BLOCK_STONE));
      BlockBreaking breaking = {0};
      CHECK(!advanceBlockBreaking(&breaking, eye, direction, 0));
      CHECK(blockBreakingProgress(&breaking) == 0 && getBlock(&target)->id == cases[i].block);
      int frames = (int)ceil(cases[i].seconds * rate);
      for (int frame = 1; frame < frames; frame++) {
        CHECK(!advanceBlockBreaking(&breaking, eye, direction, 1.0 / rate));
        CHECK(getBlock(&target)->id == cases[i].block);
        CHECK(fabs(blockBreakingProgress(&breaking) - (double)frame / rate / cases[i].seconds) < 0.00001);
      }
      // Remove only the target, and rebuild its negative-coordinate seam neighbors.
      for (int x = 0; x < CHUNKS_PER_AXIS; x++)
        for (int z = 0; z < CHUNKS_PER_AXIS; z++)
          getChunk(&(Vec2i){x, z})->dirty = false;
      CHECK(advanceBlockBreaking(&breaking, eye, direction, 1.0 / rate));
      CHECK(getBlock(&target)->id == BLOCK_AIR && getBlock(&behind)->id == BLOCK_STONE);
      CHECK(dirtyCount() == 3 && !breaking.active && blockBreakingProgress(&breaking) == 0);
      CHECK(!advanceBlockBreaking(&breaking, eye, direction, 10));
      CHECK(breaking.elapsed == 0 && getBlock(&behind)->id == BLOCK_STONE);
    }
  }
  resetWorld();
  CHECK(setBlock(&target, BLOCK_DIRT));
  BlockBreaking breaking = {0};
  CHECK(!advanceBlockBreaking(&breaking, eye, direction, 0));
  for (int i = 0; i < 4; i++)
    CHECK(!advanceBlockBreaking(&breaking, eye, direction, 0.1));
  CHECK(!advanceBlockBreaking(&breaking, eye, direction, 0.099999));
  CHECK(getBlock(&target)->id == BLOCK_DIRT);
  CHECK(advanceBlockBreaking(&breaking, eye, direction, 0.000001));

  CHECK(setBlock(&target, BLOCK_DIRT));
  CHECK(!advanceBlockBreaking(&breaking, eye, direction, 0));
  CHECK(!advanceBlockBreaking(&breaking, eye, direction, 1000));
  CHECK(fabs(blockBreakingProgress(&breaking) - 0.2) < 0.00001);
  CHECK(!advanceBlockBreaking(&breaking, eye, direction, 0) && getBlock(&target)->id == BLOCK_DIRT);
  const double invalid[] = {-1, NAN, INFINITY};
  for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
    CHECK(!advanceBlockBreaking(&breaking, eye, direction, invalid[i]) && !breaking.active);
    CHECK(getBlock(&target)->id == BLOCK_DIRT);
    CHECK(!advanceBlockBreaking(&breaking, eye, direction, 0.1));
  }
  CHECK(!advanceBlockBreaking(&breaking, eye, direction, 0.1));
  resetBlockBreaking(&breaking);
  CHECK(!breaking.active && blockBreakingProgress(&breaking) == 0);
  CHECK(!advanceBlockBreaking(&breaking, eye, direction, 0.1) && breaking.elapsed == 0);
  CHECK(!advanceBlockBreaking(&breaking, eye, direction, 0.1));
  // Changing a block's material resets its hand rate and accrued time.
  CHECK(setBlock(&target, BLOCK_STONE));
  CHECK(!advanceBlockBreaking(&breaking, eye, direction, 0.1) && breaking.elapsed == 0);
  CHECK(!advanceBlockBreaking(&breaking, eye, direction, 0.1));
  CHECK(!advanceBlockBreaking(&breaking, eye, (Vec3){0, 1, 0}, 0.1) && !breaking.active);
  CHECK(!advanceBlockBreaking(&breaking, (Vec3){-0.5f, 20.5f, -7}, direction, 0)); // Reach exactly six.
  CHECK(breaking.active);
  CHECK(!advanceBlockBreaking(&breaking, (Vec3){-0.5f, 20.5f, -7.01f}, direction, 0.1) && !breaking.active);
  CHECK(!advanceBlockBreaking(&breaking, eye, direction, 0));
  CHECK(setBlock(&behind, BLOCK_STONE) && setBlock(&target, BLOCK_AIR));
  CHECK(!advanceBlockBreaking(&breaking, eye, direction, 0.1) && breaking.elapsed == 0 && breaking.target.z == 0);
  CHECK(!advanceBlockBreaking(&breaking, eye, (Vec3){NAN, 0, 1}, 0.1) && !breaking.active);
  CHECK(!advanceBlockBreaking(NULL, eye, direction, 0.1) && blockBreakingProgress(NULL) == 0);
  resetBlockBreaking(NULL);
}

int main(void) {
  CHECK(initChunks());
  testEdits();
  testHandBreaking();
  cleanupChunks();
  CHECK(getBlock(&(Vec3i){0, 0, 0}) == NULL);
  CHECK(!setBlock(&(Vec3i){0, 0, 0}, BLOCK_STONE));
  puts("Block edit tests passed");
  return 0;
}
