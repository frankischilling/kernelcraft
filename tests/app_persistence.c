// Run the real application in separate save/load processes with registered inputs.
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "utils/inputs.h"
#include "graphics/hud.h"
#include "world/world.h"
#include "world/save.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c)                                                                                                                                                                   \
  do {                                                                                                                                                                             \
    if (!(c)) {                                                                                                                                                                    \
      fprintf(stderr, "Persistence application test: %s (line %d)\n", #c, __LINE__);                                                                                               \
      exit(EXIT_FAILURE);                                                                                                                                                          \
    }                                                                                                                                                                              \
  } while (0)
static int frame = -1, swaps, cursorMode = GLFW_CURSOR_NORMAL;
static bool failureShown;
static const Vec3 feet = {-0.5f, 40, 0.5f};
static const Vec3i removed = {-1, 41, 2}, placed = {-1, 41, 3}, exitEdit = {0, 42, 4}, cobblestone = {0, 41, 3};
static const Vec3i building[] = {{-2, 41, 3}, {1, 41, 3}};

static bool crouchScenario(void) {
  const char* phase = getenv("KERNELCRAFT_TEST_RESTART");
  return phase && !strncmp(phase, "crouch-", 7);
}

static bool failing(void) {
  const char* phase = getenv("KERNELCRAFT_TEST_RESTART");
  return phase && !strcmp(phase, "fail");
}

static bool saving(void) {
  const char* phase = getenv("KERNELCRAFT_TEST_RESTART");
  CHECK(phase);
  return !strcmp(phase, "save") || !strcmp(phase, "crouch-save") || failing();
}

static bool standardScenario(void) {
  const char* phase = getenv("KERNELCRAFT_TEST_RESTART");
  return phase && (!strcmp(phase, "save") || !strcmp(phase, "load"));
}

static uint32_t read32(const unsigned char bytes[4]) {
  return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static size_t activeDropCount(const DroppedItems* drops) {
  size_t count = 0;
  for (size_t i = 0; i < DROPPED_ITEM_CAPACITY; i++)
    count += drops->items[i].active ? 1u : 0u;
  return count;
}

static int id(Vec3i cell) {
  const Block* block = getBlock(&cell);
  CHECK(block);
  return block->id;
}

GLFWwindow* __real_glfwCreateWindow(int width, int height, const char* title, GLFWmonitor* monitor, GLFWwindow* share);

GLFWwindow* __wrap_glfwCreateWindow(int width, int height, const char* title, GLFWmonitor* monitor, GLFWwindow* share) {
  (void)width;
  (void)height;
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
  return __real_glfwCreateWindow(640, 480, title, monitor, share);
}

void __wrap_glfwSetInputMode(GLFWwindow* window, int mode, int value) {
  (void)window;
  CHECK(mode == GLFW_CURSOR);
  cursorMode = value;
}

int __wrap_glfwGetInputMode(GLFWwindow* window, int mode) {
  (void)window;
  CHECK(mode == GLFW_CURSOR);
  return cursorMode;
}

int __real_glfwGetWindowAttrib(GLFWwindow* window, int attrib);

int __wrap_glfwGetWindowAttrib(GLFWwindow* window, int attrib) {
  return attrib == GLFW_FOCUSED ? GLFW_TRUE : __real_glfwGetWindowAttrib(window, attrib);
}

int __wrap_glfwGetKey(GLFWwindow* window, int key) {
  (void)window;
  (void)key;
  return GLFW_RELEASE;
}

double __wrap_glfwGetTime(void) {
  return 1.0;
} // Freeze physics so saved feet compare exactly.

int __wrap_glfwWindowShouldClose(GLFWwindow* window) {
  frame++;
  InputState* input = glfwGetWindowUserPointer(window);
  CHECK(input && !input->flying && playerCanOccupyPosture(input->player.position, input->player.crouched));
  GLFWkeyfun key = glfwSetKeyCallback(window, NULL);
  glfwSetKeyCallback(window, key);
  GLFWmousebuttonfun mouse = glfwSetMouseButtonCallback(window, NULL);
  glfwSetMouseButtonCallback(window, mouse);
  CHECK(key && mouse && worldSeed() == 42);
  if (frame == 0)
    CHECK(!input->wireframe);
  if (frame == 0 && saving()) {
    key(window, GLFW_KEY_F4, 0, GLFW_PRESS, 0);
    CHECK(input->wireframe);
    for (int x = -2; x <= 1; x++)
      for (int z = 0; z <= 4; z++) {
        CHECK(setBlock(&(Vec3i){x, 39, z}, BLOCK_STONE));
        for (int y = 40; y <= 44; y++)
          CHECK(setBlock(&(Vec3i){x, y, z}, BLOCK_AIR));
      }

    CHECK(setBlock(&removed, BLOCK_DIRT));
    CHECK(setBlock(&(Vec3i){-1, 41, 4}, BLOCK_GRASS));
    CHECK(playerSetPosition(&input->player, feet));
    input->camera->position = playerEyePosition(&input->player);
    input->camera->yaw = 90;
    input->camera->pitch = 0;
    updateCameraVectors(input->camera);
    mouse(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
    CHECK(id(removed) == BLOCK_DIRT);
    for (int i = 0; i < 5; i++)
      processBlockBreaking(window, input, 0.1);
    CHECK(id(removed) == BLOCK_AIR);
    mouse(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
    key(window, GLFW_KEY_3, 0, GLFW_PRESS, 0);
    mouse(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
    CHECK(id(placed) == BLOCK_STONE);
    CHECK(setBlock(&(Vec3i){0, 41, 4}, BLOCK_GRASS));
    input->camera->position.x = 0.5f;
    key(window, GLFW_KEY_4, 0, GLFW_PRESS, 0);
    CHECK(selectedHotbarSlot(input) == 3 && selectedBlock(input) == BLOCK_COBBLESTONE);
    mouse(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
    CHECK(id(cobblestone) == BLOCK_COBBLESTONE);
    for (int material = 5; material <= 6; material++) {
      Vec3i cell = building[material - 5];
      CHECK(setBlock(&(Vec3i){cell.x, cell.y, cell.z + 1}, BLOCK_GRASS));
      input->camera->position.x = cell.x + 0.5f;
      key(window, GLFW_KEY_1 + material - 1, 0, GLFW_PRESS, 0);
      CHECK(selectedHotbarSlot(input) == material - 1 && selectedBlock(input) == material);
      mouse(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
      CHECK(id(cell) == material);
      mouse(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
      CHECK(id(cell) == material);
      for (int tick = 0; tick < (material == 5 ? 10 : 20); tick++)
        processBlockBreaking(window, input, 0.1);
      CHECK(id(cell) == BLOCK_AIR);
      mouse(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
      mouse(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
      CHECK(id(cell) == material);
    }

    input->camera->position = playerEyePosition(&input->player);
    CHECK(getChunk(&(Vec2i){7, 8})->dirty && getChunk(&(Vec2i){8, 8})->dirty);
    if (crouchScenario()) {
      CHECK(playerAdvance(&input->player, (PlayerMotion){.crouch = true}, PLAYER_STEP_SECONDS) == 1);
      CHECK(setBlock(&(Vec3i){-1, 41, 0}, BLOCK_STONE));
      input->camera->position = playerEyePosition(&input->player);
      input->camera->pitch = -35;
      updateCameraVectors(input->camera);
    }

    key(window, GLFW_KEY_F5, 0, GLFW_REPEAT, 0);
    CHECK(!input->saveRequested);
    key(window, GLFW_KEY_F5, 0, GLFW_PRESS, 0);
    CHECK(input->saveRequested);
  } else if (frame == 0) {
    CHECK(id(removed) == BLOCK_AIR && id(placed) == BLOCK_STONE && id(exitEdit) == BLOCK_DIRT && id(cobblestone) == BLOCK_COBBLESTONE);
    CHECK(id(building[0]) == 5 && id(building[1]) == 6);
    Vec3 expectedFeet = feet;
    if (crouchScenario()) {
      expectedFeet.y = 42;
      CHECK(id((Vec3i){-1, 41, 0}) == BLOCK_STONE);
    }

    CHECK(!memcmp(&input->player.position, &expectedFeet, sizeof(feet)));
    CHECK(!input->player.crouched && !input->player.running && !input->runInput.tapPending);
    CHECK(!input->breakHeld && !input->breaking.active);
    CHECK(input->player.velocity.x == 0 && input->player.velocity.y == 0 && input->player.velocity.z == 0);
    CHECK(input->camera->yaw == 90 && input->camera->pitch == (crouchScenario() ? -35 : 0) && selectedBlock(input) == BLOCK_AIR);
    CHECK(selectedHotbarSlot(input) == 8);
    if (standardScenario()) {
      CHECK(input->inventoryOpen && cursorMode == GLFW_CURSOR_NORMAL);
      CHECK(input->inventory.carried[2].item == ITEM_STONE && input->inventory.carried[2].count == 499);
      CHECK(!input->inventory.carried[3].count && input->inventory.offhand.item == ITEM_COBBLESTONE && input->inventory.offhand.count == 998);
      CHECK(input->inventory.armor[INVENTORY_ARMOR_HEAD].item == ITEM_LEATHER_HELMET && input->inventory.armor[INVENTORY_ARMOR_HEAD].count == 1);
      CHECK(input->inventory.armor[INVENTORY_ARMOR_CHEST].item == ITEM_LEATHER_CHESTPLATE && input->inventory.armor[INVENTORY_ARMOR_CHEST].count == 1);
      CHECK(input->inventory.armor[INVENTORY_ARMOR_LEGS].item == ITEM_LEATHER_LEGGINGS && input->inventory.armor[INVENTORY_ARMOR_LEGS].count == 1);
      CHECK(input->inventory.armor[INVENTORY_ARMOR_FEET].item == ITEM_LEATHER_BOOTS && input->inventory.armor[INVENTORY_ARMOR_FEET].count == 1);
      for (int slot = 0; slot < INVENTORY_CRAFTING_SLOT_COUNT; slot++)
        CHECK(input->inventory.crafting[slot].item == ITEM_STONE && input->inventory.crafting[slot].count == 1);
      CHECK(input->inventory.cursor.item == ITEM_STONE && input->inventory.cursor.count == 495);
      ItemStack result = inventoryCraftResult(&input->inventory);
      CHECK(result.item == ITEM_STONE_BRICKS && result.count == 4);
      CHECK(activeDropCount(&input->drops) == 3);
      CHECK(input->drops.items[0].stack.item == ITEM_DIRT && input->drops.items[0].stack.count == 1 && input->drops.items[0].position.x == -0.5f &&
            input->drops.items[0].position.y == 41.5f && input->drops.items[0].position.z == 2.5f);
      CHECK(input->drops.items[1].stack.item == ITEM_OAK_PLANKS && input->drops.items[1].stack.count == 1 && input->drops.items[1].position.x == -1.5f &&
            input->drops.items[1].position.y == 41.5f && input->drops.items[1].position.z == 3.5f);
      CHECK(input->drops.items[2].stack.item == ITEM_STONE_BRICKS && input->drops.items[2].stack.count == 1 && input->drops.items[2].position.x == 1.5f &&
            input->drops.items[2].position.y == 41.5f && input->drops.items[2].position.z == 3.5f);
      CHECK(input->drops.items[0].pickupDelay == 0.5f && input->drops.items[0].velocity.x == 0 && input->drops.items[0].velocity.y == 0 && input->drops.items[0].velocity.z == 0);

      Inventory persisted = input->inventory;
      key(window, GLFW_KEY_E, 0, GLFW_PRESS, 0);
      CHECK(!input->inventoryOpen && cursorMode == GLFW_CURSOR_DISABLED);
      CHECK(!input->inventory.cursor.count && !inventoryCraftResult(&input->inventory).count);
      for (int slot = 0; slot < INVENTORY_CRAFTING_SLOT_COUNT; slot++)
        CHECK(!input->inventory.crafting[slot].count);
      CHECK(input->inventory.carried[2].item == ITEM_STONE && input->inventory.carried[2].count == 998 && inventoryCountItem(&input->inventory, ITEM_STONE) == 998);
      CHECK(input->inventory.offhand.item == ITEM_COBBLESTONE && input->inventory.offhand.count == 998 && activeDropCount(&input->drops) == 3);

      // Keep the process-restart fixture byte-stable while still exercising the
      // close/reclaim path above; a reopened inventory is the state that exits.
      input->inventory = persisted;
      key(window, GLFW_KEY_E, 0, GLFW_PRESS, 0);
      CHECK(input->inventoryOpen && cursorMode == GLFW_CURSOR_NORMAL && inventoryCraftResult(&input->inventory).count == 4);
    }
  }

  if (frame == 1 && saving()) {
    CHECK(input->wireframe);
    CHECK(!input->saveRequested);
    const char* path = getenv("KERNELCRAFT_TEST_WORLD");
    CHECK(path);
    FILE* file = fopen(path, "rb");
    if (failing()) {
      CHECK(!file && failureShown);
    } else {
      CHECK(file);
      CHECK(fseek(file, 0, SEEK_END) == 0);
      long fileBytes = ftell(file);
      CHECK(fileBytes > 0);
      unsigned char version[4], selection[4], extension[4], dropCount[4];
      CHECK(fseek(file, 8, SEEK_SET) == 0 && fread(version, 1, 4, file) == 4 && read32(version) == 5);
      CHECK(fseek(file, 60, SEEK_SET) == 0 && fread(selection, 1, 4, file) == 4);
      CHECK(selection[0] == 6 && !selection[1] && !selection[2] && !selection[3]);
      CHECK(fseek(file, 64, SEEK_SET) == 0 && fread(extension, 1, 4, file) == 4);
      uint32_t extensionBytes = read32(extension);
      CHECK(extensionBytes == 16 + INVENTORY_SERIALIZED_STACK_COUNT * 8 + 3 * 20);
      CHECK(fileBytes == (long)(72 + WORLD_BLOCK_COUNT + extensionBytes));
      CHECK(fseek(file, (long)(72 + WORLD_BLOCK_COUNT + 12), SEEK_SET) == 0 && fread(dropCount, 1, 4, file) == 4 && read32(dropCount) == 3);
      if (crouchScenario()) {
        float savedY;
        CHECK(fseek(file, 44, SEEK_SET) == 0 && fread(&savedY, sizeof(savedY), 1, file) == 1);
        CHECK(savedY == 42 && input->player.position.y == 40 && input->player.crouched);
      }

      CHECK(fclose(file) == 0);
    }

    // A second edit after F5 must be included by the normal-exit save.
    CHECK(setBlock(&exitEdit, BLOCK_DIRT));
    key(window, GLFW_KEY_9, 0, GLFW_PRESS, 0);
    CHECK(selectedHotbarSlot(input) == 8 && selectedBlock(input) == BLOCK_AIR);
    if (standardScenario()) {
      CHECK(input->inventory.carried[2].item == ITEM_STONE && input->inventory.carried[2].count == 998);
      CHECK(input->inventory.carried[3].item == ITEM_COBBLESTONE && input->inventory.carried[3].count == 998);
      CHECK(input->inventory.carried[4].item == ITEM_OAK_PLANKS && input->inventory.carried[4].count == 997);
      CHECK(input->inventory.carried[5].item == ITEM_STONE_BRICKS && input->inventory.carried[5].count == 997);
      CHECK(activeDropCount(&input->drops) == 3);
      key(window, GLFW_KEY_E, 0, GLFW_PRESS, 0);
      CHECK(input->inventoryOpen && cursorMode == GLFW_CURSOR_NORMAL);
      for (int slot = 9; slot <= 12; slot++)
        CHECK(inventoryShiftTransfer(&input->inventory, (InventorySlotRef){INVENTORY_SLOT_CARRIED, (uint8_t)slot}));
      CHECK(inventorySwapSlots(&input->inventory, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 3}, (InventorySlotRef){INVENTORY_SLOT_OFFHAND, 0}));
      CHECK(inventoryClick(&input->inventory, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 2}, true));
      for (int slot = 0; slot < INVENTORY_CRAFTING_SLOT_COUNT; slot++)
        CHECK(inventoryClick(&input->inventory, (InventorySlotRef){INVENTORY_SLOT_CRAFTING, (uint8_t)slot}, true));
      ItemStack result = inventoryCraftResult(&input->inventory);
      CHECK(result.item == ITEM_STONE_BRICKS && result.count == 4 && input->inventory.cursor.item == ITEM_STONE && input->inventory.cursor.count == 495);
      CHECK(inventoryValidate(&input->inventory) && droppedItemsValid(&input->drops));
    }
  }

  return frame >= 2;
}

void __real_HUDDraw(GLuint program, DebugData* data);

void __wrap_HUDDraw(GLuint program, DebugData* data) {
  if (failing()) {
    CHECK(data->saveStatus && strstr(data->saveStatus, "Save failed"));
    failureShown = true;
  }

  __real_HUDDraw(program, data);
}

void __real_glfwSwapBuffers(GLFWwindow* window);

void __wrap_glfwSwapBuffers(GLFWwindow* window) {
  CHECK(glGetError() == GL_NO_ERROR);
  CHECK(!getChunk(&(Vec2i){7, 8})->dirty && !getChunk(&(Vec2i){8, 8})->dirty);
  unsigned char pixel[4] = {0};
  // The crouch-save camera is below its roof; the restarted camera is above it.
  // The standard fixture also verifies the placed stone off the crosshair.
  glReadPixels(320 + 16, 240 + 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
  if (!crouchScenario() || !saving())
    CHECK(pixel[0] || pixel[1] || pixel[2]);
  swaps++;
  __real_glfwSwapBuffers(window);
}

void __real_glfwDestroyWindow(GLFWwindow* window);

void __wrap_glfwDestroyWindow(GLFWwindow* window) {
  CHECK(glfwGetCurrentContext() == window && glGetError() == GL_NO_ERROR);
  if (frame >= 0) {
    CHECK(swaps == 2);
    puts(failing() ? "Application save failure status and cleanup checks passed" : "Application edit, F5, exit save, restart state, and rendered chunk checks passed");
  }

  __real_glfwDestroyWindow(window);
}
