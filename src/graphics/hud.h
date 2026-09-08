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
  int selectedBlock;
  bool captured;
  bool flying, grounded, modeBlocked;
  int simulationSteps;
  const RenderResult* stats;
} DebugData;
void HUDDraw(GLuint shaderProgram, DebugData* data);
void HUDInit(char* buildName, char* buildVersion);

#endif // HUD_H
