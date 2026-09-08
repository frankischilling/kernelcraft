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
#include <string.h>

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

static void UpdateEntries(DebugData* data);

/* HUD positions use framebuffer pixels and top-origin text baselines. Reserve
 * a band around the crosshair; diagnostic rows may use only the upper half. */
static void drawLabel(const TextState* state, const char* text, float x, float baseline, int availableWidth) {
  char fitted[128];
  if (availableWidth <= 0)
    return;
  size_t length = strlen(text);
  snprintf(fitted, sizeof(fitted), "%s", text);
  bool shortened = length >= sizeof(fitted);
  length = strlen(fitted);
  while (length && textWidth(state, fitted) > availableWidth) {
    fitted[--length] = '\0';
    shortened = true;
  }
  if (shortened) {
    int dots = textWidth(state, "...");
    while (length && (length + 3 >= sizeof(fitted) || textWidth(state, fitted) + dots > availableWidth))
      fitted[--length] = '\0';
    if (dots <= availableWidth)
      memcpy(fitted + length, "...", 4);
  }
  if (!fitted[0])
    return;
  int width = textWidth(state, fitted);
  float y = state->viewport[3] - baseline;
  glColor3f(0.08f, 0.08f, 0.08f);
  glBegin(GL_QUADS);
  glVertex2f(x - 2, y - 5);
  glVertex2f(x + width + 2, y - 5);
  glVertex2f(x + width + 2, y + state->fontHeight);
  glVertex2f(x - 2, y + state->fontHeight);
  glEnd();
  glColor3f(1, 1, 1);
  renderText(state, fitted, x, baseline);
}

static void drawTopLabel(const TextState* state, const char* text, float* baseline) {
  if (*baseline + 5 > state->viewport[3] * 0.5f - 14)
    return;
  drawLabel(state, text, 8, *baseline, state->viewport[2] - 16);
  *baseline += state->fontHeight + 6;
}

static void DrawControls(const TextState* state, const DebugData* data) {
  int width = state->viewport[2], height = state->viewport[3];
  float cx = width / 2 + 0.5f, cy = height / 2 + 0.5f;
  if (width >= 20 && height >= 20) {
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
  }
  glLineWidth(1);
  // Tiny windows keep only status and the crosshair until controls fit again.
  if (width < 96 || height < 120)
    return;
  int slotWidth = (width - 28) / 3;
  if (slotWidth > 100)
    slotWidth = 100;
  int barHeight = state->fontHeight + 14;
  const char* labels[] = {"1 Grass", "2 Dirt", "3 Stone"};
  const char* numbers[] = {"1", "2", "3"};
  for (int i = 0; i < 3; i++) {
    float x = (width - (3 * slotWidth + 12)) * 0.5f + i * (slotWidth + 6);
    glColor3f(0.12f, 0.12f, 0.12f);
    glBegin(GL_QUADS);
    glVertex2f(x, 8);
    glVertex2f(x + slotWidth, 8);
    glVertex2f(x + slotWidth, 8 + barHeight);
    glVertex2f(x, 8 + barHeight);
    glEnd();
    if (data->selectedBlock == i + BLOCK_GRASS)
      glColor3f(1.0f, 0.85f, 0.2f);
    else
      glColor3f(0.6f, 0.6f, 0.6f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, 8);
    glVertex2f(x + slotWidth, 8);
    glVertex2f(x + slotWidth, 8 + barHeight);
    glVertex2f(x, 8 + barHeight);
    glEnd();
    const char* label = textWidth(state, labels[i]) <= slotWidth - 12 ? labels[i] : numbers[i];
    drawLabel(state, label, x + 6, height - 8 - (barHeight - state->fontHeight) / 2, slotWidth - 12);
  }
  float baseline = height - barHeight - 22;
  if (baseline - state->fontHeight >= height * 0.5f + 14)
    drawLabel(state, data->captured ? "Left: break | Right: place | Esc" : "Esc: capture mouse to move and edit", 8, baseline, width - 16);
  baseline -= state->fontHeight + 6;
  if (baseline - state->fontHeight >= height * 0.5f + 14)
    drawLabel(state, data->flying ? "Fly: WASD + Space/Shift | F: walk" : "Walk: WASD | Space: jump | F: fly", 8, baseline, width - 16);
}

void HUDDraw(GLuint shaderProgram, DebugData* data) {
  (void)shaderProgram;
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  if (viewport[2] <= 0 || viewport[3] <= 0)
    return;
  UpdateEntries(data);
  Ray cast = data->selection;
  snprintf(entryLookingAtBlockCoords.text, sizeof(entryLookingAtBlockCoords.text), "Block coordinates: X:%d Y:%d Z:%d", cast.blockCoords.x, cast.blockCoords.y, cast.blockCoords.z);

  TextState state;
  glPushAttrib(GL_CURRENT_BIT | GL_LINE_BIT | GL_ENABLE_BIT);
  beginText(&state);
  glDisable(GL_TEXTURE_2D);
  float baseline = 8 + state.fontHeight;
  if (data->saveStatus)
    drawTopLabel(&state, entrySave.text, &baseline);
  char mode[80];
  const char* status = data->modeBlocked ? "No safe walk position" : data->flying ? "Debug flight" : data->grounded ? "Walking: grounded" : "Walking: airborne";
  snprintf(mode, sizeof(mode), "%s | F3: %s", status, data->showDebug ? "hide debug" : "debug");
  drawTopLabel(&state, mode, &baseline);
  if (data->showDebug) {
    drawTopLabel(&state, entryFPS.text, &baseline);
    if (data->stats) {
      drawTopLabel(&state, entryChunks.text, &baseline);
      drawTopLabel(&state, entryFaces.text, &baseline);
      drawTopLabel(&state, entryRebuilds.text, &baseline);
    }
    const DebugEntry* entries[] = {&entryWorldCoords, &entryChunkCoords, &entryBiome, &entryCubeCount, &entryMovement, &entryBuildInfo};
    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++)
      drawTopLabel(&state, entries[i]->text, &baseline);
    if (cast.hit)
      drawTopLabel(&state, entryLookingAtBlockCoords.text, &baseline);
  }
  DrawControls(&state, data);
  endText(&state);
  glPopAttrib();
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
    snprintf(entryFaces.text, sizeof(entryFaces.text), "Quads: %zu | Triangles: %zu", data->stats->submittedQuads, data->stats->submittedTriangles);
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
