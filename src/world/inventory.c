#include "inventory.h"
#include <string.h>

static ItemStack emptyStack(void) {
  return (ItemStack){0};
}

static bool stackEmpty(ItemStack stack) {
  return stack.item == ITEM_NONE && stack.count == 0;
}

static bool stackEqual(ItemStack a, ItemStack b) {
  return a.item == b.item && a.count == b.count;
}

bool inventoryItemValid(uint16_t item) {
  return item <= ITEM_ID_LAST;
}

bool inventoryItemIsEquipment(uint16_t item) {
  return item >= ITEM_LEATHER_HELMET && item <= ITEM_LEATHER_BOOTS;
}

int inventoryItemArmorSlot(uint16_t item) {
  switch (item) {
  case ITEM_LEATHER_HELMET:
    return INVENTORY_ARMOR_HEAD;
  case ITEM_LEATHER_CHESTPLATE:
    return INVENTORY_ARMOR_CHEST;
  case ITEM_LEATHER_LEGGINGS:
    return INVENTORY_ARMOR_LEGS;
  case ITEM_LEATHER_BOOTS:
    return INVENTORY_ARMOR_FEET;
  default:
    return -1;
  }
}

int inventoryItemArmorPoints(uint16_t item) {
  switch (item) {
  case ITEM_LEATHER_HELMET:
  case ITEM_LEATHER_BOOTS:
    return 1;
  case ITEM_LEATHER_CHESTPLATE:
    return 3;
  case ITEM_LEATHER_LEGGINGS:
    return 2;
  default:
    return 0;
  }
}

uint16_t inventoryItemMaxStack(uint16_t item) {
  if (!inventoryItemValid(item) || item == ITEM_NONE)
    return 0;
  return inventoryItemIsEquipment(item) ? 1 : INVENTORY_STACK_MAX;
}

int inventoryItemBlock(uint16_t item) {
  if (item >= ITEM_GRASS_BLOCK && item <= ITEM_STONE_BRICKS)
    return (int)item;
  switch (item) {
  case ITEM_OAK_LOG:
    return BLOCK_OAK_LOG;
  case ITEM_OAK_LEAVES:
    return BLOCK_OAK_LEAVES;
  default:
    return BLOCK_AIR;
  }
}

uint16_t inventoryBlockItem(int block) {
  if (block >= BLOCK_GRASS && block <= BLOCK_STONE_BRICKS)
    return (uint16_t)block;
  switch (block) {
  case BLOCK_OAK_LOG:
    return ITEM_OAK_LOG;
  case BLOCK_OAK_LEAVES:
    return ITEM_OAK_LEAVES;
  case BLOCK_LEAFY_GRASS:
    return ITEM_GRASS_BLOCK;
  default:
    return ITEM_NONE;
  }
}

const char* inventoryItemName(uint16_t item) {
  switch (item) {
  case ITEM_GRASS_BLOCK:
    return "Grass Block";
  case ITEM_DIRT:
    return "Dirt";
  case ITEM_STONE:
    return "Stone";
  case ITEM_COBBLESTONE:
    return "Cobblestone";
  case ITEM_OAK_PLANKS:
    return "Oak Planks";
  case ITEM_STONE_BRICKS:
    return "Stone Bricks";
  case ITEM_LEATHER_HELMET:
    return "Leather Cap";
  case ITEM_LEATHER_CHESTPLATE:
    return "Leather Tunic";
  case ITEM_LEATHER_LEGGINGS:
    return "Leather Pants";
  case ITEM_LEATHER_BOOTS:
    return "Leather Boots";
  case ITEM_OAK_LOG:
    return "Oak Log";
  case ITEM_OAK_LEAVES:
    return "Oak Leaves";
  default:
    return "";
  }
}

uint32_t inventoryItemColor(uint16_t item) {
  switch (item) {
  case ITEM_GRASS_BLOCK:
    return UINT32_C(0x6AA84F);
  case ITEM_DIRT:
    return UINT32_C(0x8B5A2B);
  case ITEM_STONE:
    return UINT32_C(0x8A8A8A);
  case ITEM_COBBLESTONE:
    return UINT32_C(0x707070);
  case ITEM_OAK_PLANKS:
    return UINT32_C(0xC49A62);
  case ITEM_STONE_BRICKS:
    return UINT32_C(0x888888);
  case ITEM_LEATHER_HELMET:
  case ITEM_LEATHER_CHESTPLATE:
  case ITEM_LEATHER_LEGGINGS:
  case ITEM_LEATHER_BOOTS:
    return UINT32_C(0xA06540);
  case ITEM_OAK_LOG:
    return UINT32_C(0x6B4F2A);
  case ITEM_OAK_LEAVES:
    return UINT32_C(0x4F7F3B);
  default:
    return 0;
  }
}

bool inventoryStackValid(ItemStack stack) {
  if (!inventoryItemValid(stack.item))
    return false;
  if (stack.item == ITEM_NONE)
    return stack.count == 0;
  return stack.count > 0 && stack.count <= inventoryItemMaxStack(stack.item);
}

bool inventorySlotRefValid(InventorySlotRef slot) {
  switch (slot.kind) {
  case INVENTORY_SLOT_CARRIED:
    return slot.index < INVENTORY_CARRIED_SLOT_COUNT;
  case INVENTORY_SLOT_ARMOR:
    return slot.index < INVENTORY_ARMOR_SLOT_COUNT;
  case INVENTORY_SLOT_CRAFTING:
    return slot.index < INVENTORY_CRAFTING_SLOT_COUNT;
  case INVENTORY_SLOT_OFFHAND:
  case INVENTORY_SLOT_CURSOR:
  case INVENTORY_SLOT_RESULT:
    return slot.index == 0;
  default:
    return false;
  }
}

bool inventorySlotAccepts(InventorySlotRef slot, ItemStack stack) {
  if (!inventorySlotRefValid(slot) || !inventoryStackValid(stack) || slot.kind == INVENTORY_SLOT_RESULT)
    return false;
  if (slot.kind != INVENTORY_SLOT_ARMOR || stackEmpty(stack))
    return true;
  return inventoryItemArmorSlot(stack.item) == slot.index;
}

static ItemStack* slotPointer(Inventory* inventory, InventorySlotRef slot) {
  if (!inventory || !inventorySlotRefValid(slot))
    return NULL;
  switch (slot.kind) {
  case INVENTORY_SLOT_CARRIED:
    return &inventory->carried[slot.index];
  case INVENTORY_SLOT_ARMOR:
    return &inventory->armor[slot.index];
  case INVENTORY_SLOT_OFFHAND:
    return &inventory->offhand;
  case INVENTORY_SLOT_CRAFTING:
    return &inventory->crafting[slot.index];
  case INVENTORY_SLOT_CURSOR:
    return &inventory->cursor;
  default:
    return NULL;
  }
}

static const ItemStack* constSlotPointer(const Inventory* inventory, InventorySlotRef slot) {
  return slotPointer((Inventory*)inventory, slot);
}

void inventoryClear(Inventory* inventory) {
  if (inventory)
    memset(inventory, 0, sizeof(*inventory));
}

void inventoryInit(Inventory* inventory) {
  if (!inventory)
    return;
  inventoryClear(inventory);
  for (int slot = 0; slot < 6; slot++)
    inventory->carried[slot] = (ItemStack){.item = (uint16_t)(slot + 1), .count = INVENTORY_STACK_MAX};
  inventory->carried[9] = (ItemStack){ITEM_LEATHER_HELMET, 1};
  inventory->carried[10] = (ItemStack){ITEM_LEATHER_CHESTPLATE, 1};
  inventory->carried[11] = (ItemStack){ITEM_LEATHER_LEGGINGS, 1};
  inventory->carried[12] = (ItemStack){ITEM_LEATHER_BOOTS, 1};
}

typedef enum {
  CRAFT_NONE,
  CRAFT_STONE_BRICKS,
  CRAFT_OAK_PLANKS,
} CraftRecipe;

typedef struct {
  CraftRecipe recipe;
  ItemStack result;
  uint16_t crafts;
  int singleSlot;
} CraftMatch;

static CraftMatch craftMatch(const Inventory* inventory) {
  CraftMatch match = {0};
  if (!inventory)
    return match;

  bool allStone = true;
  uint16_t stoneCrafts = INVENTORY_STACK_MAX;
  int occupied = 0;
  int logSlot = -1;
  for (int slot = 0; slot < INVENTORY_CRAFTING_SLOT_COUNT; slot++) {
    ItemStack stack = inventory->crafting[slot];
    if (!inventoryStackValid(stack))
      return (CraftMatch){0};
    if (stack.item != ITEM_STONE)
      allStone = false;
    else if (stack.count < stoneCrafts)
      stoneCrafts = stack.count;
    if (!stackEmpty(stack)) {
      occupied++;
      if (stack.item == ITEM_OAK_LOG)
        logSlot = slot;
    }
  }

  if (allStone)
    return (CraftMatch){.recipe = CRAFT_STONE_BRICKS, .result = {ITEM_STONE_BRICKS, 4}, .crafts = stoneCrafts, .singleSlot = -1};
  if (occupied == 1 && logSlot >= 0)
    return (CraftMatch){.recipe = CRAFT_OAK_PLANKS, .result = {ITEM_OAK_PLANKS, 4}, .crafts = inventory->crafting[logSlot].count, .singleSlot = logSlot};
  return match;
}

static void consumeCrafts(Inventory* inventory, CraftMatch match, uint16_t crafts) {
  if (match.recipe == CRAFT_STONE_BRICKS) {
    for (int slot = 0; slot < INVENTORY_CRAFTING_SLOT_COUNT; slot++) {
      inventory->crafting[slot].count = (uint16_t)(inventory->crafting[slot].count - crafts);
      if (inventory->crafting[slot].count == 0)
        inventory->crafting[slot] = emptyStack();
    }
  } else if (match.recipe == CRAFT_OAK_PLANKS) {
    ItemStack* stack = &inventory->crafting[match.singleSlot];
    stack->count = (uint16_t)(stack->count - crafts);
    if (stack->count == 0)
      *stack = emptyStack();
  }
}

ItemStack inventoryCraftResult(const Inventory* inventory) {
  return craftMatch(inventory).result;
}

ItemStack inventoryGet(const Inventory* inventory, InventorySlotRef slot) {
  if (!inventory || !inventorySlotRefValid(slot))
    return emptyStack();
  if (slot.kind == INVENTORY_SLOT_RESULT)
    return inventoryCraftResult(inventory);
  const ItemStack* stack = constSlotPointer(inventory, slot);
  return stack ? *stack : emptyStack();
}

bool inventoryValidate(const Inventory* inventory) {
  if (!inventory)
    return false;
  for (int slot = 0; slot < INVENTORY_CARRIED_SLOT_COUNT; slot++)
    if (!inventoryStackValid(inventory->carried[slot]))
      return false;
  for (int slot = 0; slot < INVENTORY_ARMOR_SLOT_COUNT; slot++) {
    InventorySlotRef ref = {INVENTORY_SLOT_ARMOR, (uint8_t)slot};
    if (!inventorySlotAccepts(ref, inventory->armor[slot]))
      return false;
  }
  if (!inventoryStackValid(inventory->offhand) || !inventoryStackValid(inventory->cursor))
    return false;
  for (int slot = 0; slot < INVENTORY_CRAFTING_SLOT_COUNT; slot++)
    if (!inventoryStackValid(inventory->crafting[slot]))
      return false;
  return true;
}

uint32_t inventoryCountItem(const Inventory* inventory, uint16_t item) {
  if (!inventory || item == ITEM_NONE || !inventoryItemValid(item))
    return 0;
  uint32_t count = 0;
  for (int slot = 0; slot < INVENTORY_CARRIED_SLOT_COUNT; slot++)
    if (inventory->carried[slot].item == item)
      count += inventory->carried[slot].count;
  for (int slot = 0; slot < INVENTORY_ARMOR_SLOT_COUNT; slot++)
    if (inventory->armor[slot].item == item)
      count += inventory->armor[slot].count;
  if (inventory->offhand.item == item)
    count += inventory->offhand.count;
  for (int slot = 0; slot < INVENTORY_CRAFTING_SLOT_COUNT; slot++)
    if (inventory->crafting[slot].item == item)
      count += inventory->crafting[slot].count;
  if (inventory->cursor.item == item)
    count += inventory->cursor.count;
  return count;
}

static uint32_t rangeCapacity(const ItemStack* slots, int first, int end, uint16_t item) {
  uint32_t capacity = 0;
  uint16_t maximum = inventoryItemMaxStack(item);
  for (int slot = first; slot < end; slot++) {
    if (slots[slot].item == item)
      capacity += maximum - slots[slot].count;
    else if (stackEmpty(slots[slot]))
      capacity += maximum;
  }
  return capacity;
}

static void insertIntoRange(ItemStack* slots, int first, int end, uint16_t item, uint32_t* remaining) {
  uint16_t maximum = inventoryItemMaxStack(item);
  for (int slot = first; slot < end && *remaining; slot++) {
    if (slots[slot].item != item || slots[slot].count >= maximum)
      continue;
    uint32_t space = maximum - slots[slot].count;
    uint32_t moved = *remaining < space ? *remaining : space;
    slots[slot].count = (uint16_t)(slots[slot].count + moved);
    *remaining -= moved;
  }
  for (int slot = first; slot < end && *remaining; slot++) {
    if (!stackEmpty(slots[slot]))
      continue;
    uint32_t moved = *remaining < maximum ? *remaining : maximum;
    slots[slot] = (ItemStack){item, (uint16_t)moved};
    *remaining -= moved;
  }
}

bool inventoryCanAdd(const Inventory* inventory, ItemStack stack) {
  if (!inventoryValidate(inventory) || !inventoryStackValid(stack) || stackEmpty(stack))
    return false;
  return rangeCapacity(inventory->carried, 0, INVENTORY_CARRIED_SLOT_COUNT, stack.item) >= stack.count;
}

bool inventoryAdd(Inventory* inventory, ItemStack stack) {
  if (!inventoryCanAdd(inventory, stack))
    return false;
  Inventory next = *inventory;
  uint32_t remaining = stack.count;
  insertIntoRange(next.carried, 0, INVENTORY_CARRIED_SLOT_COUNT, stack.item, &remaining);
  if (remaining)
    return false;
  *inventory = next;
  return true;
}

bool inventoryRemove(Inventory* inventory, InventorySlotRef source, uint16_t count, ItemStack* removed) {
  if (!inventoryValidate(inventory) || !inventorySlotRefValid(source) || source.kind == INVENTORY_SLOT_RESULT || count == 0)
    return false;
  Inventory next = *inventory;
  ItemStack* stack = slotPointer(&next, source);
  if (!stack || stackEmpty(*stack) || stack->count < count)
    return false;
  ItemStack taken = {stack->item, count};
  stack->count = (uint16_t)(stack->count - count);
  if (stack->count == 0)
    *stack = emptyStack();
  if (!inventoryValidate(&next))
    return false;
  *inventory = next;
  if (removed)
    *removed = taken;
  return true;
}

bool inventoryRemoveItem(Inventory* inventory, uint16_t item, uint16_t count) {
  if (!inventoryValidate(inventory) || item == ITEM_NONE || !inventoryItemValid(item) || count == 0)
    return false;
  uint32_t available = 0;
  for (int slot = 0; slot < INVENTORY_CARRIED_SLOT_COUNT; slot++)
    if (inventory->carried[slot].item == item)
      available += inventory->carried[slot].count;
  if (available < count)
    return false;

  Inventory next = *inventory;
  uint32_t remaining = count;
  for (int slot = 0; slot < INVENTORY_CARRIED_SLOT_COUNT && remaining; slot++) {
    ItemStack* stack = &next.carried[slot];
    if (stack->item != item)
      continue;
    uint32_t taken = remaining < stack->count ? remaining : stack->count;
    stack->count = (uint16_t)(stack->count - taken);
    remaining -= taken;
    if (stack->count == 0)
      *stack = emptyStack();
  }
  *inventory = next;
  return true;
}

bool inventorySwapSlots(Inventory* inventory, InventorySlotRef a, InventorySlotRef b) {
  if (!inventoryValidate(inventory) || !inventorySlotRefValid(a) || !inventorySlotRefValid(b) || a.kind == INVENTORY_SLOT_RESULT || b.kind == INVENTORY_SLOT_RESULT ||
      (a.kind == b.kind && a.index == b.index))
    return false;
  Inventory next = *inventory;
  ItemStack* first = slotPointer(&next, a);
  ItemStack* second = slotPointer(&next, b);
  if (!first || !second || stackEqual(*first, *second) || !inventorySlotAccepts(a, *second) || !inventorySlotAccepts(b, *first))
    return false;
  ItemStack temporary = *first;
  *first = *second;
  *second = temporary;
  if (!inventoryValidate(&next))
    return false;
  *inventory = next;
  return true;
}

bool inventoryCraftOnce(Inventory* inventory) {
  if (!inventoryValidate(inventory))
    return false;
  CraftMatch match = craftMatch(inventory);
  ItemStack result = match.result;
  if (stackEmpty(result))
    return false;
  if (!stackEmpty(inventory->cursor) && inventory->cursor.item != result.item)
    return false;
  if ((uint32_t)inventory->cursor.count + result.count > inventoryItemMaxStack(result.item))
    return false;

  Inventory next = *inventory;
  consumeCrafts(&next, match, 1);
  if (stackEmpty(next.cursor))
    next.cursor = result;
  else
    next.cursor.count = (uint16_t)(next.cursor.count + result.count);
  *inventory = next;
  return true;
}

size_t inventoryCraftAll(Inventory* inventory) {
  if (!inventoryValidate(inventory))
    return 0;
  CraftMatch match = craftMatch(inventory);
  if (stackEmpty(match.result))
    return 0;
  uint32_t capacity = rangeCapacity(inventory->carried, 0, INVENTORY_CARRIED_SLOT_COUNT, match.result.item);
  uint32_t fit = capacity / match.result.count;
  size_t crafts = match.crafts < fit ? match.crafts : fit;
  if (crafts == 0)
    return 0;

  Inventory next = *inventory;
  consumeCrafts(&next, match, (uint16_t)crafts);
  uint32_t output = (uint32_t)crafts * match.result.count;
  insertIntoRange(next.carried, 0, INVENTORY_CARRIED_SLOT_COUNT, match.result.item, &output);
  if (output)
    return 0;
  *inventory = next;
  return crafts;
}

bool inventoryClick(Inventory* inventory, InventorySlotRef slot, bool rightClick) {
  if (!inventoryValidate(inventory) || !inventorySlotRefValid(slot) || slot.kind == INVENTORY_SLOT_CURSOR)
    return false;
  if (slot.kind == INVENTORY_SLOT_RESULT)
    return inventoryCraftOnce(inventory);

  Inventory next = *inventory;
  ItemStack* target = slotPointer(&next, slot);
  ItemStack* cursor = &next.cursor;
  if (!target)
    return false;

  if (rightClick) {
    if (stackEmpty(*cursor)) {
      if (stackEmpty(*target))
        return false;
      uint16_t taken = (uint16_t)((target->count + 1) / 2);
      *cursor = (ItemStack){target->item, taken};
      target->count = (uint16_t)(target->count - taken);
      if (target->count == 0)
        *target = emptyStack();
    } else {
      if (!stackEmpty(*target) && target->item != cursor->item) {
        if (!inventorySlotAccepts(slot, *cursor))
          return false;
        ItemStack previous = *target;
        *target = *cursor;
        *cursor = previous;
      } else {
        ItemStack candidate = stackEmpty(*target) ? (ItemStack){cursor->item, 1} : (ItemStack){target->item, (uint16_t)(target->count + 1)};
        if (target->count >= inventoryItemMaxStack(cursor->item) || !inventorySlotAccepts(slot, candidate))
          return false;
        *target = candidate;
        cursor->count--;
        if (cursor->count == 0)
          *cursor = emptyStack();
      }
    }
  } else if (stackEmpty(*cursor)) {
    if (stackEmpty(*target))
      return false;
    *cursor = *target;
    *target = emptyStack();
  } else if (stackEmpty(*target)) {
    if (!inventorySlotAccepts(slot, *cursor))
      return false;
    *target = *cursor;
    *cursor = emptyStack();
  } else if (target->item == cursor->item) {
    uint16_t maximum = inventoryItemMaxStack(cursor->item);
    if (target->count >= maximum)
      return false;
    uint16_t space = (uint16_t)(maximum - target->count);
    uint16_t moved = cursor->count < space ? cursor->count : space;
    ItemStack candidate = {target->item, (uint16_t)(target->count + moved)};
    if (!inventorySlotAccepts(slot, candidate))
      return false;
    *target = candidate;
    cursor->count = (uint16_t)(cursor->count - moved);
    if (cursor->count == 0)
      *cursor = emptyStack();
  } else {
    if (!inventorySlotAccepts(slot, *cursor))
      return false;
    ItemStack previous = *target;
    *target = *cursor;
    *cursor = previous;
  }

  if (!inventoryValidate(&next))
    return false;
  *inventory = next;
  return true;
}

bool inventoryShiftTransfer(Inventory* inventory, InventorySlotRef source) {
  if (!inventoryValidate(inventory) || !inventorySlotRefValid(source) || source.kind == INVENTORY_SLOT_CURSOR)
    return false;
  if (source.kind == INVENTORY_SLOT_RESULT)
    return inventoryCraftAll(inventory) > 0;

  Inventory next = *inventory;
  ItemStack* origin = slotPointer(&next, source);
  if (!origin || stackEmpty(*origin))
    return false;
  ItemStack moving = *origin;
  uint16_t originalCount = moving.count;
  *origin = emptyStack();

  int armorSlot = inventoryItemArmorSlot(moving.item);
  if (armorSlot >= 0 && !(source.kind == INVENTORY_SLOT_ARMOR && source.index == armorSlot) && stackEmpty(next.armor[armorSlot])) {
    next.armor[armorSlot] = moving;
    moving = emptyStack();
  }

  if (!stackEmpty(moving)) {
    uint32_t remaining = moving.count;
    if (source.kind == INVENTORY_SLOT_CARRIED && source.index < INVENTORY_HOTBAR_SLOT_COUNT)
      insertIntoRange(next.carried, INVENTORY_HOTBAR_SLOT_COUNT, INVENTORY_CARRIED_SLOT_COUNT, moving.item, &remaining);
    else if (source.kind == INVENTORY_SLOT_CARRIED)
      insertIntoRange(next.carried, 0, INVENTORY_HOTBAR_SLOT_COUNT, moving.item, &remaining);
    else
      insertIntoRange(next.carried, 0, INVENTORY_CARRIED_SLOT_COUNT, moving.item, &remaining);
    moving.count = (uint16_t)remaining;
    if (moving.count == 0)
      moving = emptyStack();
  }

  *origin = moving;
  uint16_t moved = (uint16_t)(originalCount - moving.count);
  if (moved == 0 || !inventoryValidate(&next))
    return false;
  *inventory = next;
  return true;
}

bool inventoryHotbarSwap(Inventory* inventory, InventorySlotRef source, int hotbarSlot) {
  if (!inventoryValidate(inventory) || !inventorySlotRefValid(source) || hotbarSlot < 0 || hotbarSlot >= INVENTORY_HOTBAR_SLOT_COUNT || source.kind == INVENTORY_SLOT_CURSOR ||
      source.kind == INVENTORY_SLOT_RESULT)
    return false;
  if (source.kind == INVENTORY_SLOT_CARRIED && source.index == hotbarSlot)
    return false;

  Inventory next = *inventory;
  ItemStack* from = slotPointer(&next, source);
  InventorySlotRef destinationRef = {INVENTORY_SLOT_CARRIED, (uint8_t)hotbarSlot};
  ItemStack* destination = slotPointer(&next, destinationRef);
  if (!from || !destination || !inventorySlotAccepts(source, *destination) || !inventorySlotAccepts(destinationRef, *from) || stackEqual(*from, *destination))
    return false;
  ItemStack temporary = *from;
  *from = *destination;
  *destination = temporary;
  if (!inventoryValidate(&next))
    return false;
  *inventory = next;
  return true;
}

static void gatherStack(ItemStack* source, ItemStack* cursor) {
  if (source == cursor || source->item != cursor->item || stackEmpty(*source))
    return;
  uint16_t maximum = inventoryItemMaxStack(cursor->item);
  uint16_t space = (uint16_t)(maximum - cursor->count);
  uint16_t moved = source->count < space ? source->count : space;
  cursor->count = (uint16_t)(cursor->count + moved);
  source->count = (uint16_t)(source->count - moved);
  if (source->count == 0)
    *source = emptyStack();
}

bool inventoryGather(Inventory* inventory, InventorySlotRef clicked) {
  if (!inventoryValidate(inventory) || !inventorySlotRefValid(clicked) || clicked.kind == INVENTORY_SLOT_CURSOR || clicked.kind == INVENTORY_SLOT_RESULT)
    return false;
  Inventory next = *inventory;
  ItemStack* target = slotPointer(&next, clicked);
  if (!target)
    return false;
  bool changed = false;
  if (stackEmpty(next.cursor)) {
    if (stackEmpty(*target))
      return false;
    next.cursor = *target;
    *target = emptyStack();
    changed = true;
  }
  if (next.cursor.count >= inventoryItemMaxStack(next.cursor.item)) {
    *inventory = next;
    return changed;
  }

  uint16_t before = next.cursor.count;
  for (int slot = 0; slot < INVENTORY_CARRIED_SLOT_COUNT && next.cursor.count < inventoryItemMaxStack(next.cursor.item); slot++)
    gatherStack(&next.carried[slot], &next.cursor);
  for (int slot = 0; slot < INVENTORY_ARMOR_SLOT_COUNT && next.cursor.count < inventoryItemMaxStack(next.cursor.item); slot++)
    gatherStack(&next.armor[slot], &next.cursor);
  if (next.cursor.count < inventoryItemMaxStack(next.cursor.item))
    gatherStack(&next.offhand, &next.cursor);
  for (int slot = 0; slot < INVENTORY_CRAFTING_SLOT_COUNT && next.cursor.count < inventoryItemMaxStack(next.cursor.item); slot++)
    gatherStack(&next.crafting[slot], &next.cursor);
  changed = changed || next.cursor.count != before;
  if (!changed || !inventoryValidate(&next))
    return false;
  *inventory = next;
  return true;
}

static int dragOrdinal(InventorySlotRef slot) {
  switch (slot.kind) {
  case INVENTORY_SLOT_CARRIED:
    return slot.index;
  case INVENTORY_SLOT_ARMOR:
    return INVENTORY_CARRIED_SLOT_COUNT + slot.index;
  case INVENTORY_SLOT_OFFHAND:
    return INVENTORY_CARRIED_SLOT_COUNT + INVENTORY_ARMOR_SLOT_COUNT;
  case INVENTORY_SLOT_CRAFTING:
    return INVENTORY_CARRIED_SLOT_COUNT + INVENTORY_ARMOR_SLOT_COUNT + 1 + slot.index;
  default:
    return -1;
  }
}

static bool slotCanReceiveItem(const Inventory* inventory, InventorySlotRef slot, uint16_t item) {
  const ItemStack* target = constSlotPointer(inventory, slot);
  if (!target)
    return false;
  uint16_t maximum = inventoryItemMaxStack(item);
  if (stackEmpty(*target))
    return inventorySlotAccepts(slot, (ItemStack){item, 1});
  return target->item == item && target->count < maximum;
}

static bool addOneToSlot(Inventory* inventory, InventorySlotRef slot, uint16_t item) {
  ItemStack* target = slotPointer(inventory, slot);
  if (!target || !slotCanReceiveItem(inventory, slot, item) || inventory->cursor.item != item || inventory->cursor.count == 0)
    return false;
  if (stackEmpty(*target))
    *target = (ItemStack){item, 1};
  else
    target->count++;
  inventory->cursor.count--;
  if (inventory->cursor.count == 0)
    inventory->cursor = emptyStack();
  return true;
}

static uint16_t addUpToSlot(Inventory* inventory, InventorySlotRef slot, uint16_t item, uint16_t amount) {
  ItemStack* target = slotPointer(inventory, slot);
  if (!target || amount == 0 || !slotCanReceiveItem(inventory, slot, item) || inventory->cursor.item != item || inventory->cursor.count == 0)
    return 0;
  uint16_t maximum = inventoryItemMaxStack(item);
  uint16_t capacity = stackEmpty(*target) ? maximum : (uint16_t)(maximum - target->count);
  uint16_t moved = amount < capacity ? amount : capacity;
  if (moved > inventory->cursor.count)
    moved = inventory->cursor.count;
  if (stackEmpty(*target))
    *target = (ItemStack){item, moved};
  else
    target->count = (uint16_t)(target->count + moved);
  inventory->cursor.count = (uint16_t)(inventory->cursor.count - moved);
  if (inventory->cursor.count == 0)
    inventory->cursor = emptyStack();
  return moved;
}

uint16_t inventoryDragDistribute(Inventory* inventory, const InventorySlotRef* slots, size_t slotCount, InventoryDragMode mode) {
  if (!inventoryValidate(inventory) || !slots || slotCount == 0 || slotCount > INVENTORY_DRAG_SLOT_MAX || (mode != INVENTORY_DRAG_EVEN && mode != INVENTORY_DRAG_ONE_EACH) ||
      stackEmpty(inventory->cursor))
    return 0;

  InventorySlotRef unique[INVENTORY_DRAG_SLOT_MAX];
  bool seen[INVENTORY_DRAG_SLOT_MAX] = {false};
  size_t uniqueCount = 0;
  for (size_t i = 0; i < slotCount; i++) {
    if (!inventorySlotRefValid(slots[i]))
      return 0;
    int ordinal = dragOrdinal(slots[i]);
    if (ordinal < 0 || seen[ordinal] || !slotCanReceiveItem(inventory, slots[i], inventory->cursor.item))
      continue;
    seen[ordinal] = true;
    unique[uniqueCount++] = slots[i];
  }
  if (uniqueCount == 0)
    return 0;

  Inventory next = *inventory;
  uint16_t before = next.cursor.count;
  if (mode == INVENTORY_DRAG_ONE_EACH) {
    for (size_t i = 0; i < uniqueCount && next.cursor.count; i++)
      (void)addOneToSlot(&next, unique[i], next.cursor.item);
  } else {
    uint16_t share = (uint16_t)(before / uniqueCount);
    if (share == 0)
      return 0;
    uint16_t item = next.cursor.item;
    for (size_t i = 0; i < uniqueCount && next.cursor.count; i++)
      (void)addUpToSlot(&next, unique[i], item, share);
  }
  uint16_t moved = (uint16_t)(before - next.cursor.count);
  if (moved == 0 || !inventoryValidate(&next))
    return 0;
  *inventory = next;
  return moved;
}

bool inventoryExportStacks(const Inventory* inventory, ItemStack stacks[INVENTORY_SERIALIZED_STACK_COUNT]) {
  if (!inventoryValidate(inventory) || !stacks)
    return false;
  size_t index = 0;
  for (int slot = 0; slot < INVENTORY_CARRIED_SLOT_COUNT; slot++)
    stacks[index++] = inventory->carried[slot];
  for (int slot = 0; slot < INVENTORY_ARMOR_SLOT_COUNT; slot++)
    stacks[index++] = inventory->armor[slot];
  stacks[index++] = inventory->offhand;
  for (int slot = 0; slot < INVENTORY_CRAFTING_SLOT_COUNT; slot++)
    stacks[index++] = inventory->crafting[slot];
  stacks[index++] = inventory->cursor;
  return index == INVENTORY_SERIALIZED_STACK_COUNT;
}

bool inventoryImportStacks(Inventory* inventory, const ItemStack stacks[INVENTORY_SERIALIZED_STACK_COUNT]) {
  if (!inventory || !stacks)
    return false;
  Inventory next = {0};
  size_t index = 0;
  for (int slot = 0; slot < INVENTORY_CARRIED_SLOT_COUNT; slot++)
    next.carried[slot] = stacks[index++];
  for (int slot = 0; slot < INVENTORY_ARMOR_SLOT_COUNT; slot++)
    next.armor[slot] = stacks[index++];
  next.offhand = stacks[index++];
  for (int slot = 0; slot < INVENTORY_CRAFTING_SLOT_COUNT; slot++)
    next.crafting[slot] = stacks[index++];
  next.cursor = stacks[index++];
  if (index != INVENTORY_SERIALIZED_STACK_COUNT || !inventoryValidate(&next))
    return false;
  *inventory = next;
  return true;
}

bool inventoryClose(Inventory* inventory, ItemStack drops[INVENTORY_CLOSE_DROP_MAX], size_t* dropCount) {
  if (!inventoryValidate(inventory) || !drops || !dropCount)
    return false;
  Inventory next = *inventory;
  ItemStack output[INVENTORY_CLOSE_DROP_MAX] = {{0}};
  size_t count = 0;
  ItemStack* sources[INVENTORY_CLOSE_DROP_MAX] = {&next.cursor, &next.crafting[0], &next.crafting[1], &next.crafting[2], &next.crafting[3]};
  for (size_t i = 0; i < INVENTORY_CLOSE_DROP_MAX; i++) {
    if (stackEmpty(*sources[i]))
      continue;
    ItemStack moving = *sources[i];
    *sources[i] = emptyStack();
    uint32_t remaining = moving.count;
    insertIntoRange(next.carried, 0, INVENTORY_CARRIED_SLOT_COUNT, moving.item, &remaining);
    if (remaining)
      output[count++] = (ItemStack){moving.item, (uint16_t)remaining};
  }
  if (!inventoryValidate(&next))
    return false;
  *inventory = next;
  memcpy(drops, output, sizeof(output));
  *dropCount = count;
  return true;
}
