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
typedef struct {
  char text[64];
} DebugEntry;
typedef struct {
  Camera* camera;
  float fps;
  int visibleBlocks;
} DebugData;
void HUDDraw(GLuint shaderProgram, DebugData* data);
void HUDInit(char* buildName, char* buildVersion);
static void EntryDraw(GLuint shaderProgram, DebugEntry* entry, int* i);
static void UpdateEntries(DebugData* data);
static void DrawCrosshair(GLuint shaderProgram);

#endif // HUD_H