/**
 * @file graphics/hud.h
 * @brief HUD manager
 * @author Vladimir
 * @date 2024-11-30
 */

#ifndef HUD_H
#define HUD_H

#include <GL/glew.h>
#include "camera.h"

typedef struct {
    char text[64];
} DebugEntry;

// Public function declarations
void HUDDraw(GLuint shaderProgram, Camera* camera, float fps);
void HUDInit(char* buildName, char* buildVersion);

#endif // HUD_H
