#ifndef INVENTORY_INPUT_CHECKS_H
#define INVENTORY_INPUT_CHECKS_H

#include "graphics/inventory_ui.h"
#include <string.h>

static void inventoryTestPoint(GLFWwindow* window, InventorySlotRef slot) {
  int width, height, windowWidth, windowHeight;
  glfwGetFramebufferSize(window, &width, &height);
  glfwGetWindowSize(window, &windowWidth, &windowHeight);
  InventoryUILayout layout;
  InventoryUIRect rect;
  CHECK(inventoryUILayout(width, height, &layout));
  CHECK(inventoryUISlotRect(&layout, slot, &rect));
  mouseCallback(window, (rect.x + rect.width * 0.5) * windowWidth / width, (rect.y + rect.height * 0.5) * windowHeight / height);
}

static void inventoryTestClick(GLFWwindow* window, InventorySlotRef slot, int button, int mods) {
  eventSeconds += 0.4;
  inventoryTestPoint(window, slot);
  mouseButtonCallback(window, button, GLFW_PRESS, mods);
  mouseButtonCallback(window, button, GLFW_RELEASE, mods);
}

static uint32_t inventoryTestDroppedCount(const DroppedItems* drops, uint16_t item) {
  uint32_t total = 0;
  for (size_t i = 0; i < DROPPED_ITEM_CAPACITY; i++)
    if (drops->items[i].active && drops->items[i].stack.item == item)
      total += drops->items[i].stack.count;
  return total;
}

static void testInventoryInput(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  InputState original = *input;
  Camera camera = *input->camera;
  int priorKey = pressedKey, priorCursor = cursorMode;
  double priorSeconds = eventSeconds;
  eventSeconds = 100;
  pressedKey = -1;
  for (int x = 71; x <= 75; x++)
    for (int z = 71; z <= 77; z++)
      for (int y = 39; y <= 44; y++)
        CHECK(setBlock(&(Vec3i){x, y, z}, y == 39 ? BLOCK_STONE : BLOCK_AIR));
  CHECK(playerSetPosition(&input->player, (Vec3){72.5f, 40, 72.5f}));
  input->flying = false;
  input->camera->position = playerEyePosition(&input->player);
  input->camera->yaw = 90;
  input->camera->pitch = 0;
  updateCameraVectors(input->camera);
  inventoryClear(&input->inventory);
  input->drops = (DroppedItems){0};
  input->inventory.carried[0] = (ItemStack){ITEM_STONE, 10};
  input->inventory.carried[1] = (ItemStack){ITEM_DIRT, 7};
  input->inventory.carried[9] = (ItemStack){ITEM_LEATHER_HELMET, 1};
  setCursorCaptured(window, true);
  keyCallback(window, GLFW_KEY_E, 0, GLFW_PRESS, 0);
  CHECK(input->inventoryOpen && cursorMode == GLFW_CURSOR_NORMAL);
  keyCallback(window, GLFW_KEY_E, 0, GLFW_REPEAT, 0);
  Camera pausedCamera = *input->camera;
  bool flying = input->flying;
  CameraView view = input->view;
  pressedKey = GLFW_KEY_W;
  processInput(window, input, 20);
  mouseCallback(window, 10, 10);
  keyCallback(window, GLFW_KEY_ENTER, 0, GLFW_PRESS, 0);
  keyCallback(window, GLFW_KEY_F6, 0, GLFW_PRESS, 0);
  keyCallback(window, GLFW_KEY_SPACE, 0, GLFW_PRESS, 0);
  CHECK(!input->chat.open && !input->jumpRequested && input->view == view && input->flying == flying && input->simulationSteps == 0);
  CHECK(!memcmp(input->camera, &pausedCamera, sizeof(pausedCamera)));
  pressedKey = -1;

  // Registered inventory controls build one recipe, including right-click splits.
  inventoryTestClick(window, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 0}, GLFW_MOUSE_BUTTON_RIGHT, 0);
  CHECK(input->inventory.cursor.count == 5 && input->inventory.carried[0].count == 5);
  for (int slot = 0; slot < 4; slot++)
    inventoryTestClick(window, (InventorySlotRef){INVENTORY_SLOT_CRAFTING, (uint8_t)slot}, GLFW_MOUSE_BUTTON_RIGHT, 0);
  CHECK(input->inventory.cursor.count == 1 && inventoryCraftResult(&input->inventory).count == 4);
  inventoryTestClick(window, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 0}, GLFW_MOUSE_BUTTON_LEFT, 0);
  CHECK(input->inventory.carried[0].count == 6 && !input->inventory.cursor.count);
  inventoryTestClick(window, (InventorySlotRef){INVENTORY_SLOT_RESULT, 0}, GLFW_MOUSE_BUTTON_LEFT, 0);
  CHECK(input->inventory.cursor.item == ITEM_STONE_BRICKS && input->inventory.cursor.count == 4 && !inventoryCraftResult(&input->inventory).count);
  inventoryTestClick(window, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 10}, GLFW_MOUSE_BUTTON_LEFT, 0);
  CHECK(inventoryCountItem(&input->inventory, ITEM_STONE) == 6 && inventoryCountItem(&input->inventory, ITEM_STONE_BRICKS) == 4);
  inventoryTestClick(window, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 9}, GLFW_MOUSE_BUTTON_LEFT, GLFW_MOD_SHIFT);
  CHECK(input->inventory.armor[0].item == ITEM_LEATHER_HELMET && !input->inventory.carried[9].count);
  inventoryTestClick(window, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 0}, GLFW_MOUSE_BUTTON_LEFT, 0);
  Inventory invalidArmor = input->inventory;
  inventoryTestClick(window, (InventorySlotRef){INVENTORY_SLOT_ARMOR, 0}, GLFW_MOUSE_BUTTON_RIGHT, 0);
  CHECK(!memcmp(&invalidArmor, &input->inventory, sizeof(invalidArmor)));
  inventoryTestClick(window, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 0}, GLFW_MOUSE_BUTTON_LEFT, 0);
  inventoryTestPoint(window, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 10});
  keyCallback(window, GLFW_KEY_5, 0, GLFW_PRESS, 0);
  CHECK(input->inventory.carried[4].item == ITEM_STONE_BRICKS && !input->inventory.carried[10].count);
  inventoryTestPoint(window, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 4});
  keyCallback(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  CHECK(input->inventory.offhand.count == 4 && !input->inventory.carried[4].count && input->flying == flying);

  // A drag spans real pointer callbacks, retains the remainder, and cancels on focus loss.
  inventoryClear(&input->inventory);
  input->inventory.cursor = (ItemStack){ITEM_STONE, 10};
  inventoryTestPoint(window, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 20});
  mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
  inventoryTestPoint(window, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 21});
  inventoryTestPoint(window, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 22});
  processInput(window, input, 1.0 / 30);
  mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
  CHECK(input->inventory.cursor.count == 1);
  CHECK(input->inventory.carried[20].count == 3 && input->inventory.carried[21].count == 3 && input->inventory.carried[22].count == 3);
  inventoryClear(&input->inventory);
  input->inventory.carried[0] = (ItemStack){ITEM_STONE, 2};
  input->inventory.carried[9] = (ItemStack){ITEM_STONE, 3};
  input->inventory.carried[10] = (ItemStack){ITEM_STONE, 4};
  inventoryTestClick(window, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 0}, GLFW_MOUSE_BUTTON_LEFT, 0);
  eventSeconds += 0.1;
  mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
  mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
  CHECK(input->inventory.cursor.count == 9 && !input->inventory.carried[9].count && !input->inventory.carried[10].count);
  inventoryTestPoint(window, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 20});
  mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
  inventoryTestPoint(window, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 21});
  Inventory beforeFocus = input->inventory;
  focused = GLFW_FALSE;
  windowFocusCallback(window, GLFW_FALSE);
  mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
  focused = GLFW_TRUE;
  windowFocusCallback(window, GLFW_TRUE);
  CHECK(input->inventoryOpen && !input->inventoryGesture.pending && !memcmp(&beforeFocus, &input->inventory, sizeof(beforeFocus)));
  keyCallback(window, GLFW_KEY_E, 0, GLFW_PRESS, 0);
  CHECK(!input->inventoryOpen && cursorMode == GLFW_CURSOR_DISABLED && input->inventory.carried[0].count == 9);
  float yaw = input->camera->yaw;
  mouseCallback(window, 2000, 2000);
  CHECK(input->camera->yaw == yaw);
  keyCallback(window, GLFW_KEY_1, 0, GLFW_PRESS, 0);
  keyCallback(window, GLFW_KEY_Q, 0, GLFW_PRESS, 0);
  CHECK(input->inventory.carried[0].count == 8 && inventoryTestDroppedCount(&input->drops, ITEM_STONE) == 1);
  keyCallback(window, GLFW_KEY_Q, 0, GLFW_PRESS, GLFW_MOD_CONTROL);
  CHECK(!input->inventory.carried[0].count && inventoryTestDroppedCount(&input->drops, ITEM_STONE) == 9);

  // Full inventory and drop storage reject closure without losing transient items.
  keyCallback(window, GLFW_KEY_E, 0, GLFW_PRESS, 0);
  inventoryClear(&input->inventory);
  for (int slot = 0; slot < INVENTORY_CARRIED_SLOT_COUNT; slot++)
    input->inventory.carried[slot] = (ItemStack){ITEM_GRASS_BLOCK, 999};
  input->inventory.cursor = (ItemStack){ITEM_STONE, 2};
  for (int slot = 0; slot < 4; slot++)
    input->inventory.crafting[slot] = (ItemStack){ITEM_STONE, 1};
  input->drops = (DroppedItems){0};
  for (int slot = 0; slot < DROPPED_ITEM_CAPACITY; slot++)
    CHECK(droppedItemsSpawn(&input->drops, (ItemStack){ITEM_DIRT, 999}, (Vec3){-120.0f + (slot % 16) * 2, 50, -120.0f + (slot / 16) * 2}, (Vec3){0}, 0));
  Inventory beforeClose = input->inventory;
  DroppedItems beforeDrops = input->drops;
  keyCallback(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  CHECK(input->inventoryOpen && input->inventoryNotice && !memcmp(&beforeClose, &input->inventory, sizeof(beforeClose)) &&
        !memcmp(&beforeDrops, &input->drops, sizeof(beforeDrops)));
  input->drops.items[0] = (DroppedItem){0};
  keyCallback(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  CHECK(!input->inventoryOpen && !input->inventory.cursor.count && !input->inventory.crafting[0].count);
  CHECK(inventoryTestDroppedCount(&input->drops, ITEM_STONE) == 6);

  // Placement consumes only committed blocks and falls back to a placeable offhand.
  input->drops = (DroppedItems){0};
  inventoryClear(&input->inventory);
  input->inventory.carried[0] = (ItemStack){ITEM_STONE, 1};
  input->inventory.offhand = (ItemStack){ITEM_STONE_BRICKS, 1};
  input->camera->front = (Vec3){0, 0, 1};
  CHECK(setBlock(&(Vec3i){72, 41, 76}, BLOCK_DIRT));
  mouseButtonCallback(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
  CHECK(getBlock(&(Vec3i){72, 41, 75})->id == BLOCK_STONE && !input->inventory.carried[0].count);
  mouseButtonCallback(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
  CHECK(getBlock(&(Vec3i){72, 41, 74})->id == BLOCK_STONE_BRICKS && !input->inventory.offhand.count);
  input->inventory.carried[0] = (ItemStack){ITEM_STONE, 1};
  input->camera->front = (Vec3){0, -1, 0};
  mouseButtonCallback(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
  CHECK(input->inventory.carried[0].count == 1 && getBlock(&(Vec3i){72, 40, 72})->id == BLOCK_AIR);
  input->camera->front = (Vec3){0, 0, 1};
  mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
  finishHandBreak(window);
  CHECK(getBlock(&(Vec3i){72, 41, 74})->id == BLOCK_AIR && inventoryTestDroppedCount(&input->drops, ITEM_STONE_BRICKS) == 1);

  *input->camera = camera;
  *input = original;
  cursorMode = priorCursor;
  pressedKey = priorKey;
  eventSeconds = priorSeconds;
  puts("Application inventory clicks, crafting, equipment, dragging, dropping, overflow and placement checks passed");
}

static unsigned inventoryLivePanels;
static unsigned inventoryPausedTick;
static Vec3 inventoryPausedPosition;

static void inventoryLiveFrame(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  if (frame == 102) {
    CHECK(playerSetPosition(&input->player, (Vec3){-60.5f, 40, -60.5f}));
    input->camera->position = playerEyePosition(&input->player);
    input->view = CAMERA_FIRST_PERSON;
    input->showDebug = false;
    inventoryInit(&input->inventory);
    input->drops = (DroppedItems){0};
    input->selectedSlot = 2;
    keyCallback(window, GLFW_KEY_E, 0, GLFW_PRESS, 0);
    CHECK(input->inventoryOpen);
    pressedKey = GLFW_KEY_W;
  }
  if (frame == 103) {
    for (int slot = 9; slot < 13; slot++)
      inventoryTestClick(window, (InventorySlotRef){INVENTORY_SLOT_CARRIED, (uint8_t)slot}, GLFW_MOUSE_BUTTON_LEFT, GLFW_MOD_SHIFT);
    for (int slot = 0; slot < 4; slot++)
      CHECK(input->inventory.armor[slot].item == ITEM_LEATHER_HELMET + slot);
  }
  if (frame == 104) {
    input->inventory.carried[2].count = 8;
    inventoryTestClick(window, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 2}, GLFW_MOUSE_BUTTON_LEFT, 0);
    inventoryTestPoint(window, (InventorySlotRef){INVENTORY_SLOT_CRAFTING, 0});
    mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
    for (int slot = 1; slot < 4; slot++)
      inventoryTestPoint(window, (InventorySlotRef){INVENTORY_SLOT_CRAFTING, (uint8_t)slot});
    mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
    CHECK(!input->inventory.cursor.count && inventoryCraftResult(&input->inventory).count == 4);
  }
  if (frame == 105) {
    inventoryTestClick(window, (InventorySlotRef){INVENTORY_SLOT_RESULT, 0}, GLFW_MOUSE_BUTTON_LEFT, 0);
    CHECK(input->inventory.cursor.count == 4 && input->inventory.crafting[0].count == 1);
  }
  if (frame == 106) {
    inventoryTestClick(window, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 13}, GLFW_MOUSE_BUTTON_LEFT, 0);
    keyCallback(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
    CHECK(!input->inventoryOpen && !input->inventory.crafting[0].count && input->inventory.carried[2].count == 4);
    input->view = CAMERA_THIRD_PERSON_FRONT;
    pressedKey = -1;
  }
  if (frame == 107) {
    keyCallback(window, GLFW_KEY_E, 0, GLFW_PRESS, 0);
    // Inspect the same equipped model with a portrait framebuffer and pointer.
    glfwSetWindowSize(window, 360, 640);
    glfwPollEvents();
    inventoryTestPoint(window, (InventorySlotRef){INVENTORY_SLOT_ARMOR, 0});
  }
  if (frame == 108)
    iconified = true;
  if (frame == 109) {
    iconified = false;
    glfwSetWindowSize(window, 960, 540);
    glfwPollEvents();
    input->view = CAMERA_FIRST_PERSON;
    inventoryTestPoint(window, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 13});
    CHECK(input->inventoryOpen && !input->inventoryGesture.pending);
  }
  if (frame == 110) {
    keyCallback(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
    CHECK(!input->inventoryOpen && cursorMode == GLFW_CURSOR_DISABLED);
    CHECK(inventoryLivePanels == 6);
  }
  eventSeconds = -1;
  inventoryPausedTick = input->clock.tick;
  inventoryPausedPosition = input->camera->position;
}

static void checkInventoryLiveFrame(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  if (!input->inventoryOpen)
    return;
  CHECK(input->simulationSteps == 0 && input->clock.tick == inventoryPausedTick);
  CHECK(!memcmp(&input->camera->position, &inventoryPausedPosition, sizeof(inventoryPausedPosition)));
  CHECK(inventoryPreviewFrame == frame);
  inventoryLivePanels++;
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  InventoryUILayout layout;
  CHECK(inventoryUILayout(width, height, &layout));
  // Sample a blank panel margin, away from its bevel, labels, slots and preview.
  int x = layout.panel.x + (int)(3 * layout.scale);
  int y = height - layout.panel.y - (int)(84 * layout.scale);
  unsigned char panel[3];
  glReadPixels(x, y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, panel);
  CHECK(panel[0] >= 180 && panel[0] <= 210 && abs(panel[0] - panel[1]) <= 1 && abs(panel[1] - panel[2]) <= 1);
}

#endif
