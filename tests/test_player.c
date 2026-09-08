#include "world/player.h"
#include "world/world.h"
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                                                                                                           \
  do {                                                                                                                                                                             \
    if (!(condition)) {                                                                                                                                                            \
      fprintf(stderr, "Player test: %s (line %d)\n", #condition, __LINE__);                                                                                                        \
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

static void tick(Player* player, Vec3 wish, bool jump) {
  CHECK(playerAdvance(player, wish, jump, PLAYER_STEP_SECONDS) == 1);
  CHECK(playerCanOccupy(player->position));
}

static void testSpawn(void) {
  Player player = {0};
  CHECK(playerFindSpawn(&player, (Vec3){0, 10, 3}));
  CHECK(player.grounded && playerCanOccupy(player.position));
  Vec3 eye = playerEyePosition(&player);
  CHECK(fabsf(eye.y - player.position.y - PLAYER_EYE_HEIGHT) < 0.00001f);
  clearWorld();
  for (int y = 0; y < CHUNK_HEIGHT; y++)
    CHECK(setBlock(&(Vec3i){0, y, 0}, BLOCK_STONE));
  CHECK(playerFindSpawn(&player, (Vec3){0.5f, 50, 0.5f}));
  CHECK(playerCanOccupy(player.position) && player.grounded);
  CHECK(player.position.x != 0.5f || player.position.z != 0.5f);
  Vec3 saved = player.position;
  CHECK(!playerSetPosition(&player, (Vec3){0.5f, 2, 0.5f}));
  CHECK(player.position.x == saved.x && player.position.y == saved.y && player.position.z == saved.z);
  CHECK(!playerCanOccupy((Vec3){NAN, 0, 0}) && !playerCanOccupy((Vec3){FLT_MAX, 0, 0}));
  CHECK(!playerCanOccupy((Vec3){0, -0.1f, 0}) && !playerCanOccupy((Vec3){0, 63, 0}));
  CHECK(playerFindSpawn(&player, (Vec3){FLT_MAX, 0, -FLT_MAX}));
  CHECK(playerCanOccupy(player.position));
  for (int x = 0; x < CHUNKS_PER_AXIS; x++)
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Chunk* chunk = getChunk(&(Vec2i){x, z});
      memset(chunk->blocks, BLOCK_STONE, sizeof(chunk->blocks));
    }
  CHECK(!playerFindSpawn(&player, (Vec3){0, 0, 0}));
}

static void testFloorJumpAndCeiling(void) {
  clearWorld();
  CHECK(setBlock(&(Vec3i){-1, 3, -1}, BLOCK_STONE));
  Player player;
  CHECK(playerSetPosition(&player, (Vec3){-0.5f, 40, -0.5f}));
  for (int i = 0; i < 300; i++)
    tick(&player, (Vec3){0}, false);
  CHECK(player.position.y == 4 && player.grounded && player.velocity.y == 0);
  tick(&player, (Vec3){0}, true);
  CHECK(player.position.y > 4 && !player.grounded && player.velocity.y > 0);
  float peak = player.position.y;
  for (int i = 0; i < 160; i++) {
    tick(&player, (Vec3){0}, false);
    peak = fmaxf(peak, player.position.y);
  }
  CHECK(peak > 5.2f && peak < 5.4f && player.grounded && player.position.y == 4);
  CHECK(setBlock(&(Vec3i){-1, 6, -1}, BLOCK_STONE));
  tick(&player, (Vec3){0}, true);
  bool hitCeiling = false;
  for (int i = 0; i < 100; i++) {
    tick(&player, (Vec3){0}, false);
    CHECK(player.position.y + PLAYER_HEIGHT <= 6.00001f);
    if (player.velocity.y == 0 && !player.grounded)
      hitCeiling = true;
  }
  CHECK(hitCeiling && player.grounded && player.position.y == 4);
  CHECK(setBlock(&(Vec3i){-1, 3, -1}, BLOCK_AIR));
  tick(&player, (Vec3){0}, true);
  CHECK(player.position.y < 4 && !player.grounded); // Removed support cannot launch a jump.
  for (int i = 0; i < 200; i++)
    tick(&player, (Vec3){0}, false);
  CHECK(player.position.y == 0 && player.grounded);
}

static void testWallsSeamsAndCorners(void) {
  const int seams[] = {-16, 0, 16};
  for (int axis = 0; axis < 2; axis++)
    for (int side = -1; side <= 1; side += 2)
      for (int i = 0; i < 3; i++) {
        clearWorld();
        int seam = seams[i];
        for (int y = 0; y < 5; y++)
          for (int t = -4; t <= 4; t++)
            CHECK(setBlock(&(Vec3i){axis == 0 ? seam : t, y, axis == 0 ? t : seam}, BLOCK_STONE));
        Player player;
        float start = side > 0 ? seam - 2.0f : seam + 3.0f;
        CHECK(playerSetPosition(&player, (Vec3){axis == 0 ? start : 0.5f, 0, axis == 0 ? 0.5f : start}));
        Vec3 wish = {axis == 0 ? side : 0, 0, axis == 0 ? 0 : side};
        for (int step = 0; step < 120; step++)
          tick(&player, wish, false);
        float position = axis == 0 ? player.position.x : player.position.z;
        float expected = side > 0 ? seam - PLAYER_RADIUS : seam + 1 + PLAYER_RADIUS;
        CHECK(fabsf(position - expected) < 0.00001f);
      }
  clearWorld();
  for (int y = 0; y < 5; y++) {
    for (int t = -4; t <= 4; t++) {
      CHECK(setBlock(&(Vec3i){1, y, t}, BLOCK_STONE));
      CHECK(setBlock(&(Vec3i){t, y, 1}, BLOCK_STONE));
    }
  }
  Player player;
  CHECK(playerSetPosition(&player, (Vec3){-1, 0, -1}));
  for (int i = 0; i < 120; i++)
    tick(&player, (Vec3){1, 0, 1}, false);
  CHECK(fabsf(player.position.x - 0.7f) < 0.00001f && fabsf(player.position.z - 0.7f) < 0.00001f);
  for (int i = 0; i < 30; i++)
    tick(&player, (Vec3){1, 0, -1}, false);
  CHECK(player.position.z < 0 && fabsf(player.position.x - 0.7f) < 0.00001f);
  clearWorld();
  CHECK(playerSetPosition(&player, (Vec3){127, 0, -127}));
  for (int i = 0; i < 120; i++)
    tick(&player, (Vec3){1, 0, -1}, false);
  CHECK(player.position.x <= 128 - PLAYER_RADIUS && player.position.z >= -128 + PLAYER_RADIUS);
}

static void testContactRounding(void) {
  clearWorld();
  for (int y = 0; y < 3; y++)
    CHECK(setBlock(&(Vec3i){3, y, 0}, BLOCK_STONE));
  Player player;
  CHECK(playerSetPosition(&player, (Vec3){0x1.599998p+1f, 0, 0.5f}));
  tick(&player, (Vec3){0x1.4p-18f, 0, 0}, false);
  float contact = player.position.x;
  tick(&player, (Vec3){-1, 0, 0}, false);
  CHECK(player.position.x < contact);
  clearWorld();
  for (int y = 0; y < 3; y++)
    CHECK(setBlock(&(Vec3i){-63, y, 0}, BLOCK_STONE));
  CHECK(playerSetPosition(&player, (Vec3){-63.300655364990234f, 0, 0.5f}));
  // A normal walking direction one degree from parallel can hit the same edge.
  tick(&player, (Vec3){0.017452405765652657f, 0, 0.99984771013259888f}, false);
  contact = player.position.x;
  tick(&player, (Vec3){-1, 0, 0}, false);
  CHECK(player.position.x < contact);
}

static void testTiming(void) {
  clearWorld();
  Player fine, coarse, diagonal;
  CHECK(playerSetPosition(&fine, (Vec3){-10, 0, -10}));
  coarse = diagonal = fine;
  for (int i = 0; i < 120; i++)
    tick(&fine, (Vec3){1, 0, 0}, false);
  for (int i = 0; i < 30; i++)
    CHECK(playerAdvance(&coarse, (Vec3){1, 0, 0}, false, 1.0 / 30) == 4);
  for (int i = 0; i < 120; i++)
    tick(&diagonal, (Vec3){1, 0, 1}, false);
  CHECK(fabsf(fine.position.x - (-5.5f)) < 0.0001f && fine.position.x == coarse.position.x);
  Vec3 start = {-10, 0, -10};
  CHECK(fabsf(vec3_distance(&diagonal.position, &start) - 4.5f) < 0.0001f);
  CHECK(playerSetPosition(&fine, (Vec3){0, 0, 0}));
  CHECK(playerAdvance(&fine, (Vec3){1, 0, 0}, false, 1000) == PLAYER_MAX_STEPS);
  CHECK(fine.position.x <= 0.30001f && fine.accumulator < PLAYER_STEP_SECONDS);
  CHECK(playerAdvance(&fine, (Vec3){0}, true, PLAYER_STEP_SECONDS / 2) == 0);
  CHECK(playerAdvance(&fine, (Vec3){0}, false, PLAYER_STEP_SECONDS / 2) == 1);
  CHECK(fine.velocity.y > 0); // Preserve a press until the next simulation tick.
  Vec3 saved = fine.position;
  CHECK(playerAdvance(&fine, (Vec3){0}, false, NAN) == 0);
  CHECK(playerAdvance(&fine, (Vec3){0}, false, -1) == 0);
  CHECK(playerAdvance(&fine, (Vec3){NAN, 0, 0}, false, 1) == 0);
  CHECK(fine.position.x == saved.x && fine.position.y == saved.y && fine.position.z == saved.z);
  playerResetTiming(&fine);
  CHECK(fine.accumulator == 0 && !fine.jumpPending);
}

static void testFastFallAndAirborneJump(void) {
  clearWorld();
  Player player;
  CHECK(playerSetPosition(&player, (Vec3){0.5f, 62, 0.5f}));
  player.velocity.y = PLAYER_JUMP_SPEED;
  bool hitTop = false;
  for (int i = 0; i < 180; i++) {
    tick(&player, (Vec3){0}, false);
    CHECK((double)player.position.y + PLAYER_HEIGHT <= 64);
    CHECK(player.velocity.y >= -PLAYER_TERMINAL_SPEED);
    if (player.velocity.y == 0 && !player.grounded)
      hitTop = true;
  }
  CHECK(hitTop);
  CHECK(setBlock(&(Vec3i){0, 1, 0}, BLOCK_STONE));
  CHECK(playerSetPosition(&player, (Vec3){0.5f, 2.2f, 0.5f}));
  player.velocity.y = -PLAYER_TERMINAL_SPEED;
  tick(&player, (Vec3){0}, true);
  CHECK(player.position.y == 2 && player.grounded && player.velocity.y == 0);
  tick(&player, (Vec3){0}, false);
  CHECK(player.position.y == 2); // Airborne press cannot launch on landing.
}

int main(void) {
  CHECK(initChunks());
  testSpawn();
  testFloorJumpAndCeiling();
  testWallsSeamsAndCorners();
  testContactRounding();
  testTiming();
  testFastFallAndAirborneJump();
  cleanupChunks();
  Player player;
  CHECK(!playerFindSpawn(&player, (Vec3){0}) && !playerCanOccupy((Vec3){0}));
  puts("Player collision, spawn, jump, and fixed-step tests passed");
  return 0;
}
