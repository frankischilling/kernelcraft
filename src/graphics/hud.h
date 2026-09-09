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
#include "../utils/raycast.h"
typedef struct {
  char text[64];
} DebugEntry;
typedef struct {
  Camera* camera;
  float fps;
  int visibleBlocks;
  Ray selection;
  int selectedSlot;
  bool captured;
  bool flying, grounded, modeBlocked;
  bool crouched, running;
  int simulationSteps;
  const char* saveStatus;
  const RenderResult* stats;
  bool showDebug;
} DebugData;
void HUDDraw(GLuint shaderProgram, DebugData* data);
// Initialization and cleanup require the current rendering context.
bool HUDInit(const char* buildName, const char* buildVersion);
void HUDCleanup(void);

#endif // HUD_H
