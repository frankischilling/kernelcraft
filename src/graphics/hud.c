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
static DebugEntry entryChunks, entryFaces, entryRebuilds;
static DebugEntry entryMovement;
static DebugEntry entrySave;

static void EntryDraw(const TextState* state, DebugEntry* entry, int* entryIndex);
static void UpdateEntries(DebugData* data);

static void DrawControls(const TextState* state, const DebugData* data) {
  float cx = state->viewport[2] / 2 + 0.5f, cy = state->viewport[3] / 2 + 0.5f;
  for (int pass = 0; pass < 2; pass++) {
    glLineWidth(pass ? 1 : 3);
    float color = pass ? (data->captured ? 1.0f : 0.6f) : 0.0f;
    glColor3f(color, color, color);
    glBegin(GL_LINES);
    glVertex2f(cx - 7, cy);
    glVertex2f(cx + 7, cy);
    glVertex2f(cx, cy - 7);
    glVertex2f(cx, cy + 7);
    glEnd();
  }
  glLineWidth(1);
  const char* labels[] = {"[1] Grass", "[2] Dirt", "[3] Stone"};
  for (int i = 0; i < 3; i++) {
    float x = (state->viewport[2] - 312) * 0.5f + i * 106;
    glColor3f(0.12f, 0.12f, 0.12f);
    glBegin(GL_QUADS);
    glVertex2f(x, 8);
    glVertex2f(x + 100, 8);
    glVertex2f(x + 100, 42);
    glVertex2f(x, 42);
    glEnd();
    if (data->selectedBlock == i + BLOCK_GRASS)
      glColor3f(1.0f, 0.85f, 0.2f);
    else
      glColor3f(0.6f, 0.6f, 0.6f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, 8);
    glVertex2f(x + 100, 8);
    glVertex2f(x + 100, 42);
    glVertex2f(x, 42);
    glEnd();
    renderText(state, labels[i], x + 8, state->viewport[3] - 20);
  }
  glColor3f(1, 1, 1);
  renderText(state, data->flying ? "Fly: WASD + Space/Shift | F: walk" : "Walk: WASD | Space: jump | F: fly", 10, state->viewport[3] - 80);
  if (data->modeBlocked)
    renderText(state, "No clear standing space; still flying", 10, state->viewport[3] - 104);
  renderText(state, data->captured ? "Left: break | Right: place | Esc" : "Esc: capture mouse to move and edit", 10, state->viewport[3] - 56);
}

void HUDDraw(GLuint shaderProgram, DebugData* data) {
  (void)shaderProgram;
  UpdateEntries(data);
  Ray cast = data->selection;
  snprintf(entryLookingAtBlockCoords.text, sizeof(entryLookingAtBlockCoords.text), "Block coordinates: X:%d Y:%d Z:%d", cast.blockCoords.x, cast.blockCoords.y, cast.blockCoords.z);

  int i = 0;
  TextState state;
  glPushAttrib(GL_CURRENT_BIT | GL_LINE_BIT | GL_ENABLE_BIT);
  beginText(&state);
  glDisable(GL_TEXTURE_2D);
  glColor3f(1, 1, 1);
  EntryDraw(&state, &entryBiome, &i);
  EntryDraw(&state, &entryChunkCoords, &i);
  EntryDraw(&state, &entryWorldCoords, &i);
  EntryDraw(&state, &entryCubeCount, &i);
  EntryDraw(&state, &entryFPS, &i);
  EntryDraw(&state, &entryMovement, &i);
  if (data->saveStatus)
    EntryDraw(&state, &entrySave, &i);
  EntryDraw(&state, &entryBuildInfo, &i);
  if (data->stats) {
    EntryDraw(&state, &entryChunks, &i);
    EntryDraw(&state, &entryFaces, &i);
    EntryDraw(&state, &entryRebuilds, &i);
  }
  if (cast.hit) {
    EntryDraw(&state, &entryLookingAtBlockCoords, &i);
  }
  DrawControls(&state, data);
  endText(&state);
  glPopAttrib();
}
static void EntryDraw(const TextState* state, DebugEntry* entry, int* entryIndex) {
  renderText(state, entry->text, 10.0f, 24.0f + *entryIndex);
  *entryIndex += 20;
}
static void UpdateEntries(DebugData* data) {
  snprintf(entrySave.text, sizeof(entrySave.text), "Seed: %u | F5: %s", (unsigned)worldSeed(), data->saveStatus ? data->saveStatus : "Save");
  snprintf(entryMovement.text, sizeof(entryMovement.text), "%s | Steps/frame: %d",
           data->flying     ? "Debug flight"
           : data->grounded ? "Walking: grounded"
                            : "Walking: airborne",
           data->simulationSteps);
  snprintf(entryFPS.text, sizeof(entryFPS.text), "FPS: %.1f", data->fps);
  snprintf(entryBiome.text, sizeof(entryBiome.text), "Current biome: %s", getCurrentBiomeText(data->camera->position.x, data->camera->position.z));
  snprintf(entryCubeCount.text, sizeof(entryCubeCount.text), "Surface blocks: %d", data->visibleBlocks);
  if (data->stats) {
    snprintf(entryChunks.text, sizeof(entryChunks.text), "Chunks: %d/%d | Draws: %d", data->stats->chunksRendered, data->stats->chunksConsidered, data->stats->terrainDrawCalls);
    snprintf(entryFaces.text, sizeof(entryFaces.text), "Faces: %zu | Triangles: %zu", data->stats->submittedFaces, data->stats->submittedTriangles);
    snprintf(entryRebuilds.text, sizeof(entryRebuilds.text), "Rebuilt: %d | Update: %.2f ms", data->stats->chunksRebuilt, data->stats->meshUpdateMilliseconds);
  }

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
