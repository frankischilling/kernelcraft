#ifndef ITEM_MODEL_H
#define ITEM_MODEL_H

#include "inventory.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { ITEM_MODEL_VERTEX_CAPACITY = 576 };

#define ITEM_MODEL_SOLID_LAYER (-1.0f)

typedef struct {
  Vec3 position;
  Vec3 normal;
  float u, v;
  // Nonnegative values are terrain texture-array layers. -1 is a solid-color
  // primitive; renderers can use inventoryItemColor(item) for that item.
  float layer;
} ItemModelVertex;

typedef struct {
  // Non-indexed outward-wound triangles. count is always divisible by three.
  ItemModelVertex vertices[ITEM_MODEL_VERTEX_CAPACITY];
  size_t count;
  Vec3 min, max;
} ItemModel;

// Builds a bounded CPU-only model for each current nonempty stable item ID.
// Invalid IDs, NULL output, or an internal capacity failure return false without
// modifying the caller's output.
bool itemModelBuild(uint16_t item, ItemModel* output);

#endif
