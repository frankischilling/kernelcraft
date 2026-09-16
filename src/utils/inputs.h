/**
 * @file utils/inputs.h
 * @brief Input processing module for handling user input.
 * @author frankischilling
 * @version 0.1
 * @date 2024-11-23
 *
 */
#ifndef INPUTS_H
#define INPUTS_H

#include "../graphics/camera.h"
#include "../world/player.h"
#include "../world/player_model.h"
#include "../world/edit.h"
#include "../world/save.h"
#include "../world/chat.h"
#include "../world/inventory.h"
#include "../world/dropped_items.h"
#include <GLFW/glfw3.h>
#include <stdbool.h>

typedef struct {
  bool pending;
  int button;
  size_t count;
  InventorySlotRef slots[INVENTORY_DRAG_SLOT_MAX];
} InventoryGesture;

typedef enum {
  BLOCK_PLACEMENT_HAND_NONE = 0,
  BLOCK_PLACEMENT_HAND_MAIN,
  BLOCK_PLACEMENT_HAND_OFFHAND,
} BlockPlacementHand;

typedef struct {
  uint16_t item;
  BlockPlacementHand hand;
  double elapsed;
  bool active;
} BlockPlacementAnimation;

typedef struct {
  Camera* camera;
  Chat chat;
  DayNightClock clock;
  Player player;
  PlayerRunInput runInput;
  BlockBreaking breaking;
  double breakVisualElapsed;
  BlockPlacementAnimation placement;
  PlayerModelAnimation animation;
  CameraView view;
  Inventory inventory;
  DroppedItems drops;
  int selectedSlot;
  bool inventoryOpen, inventoryResumeCapture;
  double inventoryMouseX, inventoryMouseY;
  int inventoryWidth, inventoryHeight;
  InventoryGesture inventoryGesture;
  double inventoryClickTime;
  InventorySlotRef inventoryClickSlot;
  bool inventoryClickValid;
  const char* inventoryNotice;
  bool breakHeld;
  bool flying;
  bool jumpRequested;
  bool saveRequested;
  bool modeBlocked;
  bool showDebug;
  bool wireframe;
  int simulationSteps;
} InputState;

bool initInputs(InputState* input, Camera* camera);
// The caller supplies state already validated by loadWorld.
bool initSavedInputs(InputState* input, Camera* camera, const SavedPlayer* saved);
bool snapshotPlayer(const InputState* input, SavedPlayer* saved);
// Feet and visual pose share the same exact flight offset used by saving.
Vec3 inputBodyFeet(const InputState* input);
void inputPlayerPose(const InputState* input, PlayerModelPose* pose);
// Resolves the visible first-person/third-person hands, retaining the last
// consumed block until its placement swing returns to rest.
void inputHeldItems(const InputState* input, ItemStack* mainHand, ItemStack* offhand);
// Discard simulation backlog, queued jumps, run/tap state, and cached mouse position.
// Retain the current body until active simulation can check standing clearance.
void pauseInput(InputState* input);
// World simulation runs while normal gameplay is captured or while the focused
// inventory is open. Chat, an ordinary released cursor, focus loss,
// iconification, and zero-size framebuffers pause simulation.
bool inputSimulationActive(GLFWwindow* window, const InputState* input);
void processInput(GLFWwindow* window, InputState* input, double deltaTime);
// Run after movement and before rendering/saving so selection uses the new eye.
void processBlockBreaking(GLFWwindow* window, InputState* input, double deltaTime);
void mouseCallback(GLFWwindow* window, double xpos, double ypos);
void setCursorCaptured(GLFWwindow* window, bool captured);
void windowFocusCallback(GLFWwindow* window, int focused);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void characterCallback(GLFWwindow* window, unsigned int codepoint);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
// Converts logical window coordinates to top-origin framebuffer pixels.
bool inventoryPointer(GLFWwindow* window, const InputState* input, int* x, int* y);
int selectedBlock(const InputState* input);
int selectedHotbarSlot(const InputState* input);

#endif
