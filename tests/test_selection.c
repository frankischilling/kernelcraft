#include "utils/raycast.h"
#include "world/world.h"
#include "world/edit.h"
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                                                                                                           \
  do {                                                                                                                                                                             \
    if (!(condition)) {                                                                                                                                                            \
      fprintf(stderr, "Selection test: %s (line %d)\n", #condition, __LINE__);                                                                                                     \
      exit(EXIT_FAILURE);                                                                                                                                                          \
    }                                                                                                                                                                              \
  } while (0)

static void clearWorld(void) {
  for (int x = 0; x < CHUNKS_PER_AXIS; x++)
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Chunk* chunk = getChunk(&(Vec2i){x, z});
      memset(chunk->blocks, 0, sizeof(chunk->blocks));
    }
}

static bool equal(Vec3i a, Vec3i b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

static void testAxes(void) {
  const Vec3i normals[] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
  clearWorld();
  Vec3i block = {0, 20, 0};
  CHECK(setBlock(&block, BLOCK_STONE));
  for (int i = 0; i < 6; i++) {
    Vec3i n = normals[i];
    Vec3 direction = {-100.0f * n.x, -100.0f * n.y, -100.0f * n.z};
    Vec3 origin = {0.5f + 3 * n.x, 20.5f + 3 * n.y, 0.5f + 3 * n.z};
    Ray ray = rayCast(origin, direction, 2.5f);
    CHECK(ray.hit && equal(ray.blockCoords, block) && equal(ray.normal, n));
    CHECK(ray.hasPlacementFace && equal(ray.adjacent, (Vec3i){block.x + n.x, block.y + n.y, block.z + n.z}));
    CHECK(fabsf(ray.distance - 2.5f) < 0.00001f);
    CHECK(!rayCast(origin, direction, 2.499f).hit);
    origin = (Vec3){0.5f + 0.5f * n.x, 20.5f + 0.5f * n.y, 0.5f + 0.5f * n.z};
    ray = rayCast(origin, direction, 0);
    CHECK(ray.hit && ray.distance == 0 && equal(ray.normal, n));
  }
  Ray inside = rayCast((Vec3){0.5f, 20.5f, 0.5f}, (Vec3){1, 0, 0}, 6);
  CHECK(inside.hit && inside.distance == 0 && !inside.hasPlacementFace);
  CHECK(equal(inside.normal, (Vec3i){0, 0, 0}));
}

static void testBoundaries(void) {
  clearWorld();
  CHECK(setBlock(&(Vec3i){-17, 20, -1}, BLOCK_STONE));
  Ray ray = rayCast((Vec3){-16, 20.5f, -0.5f}, (Vec3){-1, 0, 0}, 6);
  CHECK(ray.hit && equal(ray.blockCoords, (Vec3i){-17, 20, -1}) && equal(ray.normal, (Vec3i){1, 0, 0}));
  CHECK(!rayCast((Vec3){-16, 20.5f, -0.5f}, (Vec3){1, 0, 0}, 6).hit);
  CHECK(setBlock(&(Vec3i){-128, 20, 0}, BLOCK_STONE));
  ray = rayCast((Vec3){-130, 20.5f, 0.5f}, (Vec3){1, 0, 0}, 2);
  CHECK(ray.hit && ray.distance == 2 && ray.adjacent.x == -129);
  CHECK(!rayCast((Vec3){-128, 20.5f, 0.5f}, (Vec3){-1, 0, 0}, 6).hit);
  CHECK(setBlock(&(Vec3i){127, 63, 0}, BLOCK_STONE));
  ray = rayCast((Vec3){128, 63.5f, 0.5f}, (Vec3){-1, 0, 0}, 0);
  CHECK(ray.hit && ray.distance == 0 && ray.normal.x == 1);
  CHECK(!rayCast((Vec3){128, 63.5f, 0.5f}, (Vec3){0, -1, 0}, 6).hit);
  ray = rayCast((Vec3){127.5f, 64, 0.5f}, (Vec3){0, -1, 0}, 0);
  CHECK(ray.hit && ray.normal.y == 1);
  ray = rayCast((Vec3){1000000, 63.5f, 0.5f}, (Vec3){-1, 0, 0}, 1000000);
  CHECK(ray.hit && ray.blockCoords.x == 127 && ray.distance == 999872);
  CHECK(!rayCast((Vec3){FLT_MAX, 20, 0}, (Vec3){-1, 0, 0}, 6).hit);
}

static void testTiesAndInvalidInput(void) {
  clearWorld();
  CHECK(setBlock(&(Vec3i){1, 20, 0}, BLOCK_STONE));
  CHECK(setBlock(&(Vec3i){1, 20, 1}, BLOCK_STONE));
  Ray ray = rayCast((Vec3){0.5f, 20.5f, 0.5f}, (Vec3){1, 0, 1}, 6);
  CHECK(ray.hit && equal(ray.blockCoords, (Vec3i){1, 20, 1}));
  CHECK(equal(ray.normal, (Vec3i){-1, 0, 0}) && fabsf(ray.distance - sqrtf(0.5f)) < 0.00001f);
  CHECK(setBlock(&(Vec3i){1, 20, 1}, BLOCK_AIR));
  CHECK(!rayCast((Vec3){0.5f, 20.5f, 0.5f}, (Vec3){1, 0, 1}, 6).hit);
  CHECK(setBlock(&(Vec3i){1, 21, 1}, BLOCK_STONE));
  ray = rayCast((Vec3){0.5f, 20.5f, 0.5f}, (Vec3){1, 1, 1}, 6);
  CHECK(ray.hit && equal(ray.blockCoords, (Vec3i){1, 21, 1}) && ray.normal.x == -1);
  clearWorld();
  CHECK(setBlock(&(Vec3i){0, 20, 0}, BLOCK_STONE));
  // The ray crosses less than 0.01 unit of this block near an edge.
  ray = rayCast((Vec3){-0.001f, 20.999f, 0.5f}, (Vec3){1, 0.5f, 0}, 6);
  CHECK(ray.hit && equal(ray.blockCoords, (Vec3i){0, 20, 0}));
  Vec3 origin = {0.5f, 20.5f, 0.5f};
  CHECK(!rayCast(origin, (Vec3){0, 0, 0}, 6).hit);
  CHECK(!rayCast(origin, (Vec3){NAN, 0, 1}, 6).hit);
  CHECK(!rayCast(origin, (Vec3){INFINITY, 0, 1}, 6).hit);
  CHECK(!rayCast((Vec3){NAN, 0, 0}, (Vec3){1, 0, 0}, 6).hit);
  CHECK(!rayCast(origin, (Vec3){1, 0, 0}, -1).hit);
  CHECK(!rayCast(origin, (Vec3){1, 0, 0}, NAN).hit);
  CHECK(!rayCast(origin, (Vec3){1, 0, 0}, INFINITY).hit);
  CHECK(rayCast(origin, (Vec3){FLT_MIN, 0, 0}, 6).hit);
}

static void testPlacement(void) {
  clearWorld();
  Vec3i target = {0, 20, 0}, adjacent = {0, 20, -1};
  Vec3 eye = {0.5f, 20.5f, -2.5f}, direction = {0, 0, 1};
  CHECK(setBlock(&target, BLOCK_STONE));
  CHECK(editTarget(eye, direction, BLOCK_GRASS, true));
  CHECK(getBlock(&adjacent)->id == BLOCK_GRASS);
  CHECK(editTarget(eye, direction, BLOCK_GRASS, false));
  CHECK(getBlock(&adjacent)->id == BLOCK_AIR && getBlock(&target)->id == BLOCK_STONE);
  CHECK(!editTarget(eye, direction, BLOCK_AIR, true));
  CHECK(!editTarget(eye, direction, 256, true));
  CHECK(!editTarget((Vec3){0.5f, 20.5f, -6.1f}, direction, BLOCK_DIRT, false));
  // Body width overlaps the placement cell even when the eye's own cell does not.
  CHECK(!editTarget((Vec3){0.5f, 20.5f, -1.1f}, direction, BLOCK_DIRT, true));
  CHECK(!editTarget((Vec3){0.5f, 20.5f, 0.5f}, direction, BLOCK_DIRT, true));
  // The body extends below the eye, blocking a placement through the feet.
  CHECK(!editTarget((Vec3){0.5f, 22.4f, 0.5f}, (Vec3){0, -1, 0}, BLOCK_DIRT, true));
  CHECK(editTarget((Vec3){0.5f, 23.7f, 0.5f}, (Vec3){0, -1, 0}, BLOCK_DIRT, true));
  CHECK(getBlock(&(Vec3i){0, 21, 0})->id == BLOCK_DIRT);
  clearWorld();
  CHECK(setBlock(&(Vec3i){-128, 20, 0}, BLOCK_STONE));
  CHECK(!editTarget((Vec3){-130, 20.5f, 0.5f}, (Vec3){1, 0, 0}, BLOCK_STONE, true));
}

int main(void) {
  CHECK(initChunks());
  testAxes();
  testBoundaries();
  testTiesAndInvalidInput();
  testPlacement();
  cleanupChunks();
  puts("DDA selection tests passed");
  return 0;
}
