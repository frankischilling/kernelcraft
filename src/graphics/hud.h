/**
 * @file graphics/hud.h
 * @brief HUD manager
 * @author VladimirJanus
 * @date 2024-11-30
 */
#ifndef HUD_H
#define HUD_H

#include <GL/glew.h>
#include "camera.h"
#include "world_renderer.h"
#include "item_renderer.h"
#include "../utils/raycast.h"
#include "../world/chat.h"
#include "../world/inventory.h"

typedef struct {
  char text[64];
} DebugEntry;

typedef struct {
  Camera* camera;
  float fps;
  int visibleBlocks;
  Ray selection;
  int selectedSlot;
  float breakingProgress;
  bool captured;
  bool flying, grounded, modeBlocked;
  bool crouched, running;
  int simulationSteps;
  const char* saveStatus;
  const RenderResult* stats;
  bool showDebug;
  bool wireframe;
  const Chat* chat;
  const Inventory* inventory;
  bool inventoryOpen;
  const char* inventoryNotice;
} DebugData;

void HUDDraw(GLuint shaderProgram, DebugData* data);
// Initialization and cleanup require the current rendering context.
bool HUDInit(const char* buildName, const char* buildVersion);
void HUDCleanup(void);
// Borrowed handles, indexed by placeable item ID minus one; HUD owns them.
void HUDItemTextures(GLuint textures[6]);
// Shared item meshes, cached GUI images, and reusable first-person target.
ItemRenderer* HUDItems(void);

#endif // HUD_H
