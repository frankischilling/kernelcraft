#include "dropped_items.h"
#include "world.h"
#include "player.h"
#include <math.h>

bool droppedItemPositionValid(Vec3 position) {
  return isfinite(position.x) && isfinite(position.y) && isfinite(position.z) && position.x >= -WORLD_SIZE / 2 && position.x < WORLD_SIZE / 2 && position.z >= -WORLD_SIZE / 2 &&
         position.z < WORLD_SIZE / 2 && position.y >= 0 && position.y < CHUNK_HEIGHT;
}

static bool velocityValid(Vec3 velocity) {
  return isfinite(velocity.x) && isfinite(velocity.y) && isfinite(velocity.z) && fabsf(velocity.x) <= 20 && fabsf(velocity.y) <= 20 && fabsf(velocity.z) <= 20;
}

bool droppedItemsValid(const DroppedItems* drops) {
  if (!drops || !isfinite(drops->accumulator) || drops->accumulator < 0 || drops->accumulator > PLAYER_STEP_SECONDS || !isfinite(drops->animationSeconds) ||
      drops->animationSeconds < 0 || drops->animationSeconds >= 60)
    return false;
  for (size_t i = 0; i < DROPPED_ITEM_CAPACITY; i++) {
    const DroppedItem* item = &drops->items[i];
    if (item->active && (!inventoryStackValid(item->stack) || !item->stack.count || !droppedItemPositionValid(item->position) || !velocityValid(item->velocity) ||
                         !isfinite(item->pickupDelay) || item->pickupDelay < 0 || item->pickupDelay > 60))
      return false;
  }
  return true;
}

bool droppedItemsSpawn(DroppedItems* drops, ItemStack stack, Vec3 position, Vec3 velocity, float pickupDelay) {
  if (!droppedItemsValid(drops) || !inventoryStackValid(stack) || !stack.count || !droppedItemPositionValid(position) || !velocityValid(velocity) || !isfinite(pickupDelay) ||
      pickupDelay < 0 || pickupDelay > 60)
    return false;
  DroppedItems next = *drops;
  uint16_t remaining = stack.count;
  for (size_t i = 0; i < DROPPED_ITEM_CAPACITY && remaining; i++) {
    DroppedItem* item = &next.items[i];
    if (!item->active || item->stack.item != stack.item || vec3_distance(&item->position, &position) > 0.75f)
      continue;
    unsigned space = inventoryItemMaxStack(stack.item) - item->stack.count;
    unsigned transfer = remaining < space ? remaining : space;
    item->stack.count += (uint16_t)transfer;
    remaining -= (uint16_t)transfer;
    if (transfer)
      item->pickupDelay = fmaxf(item->pickupDelay, pickupDelay);
  }
  for (size_t i = 0; i < DROPPED_ITEM_CAPACITY && remaining; i++) {
    if (next.items[i].active)
      continue;
    next.items[i] = (DroppedItem){.stack = {stack.item, remaining}, .position = position, .velocity = velocity, .pickupDelay = pickupDelay, .active = true};
    remaining = 0;
  }
  if (remaining)
    return false;
  *drops = next;
  return true;
}

static bool clearPosition(Vec3 position) {
  const float r = DROPPED_ITEM_RADIUS;
  if (position.x - r < -WORLD_SIZE / 2 || position.x + r > WORLD_SIZE / 2 || position.z - r < -WORLD_SIZE / 2 || position.z + r > WORLD_SIZE / 2 || position.y - r < 0 ||
      position.y + r > CHUNK_HEIGHT)
    return false;
  Vec3i first = {(int)floorf(position.x - r + 0.00001f), (int)floorf(position.y - r + 0.00001f), (int)floorf(position.z - r + 0.00001f)};
  Vec3i last = {(int)floorf(position.x + r - 0.00001f), (int)floorf(position.y + r - 0.00001f), (int)floorf(position.z + r - 0.00001f)};
  for (int x = first.x; x <= last.x; x++)
    for (int y = first.y; y <= last.y; y++)
      for (int z = first.z; z <= last.z; z++) {
        const Block* block = getBlock(&(Vec3i){x, y, z});
        if (!block || blockIsSolid(block->id))
          return false;
      }
  return true;
}

static void moveAxis(DroppedItem* item, int axis, float delta) {
  Vec3 next = item->position;
  float* coordinate = axis == 0 ? &next.x : axis == 1 ? &next.y : &next.z;
  float start = *coordinate;
  *coordinate += delta;
  if (clearPosition(next)) {
    item->position = next;
    return;
  }
  // A tick moves at most 1/6 block, so the first contact cannot skip a voxel.
  float low = 0, high = 1;
  for (int iteration = 0; iteration < 12; iteration++) {
    float middle = (low + high) * 0.5f;
    *coordinate = start + delta * middle;
    if (clearPosition(next))
      low = middle;
    else
      high = middle;
  }
  *coordinate = start + delta * low;
  item->position = next;
  if (axis == 0)
    item->velocity.x = 0;
  else if (axis == 1)
    item->velocity.y = 0;
  else
    item->velocity.z = 0;
}

static void collectItem(DroppedItem* item, Inventory* inventory, Vec3 feet) {
  float closestY = fmaxf(feet.y, fminf(item->position.y, feet.y + PLAYER_HEIGHT));
  Vec3 closest = {feet.x, closestY, feet.z};
  if (item->pickupDelay > 0 || vec3_distance(&item->position, &closest) > 1.25f)
    return;
  ItemStack available = item->stack;
  if (!inventoryCanAdd(inventory, available)) {
    available.count = 1;
    if (!inventoryCanAdd(inventory, available))
      return;
    unsigned low = 1, high = item->stack.count;
    while (low < high) {
      unsigned middle = low + (high - low + 1) / 2;
      available.count = (uint16_t)middle;
      if (inventoryCanAdd(inventory, available))
        low = middle;
      else
        high = middle - 1;
    }
    available.count = (uint16_t)low;
  }
  if (inventoryAdd(inventory, available)) {
    item->stack.count -= available.count;
    if (!item->stack.count)
      *item = (DroppedItem){0};
  }
}

void droppedItemsAdvance(DroppedItems* drops, Inventory* inventory, Vec3 feet, double seconds, bool active) {
  if (!drops || !inventory)
    return;
  if (!active || !isfinite(seconds) || seconds <= 0 || !isfinite(feet.x) || !isfinite(feet.y) || !isfinite(feet.z)) {
    drops->accumulator = 0;
    return;
  }
  drops->accumulator += fmin(seconds, PLAYER_MAX_STEPS * PLAYER_STEP_SECONDS);
  int steps = 0;
  while (drops->accumulator + 1e-12 >= PLAYER_STEP_SECONDS && steps < PLAYER_MAX_STEPS) {
    drops->accumulator = fmax(0, drops->accumulator - PLAYER_STEP_SECONDS);
    drops->animationSeconds = fmod(drops->animationSeconds + PLAYER_STEP_SECONDS, 60.0);
    steps++;
    for (size_t i = 0; i < DROPPED_ITEM_CAPACITY; i++) {
      DroppedItem* item = &drops->items[i];
      if (!item->active)
        continue;
      item->pickupDelay = fmaxf(0, item->pickupDelay - (float)PLAYER_STEP_SECONDS);
      if (clearPosition(item->position)) {
        item->velocity.y = fmaxf(-20, item->velocity.y - 12 * (float)PLAYER_STEP_SECONDS);
        moveAxis(item, 0, item->velocity.x * (float)PLAYER_STEP_SECONDS);
        moveAxis(item, 1, item->velocity.y * (float)PLAYER_STEP_SECONDS);
        moveAxis(item, 2, item->velocity.z * (float)PLAYER_STEP_SECONDS);
        item->velocity.x *= 0.98f;
        item->velocity.z *= 0.98f;
      } else {
        // A block placed over an item does not delete it. It remains available
        // for pickup and falls again when the obstruction is removed.
        item->velocity = (Vec3){0};
      }
      collectItem(item, inventory, feet);
    }
  }
}
