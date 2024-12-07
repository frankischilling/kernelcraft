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

static DebugEntry entryBiome;
static DebugEntry entryFPS;
static DebugEntry entryCubeCount;
static DebugEntry entryBuildInfo;
static DebugEntry entryWorldCoords;
static DebugEntry entryChunkCoords;
static DebugEntry entryLookingAtBlockCoords;

void HUDDraw(GLuint shaderProgram, DebugData* data) {
  UpdateEntries(data);
  Ray cast = rayCast(data->camera);
  snprintf(entryLookingAtBlockCoords.text, sizeof(entryLookingAtBlockCoords.text), "Block coordinates: X:%d Y:%d Z:%d", cast.blockCoords.x, cast.blockCoords.y, cast.blockCoords.z);

  int i = 0;
  EntryDraw(shaderProgram, &entryBiome, &i);
  EntryDraw(shaderProgram, &entryChunkCoords, &i);
  EntryDraw(shaderProgram, &entryWorldCoords, &i);
  EntryDraw(shaderProgram, &entryCubeCount, &i);
  EntryDraw(shaderProgram, &entryFPS, &i);
  EntryDraw(shaderProgram, &entryBuildInfo, &i);
  if (cast.hit) {
    EntryDraw(shaderProgram, &entryLookingAtBlockCoords, &i);
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
static void EntryDraw(GLuint shaderProgram, DebugEntry* entry, int* entryIndex) {
  renderText(shaderProgram, entry->text, 10.0f, 100.0f + *entryIndex);
  *entryIndex += 20;
}
static void UpdateEntries(DebugData* data) {
  snprintf(entryFPS.text, sizeof(entryFPS.text), "FPS: %.1f", data->fps);
  snprintf(entryBiome.text, sizeof(entryBiome.text), "Current biome: %s", getCurrentBiomeText(data->camera->position.x, data->camera->position.z));
  snprintf(entryCubeCount.text, sizeof(entryCubeCount.text), "Visible Cubes: %d", data->visibleBlocks);

  snprintf(entryWorldCoords.text, sizeof(entryWorldCoords.text), "World coordinates: X:%.1f Y:%.1f Z:%.1f", data->camera->position.x, data->camera->position.y,
           data->camera->position.z);

  int currentChunkX = (int)floor(data->camera->position.x / (CHUNK_SIZE * CUBE_SIZE));
  int currentChunkZ = (int)floor(data->camera->position.z / (CHUNK_SIZE * CUBE_SIZE));

  snprintf(entryChunkCoords.text, sizeof(entryChunkCoords.text), "Chunk coordinates: X:%d Z:%d", currentChunkX, currentChunkZ);
}
void HUDInit(char* buildName, char* buildVersion) {
  entryBiome.text[0] = '\0';
  entryFPS.text[0] = '\0';
  entryCubeCount.text[0] = '\0';
  snprintf(entryBuildInfo.text, sizeof(entryBuildInfo.text), "%s %s", buildName, buildVersion);
}
