/**
 * @file graphics/hud.h
 * @brief HUD manager
 * @author VladimirJanus, frankischilling
 * @date 2024-11-30
 */
#include <stdio.h>
#include "hud.h"
#include "../utils/text.h"
#include "../world/world.h"
#include "../utils/raycast.h"
static float currentEntryIndex;
static DebugEntry entryBiome;
static DebugEntry entryFPS;
static DebugEntry entryCubeCount;
static DebugEntry entryBuildInfo;
static DebugEntry entryWorldCoords;
static DebugEntry entryChunkCoords;
static DebugEntry entryLookingAtBlockCoords;

void DrawCrosshair(GLuint crosshairShaderProgram) {
  extern int windowWidth;
  extern int windowHeight;

  // Crosshair size (e.g., 2% of window height)
  float crosshairSize = windowHeight * 0.02f;

  // Center of the window
  float centerX = windowWidth / 2.0f;
  float centerY = windowHeight / 2.0f;

  // Crosshair vertices in screen coordinates
  float vertices[] = {
      // Horizontal line
      centerX - crosshairSize, centerY,
      centerX + crosshairSize, centerY,
      // Vertical line
      centerX, centerY - crosshairSize,
      centerX, centerY + crosshairSize
  };

  // Orthographic projection matrix
  Mat4 orthoProjection;
  mat4_ortho(orthoProjection, 0.0f, (float)windowWidth, 0.0f, (float)windowHeight, -1.0f, 1.0f);

  // OpenGL setup (VAO, VBO)
  static GLuint VAO = 0, VBO = 0;
  if (VAO == 0) {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
  }

  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  // Use crosshair shader program
  glUseProgram(crosshairShaderProgram);

  // Pass projection matrix to shader
  glUniformMatrix4fv(glGetUniformLocation(crosshairShaderProgram, "projection"), 1, GL_FALSE, orthoProjection);

  // Set crosshair color
  glUniform3f(glGetUniformLocation(crosshairShaderProgram, "objectColor"), 1.0f, 1.0f, 1.0f);

  // Disable depth test
  glDisable(GL_DEPTH_TEST);

  // Draw crosshair lines
  glDrawArrays(GL_LINES, 0, 4);

  // Enable depth test
  glEnable(GL_DEPTH_TEST);

  // Cleanup
  glBindVertexArray(0);
}

void HUDDraw(GLuint shaderProgram, GLuint crosshairShaderProgram, Camera* camera, float fps) {
    UpdateEntries(camera, fps);

    currentEntryIndex = 0;
    EntryDraw(shaderProgram, &entryBiome);
    EntryDraw(shaderProgram, &entryChunkCoords);
    EntryDraw(shaderProgram, &entryWorldCoords);
    EntryDraw(shaderProgram, &entryCubeCount);
    EntryDraw(shaderProgram, &entryFPS);
    EntryDraw(shaderProgram, &entryBuildInfo);
    Ray cast = rayCast(camera);
    if (cast.hit) {
        snprintf(entryLookingAtBlockCoords.text, sizeof(entryLookingAtBlockCoords.text),
                 "Block coordinates: X:%.1f Y:%.1f Z:%.1f", cast.coords.x, cast.coords.z, cast.coords.y);
        EntryDraw(shaderProgram, &entryLookingAtBlockCoords);
    }

    DrawCrosshair(crosshairShaderProgram);
}

static void EntryDraw(GLuint shaderProgram, DebugEntry* entry) {
  renderText(shaderProgram, entry->text, 10.0f, 100.0f + currentEntryIndex);
  currentEntryIndex += 20;
}
static void UpdateEntries(Camera* camera, float fps) {
  snprintf(entryFPS.text, sizeof(entryFPS.text), "FPS: %.1f", fps);
  snprintf(entryBiome.text, sizeof(entryBiome.text), "Current biome: %s", getCurrentBiomeText(camera->position.x, camera->position.z));
  snprintf(entryCubeCount.text, sizeof(entryCubeCount.text), "Visible Cubes: %d", getVisibleCubesCount());

  snprintf(entryWorldCoords.text, sizeof(entryWorldCoords.text), "World coordinates: X:%.1f Y:%.1f Z:%.1f", camera->position.x, camera->position.y, camera->position.z);
  int currentChunkX = (int)floor(camera->position.x / (CHUNK_SIZE_X * CUBE_SIZE));
  int currentChunkZ = (int)floor(camera->position.z / (CHUNK_SIZE_Z * CUBE_SIZE));
  snprintf(entryChunkCoords.text, sizeof(entryChunkCoords.text), "Chunk coordinates: X:%d Z:%d", currentChunkX, currentChunkZ);
}
void HUDInit(char* buildName, char* buildVersion) {
  entryBiome.text[0] = '\0';
  entryFPS.text[0] = '\0';
  entryCubeCount.text[0] = '\0';
  snprintf(entryBuildInfo.text, sizeof(entryBuildInfo.text), "%s %s", buildName, buildVersion);
}
