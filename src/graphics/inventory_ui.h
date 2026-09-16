#ifndef INVENTORY_UI_H
#define INVENTORY_UI_H

#include <GL/glew.h>
#include "player_renderer.h"
#include "../world/inventory.h"
#include <stdbool.h>

enum {
  INVENTORY_UI_LOGICAL_WIDTH = 176,
  INVENTORY_UI_LOGICAL_HEIGHT = 166,
  INVENTORY_UI_BLOCK_TEXTURE_COUNT = 6,
};

typedef struct {
  int x;
  int y;
  int width;
  int height;
} InventoryUIRect;

typedef struct {
  int framebufferWidth;
  int framebufferHeight;
  InventoryUIRect panel;
  float scale;
} InventoryUILayout;

typedef struct {
  GLuint previewFramebuffer;
  GLuint previewTexture;
  GLuint previewDepth;
  int previewWidth;
  int previewHeight;
} InventoryUI;

// Creates the reusable preview framebuffer objects. A current GL context is
// required. Storage is resized lazily when the panel's pixel scale changes.
bool inventoryUIInit(InventoryUI* ui);
void inventoryUICleanup(InventoryUI* ui);

// Layout and pointer coordinates use framebuffer pixels with a top-left origin.
// Normal windows use the classic automatic GUI scale (320x240 per step); below
// 176x166 the panel fits fractionally. Every logical edge rounds to a framebuffer
// pixel so slot hit testing and rendering share exact bounds.
bool inventoryUILayout(int framebufferWidth, int framebufferHeight, InventoryUILayout* layout);
bool inventoryUISlotRect(const InventoryUILayout* layout, InventorySlotRef slot, InventoryUIRect* rect);
bool inventoryUIHitTest(const InventoryUILayout* layout, int mouseX, int mouseY, InventorySlotRef* slot);

// Draws the complete survival inventory panel. blockTextures maps item IDs 1..6
// to the existing HUD block-icon textures. The player preview uses ui's private
// framebuffer so this pass never clears or replaces world depth.
void inventoryUIDraw(InventoryUI* ui, const Inventory* inventory, const PlayerRenderer* playerRenderer, const PlayerModelPose* playerPose, const DayNightState* daylight,
                     const GLuint blockTextures[INVENTORY_UI_BLOCK_TEXTURE_COUNT], int framebufferWidth, int framebufferHeight, int mouseX, int mouseY);

#endif
