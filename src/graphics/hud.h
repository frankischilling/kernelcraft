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

void HUDDraw(GLuint shaderProgram, Camera* camera, float fps);
void HUDInit(char* buildName, char* buildVersion);
static void EntryDraw(GLuint shaderProgram, DebugEntry* entry);
static void UpdateEntries(Camera* camera, float fps);
static void DrawCrosshair(GLuint shaderProgram);

#endif // HUD_H