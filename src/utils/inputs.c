/**
 * @file utils/inputs.c
 * @brief Input processing module for handling user input.
 * @author frankischilling
 * @version 0.1
 * @date 2024-11-23
 *
 */
#include <GL/glew.h>
#include "inputs.h"
#include "../graphics/camera.h"
#include "../graphics/inventory_ui.h"
#include "../world/edit.h"
#include "../world/hotbar.h"
#include "../world/world.h"
#include "raycast.h"
#include <GLFW/glfw3.h>
#include <stdbool.h>

static double lastX, lastY;
static bool firstMouse = true;

static void cancelBreaking(InputState* input) {
  if (input) {
    input->breakHeld = false;
    resetBlockBreaking(&input->breaking);
  }
}

bool initInputs(InputState* input, Camera* camera) {
  *input = (InputState){.camera = camera};
  initDayNight(&input->clock);
  camera->fov = CAMERA_BASE_FOV;
  if (!playerFindSpawn(&input->player, camera->position))
    return false;
  camera->position = playerEyePosition(&input->player);
  resetPlayerModelAnimation(&input->animation, input->player.position);
  inventoryInit(&input->inventory);
  return true;
}

bool initSavedInputs(InputState* input, Camera* camera, const SavedPlayer* saved) {
  *input = (InputState){.camera = camera};
  initDayNight(&input->clock);
  camera->fov = CAMERA_BASE_FOV;
  if (!saved || saved->selectedSlot < 0 || saved->selectedSlot >= HOTBAR_SLOT_COUNT || !inventoryValidate(&saved->inventory) || !droppedItemsValid(&saved->drops) ||
      !playerSetPosition(&input->player, saved->feet))
    return false;
  camera->position = playerEyePosition(&input->player);
  camera->yaw = saved->yaw;
  camera->pitch = saved->pitch;
  updateCameraVectors(camera);
  resetPlayerModelAnimation(&input->animation, input->player.position);
  input->selectedSlot = saved->selectedSlot;
  input->inventory = saved->inventory;
  input->drops = saved->drops;
  input->inventoryOpen = input->inventory.cursor.count != 0;
  for (size_t i = 0; i < INVENTORY_CRAFTING_SLOT_COUNT; i++)
    input->inventoryOpen |= input->inventory.crafting[i].count != 0;
  input->inventoryResumeCapture = true;
  return true;
}

static void resetInputTiming(InputState* input) {
  if (!input)
    return;
  playerResetTiming(&input->player);
  playerResetRunInput(&input->runInput);
  resetPlayerModelAnimation(&input->animation, input->player.position);
  input->camera->fov = CAMERA_BASE_FOV;
  input->jumpRequested = false;
  input->simulationSteps = 0;
  cancelBreaking(input);
}

void pauseInput(InputState* input) {
  // A pause may deliver no cursor events. The next position starts a new delta.
  firstMouse = true;
  resetInputTiming(input);
  if (input) {
    input->inventoryGesture = (InventoryGesture){0};
    input->inventoryClickValid = false;
    input->drops.accumulator = 0;
  }
}

void setCursorCaptured(GLFWwindow* window, bool captured) {
  // GLFW may move the cursor while changing mode. Discard the next delta.
  pauseInput(glfwGetWindowUserPointer(window));
  glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void windowFocusCallback(GLFWwindow* window, int focused) {
  firstMouse = true;
  if (!focused)
    setCursorCaptured(window, false);
}

static bool acceptsWindowInput(GLFWwindow* window) {
  if (!window)
    return false;
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  return width > 0 && height > 0 && glfwGetWindowAttrib(window, GLFW_FOCUSED) && !glfwGetWindowAttrib(window, GLFW_ICONIFIED);
}

bool inputSimulationActive(GLFWwindow* window, const InputState* input) {
  if (!input || input->chat.open || !acceptsWindowInput(window))
    return false;
  return input->inventoryOpen || glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
}

static bool acceptsEditing(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  return input && !input->inventoryOpen && inputSimulationActive(window, input);
}

int selectedBlock(const InputState* input) {
  if (!input || input->selectedSlot < 0 || input->selectedSlot >= HOTBAR_SLOT_COUNT)
    return BLOCK_AIR;
  return inventoryItemBlock(input->inventory.carried[input->selectedSlot].item);
}

int selectedHotbarSlot(const InputState* input) {
  return input ? input->selectedSlot : 0;
}

Vec3 inputBodyFeet(const InputState* input) {
  if (!input->flying)
    return input->player.position;
  Vec3 feet = input->camera->position;
  Vec3 initialEye = playerEyePosition(&input->player);
  // Preserve the exact standing height when flight starts. Subtracting the eye
  // offset directly can round below the floor even without any camera motion.
  feet.y = (float)(input->player.position.y + ((double)feet.y - initialEye.y));
  return feet;
}

void inputPlayerPose(const InputState* input, PlayerModelPose* pose) {
  PlayerPoseInput visual = {.yaw = input->camera->yaw,
                            .pitch = input->camera->pitch,
                            .gaitPhase = input->animation.gaitPhase,
                            .gaitWeight = input->animation.gaitWeight,
                            .punch = input->breakHeld ? playerModelPunch(&input->breaking) : 0,
                            .crouched = input->player.crouched,
                            .running = input->player.running,
                            .grounded = input->player.grounded,
                            .flying = input->flying};
  playerModelPose(pose, &visual);
}

bool snapshotPlayer(const InputState* input, SavedPlayer* saved) {
  if (!input || !input->camera || !saved)
    return false;
  Player standing = {0};
  Vec3 feet = inputBodyFeet(input);
  if (!playerSetPosition(&standing, feet) && ((!input->flying && !input->player.crouched) || !playerFindSpawn(&standing, feet)))
    return false;
  float yaw = fmodf(input->camera->yaw, 360.0f);
  if (yaw < 0)
    yaw += 360.0f;
  // Adding 360 can round a tiny negative remainder to exactly 360.
  if (yaw >= 360)
    yaw = 0;
  *saved = (SavedPlayer){
      .feet = standing.position, .yaw = yaw, .pitch = input->camera->pitch, .selectedSlot = input->selectedSlot, .inventory = input->inventory, .drops = input->drops};
  return true;
}

bool inventoryPointer(GLFWwindow* window, const InputState* input, int* x, int* y) {
  int width, height, windowWidth, windowHeight;
  if (!window || !input || !x || !y || !isfinite(input->inventoryMouseX) || !isfinite(input->inventoryMouseY))
    return false;
  glfwGetFramebufferSize(window, &width, &height);
  glfwGetWindowSize(window, &windowWidth, &windowHeight);
  if (width <= 0 || height <= 0 || windowWidth <= 0 || windowHeight <= 0)
    return false;
  *x = (int)floor(fmax(-1, fmin(width, input->inventoryMouseX * width / windowWidth)));
  *y = (int)floor(fmax(-1, fmin(height, input->inventoryMouseY * height / windowHeight)));
  return true;
}

static bool inventoryHover(GLFWwindow* window, const InputState* input, InventorySlotRef* slot, bool* outside) {
  int x, y, width, height;
  InventoryUILayout layout;
  if (outside)
    *outside = false;
  glfwGetFramebufferSize(window, &width, &height);
  if (!inventoryPointer(window, input, &x, &y) || !inventoryUILayout(width, height, &layout))
    return false;
  if (outside)
    *outside = x < layout.panel.x || x >= layout.panel.x + layout.panel.width || y < layout.panel.y || y >= layout.panel.y + layout.panel.height;
  return inventoryUIHitTest(&layout, x, y, slot);
}

static bool sameInventorySlot(InventorySlotRef a, InventorySlotRef b) {
  return a.kind == b.kind && a.index == b.index;
}

static bool throwStack(InputState* input, DroppedItems* drops, ItemStack stack) {
  Vec3 position = inputBodyFeet(input);
  position.y += 0.7f;
  Vec3 velocity;
  vec3_scale(&velocity, &input->camera->front, 4);
  velocity.y += 2;
  return droppedItemsSpawn(drops, stack, position, velocity, 0.75f);
}

static bool dropInventorySlot(InputState* input, InventorySlotRef slot, bool wholeStack) {
  ItemStack stack = inventoryGet(&input->inventory, slot);
  if (!stack.count || slot.kind == INVENTORY_SLOT_RESULT)
    return false;
  Inventory next = input->inventory;
  uint16_t count = wholeStack ? stack.count : 1;
  if (!inventoryRemove(&next, slot, count, &stack) || !throwStack(input, &input->drops, stack)) {
    input->inventoryNotice = "Cannot drop here or dropped-item storage is full";
    return false;
  }
  input->inventory = next;
  input->inventoryNotice = NULL;
  return true;
}

static bool closeInventory(GLFWwindow* window, InputState* input) {
  Inventory next = input->inventory;
  DroppedItems drops = input->drops;
  ItemStack remaining[INVENTORY_CLOSE_DROP_MAX];
  size_t count = 0;
  if (!inventoryClose(&next, remaining, &count))
    return false;
  for (size_t i = 0; i < count; i++) {
    if (!throwStack(input, &drops, remaining[i])) {
      input->inventoryNotice = "Make room before closing: remaining items cannot be dropped";
      return false;
    }
  }
  input->inventory = next;
  input->drops = drops;
  input->inventoryOpen = false;
  input->inventoryNotice = NULL;
  setCursorCaptured(window, input->inventoryResumeCapture);
  return true;
}

static void openInventory(GLFWwindow* window, InputState* input) {
  input->inventoryResumeCapture = glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
  input->inventoryOpen = true;
  input->inventoryNotice = NULL;
  setCursorCaptured(window, false);
  glfwGetCursorPos(window, &input->inventoryMouseX, &input->inventoryMouseY);
  glfwGetFramebufferSize(window, &input->inventoryWidth, &input->inventoryHeight);
}

static void inventoryMouseButton(GLFWwindow* window, InputState* input, int button, int action, int mods) {
  if (button != GLFW_MOUSE_BUTTON_LEFT && button != GLFW_MOUSE_BUTTON_RIGHT)
    return;
  InventorySlotRef slot = {0};
  bool outside = false;
  bool hit = inventoryHover(window, input, &slot, &outside);
  InventoryGesture* gesture = &input->inventoryGesture;
  if (action == GLFW_RELEASE) {
    if (gesture->pending && gesture->button == button) {
      if (gesture->count > 1)
        inventoryDragDistribute(&input->inventory, gesture->slots, gesture->count, button == GLFW_MOUSE_BUTTON_RIGHT ? INVENTORY_DRAG_ONE_EACH : INVENTORY_DRAG_EVEN);
      else if (hit && gesture->count == 1 && sameInventorySlot(slot, gesture->slots[0]))
        inventoryClick(&input->inventory, slot, button == GLFW_MOUSE_BUTTON_RIGHT);
    }
    *gesture = (InventoryGesture){0};
    return;
  }
  if (action != GLFW_PRESS)
    return;
  *gesture = (InventoryGesture){0};
  input->inventoryNotice = NULL;
  if (!hit) {
    input->inventoryClickValid = false;
    if (outside)
      dropInventorySlot(input, (InventorySlotRef){INVENTORY_SLOT_CURSOR, 0}, button == GLFW_MOUSE_BUTTON_LEFT);
    return;
  }
  if (mods & GLFW_MOD_SHIFT) {
    inventoryShiftTransfer(&input->inventory, slot);
    input->inventoryClickValid = false;
    return;
  }
  double now = glfwGetTime();
  if (button == GLFW_MOUSE_BUTTON_LEFT && input->inventoryClickValid && sameInventorySlot(slot, input->inventoryClickSlot) && now >= input->inventoryClickTime &&
      now - input->inventoryClickTime <= 0.25 && input->inventory.cursor.count && slot.kind != INVENTORY_SLOT_RESULT) {
    inventoryGather(&input->inventory, slot);
    input->inventoryClickValid = false;
    return;
  }
  input->inventoryClickValid = button == GLFW_MOUSE_BUTTON_LEFT;
  input->inventoryClickSlot = slot;
  input->inventoryClickTime = now;
  if (input->inventory.cursor.count && slot.kind != INVENTORY_SLOT_RESULT) {
    *gesture = (InventoryGesture){.pending = true, .button = button, .count = 1, .slots = {slot}};
  } else {
    inventoryClick(&input->inventory, slot, button == GLFW_MOUSE_BUTTON_RIGHT);
  }
}

void characterCallback(GLFWwindow* window, unsigned int codepoint) {
  InputState* input = glfwGetWindowUserPointer(window);
  if (input && acceptsWindowInput(window))
    appendChatCharacter(&input->chat, codepoint);
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  (void)scancode;
  InputState* input = glfwGetWindowUserPointer(window);
  if (input && acceptsWindowInput(window)) {
    if (input->chat.open) {
      if (key == GLFW_KEY_BACKSPACE && (action == GLFW_PRESS || action == GLFW_REPEAT))
        backspaceChat(&input->chat);
      if (action == GLFW_PRESS && (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_ESCAPE)) {
        if (key == GLFW_KEY_ESCAPE)
          cancelChat(&input->chat);
        else
          submitChat(&input->chat, &input->clock);
        pauseInput(input);
        advanceDayNight(&input->clock, 0, false);
      }
      return;
    }
    if (input->inventoryOpen) {
      if (action == GLFW_PRESS) {
        InventorySlotRef hover;
        bool hasHover = inventoryHover(window, input, &hover, NULL);
        input->inventoryGesture = (InventoryGesture){0};
        input->inventoryClickValid = false;
        if (key == GLFW_KEY_E || key == GLFW_KEY_ESCAPE)
          closeInventory(window, input);
        else if (hasHover && key >= GLFW_KEY_1 && key <= GLFW_KEY_9)
          inventoryHotbarSwap(&input->inventory, hover, key - GLFW_KEY_1);
        else if (hasHover && key == GLFW_KEY_F)
          inventorySwapSlots(&input->inventory, hover, (InventorySlotRef){INVENTORY_SLOT_OFFHAND, 0});
        else if (key == GLFW_KEY_Q) {
          if (input->inventory.cursor.count)
            dropInventorySlot(input, (InventorySlotRef){INVENTORY_SLOT_CURSOR, 0}, (mods & GLFW_MOD_CONTROL) != 0);
          else if (hasHover)
            dropInventorySlot(input, hover, (mods & GLFW_MOD_CONTROL) != 0);
        }
      }
      return;
    }
    if (action == GLFW_PRESS && key == GLFW_KEY_E) {
      openInventory(window, input);
      return;
    }
    if (action == GLFW_PRESS && (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)) {
      openChat(&input->chat);
      pauseInput(input);
      advanceDayNight(&input->clock, 0, false);
      return;
    }
  }
  if (input && !input->flying && acceptsEditing(window)) {
    bool crouchHeld = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    if ((action == GLFW_PRESS && (key == GLFW_KEY_S || key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)) || crouchHeld || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
        input->player.crouched) {
      playerResetRunInput(&input->runInput);
      input->player.running = false;
    } else if (key == GLFW_KEY_W && (action == GLFW_PRESS || action == GLFW_RELEASE)) {
      playerForwardEvent(&input->runInput, action == GLFW_PRESS, glfwGetTime());
      if (action == GLFW_RELEASE)
        input->player.running = false;
    }
  }

  if (action != GLFW_PRESS)
    return;
  if (key == GLFW_KEY_ESCAPE && acceptsWindowInput(window)) {
    setCursorCaptured(window, glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED);
    return;
  }

  // Diagnostics remain accessible while the cursor is released. They never
  // resume movement or alter the world, and repeats are rejected above.
  if (input && key == GLFW_KEY_F3 && acceptsWindowInput(window)) {
    input->showDebug = !input->showDebug;
    return;
  }

  if (input && key == GLFW_KEY_F4 && acceptsWindowInput(window)) {
    input->wireframe = !input->wireframe;
    return;
  }

  if (input && key == GLFW_KEY_F6 && acceptsWindowInput(window)) {
    input->view = (CameraView)((input->view + 1) % CAMERA_VIEW_COUNT);
    return;
  }

  if (acceptsEditing(window) && key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
    if (input->selectedSlot != key - GLFW_KEY_1)
      cancelBreaking(input);
    input->selectedSlot = key - GLFW_KEY_1;
  }

  if (!input || !acceptsEditing(window))
    return;
  if (key == GLFW_KEY_F5)
    input->saveRequested = true;
  if (key == GLFW_KEY_SPACE && !input->flying)
    input->jumpRequested = true;
  if (key == GLFW_KEY_Q) {
    cancelBreaking(input);
    dropInventorySlot(input, (InventorySlotRef){INVENTORY_SLOT_CARRIED, (uint8_t)input->selectedSlot}, (mods & GLFW_MOD_CONTROL) != 0);
  }
  if (key == GLFW_KEY_F) {
    resetInputTiming(input);
    input->modeBlocked = false;
    if (!input->flying) {
      input->flying = true;
    } else {
      Vec3 feet = inputBodyFeet(input);
      if (playerSetPosition(&input->player, feet) || playerFindSpawn(&input->player, feet)) {
        input->flying = false;
        input->camera->position = playerEyePosition(&input->player);
      } else {
        input->modeBlocked = true;
      }
    }
  }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
  InputState* input = glfwGetWindowUserPointer(window);
  if (input && input->inventoryOpen) {
    if (acceptsWindowInput(window))
      inventoryMouseButton(window, input, button, action, mods);
    return;
  }
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
    cancelBreaking(input);
    return;
  }

  if (!input || action != GLFW_PRESS || !acceptsEditing(window))
    return;
  Camera* camera = input->camera;
  if (button == GLFW_MOUSE_BUTTON_LEFT && !input->breakHeld) {
    input->breakHeld = true;
    advanceBlockBreaking(&input->breaking, camera->position, camera->front, 0);
  }

  if (button == GLFW_MOUSE_BUTTON_RIGHT) {
    cancelBreaking(input);
    InventorySlotRef source = {INVENTORY_SLOT_CARRIED, (uint8_t)input->selectedSlot};
    ItemStack stack = inventoryGet(&input->inventory, source);
    int armor = inventoryItemArmorSlot(stack.item);
    if (armor >= 0) {
      inventorySwapSlots(&input->inventory, source, (InventorySlotRef){INVENTORY_SLOT_ARMOR, (uint8_t)armor});
      return;
    }
    int block = inventoryItemBlock(stack.item);
    if (!block) {
      source = (InventorySlotRef){INVENTORY_SLOT_OFFHAND, 0};
      stack = inventoryGet(&input->inventory, source);
      block = inventoryItemBlock(stack.item);
    }
    Inventory next = input->inventory;
    if (block && inventoryRemove(&next, source, 1, NULL) &&
        editTarget(camera->position, camera->front, inputBodyFeet(input), !input->flying && input->player.crouched, block, true)) {
      input->inventory = next;
      input->inventoryNotice = NULL;
    }
  }
}

void processBlockBreaking(GLFWwindow* window, InputState* input, double deltaTime) {
  if (!input)
    return;
  if (!acceptsEditing(window)) {
    cancelBreaking(input);
    return;
  }

  if (input->breakHeld) {
    Ray ray = rayCast(input->camera->position, input->camera->front, EDIT_REACH);
    const Block* block = ray.hit ? getBlock(&ray.blockCoords) : NULL;
    DroppedItems next = input->drops;
    if (block && blockHandBreakSeconds(block->id) > 0) {
      Vec3 position = {ray.blockCoords.x + 0.5f, ray.blockCoords.y + 0.5f, ray.blockCoords.z + 0.5f};
      if (!droppedItemsSpawn(&next, (ItemStack){inventoryBlockItem(block->id), 1}, position, (Vec3){0, 1, 0}, 0.1f)) {
        input->inventoryNotice = "Collect nearby dropped items before breaking more blocks";
        cancelBreaking(input);
        return;
      }
    }
    if (advanceBlockBreaking(&input->breaking, input->camera->position, input->camera->front, deltaTime)) {
      input->drops = next;
      input->inventoryNotice = NULL;
    }
  }
}

void processInput(GLFWwindow* window, InputState* input, double deltaTime) {
  if (!input)
    return;
  input->simulationSteps = 0;
  if (input->inventoryOpen) {
    if (!inputSimulationActive(window, input)) {
      pauseInput(input);
      return;
    }
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    if (width != input->inventoryWidth || height != input->inventoryHeight) {
      input->inventoryGesture = (InventoryGesture){0};
      input->inventoryClickValid = false;
      input->inventoryWidth = width;
      input->inventoryHeight = height;
    }
    firstMouse = true;
    if (!isfinite(deltaTime) || deltaTime <= 0)
      return;
    if (!input->flying) {
      input->simulationSteps = playerAdvance(&input->player, (PlayerMotion){0}, deltaTime);
      input->camera->position = playerEyePosition(&input->player);
      updateCameraFov(input->camera, false, deltaTime);
      if (input->simulationSteps > 0)
        advancePlayerModelAnimation(&input->animation, &input->player, false, true, input->simulationSteps * PLAYER_STEP_SECONDS);
    } else {
      updateCameraFov(input->camera, false, deltaTime);
      advancePlayerModelAnimation(&input->animation, &input->player, true, true, deltaTime);
    }
    return;
  }
  if (!acceptsEditing(window)) {
    pauseInput(input);
    return;
  }

  if (!isfinite(deltaTime) || deltaTime <= 0)
    return;
  Camera* camera = input->camera;
  if (!input->flying) {
    float yaw = toRadians(camera->yaw);
    Vec3 forward = {cosf(yaw), 0, sinf(yaw)}, wish = {0};
    int longitudinal = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) - (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS);
    int lateral = (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) - (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS);
    wish.x = forward.x * longitudinal - forward.z * lateral;
    wish.z = forward.z * longitudinal + forward.x * lateral;
    bool crouch = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    if (crouch || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || input->player.crouched)
      playerResetRunInput(&input->runInput);
    else if (glfwGetKey(window, GLFW_KEY_W) != GLFW_PRESS)
      playerForwardEvent(&input->runInput, false, glfwGetTime());
    PlayerMotion motion = {.wish = wish, .jump = input->jumpRequested, .crouch = crouch, .run = input->runInput.running && longitudinal > 0};
    input->simulationSteps = playerAdvance(&input->player, motion, deltaTime);
    input->jumpRequested = false;
    camera->position = playerEyePosition(&input->player);
    updateCameraFov(camera, input->player.running, deltaTime);
    // A frame without a physics tick has no new displacement sample. Sampling
    // only credited ticks keeps gait amplitude consistent at high frame rates.
    if (input->simulationSteps > 0)
      advancePlayerModelAnimation(&input->animation, &input->player, false, true, input->simulationSteps * PLAYER_STEP_SECONDS);
    return;
  }

  float velocity = camera->speed * (float)fmin(deltaTime, 0.1);
  Vec3 temp;

  // Forward/Backward
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
    vec3_scale(&temp, &camera->front, velocity);
    vec3_add(&camera->position, &camera->position, &temp);
  }

  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
    vec3_scale(&temp, &camera->front, velocity);
    vec3_subtract(&camera->position, &camera->position, &temp);
  }

  // Up/Down
  if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
    vec3_scale(&temp, &camera->up, velocity);
    vec3_add(&camera->position, &camera->position, &temp);
  }

  if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
    vec3_scale(&temp, &camera->up, velocity);
    vec3_subtract(&camera->position, &camera->position, &temp);
  }

  Vec3 right;
  vec3_cross(&right, &camera->front, &camera->up);
  vec3_normalize(&right, &right);

  // Left/Right
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
    vec3_scale(&temp, &right, velocity);
    vec3_add(&camera->position, &camera->position, &temp);
  }

  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
    vec3_scale(&temp, &right, velocity);
    vec3_subtract(&camera->position, &camera->position, &temp);
  }
  advancePlayerModelAnimation(&input->animation, &input->player, true, true, deltaTime);
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
  InputState* input = glfwGetWindowUserPointer(window);
  if (input && input->inventoryOpen) {
    if (acceptsWindowInput(window) && isfinite(xpos) && isfinite(ypos)) {
      input->inventoryMouseX = xpos;
      input->inventoryMouseY = ypos;
      InventoryGesture* gesture = &input->inventoryGesture;
      InventorySlotRef hover;
      if (gesture->pending && gesture->count < INVENTORY_DRAG_SLOT_MAX && inventoryHover(window, input, &hover, NULL) && hover.kind != INVENTORY_SLOT_RESULT) {
        bool present = false;
        for (size_t i = 0; i < gesture->count; i++)
          present |= sameInventorySlot(hover, gesture->slots[i]);
        if (!present) {
          gesture->slots[gesture->count++] = hover;
          input->inventoryClickValid = false;
        }
      }
    }
    firstMouse = true;
    return;
  }
  if (!input || !acceptsEditing(window)) {
    firstMouse = true;
    return;
  }

  Camera* camera = input->camera;

  if (firstMouse) {
    lastX = xpos;
    lastY = ypos;
    firstMouse = false;
    return;
  }

  float xoffset = xpos - lastX;
  float yoffset = lastY - ypos; // Reversed since y-coordinates range from bottom to top
  lastX = xpos;
  lastY = ypos;

  xoffset *= camera->sensitivity;
  yoffset *= camera->sensitivity;

  camera->yaw += xoffset;
  camera->pitch += yoffset;

  // Constrain pitch
  if (camera->pitch > 89.0f)
    camera->pitch = 89.0f;
  if (camera->pitch < -89.0f)
    camera->pitch = -89.0f;

  // Update camera vectors
  updateCameraVectors(camera);
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
  (void)xoffset;
  InputState* input = glfwGetWindowUserPointer(window);
  if (!input || !acceptsEditing(window) || !isfinite(yoffset) || yoffset == 0)
    return;
  int step = yoffset > 0 ? 1 : -1;
  input->selectedSlot = (input->selectedSlot - step + HOTBAR_SLOT_COUNT) % HOTBAR_SLOT_COUNT;
  cancelBreaking(input);
}
