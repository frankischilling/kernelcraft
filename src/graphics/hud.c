/**
 * @file graphics/hud.h
 * @brief HUD manager
 * @author VladimirJanus
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

void HUDDraw(GLuint shaderProgram, Camera* camera, float fps) {
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
    snprintf(entryLookingAtBlockCoords.text, sizeof(entryLookingAtBlockCoords.text), "Block coordinates: X:%.1f Y:%.1f Z:%.1f", cast.coords.x, cast.coords.z, cast.coords.y);
    EntryDraw(shaderProgram, &entryLookingAtBlockCoords);
  }

  DrawCrosshair(shaderProgram);
}
static void DrawCrosshair(GLuint shaderProgram) {
  float screenWidth = 1920.0f;
  float screenHeight = 1080.0f;
  float centerX = screenWidth / 2.0f;
  float centerY = screenHeight / 2.0f;

  float crosshairLength = 10.0f;

  // MISSING DRAWING THE ACTUAL CROSSHAIR
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
  int currentChunkX = (int)floor(camera->position.x / (CHUNK_SIZE * CUBE_SIZE));
  int currentChunkZ = (int)floor(camera->position.z / (CHUNK_SIZE * CUBE_SIZE));
  snprintf(entryChunkCoords.text, sizeof(entryChunkCoords.text), "Chunk coordinates: X:%d Z:%d", currentChunkX, currentChunkZ);
}
void HUDInit(char* buildName, char* buildVersion) {
  entryBiome.text[0] = '\0';
  entryFPS.text[0] = '\0';
  entryCubeCount.text[0] = '\0';
  snprintf(entryBuildInfo.text, sizeof(entryBuildInfo.text), "%s %s", buildName, buildVersion);
}
