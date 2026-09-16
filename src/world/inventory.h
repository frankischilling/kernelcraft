#ifndef INVENTORY_H
#define INVENTORY_H

#include "cube.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
  INVENTORY_HOTBAR_SLOT_COUNT = 9,
  INVENTORY_CARRIED_SLOT_COUNT = 36,
  INVENTORY_ARMOR_SLOT_COUNT = 4,
  INVENTORY_CRAFTING_SLOT_COUNT = 4,
  INVENTORY_STACK_MAX = 999,
  INVENTORY_SERIALIZED_STACK_COUNT = 46,
  INVENTORY_CLOSE_DROP_MAX = 5,
  INVENTORY_DRAG_SLOT_MAX = 45,
};

// IDs 1..6 intentionally match the original persisted world block IDs. Item
// IDs are a save-format contract: append new items rather than reordering
// existing IDs, even when newer placeable blocks use different block IDs.
typedef enum {
  ITEM_NONE = 0,
  ITEM_GRASS_BLOCK = BLOCK_GRASS,
  ITEM_DIRT = BLOCK_DIRT,
  ITEM_STONE = BLOCK_STONE,
  ITEM_COBBLESTONE = BLOCK_COBBLESTONE,
  ITEM_OAK_PLANKS = BLOCK_OAK_PLANKS,
  ITEM_STONE_BRICKS = BLOCK_STONE_BRICKS,
  ITEM_LEATHER_HELMET = 7,
  ITEM_LEATHER_CHESTPLATE = 8,
  ITEM_LEATHER_LEGGINGS = 9,
  ITEM_LEATHER_BOOTS = 10,
  ITEM_OAK_LOG = 11,
  ITEM_OAK_LEAVES = 12,
  ITEM_ID_LAST = ITEM_OAK_LEAVES,
} ItemID;

typedef struct {
  uint16_t item;
  uint16_t count;
} ItemStack;

typedef enum {
  INVENTORY_ARMOR_HEAD = 0,
  INVENTORY_ARMOR_CHEST = 1,
  INVENTORY_ARMOR_LEGS = 2,
  INVENTORY_ARMOR_FEET = 3,
} InventoryArmorSlot;

typedef enum {
  INVENTORY_SLOT_CARRIED = 0,
  INVENTORY_SLOT_ARMOR = 1,
  INVENTORY_SLOT_OFFHAND = 2,
  INVENTORY_SLOT_CRAFTING = 3,
  INVENTORY_SLOT_CURSOR = 4,
  INVENTORY_SLOT_RESULT = 5,
} InventorySlotKind;

typedef struct {
  InventorySlotKind kind;
  uint8_t index;
} InventorySlotRef;

typedef enum {
  INVENTORY_DRAG_EVEN = 0,
  INVENTORY_DRAG_ONE_EACH = 1,
} InventoryDragMode;

typedef struct {
  ItemStack carried[INVENTORY_CARRIED_SLOT_COUNT]; // 0..8 hotbar, 9..35 storage.
  ItemStack armor[INVENTORY_ARMOR_SLOT_COUNT];     // Head, chest, legs, feet.
  ItemStack offhand;
  ItemStack crafting[INVENTORY_CRAFTING_SLOT_COUNT]; // Row-major 2x2 input.
  ItemStack cursor;
} Inventory;

// New games and legacy saves use the same starter inventory: six maxed block
// stacks in hotbar slots 0..5 and one leather set in carried slots 9..12.
void inventoryInit(Inventory* inventory);
void inventoryClear(Inventory* inventory);

bool inventoryItemValid(uint16_t item);
bool inventoryItemIsEquipment(uint16_t item);
int inventoryItemArmorSlot(uint16_t item); // InventoryArmorSlot or -1.
int inventoryItemArmorPoints(uint16_t item);
uint16_t inventoryItemMaxStack(uint16_t item);
int inventoryItemBlock(uint16_t item);  // BLOCK_AIR for non-placeable items.
uint16_t inventoryBlockItem(int block); // ITEM_NONE for air/invalid blocks.
const char* inventoryItemName(uint16_t item);
uint32_t inventoryItemColor(uint16_t item); // 0xRRGGBB, zero for none/invalid.

bool inventoryStackValid(ItemStack stack);
bool inventorySlotRefValid(InventorySlotRef slot);
bool inventorySlotAccepts(InventorySlotRef slot, ItemStack stack);
ItemStack inventoryGet(const Inventory* inventory, InventorySlotRef slot);
ItemStack inventoryCraftResult(const Inventory* inventory);
bool inventoryValidate(const Inventory* inventory);
uint32_t inventoryCountItem(const Inventory* inventory, uint16_t item);

// Pickups add to carried slots only. These operations are all-or-nothing.
bool inventoryCanAdd(const Inventory* inventory, ItemStack stack);
bool inventoryAdd(Inventory* inventory, ItemStack stack);
// Remove exactly count items from one owned slot (result is read-only). On
// success, removed receives the transferred stack when non-NULL.
bool inventoryRemove(Inventory* inventory, InventorySlotRef source, uint16_t count, ItemStack* removed);
// Remove exactly count matching items from carried slots, in slot order.
bool inventoryRemoveItem(Inventory* inventory, uint16_t item, uint16_t count);
// Swap two owned slots atomically when each stack is legal in the other slot.
// The cursor is an owned slot; the computed crafting result is not.
bool inventorySwapSlots(Inventory* inventory, InventorySlotRef a, InventorySlotRef b);

// Cursor-based inventory interactions. Left click moves/merges/swaps. Right
// click takes half, places one onto an empty/matching stack, or swaps a
// different compatible stack.
bool inventoryClick(Inventory* inventory, InventorySlotRef slot, bool rightClick);
bool inventoryShiftTransfer(Inventory* inventory, InventorySlotRef source);
bool inventoryHotbarSwap(Inventory* inventory, InventorySlotRef source, int hotbarSlot);
bool inventoryGather(Inventory* inventory, InventorySlotRef clicked);
// EVEN gives each unique eligible target floor(original cursor count / target
// count), capped by that target's space; remainder/capped surplus stays held.
// ONE_EACH gives at most one to each unique eligible target in caller order.
uint16_t inventoryDragDistribute(Inventory* inventory, const InventorySlotRef* slots, size_t slotCount, InventoryDragMode mode);

// Recipes are shapeless within the 2x2 input. Four occupied stone cells yield
// four stone bricks; one occupied oak-log cell with the other three cells empty
// yields four oak planks. Craft-once targets the cursor atomically. Craft-all
// performs the maximum whole crafts whose complete outputs fit in carried
// slots; it never consumes a partial recipe output.
bool inventoryCraftOnce(Inventory* inventory);
size_t inventoryCraftAll(Inventory* inventory);

// Canonical persistence order is carried[0..35], armor head/chest/legs/feet,
// offhand, crafting[0..3], cursor. The derived result is never serialized.
// Persist item/count fields explicitly; do not rely on ItemStack struct layout.
bool inventoryExportStacks(const Inventory* inventory, ItemStack stacks[INVENTORY_SERIALIZED_STACK_COUNT]);
bool inventoryImportStacks(Inventory* inventory, const ItemStack stacks[INVENTORY_SERIALIZED_STACK_COUNT]);

// Closing returns cursor/crafting ownership to carried slots where possible.
// Any remainder is transferred to drops (at most one per transient source) and
// removed from the inventory. Callers that can fail to create world drops must
// invoke this on a copy and commit inventory + drop storage together.
bool inventoryClose(Inventory* inventory, ItemStack drops[INVENTORY_CLOSE_DROP_MAX], size_t* dropCount);

#endif
