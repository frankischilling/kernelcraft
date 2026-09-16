#ifndef DROPPED_ITEMS_H
#define DROPPED_ITEMS_H

#include "inventory.h"
#include "../math/math.h"

#define DROPPED_ITEM_CAPACITY 128
#define DROPPED_ITEM_RADIUS 0.125f

typedef struct {
  ItemStack stack;
  Vec3 position;
  Vec3 velocity;
  float pickupDelay;
  bool active;
} DroppedItem;

typedef struct {
  DroppedItem items[DROPPED_ITEM_CAPACITY];
  double accumulator;
  double animationSeconds;
} DroppedItems;

bool droppedItemPositionValid(Vec3 position);
bool droppedItemsValid(const DroppedItems* drops);
// A full pool never discards an existing stack. Failure changes nothing.
bool droppedItemsSpawn(DroppedItems* drops, ItemStack stack, Vec3 position, Vec3 velocity, float pickupDelay);
// Fixed-step movement and pickup pause with the game. Items do not expire.
void droppedItemsAdvance(DroppedItems* drops, Inventory* inventory, Vec3 feet, double seconds, bool active);

#endif
