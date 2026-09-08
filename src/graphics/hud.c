/**
 * @file graphics/hud.h
 * @brief HUD manager
 * @author VladimirJanus
 * @date 2024-11-30
 */

#include "hud.h"
#include "../utils/raycast.h"
#include "../utils/text.h"
#include "../world/world.h"
#include <stdio.h>

static DebugEntry entryBiome;
static DebugEntry entryFPS;
static DebugEntry entryCubeCount;
static DebugEntry entryBuildInfo;
static DebugEntry entryWorldCoords;
static DebugEntry entryChunkCoords;
static DebugEntry entryLookingAtBlockCoords;

static void EntryDraw(const TextState* state, DebugEntry* entry, int* entryIndex);
static void UpdateEntries(DebugData* data);

void HUDDraw(GLuint shaderProgram, DebugData* data) {
  (void)shaderProgram;
  UpdateEntries(data);
  Ray cast = rayCast(data->camera);
  snprintf(entryLookingAtBlockCoords.text, sizeof(entryLookingAtBlockCoords.text), "Block coordinates: X:%d Y:%d Z:%d", cast.blockCoords.x, cast.blockCoords.y, cast.blockCoords.z);

  int i = 0;
  TextState state;
  beginText(&state);
  EntryDraw(&state, &entryBiome, &i);
  EntryDraw(&state, &entryChunkCoords, &i);
  EntryDraw(&state, &entryWorldCoords, &i);
  EntryDraw(&state, &entryCubeCount, &i);
  EntryDraw(&state, &entryFPS, &i);
  EntryDraw(&state, &entryBuildInfo, &i);
  if (cast.hit) {
    EntryDraw(&state, &entryLookingAtBlockCoords, &i);
  }
  endText(&state);
}
static void EntryDraw(const TextState* state, DebugEntry* entry, int* entryIndex) {
  renderText(state, entry->text, 10.0f, 100.0f + *entryIndex);
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
