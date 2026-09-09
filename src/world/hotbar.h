#ifndef HOTBAR_H
#define HOTBAR_H

#include "cube.h"

enum { HOTBAR_SLOT_COUNT = 9 };

// Slots are zero-based internally. Cosmetic terrain variants are not items.
static inline int hotbarBlock(int slot) {
  const int blocks[HOTBAR_SLOT_COUNT] = {BLOCK_GRASS, BLOCK_DIRT, BLOCK_STONE};
  return slot >= 0 && slot < HOTBAR_SLOT_COUNT ? blocks[slot] : BLOCK_AIR;
}

#endif
