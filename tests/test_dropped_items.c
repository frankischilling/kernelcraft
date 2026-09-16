#include "world/dropped_items.h"
#include "world/world.h"
#include "world/player.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                                                                                                           \
  do {                                                                                                                                                                             \
    if (!(condition)) {                                                                                                                                                            \
      fprintf(stderr, "Dropped-item test: %s (line %d)\n", #condition, __LINE__);                                                                                                  \
      exit(EXIT_FAILURE);                                                                                                                                                          \
    }                                                                                                                                                                              \
  } while (0)

static void clearWorld(void) {
  for (int x = 0; x < CHUNKS_PER_AXIS; x++)
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Chunk* chunk = getChunk(&(Vec2i){x, z});
      memset(chunk->blocks, 0, sizeof(chunk->blocks));
      chunk->dirty = false;
    }
}

static size_t activeCount(const DroppedItems* drops) {
  size_t count = 0;
  for (size_t i = 0; i < DROPPED_ITEM_CAPACITY; i++)
    count += drops->items[i].active;
  return count;
}

static uint32_t droppedCount(const DroppedItems* drops, uint16_t item) {
  uint32_t count = 0;
  for (size_t i = 0; i < DROPPED_ITEM_CAPACITY; i++)
    if (drops->items[i].active && drops->items[i].stack.item == item)
      count += drops->items[i].stack.count;
  return count;
}

static void fillCarried(Inventory* inventory, uint16_t item, uint16_t count) {
  inventoryClear(inventory);
  for (int slot = 0; slot < INVENTORY_CARRIED_SLOT_COUNT; slot++)
    inventory->carried[slot] = (ItemStack){item, count};
}

static void expectSpawnRejected(DroppedItems* drops, ItemStack stack, Vec3 position, Vec3 velocity, float pickupDelay) {
  DroppedItems before = *drops;
  CHECK(!droppedItemsSpawn(drops, stack, position, velocity, pickupDelay));
  CHECK(memcmp(drops, &before, sizeof(*drops)) == 0);
}

static void testValidationAndSpawn(void) {
  DroppedItems drops = {0};
  CHECK(droppedItemsValid(&drops));
  CHECK(!droppedItemsValid(NULL));
  CHECK(droppedItemPositionValid((Vec3){-WORLD_SIZE / 2.0f, 0, -WORLD_SIZE / 2.0f}));
  CHECK(droppedItemPositionValid((Vec3){WORLD_SIZE / 2.0f - 0.001f, CHUNK_HEIGHT - 0.001f, WORLD_SIZE / 2.0f - 0.001f}));
  CHECK(!droppedItemPositionValid((Vec3){WORLD_SIZE / 2.0f, 1, 0}));
  CHECK(!droppedItemPositionValid((Vec3){0, CHUNK_HEIGHT, 0}));
  CHECK(!droppedItemPositionValid((Vec3){NAN, 1, 0}));
  CHECK(!droppedItemPositionValid((Vec3){0, INFINITY, 0}));

  const Vec3 position = {0.5f, 5, 0.5f};
  const Vec3 velocity = {1, 2, 3};
  CHECK(!droppedItemsSpawn(NULL, (ItemStack){ITEM_DIRT, 1}, position, velocity, 0));
  expectSpawnRejected(&drops, (ItemStack){0}, position, velocity, 0);
  expectSpawnRejected(&drops, (ItemStack){ITEM_DIRT, INVENTORY_STACK_MAX + 1}, position, velocity, 0);
  expectSpawnRejected(&drops, (ItemStack){ITEM_ID_LAST + 1, 1}, position, velocity, 0);
  expectSpawnRejected(&drops, (ItemStack){ITEM_DIRT, 1}, (Vec3){NAN, 5, 0.5f}, velocity, 0);
  expectSpawnRejected(&drops, (ItemStack){ITEM_DIRT, 1}, (Vec3){0.5f, -INFINITY, 0.5f}, velocity, 0);
  expectSpawnRejected(&drops, (ItemStack){ITEM_DIRT, 1}, (Vec3){WORLD_SIZE / 2.0f, 5, 0.5f}, velocity, 0);
  expectSpawnRejected(&drops, (ItemStack){ITEM_DIRT, 1}, position, (Vec3){NAN, 0, 0}, 0);
  expectSpawnRejected(&drops, (ItemStack){ITEM_DIRT, 1}, position, (Vec3){0, INFINITY, 0}, 0);
  expectSpawnRejected(&drops, (ItemStack){ITEM_DIRT, 1}, position, (Vec3){20.01f, 0, 0}, 0);
  expectSpawnRejected(&drops, (ItemStack){ITEM_DIRT, 1}, position, velocity, NAN);
  expectSpawnRejected(&drops, (ItemStack){ITEM_DIRT, 1}, position, velocity, -0.01f);
  expectSpawnRejected(&drops, (ItemStack){ITEM_DIRT, 1}, position, velocity, 60.01f);

  CHECK(droppedItemsSpawn(&drops, (ItemStack){ITEM_DIRT, 7}, position, (Vec3){20, -20, 0}, 60));
  CHECK(activeCount(&drops) == 1 && drops.items[0].stack.item == ITEM_DIRT && drops.items[0].stack.count == 7);
  CHECK(drops.items[0].position.x == position.x && drops.items[0].pickupDelay == 60);
  CHECK(droppedItemsValid(&drops));

  DroppedItems invalid = {0};
  invalid.items[0] = (DroppedItem){.stack = {ITEM_DIRT, INVENTORY_STACK_MAX + 1}, .position = position, .active = true};
  CHECK(!droppedItemsValid(&invalid));
  invalid = (DroppedItems){0};
  invalid.accumulator = -1;
  CHECK(!droppedItemsValid(&invalid));
  invalid = (DroppedItems){0};
  invalid.animationSeconds = 60;
  CHECK(!droppedItemsValid(&invalid));
}

static void testMergingAndPoolRollback(void) {
  DroppedItems drops = {0};
  const Vec3 first = {0.5f, 5, 0.5f};
  const Vec3 nearby = {0.75f, 5, 0.5f};
  CHECK(droppedItemsSpawn(&drops, (ItemStack){ITEM_DIRT, 500}, first, (Vec3){0}, 0.1f));
  CHECK(droppedItemsSpawn(&drops, (ItemStack){ITEM_DIRT, 700}, nearby, (Vec3){1, 0, 0}, 0.5f));
  CHECK(activeCount(&drops) == 2 && droppedCount(&drops, ITEM_DIRT) == 1200);
  CHECK(drops.items[0].stack.count == INVENTORY_STACK_MAX && drops.items[0].pickupDelay == 0.5f);
  CHECK(drops.items[1].stack.count == 201 && drops.items[1].position.x == nearby.x && drops.items[1].pickupDelay == 0.5f);

  drops = (DroppedItems){0};
  for (size_t i = 0; i < DROPPED_ITEM_CAPACITY; i++)
    drops.items[i] = (DroppedItem){.stack = {ITEM_DIRT, INVENTORY_STACK_MAX}, .position = first, .active = true};
  drops.items[0].stack.count = INVENTORY_STACK_MAX - 1;
  CHECK(droppedItemsValid(&drops));
  DroppedItems before = drops;
  CHECK(!droppedItemsSpawn(&drops, (ItemStack){ITEM_DIRT, 2}, nearby, (Vec3){0}, 0.25f));
  CHECK(memcmp(&drops, &before, sizeof(drops)) == 0);

  before = drops;
  CHECK(!droppedItemsSpawn(&drops, (ItemStack){ITEM_STONE, 1}, first, (Vec3){0}, 0));
  CHECK(memcmp(&drops, &before, sizeof(drops)) == 0);
}

static void testNonstackableEquipment(void) {
  DroppedItems drops = {0};
  const Vec3 position = {2.5f, 3, 2.5f};
  CHECK(droppedItemsSpawn(&drops, (ItemStack){ITEM_LEATHER_HELMET, 1}, position, (Vec3){0}, 0));
  CHECK(droppedItemsSpawn(&drops, (ItemStack){ITEM_LEATHER_HELMET, 1}, position, (Vec3){0}, 0));
  CHECK(activeCount(&drops) == 2 && droppedCount(&drops, ITEM_LEATHER_HELMET) == 2);
  CHECK(drops.items[0].stack.count == 1 && drops.items[1].stack.count == 1);

  Inventory inventory;
  fillCarried(&inventory, ITEM_DIRT, INVENTORY_STACK_MAX);
  clearWorld();
  const Vec3 feet = position;
  droppedItemsAdvance(&drops, &inventory, feet, PLAYER_STEP_SECONDS, true);
  CHECK(activeCount(&drops) == 2 && droppedCount(&drops, ITEM_LEATHER_HELMET) == 2);

  inventory.carried[17] = (ItemStack){0};
  droppedItemsAdvance(&drops, &inventory, feet, PLAYER_STEP_SECONDS, true);
  CHECK(inventory.carried[17].item == ITEM_LEATHER_HELMET && inventory.carried[17].count == 1);
  CHECK(activeCount(&drops) == 1 && droppedCount(&drops, ITEM_LEATHER_HELMET) == 1);
}

static void testPauseDropsBacklog(void) {
  clearWorld();
  DroppedItems drops = {0};
  Inventory inventory;
  inventoryClear(&inventory);
  const Vec3 position = {10.5f, 10, 10.5f};
  const Vec3 feet = {-10.5f, 0, -10.5f};
  CHECK(droppedItemsSpawn(&drops, (ItemStack){ITEM_STONE, 1}, position, (Vec3){1, 0, 0}, 1));

  droppedItemsAdvance(&drops, &inventory, feet, PLAYER_STEP_SECONDS / 2, true);
  CHECK(fabs(drops.accumulator - PLAYER_STEP_SECONDS / 2) < 1e-12);
  CHECK(drops.items[0].position.x == position.x && drops.items[0].pickupDelay == 1 && drops.animationSeconds == 0);

  droppedItemsAdvance(&drops, &inventory, feet, 1000, false);
  CHECK(drops.accumulator == 0 && drops.items[0].position.x == position.x && drops.items[0].pickupDelay == 1 && drops.animationSeconds == 0);
  droppedItemsAdvance(&drops, &inventory, feet, PLAYER_STEP_SECONDS / 2, true);
  CHECK(fabs(drops.accumulator - PLAYER_STEP_SECONDS / 2) < 1e-12 && drops.items[0].position.x == position.x);
  droppedItemsAdvance(&drops, &inventory, feet, PLAYER_STEP_SECONDS / 2, true);
  CHECK(drops.accumulator < PLAYER_STEP_SECONDS && drops.items[0].position.x > position.x && drops.items[0].pickupDelay < 1 && drops.animationSeconds > 0);

  float moved = drops.items[0].position.x;
  droppedItemsAdvance(&drops, &inventory, feet, PLAYER_STEP_SECONDS / 2, true);
  CHECK(drops.accumulator > 0);
  droppedItemsAdvance(&drops, &inventory, feet, NAN, true);
  CHECK(drops.accumulator == 0 && drops.items[0].position.x == moved);
}

static void testGravityAndContacts(void) {
  Inventory inventory;
  inventoryClear(&inventory);
  const Vec3 farFeet = {-20.5f, 0, -20.5f};

  clearWorld();
  CHECK(setBlock(&(Vec3i){0, 0, 0}, BLOCK_STONE));
  DroppedItems drops = {0};
  CHECK(droppedItemsSpawn(&drops, (ItemStack){ITEM_STONE, 1}, (Vec3){0.5f, 3, 0.5f}, (Vec3){0}, 60));
  for (int tick = 0; tick < 240; tick++)
    droppedItemsAdvance(&drops, &inventory, farFeet, PLAYER_STEP_SECONDS, true);
  CHECK(drops.items[0].active && fabsf(drops.items[0].position.y - (1.0f + DROPPED_ITEM_RADIUS)) < 0.001f);
  CHECK(drops.items[0].velocity.y == 0);

  clearWorld();
  for (int x = 0; x <= 2; x++)
    CHECK(setBlock(&(Vec3i){x, 0, 0}, BLOCK_STONE));
  CHECK(setBlock(&(Vec3i){2, 1, 0}, BLOCK_STONE));
  drops = (DroppedItems){0};
  CHECK(droppedItemsSpawn(&drops, (ItemStack){ITEM_COBBLESTONE, 1}, (Vec3){0.5f, 1.0f + DROPPED_ITEM_RADIUS, 0.5f}, (Vec3){20, 0, 0}, 60));
  for (int tick = 0; tick < 30; tick++)
    droppedItemsAdvance(&drops, &inventory, farFeet, PLAYER_STEP_SECONDS, true);
  CHECK(drops.items[0].active && fabsf(drops.items[0].position.x - (2.0f - DROPPED_ITEM_RADIUS)) < 0.001f);
  CHECK(drops.items[0].velocity.x == 0);
  CHECK(fabsf(drops.items[0].position.y - (1.0f + DROPPED_ITEM_RADIUS)) < 0.001f);
}

static void testTimedPartialPickup(void) {
  clearWorld();
  DroppedItems drops = {0};
  Inventory inventory;
  fillCarried(&inventory, ITEM_DIRT, INVENTORY_STACK_MAX);
  inventory.carried[0].count = INVENTORY_STACK_MAX - 3;
  const Vec3 feet = {0.5f, 0, 0.5f};
  CHECK(droppedItemsSpawn(&drops, (ItemStack){ITEM_DIRT, 5}, (Vec3){0.5f, 0.5f, 0.5f}, (Vec3){0}, 0.03f));
  for (int tick = 0; tick < 3; tick++) {
    droppedItemsAdvance(&drops, &inventory, feet, PLAYER_STEP_SECONDS, true);
    CHECK(inventory.carried[0].count == INVENTORY_STACK_MAX - 3 && drops.items[0].stack.count == 5);
  }

  droppedItemsAdvance(&drops, &inventory, feet, PLAYER_STEP_SECONDS, true);
  CHECK(inventory.carried[0].count == INVENTORY_STACK_MAX);
  CHECK(drops.items[0].active && drops.items[0].stack.item == ITEM_DIRT && drops.items[0].stack.count == 2);
  CHECK(inventoryCountItem(&inventory, ITEM_DIRT) + droppedCount(&drops, ITEM_DIRT) == (uint32_t)INVENTORY_CARRIED_SLOT_COUNT * INVENTORY_STACK_MAX + 2);

  droppedItemsAdvance(&drops, &inventory, feet, PLAYER_STEP_SECONDS, true);
  CHECK(drops.items[0].active && drops.items[0].stack.count == 2);
}

static void testNoExpiry(void) {
  clearWorld();
  DroppedItems drops = {0};
  Inventory inventory;
  inventoryClear(&inventory);
  CHECK(droppedItemsSpawn(&drops, (ItemStack){ITEM_OAK_PLANKS, 23}, (Vec3){12.5f, 3, 12.5f}, (Vec3){0}, 0));
  const Vec3 farFeet = {-12.5f, 0, -12.5f};
  const int calls = 301 * 15;
  for (int call = 0; call < calls; call++)
    droppedItemsAdvance(&drops, &inventory, farFeet, PLAYER_MAX_STEPS * PLAYER_STEP_SECONDS, true);
  CHECK(activeCount(&drops) == 1 && droppedCount(&drops, ITEM_OAK_PLANKS) == 23);
  CHECK(droppedItemsValid(&drops) && drops.animationSeconds >= 0 && drops.animationSeconds < 60);
}

int main(void) {
  CHECK(initChunks());
  testValidationAndSpawn();
  testMergingAndPoolRollback();
  testNonstackableEquipment();
  testPauseDropsBacklog();
  testGravityAndContacts();
  testTimedPartialPickup();
  testNoExpiry();
  cleanupChunks();
  puts("Dropped-item bounds, physics, pickup, and conservation tests passed");
  return 0;
}
