#include "world/inventory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                                                                                                           \
  do {                                                                                                                                                                             \
    if (!(condition)) {                                                                                                                                                            \
      fprintf(stderr, "Inventory test: %s (line %d)\n", #condition, __LINE__);                                                                                                     \
      exit(EXIT_FAILURE);                                                                                                                                                          \
    }                                                                                                                                                                              \
  } while (0)

static InventorySlotRef slot(InventorySlotKind kind, int index) {
  return (InventorySlotRef){kind, (uint8_t)index};
}

static bool empty(ItemStack stack) {
  return stack.item == ITEM_NONE && stack.count == 0;
}

static uint32_t totalUnits(const Inventory* inventory) {
  uint32_t total = 0;
  for (uint16_t item = ITEM_GRASS_BLOCK; item <= ITEM_ID_LAST; item++)
    total += inventoryCountItem(inventory, item);
  return total;
}

static uint32_t dropUnits(const ItemStack* drops, size_t count) {
  uint32_t total = 0;
  for (size_t i = 0; i < count; i++)
    total += drops[i].count;
  return total;
}

static void fillCarried(Inventory* inventory, uint16_t item, uint16_t count) {
  for (int i = 0; i < INVENTORY_CARRIED_SLOT_COUNT; i++)
    inventory->carried[i] = (ItemStack){item, count};
}

static void testIdentityAndDefaults(void) {
  Inventory inventory;
  inventoryInit(&inventory);
  CHECK(inventoryValidate(&inventory));
  CHECK(ITEM_LEATHER_HELMET == 7 && ITEM_LEATHER_CHESTPLATE == 8 && ITEM_LEATHER_LEGGINGS == 9 && ITEM_LEATHER_BOOTS == 10);
  CHECK(ITEM_OAK_LOG == 11 && ITEM_OAK_LEAVES == 12 && ITEM_ID_LAST == 12);
  for (int slotIndex = 0; slotIndex < 6; slotIndex++) {
    CHECK(inventory.carried[slotIndex].item == (uint16_t)(slotIndex + 1));
    CHECK(inventory.carried[slotIndex].count == INVENTORY_STACK_MAX);
    CHECK(inventoryItemBlock(inventory.carried[slotIndex].item) == slotIndex + 1);
    CHECK(inventoryBlockItem(slotIndex + 1) == slotIndex + 1);
    CHECK(inventoryItemName((uint16_t)(slotIndex + 1))[0] != '\0');
    CHECK(inventoryItemColor((uint16_t)(slotIndex + 1)) != 0);
  }
  for (int slotIndex = 6; slotIndex < INVENTORY_HOTBAR_SLOT_COUNT; slotIndex++)
    CHECK(empty(inventory.carried[slotIndex]));
  CHECK(inventory.carried[9].item == ITEM_LEATHER_HELMET && inventory.carried[9].count == 1);
  CHECK(inventory.carried[10].item == ITEM_LEATHER_CHESTPLATE && inventory.carried[10].count == 1);
  CHECK(inventory.carried[11].item == ITEM_LEATHER_LEGGINGS && inventory.carried[11].count == 1);
  CHECK(inventory.carried[12].item == ITEM_LEATHER_BOOTS && inventory.carried[12].count == 1);
  CHECK(inventoryItemArmorSlot(ITEM_LEATHER_HELMET) == INVENTORY_ARMOR_HEAD);
  CHECK(inventoryItemArmorSlot(ITEM_LEATHER_CHESTPLATE) == INVENTORY_ARMOR_CHEST);
  CHECK(inventoryItemArmorSlot(ITEM_LEATHER_LEGGINGS) == INVENTORY_ARMOR_LEGS);
  CHECK(inventoryItemArmorSlot(ITEM_LEATHER_BOOTS) == INVENTORY_ARMOR_FEET);
  CHECK(inventoryItemArmorPoints(ITEM_LEATHER_HELMET) == 1 && inventoryItemArmorPoints(ITEM_LEATHER_CHESTPLATE) == 3);
  CHECK(inventoryItemArmorPoints(ITEM_LEATHER_LEGGINGS) == 2 && inventoryItemArmorPoints(ITEM_LEATHER_BOOTS) == 1);
  CHECK(inventoryItemMaxStack(ITEM_STONE) == 999 && inventoryItemMaxStack(ITEM_OAK_LOG) == 999 && inventoryItemMaxStack(ITEM_OAK_LEAVES) == 999);
  CHECK(inventoryItemMaxStack(ITEM_LEATHER_HELMET) == 1 && !inventoryItemIsEquipment(ITEM_OAK_LOG) && !inventoryItemIsEquipment(ITEM_OAK_LEAVES));
  CHECK(inventoryItemBlock(ITEM_LEATHER_HELMET) == BLOCK_AIR && inventoryBlockItem(BLOCK_AIR) == ITEM_NONE);
  CHECK(inventoryItemBlock(ITEM_OAK_LOG) == BLOCK_OAK_LOG && inventoryBlockItem(BLOCK_OAK_LOG) == ITEM_OAK_LOG);
  CHECK(inventoryItemBlock(ITEM_OAK_LEAVES) == BLOCK_OAK_LEAVES && inventoryBlockItem(BLOCK_OAK_LEAVES) == ITEM_OAK_LEAVES);
  CHECK(inventoryBlockItem(BLOCK_LEAFY_GRASS) == ITEM_GRASS_BLOCK && inventoryItemBlock(ITEM_GRASS_BLOCK) == BLOCK_GRASS);
  CHECK(strcmp(inventoryItemName(ITEM_OAK_LOG), "Oak Log") == 0 && strcmp(inventoryItemName(ITEM_OAK_LEAVES), "Oak Leaves") == 0);
  CHECK(inventoryItemColor(ITEM_OAK_LOG) == UINT32_C(0x6B4F2A) && inventoryItemColor(ITEM_OAK_LEAVES) == UINT32_C(0x4F7F3B));

  CHECK(inventoryStackValid((ItemStack){0}));
  CHECK(inventoryStackValid((ItemStack){ITEM_DIRT, 999}));
  CHECK(inventoryStackValid((ItemStack){ITEM_OAK_LOG, 999}) && inventoryStackValid((ItemStack){ITEM_OAK_LEAVES, 999}));
  CHECK(!inventoryStackValid((ItemStack){ITEM_DIRT, 0}));
  CHECK(!inventoryStackValid((ItemStack){ITEM_DIRT, 1000}));
  CHECK(!inventoryStackValid((ItemStack){ITEM_LEATHER_HELMET, 2}));
  CHECK(!inventoryStackValid((ItemStack){ITEM_ID_LAST + 1, 1}));
  CHECK(!inventoryStackValid((ItemStack){ITEM_NONE, 1}));
  CHECK(inventorySlotRefValid(slot(INVENTORY_SLOT_CARRIED, 35)));
  CHECK(!inventorySlotRefValid(slot(INVENTORY_SLOT_CARRIED, 36)));
  CHECK(inventorySlotRefValid(slot(INVENTORY_SLOT_RESULT, 0)));
  CHECK(!inventorySlotRefValid(slot(INVENTORY_SLOT_RESULT, 1)));
  CHECK(inventorySlotAccepts(slot(INVENTORY_SLOT_ARMOR, INVENTORY_ARMOR_HEAD), (ItemStack){ITEM_LEATHER_HELMET, 1}));
  CHECK(!inventorySlotAccepts(slot(INVENTORY_SLOT_ARMOR, INVENTORY_ARMOR_HEAD), (ItemStack){ITEM_DIRT, 1}));
}

static void testAddAndRemove(void) {
  Inventory inventory;
  inventoryClear(&inventory);
  inventory.carried[0] = (ItemStack){ITEM_DIRT, 998};
  CHECK(inventoryCanAdd(&inventory, (ItemStack){ITEM_DIRT, 2}));
  CHECK(inventoryAdd(&inventory, (ItemStack){ITEM_DIRT, 2}));
  CHECK(inventory.carried[0].count == 999 && inventory.carried[1].item == ITEM_DIRT && inventory.carried[1].count == 1);

  inventoryClear(&inventory);
  fillCarried(&inventory, ITEM_STONE, 999);
  Inventory before = inventory;
  CHECK(!inventoryCanAdd(&inventory, (ItemStack){ITEM_STONE, 1}));
  CHECK(!inventoryAdd(&inventory, (ItemStack){ITEM_STONE, 1}));
  CHECK(memcmp(&inventory, &before, sizeof(inventory)) == 0);
  CHECK(!inventoryAdd(&inventory, (ItemStack){ITEM_STONE, 1000}));
  CHECK(memcmp(&inventory, &before, sizeof(inventory)) == 0);

  inventoryClear(&inventory);
  inventory.carried[4] = (ItemStack){ITEM_OAK_PLANKS, 7};
  ItemStack removed = {ITEM_DIRT, 99};
  before = inventory;
  CHECK(!inventoryRemove(&inventory, slot(INVENTORY_SLOT_CARRIED, 4), 8, &removed));
  CHECK(memcmp(&inventory, &before, sizeof(inventory)) == 0 && removed.item == ITEM_DIRT && removed.count == 99);
  CHECK(inventoryRemove(&inventory, slot(INVENTORY_SLOT_CARRIED, 4), 3, &removed));
  CHECK(removed.item == ITEM_OAK_PLANKS && removed.count == 3 && inventory.carried[4].count == 4);
  CHECK(!inventoryRemove(&inventory, slot(INVENTORY_SLOT_RESULT, 0), 1, NULL));

  inventoryClear(&inventory);
  inventory.carried[0] = (ItemStack){ITEM_DIRT, 2};
  inventory.carried[1] = (ItemStack){ITEM_DIRT, 3};
  CHECK(inventoryRemoveItem(&inventory, ITEM_DIRT, 4));
  CHECK(empty(inventory.carried[0]) && inventory.carried[1].count == 1);
  before = inventory;
  CHECK(!inventoryRemoveItem(&inventory, ITEM_DIRT, 2));
  CHECK(memcmp(&inventory, &before, sizeof(inventory)) == 0);

  inventoryClear(&inventory);
  inventory.carried[0] = (ItemStack){ITEM_OAK_LOG, 998};
  CHECK(inventoryAdd(&inventory, (ItemStack){ITEM_OAK_LOG, 2}));
  CHECK(inventory.carried[0].item == ITEM_OAK_LOG && inventory.carried[0].count == 999);
  CHECK(inventory.carried[1].item == ITEM_OAK_LOG && inventory.carried[1].count == 1);
  CHECK(inventoryAdd(&inventory, (ItemStack){ITEM_OAK_LEAVES, 3}));
  CHECK(inventory.carried[2].item == ITEM_OAK_LEAVES && inventory.carried[2].count == 3);
}

static void testCursorClicks(void) {
  Inventory inventory;
  inventoryClear(&inventory);
  inventory.carried[0] = (ItemStack){ITEM_DIRT, 5};
  uint32_t total = totalUnits(&inventory);
  CHECK(inventoryClick(&inventory, slot(INVENTORY_SLOT_CARRIED, 0), true));
  CHECK(inventory.cursor.item == ITEM_DIRT && inventory.cursor.count == 3 && inventory.carried[0].count == 2);
  CHECK(inventoryClick(&inventory, slot(INVENTORY_SLOT_CARRIED, 1), true));
  CHECK(inventory.cursor.count == 2 && inventory.carried[1].count == 1);
  CHECK(inventoryClick(&inventory, slot(INVENTORY_SLOT_CARRIED, 0), false));
  CHECK(empty(inventory.cursor) && inventory.carried[0].count == 4 && inventory.carried[1].count == 1);
  CHECK(totalUnits(&inventory) == total);

  inventory.cursor = (ItemStack){ITEM_STONE, 2};
  inventory.carried[2] = (ItemStack){ITEM_DIRT, 3};
  total = totalUnits(&inventory);
  CHECK(inventoryClick(&inventory, slot(INVENTORY_SLOT_CARRIED, 2), true));
  CHECK(inventory.carried[2].item == ITEM_STONE && inventory.carried[2].count == 2);
  CHECK(inventory.cursor.item == ITEM_DIRT && inventory.cursor.count == 3 && totalUnits(&inventory) == total);

  inventory.cursor = (ItemStack){ITEM_OAK_PLANKS, 2};
  inventory.carried[3] = (ItemStack){ITEM_COBBLESTONE, 4};
  total = totalUnits(&inventory);
  CHECK(inventoryClick(&inventory, slot(INVENTORY_SLOT_CARRIED, 3), false));
  CHECK(inventory.carried[3].item == ITEM_OAK_PLANKS && inventory.carried[3].count == 2);
  CHECK(inventory.cursor.item == ITEM_COBBLESTONE && inventory.cursor.count == 4 && totalUnits(&inventory) == total);

  inventory.cursor = (ItemStack){ITEM_DIRT, 1};
  Inventory before = inventory;
  CHECK(!inventoryClick(&inventory, slot(INVENTORY_SLOT_ARMOR, INVENTORY_ARMOR_HEAD), true));
  CHECK(memcmp(&inventory, &before, sizeof(inventory)) == 0);
  CHECK(!inventoryClick(&inventory, slot(INVENTORY_SLOT_ARMOR, INVENTORY_ARMOR_HEAD), false));
  CHECK(memcmp(&inventory, &before, sizeof(inventory)) == 0);
  inventory.cursor = (ItemStack){ITEM_LEATHER_HELMET, 1};
  CHECK(inventoryClick(&inventory, slot(INVENTORY_SLOT_ARMOR, INVENTORY_ARMOR_HEAD), false));
  CHECK(inventory.armor[INVENTORY_ARMOR_HEAD].item == ITEM_LEATHER_HELMET && empty(inventory.cursor));
}

static void testShiftAndHotbar(void) {
  Inventory inventory;
  inventoryClear(&inventory);
  inventory.carried[0] = (ItemStack){ITEM_DIRT, 500};
  inventory.carried[9] = (ItemStack){ITEM_DIRT, 600};
  uint32_t total = totalUnits(&inventory);
  CHECK(inventoryShiftTransfer(&inventory, slot(INVENTORY_SLOT_CARRIED, 0)));
  CHECK(empty(inventory.carried[0]) && inventory.carried[9].count == 999 && inventory.carried[10].item == ITEM_DIRT && inventory.carried[10].count == 101);
  CHECK(totalUnits(&inventory) == total);

  inventoryClear(&inventory);
  inventory.carried[15] = (ItemStack){ITEM_LEATHER_HELMET, 1};
  CHECK(inventoryShiftTransfer(&inventory, slot(INVENTORY_SLOT_CARRIED, 15)));
  CHECK(empty(inventory.carried[15]) && inventory.armor[INVENTORY_ARMOR_HEAD].item == ITEM_LEATHER_HELMET);
  CHECK(inventoryShiftTransfer(&inventory, slot(INVENTORY_SLOT_ARMOR, INVENTORY_ARMOR_HEAD)));
  CHECK(empty(inventory.armor[INVENTORY_ARMOR_HEAD]) && inventory.carried[0].item == ITEM_LEATHER_HELMET);

  inventoryClear(&inventory);
  inventory.armor[INVENTORY_ARMOR_HEAD] = (ItemStack){ITEM_LEATHER_HELMET, 1};
  inventory.carried[15] = (ItemStack){ITEM_LEATHER_HELMET, 1};
  CHECK(inventoryShiftTransfer(&inventory, slot(INVENTORY_SLOT_CARRIED, 15)));
  CHECK(inventory.carried[0].item == ITEM_LEATHER_HELMET && empty(inventory.carried[15]));

  inventoryClear(&inventory);
  inventory.carried[2] = (ItemStack){ITEM_STONE, 4};
  inventory.carried[9] = (ItemStack){ITEM_DIRT, 12};
  total = totalUnits(&inventory);
  CHECK(inventoryHotbarSwap(&inventory, slot(INVENTORY_SLOT_CARRIED, 9), 2));
  CHECK(inventory.carried[2].item == ITEM_DIRT && inventory.carried[2].count == 12);
  CHECK(inventory.carried[9].item == ITEM_STONE && inventory.carried[9].count == 4 && totalUnits(&inventory) == total);

  inventory.armor[INVENTORY_ARMOR_HEAD] = (ItemStack){ITEM_LEATHER_HELMET, 1};
  Inventory before = inventory;
  CHECK(!inventoryHotbarSwap(&inventory, slot(INVENTORY_SLOT_ARMOR, INVENTORY_ARMOR_HEAD), 2));
  CHECK(memcmp(&inventory, &before, sizeof(inventory)) == 0);
  inventory.carried[2] = (ItemStack){0};
  CHECK(inventoryHotbarSwap(&inventory, slot(INVENTORY_SLOT_ARMOR, INVENTORY_ARMOR_HEAD), 2));
  CHECK(empty(inventory.armor[INVENTORY_ARMOR_HEAD]) && inventory.carried[2].item == ITEM_LEATHER_HELMET);
}

static void testDirectSwap(void) {
  Inventory inventory;
  inventoryClear(&inventory);
  inventory.cursor = (ItemStack){ITEM_LEATHER_HELMET, 1};
  CHECK(inventorySwapSlots(&inventory, slot(INVENTORY_SLOT_CURSOR, 0), slot(INVENTORY_SLOT_ARMOR, INVENTORY_ARMOR_HEAD)));
  CHECK(empty(inventory.cursor) && inventory.armor[INVENTORY_ARMOR_HEAD].item == ITEM_LEATHER_HELMET);

  inventory.cursor = (ItemStack){ITEM_DIRT, 1};
  Inventory before = inventory;
  CHECK(!inventorySwapSlots(&inventory, slot(INVENTORY_SLOT_CURSOR, 0), slot(INVENTORY_SLOT_ARMOR, INVENTORY_ARMOR_HEAD)));
  CHECK(memcmp(&inventory, &before, sizeof(inventory)) == 0);

  inventoryClear(&inventory);
  inventory.carried[7] = (ItemStack){ITEM_DIRT, 12};
  inventory.offhand = (ItemStack){ITEM_STONE, 3};
  CHECK(inventorySwapSlots(&inventory, slot(INVENTORY_SLOT_CARRIED, 7), slot(INVENTORY_SLOT_OFFHAND, 0)));
  CHECK(inventory.carried[7].item == ITEM_STONE && inventory.carried[7].count == 3);
  CHECK(inventory.offhand.item == ITEM_DIRT && inventory.offhand.count == 12);
  before = inventory;
  CHECK(!inventorySwapSlots(&inventory, slot(INVENTORY_SLOT_RESULT, 0), slot(INVENTORY_SLOT_OFFHAND, 0)));
  CHECK(memcmp(&inventory, &before, sizeof(inventory)) == 0);
}

static void testGatherAndDrag(void) {
  Inventory inventory;
  inventoryClear(&inventory);
  inventory.carried[0] = (ItemStack){ITEM_DIRT, 2};
  inventory.carried[1] = (ItemStack){ITEM_DIRT, 3};
  CHECK(inventoryGather(&inventory, slot(INVENTORY_SLOT_CARRIED, 0)));
  CHECK(inventory.cursor.item == ITEM_DIRT && inventory.cursor.count == 5);
  CHECK(empty(inventory.carried[0]) && empty(inventory.carried[1]));

  inventoryClear(&inventory);
  inventory.cursor = (ItemStack){ITEM_DIRT, 900};
  inventory.carried[0] = (ItemStack){ITEM_DIRT, 50};
  inventory.offhand = (ItemStack){ITEM_DIRT, 20};
  inventory.crafting[0] = (ItemStack){ITEM_DIRT, 60};
  uint32_t total = totalUnits(&inventory);
  CHECK(inventoryGather(&inventory, slot(INVENTORY_SLOT_CARRIED, 0)));
  CHECK(inventory.cursor.count == 999 && empty(inventory.carried[0]) && empty(inventory.offhand));
  CHECK(inventory.crafting[0].item == ITEM_DIRT && inventory.crafting[0].count == 31 && totalUnits(&inventory) == total);

  inventoryClear(&inventory);
  inventory.cursor = (ItemStack){ITEM_DIRT, 10};
  InventorySlotRef evenSlots[] = {slot(INVENTORY_SLOT_CARRIED, 0), slot(INVENTORY_SLOT_CARRIED, 1), slot(INVENTORY_SLOT_CARRIED, 2), slot(INVENTORY_SLOT_CARRIED, 1)};
  CHECK(inventoryDragDistribute(&inventory, evenSlots, 4, INVENTORY_DRAG_EVEN) == 9);
  CHECK(inventory.carried[0].count == 3 && inventory.carried[1].count == 3 && inventory.carried[2].count == 3 && inventory.cursor.count == 1);

  inventoryClear(&inventory);
  inventory.cursor = (ItemStack){ITEM_DIRT, 10};
  inventory.carried[0] = (ItemStack){ITEM_DIRT, 998};
  InventorySlotRef cappedSlots[] = {slot(INVENTORY_SLOT_CARRIED, 0), slot(INVENTORY_SLOT_CARRIED, 1)};
  CHECK(inventoryDragDistribute(&inventory, cappedSlots, 2, INVENTORY_DRAG_EVEN) == 6);
  CHECK(inventory.carried[0].count == 999 && inventory.carried[1].count == 5 && inventory.cursor.count == 4);

  inventoryClear(&inventory);
  inventory.cursor = (ItemStack){ITEM_STONE, 5};
  InventorySlotRef oneSlots[] = {slot(INVENTORY_SLOT_CARRIED, 0), slot(INVENTORY_SLOT_CARRIED, 1), slot(INVENTORY_SLOT_CARRIED, 2)};
  CHECK(inventoryDragDistribute(&inventory, oneSlots, 3, INVENTORY_DRAG_ONE_EACH) == 3);
  CHECK(inventory.carried[0].count == 1 && inventory.carried[1].count == 1 && inventory.carried[2].count == 1 && inventory.cursor.count == 2);

  Inventory before = inventory;
  InventorySlotRef bad[] = {slot(INVENTORY_SLOT_CARRIED, 36)};
  CHECK(inventoryDragDistribute(&inventory, bad, 1, INVENTORY_DRAG_EVEN) == 0);
  CHECK(memcmp(&inventory, &before, sizeof(inventory)) == 0);
}

static void setStoneRecipe(Inventory* inventory, uint16_t count) {
  for (int slotIndex = 0; slotIndex < INVENTORY_CRAFTING_SLOT_COUNT; slotIndex++)
    inventory->crafting[slotIndex] = (ItemStack){ITEM_STONE, count};
}

static void testCrafting(void) {
  Inventory inventory;
  inventoryClear(&inventory);
  setStoneRecipe(&inventory, 2);
  CHECK(inventoryCraftResult(&inventory).item == ITEM_STONE_BRICKS && inventoryCraftResult(&inventory).count == 4);
  CHECK(inventoryGet(&inventory, slot(INVENTORY_SLOT_RESULT, 0)).count == 4);
  inventory.cursor = (ItemStack){ITEM_STONE_BRICKS, 996};
  Inventory before = inventory;
  CHECK(!inventoryCraftOnce(&inventory));
  CHECK(memcmp(&inventory, &before, sizeof(inventory)) == 0);
  inventory.cursor.count = 995;
  uint32_t total = totalUnits(&inventory);
  CHECK(inventoryCraftOnce(&inventory));
  CHECK(inventory.cursor.count == 999 && totalUnits(&inventory) == total);
  for (int slotIndex = 0; slotIndex < INVENTORY_CRAFTING_SLOT_COUNT; slotIndex++)
    CHECK(inventory.crafting[slotIndex].count == 1);
  CHECK(inventoryShiftTransfer(&inventory, slot(INVENTORY_SLOT_RESULT, 0)));
  CHECK(inventory.carried[0].item == ITEM_STONE_BRICKS && inventory.carried[0].count == 4 && totalUnits(&inventory) == total);
  for (int slotIndex = 0; slotIndex < INVENTORY_CRAFTING_SLOT_COUNT; slotIndex++)
    CHECK(empty(inventory.crafting[slotIndex]));

  inventoryClear(&inventory);
  setStoneRecipe(&inventory, 3);
  fillCarried(&inventory, ITEM_DIRT, 999);
  before = inventory;
  CHECK(inventoryCraftAll(&inventory) == 0);
  CHECK(memcmp(&inventory, &before, sizeof(inventory)) == 0);

  inventory.carried[35] = (ItemStack){ITEM_STONE_BRICKS, 996}; // Only three output spaces: still no craft.
  before = inventory;
  CHECK(inventoryCraftAll(&inventory) == 0);
  CHECK(memcmp(&inventory, &before, sizeof(inventory)) == 0);

  inventory.carried[35] = (ItemStack){ITEM_STONE_BRICKS, 991}; // Eight spaces: exactly two whole crafts.
  total = totalUnits(&inventory);
  CHECK(inventoryCraftAll(&inventory) == 2);
  CHECK(inventory.carried[35].count == 999 && totalUnits(&inventory) == total);
  for (int slotIndex = 0; slotIndex < INVENTORY_CRAFTING_SLOT_COUNT; slotIndex++)
    CHECK(inventory.crafting[slotIndex].count == 1);

  for (int logSlot = 0; logSlot < INVENTORY_CRAFTING_SLOT_COUNT; logSlot++) {
    inventoryClear(&inventory);
    inventory.crafting[logSlot] = (ItemStack){ITEM_OAK_LOG, 3};
    ItemStack result = inventoryCraftResult(&inventory);
    CHECK(result.item == ITEM_OAK_PLANKS && result.count == 4);
    for (int slotIndex = 0; slotIndex < INVENTORY_CRAFTING_SLOT_COUNT; slotIndex++)
      CHECK(slotIndex == logSlot || empty(inventory.crafting[slotIndex]));

    inventory.cursor = (ItemStack){ITEM_OAK_PLANKS, 996};
    before = inventory;
    CHECK(!inventoryCraftOnce(&inventory));
    CHECK(memcmp(&inventory, &before, sizeof(inventory)) == 0);
    inventory.cursor.count = 995;
    CHECK(inventoryCraftOnce(&inventory));
    CHECK(inventory.cursor.item == ITEM_OAK_PLANKS && inventory.cursor.count == 999);
    CHECK(inventory.crafting[logSlot].item == ITEM_OAK_LOG && inventory.crafting[logSlot].count == 2);
  }

  inventoryClear(&inventory);
  inventory.crafting[0] = (ItemStack){ITEM_OAK_LOG, 2};
  inventory.crafting[3] = (ItemStack){ITEM_DIRT, 1};
  CHECK(empty(inventoryCraftResult(&inventory)));
  before = inventory;
  CHECK(!inventoryCraftOnce(&inventory) && inventoryCraftAll(&inventory) == 0);
  CHECK(memcmp(&inventory, &before, sizeof(inventory)) == 0);

  inventoryClear(&inventory);
  inventory.crafting[2] = (ItemStack){ITEM_OAK_LOG, 3};
  fillCarried(&inventory, ITEM_DIRT, 999);
  inventory.carried[35] = (ItemStack){ITEM_OAK_PLANKS, 996};
  before = inventory;
  CHECK(inventoryCraftAll(&inventory) == 0);
  CHECK(memcmp(&inventory, &before, sizeof(inventory)) == 0);
  inventory.carried[35].count = 991;
  CHECK(inventoryCraftAll(&inventory) == 2);
  CHECK(inventory.carried[35].item == ITEM_OAK_PLANKS && inventory.carried[35].count == 999);
  CHECK(inventory.crafting[2].item == ITEM_OAK_LOG && inventory.crafting[2].count == 1);
}

static void testSerialization(void) {
  Inventory source;
  inventoryClear(&source);
  source.carried[0] = (ItemStack){ITEM_GRASS_BLOCK, 1};
  source.carried[34] = (ItemStack){ITEM_OAK_LOG, 7};
  source.carried[35] = (ItemStack){ITEM_DIRT, 2};
  source.armor[INVENTORY_ARMOR_HEAD] = (ItemStack){ITEM_LEATHER_HELMET, 1};
  source.offhand = (ItemStack){ITEM_OAK_LEAVES, 3};
  source.crafting[3] = (ItemStack){ITEM_COBBLESTONE, 4};
  source.cursor = (ItemStack){ITEM_OAK_PLANKS, 5};
  CHECK(inventoryValidate(&source));

  ItemStack stacks[INVENTORY_SERIALIZED_STACK_COUNT];
  CHECK(inventoryExportStacks(&source, stacks));
  CHECK(stacks[0].item == ITEM_GRASS_BLOCK && stacks[35].item == ITEM_DIRT);
  CHECK(stacks[34].item == ITEM_OAK_LOG && stacks[34].count == 7);
  CHECK(stacks[36].item == ITEM_LEATHER_HELMET);
  CHECK(stacks[40].item == ITEM_OAK_LEAVES && stacks[40].count == 3);
  CHECK(stacks[44].item == ITEM_COBBLESTONE);
  CHECK(stacks[45].item == ITEM_OAK_PLANKS);

  Inventory loaded;
  inventoryInit(&loaded);
  CHECK(inventoryImportStacks(&loaded, stacks));
  CHECK(memcmp(&loaded, &source, sizeof(source)) == 0);

  Inventory before = loaded;
  stacks[0] = (ItemStack){ITEM_DIRT, 1000};
  CHECK(!inventoryImportStacks(&loaded, stacks));
  CHECK(memcmp(&loaded, &before, sizeof(loaded)) == 0);
  stacks[0] = source.carried[0];
  stacks[36] = (ItemStack){ITEM_DIRT, 1};
  CHECK(!inventoryImportStacks(&loaded, stacks));
  CHECK(memcmp(&loaded, &before, sizeof(loaded)) == 0);
}

static void testCloseAndInvalidState(void) {
  Inventory inventory;
  inventoryClear(&inventory);
  fillCarried(&inventory, ITEM_DIRT, 999);
  inventory.cursor = (ItemStack){ITEM_STONE, 7};
  inventory.crafting[0] = (ItemStack){ITEM_STONE, 3};
  inventory.crafting[1] = (ItemStack){ITEM_COBBLESTONE, 4};
  inventory.crafting[2] = (ItemStack){ITEM_OAK_PLANKS, 5};
  inventory.crafting[3] = (ItemStack){ITEM_STONE_BRICKS, 6};
  uint32_t total = totalUnits(&inventory);
  ItemStack drops[INVENTORY_CLOSE_DROP_MAX];
  size_t dropCount = 99;
  CHECK(inventoryClose(&inventory, drops, &dropCount));
  CHECK(dropCount == 5 && empty(inventory.cursor));
  for (int slotIndex = 0; slotIndex < INVENTORY_CRAFTING_SLOT_COUNT; slotIndex++)
    CHECK(empty(inventory.crafting[slotIndex]));
  CHECK(totalUnits(&inventory) + dropUnits(drops, dropCount) == total);
  CHECK(inventoryValidate(&inventory));

  inventoryClear(&inventory);
  inventory.carried[0] = (ItemStack){ITEM_STONE, 998};
  inventory.cursor = (ItemStack){ITEM_STONE, 2};
  inventory.crafting[0] = (ItemStack){ITEM_STONE, 1};
  total = totalUnits(&inventory);
  CHECK(inventoryClose(&inventory, drops, &dropCount));
  CHECK(dropCount == 0 && inventory.carried[0].count == 999 && inventory.carried[1].item == ITEM_STONE && inventory.carried[1].count == 2);
  CHECK(totalUnits(&inventory) == total);

  inventoryClear(&inventory);
  inventory.armor[INVENTORY_ARMOR_HEAD] = (ItemStack){ITEM_DIRT, 1};
  CHECK(!inventoryValidate(&inventory));
  Inventory before = inventory;
  CHECK(!inventoryAdd(&inventory, (ItemStack){ITEM_DIRT, 1}));
  CHECK(memcmp(&inventory, &before, sizeof(inventory)) == 0);
  CHECK(!inventoryClose(&inventory, drops, &dropCount));
  CHECK(memcmp(&inventory, &before, sizeof(inventory)) == 0);

  inventoryClear(NULL);
  inventoryInit(NULL);
  CHECK(!inventoryValidate(NULL));
  CHECK(empty(inventoryGet(NULL, slot(INVENTORY_SLOT_CARRIED, 0))));
  CHECK(!inventoryCanAdd(NULL, (ItemStack){ITEM_DIRT, 1}));
  CHECK(!inventoryClose(NULL, drops, &dropCount));
}

int main(void) {
  testIdentityAndDefaults();
  testAddAndRemove();
  testCursorClicks();
  testShiftAndHotbar();
  testDirectSwap();
  testGatherAndDrag();
  testCrafting();
  testSerialization();
  testCloseAndInvalidState();
  puts("Inventory ownership, transfer, crafting, and serialization tests passed");
  return 0;
}
