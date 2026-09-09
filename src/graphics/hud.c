/**
 * @file graphics/hud.h
 * @brief HUD manager
 * @author VladimirJanus
 * @date 2024-11-30
 */

#include "hud.h"
#include "texture.h"
#include "../utils/raycast.h"
#include "../utils/text.h"
#include "../world/hotbar.h"
#include "../world/world.h"
#include <GL/freeglut.h>
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
static GLuint itemTextures[HOTBAR_SLOT_COUNT];

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
  if (data->captured && data->breakingProgress > 0 && width >= 64 && height >= 40) {
    glColor3f(0.08f, 0.08f, 0.08f);
    glBegin(GL_QUADS);
    glVertex2f(cx - 25, cy + 10);
    glVertex2f(cx + 25, cy + 10);
    glVertex2f(cx + 25, cy + 14);
    glVertex2f(cx - 25, cy + 14);
    glEnd();
    float right = cx - 24 + 48 * fminf(data->breakingProgress, 1.0f);
    glColor3f(1.0f, 0.85f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(cx - 24, cy + 11);
    glVertex2f(right, cy + 11);
    glVertex2f(right, cy + 13);
    glVertex2f(cx - 24, cy + 13);
    glEnd();
  }
  // Tiny windows keep only status and the crosshair until controls fit again.
  if (width < 192 || height < 120)
    return;
  TextState numbers = *state;
  numbers.font = GLUT_BITMAP_HELVETICA_10;
  numbers.fontHeight = glutBitmapHeight(numbers.font);
  int pitch = (width - 16) / HOTBAR_SLOT_COUNT;
  if (pitch > 48)
    pitch = 48;
  // Wide, short windows still need a clear band around the crosshair.
  int verticalPitch = height / 2 - numbers.fontHeight - 26;
  if (pitch > verticalPitch)
    pitch = verticalPitch;
  int slotWidth = pitch - 2, barHeight = pitch + numbers.fontHeight + 4;
  // Prefer whole multiples of the 16-pixel tiles when there is room.
  int iconSize = pitch >= 40 ? 32 : pitch >= 24 ? 16 : pitch - 8;
  int left = (width - (HOTBAR_SLOT_COUNT * pitch - 2)) / 2;
  for (int i = 0; i < HOTBAR_SLOT_COUNT; i++) {
    float x = left + i * pitch;
    glColor3f(0.12f, 0.12f, 0.12f);
    glBegin(GL_QUADS);
    glVertex2f(x, 8);
    glVertex2f(x + slotWidth, 8);
    glVertex2f(x + slotWidth, 8 + barHeight);
    glVertex2f(x, 8 + barHeight);
    glEnd();
    if (data->selectedSlot == i)
      glColor3f(1.0f, 0.85f, 0.2f);
    else
      glColor3f(0.6f, 0.6f, 0.6f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, 8);
    glVertex2f(x + slotWidth, 8);
    glVertex2f(x + slotWidth, 8 + barHeight);
    glVertex2f(x, 8 + barHeight);
    glEnd();
    if (hotbarBlock(i) != BLOCK_AIR) {
      float iconX = x + (slotWidth - iconSize) / 2;
      float iconY = 8 + numbers.fontHeight + 8;
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, itemTextures[i]);
      glColor3f(1, 1, 1);
      glBegin(GL_QUADS);
      // PNG row zero is the top of the icon; the HUD uses bottom-origin quads.
      glTexCoord2f(0, 1);
      glVertex2f(iconX, iconY);
      glTexCoord2f(1, 1);
      glVertex2f(iconX + iconSize, iconY);
      glTexCoord2f(1, 0);
      glVertex2f(iconX + iconSize, iconY + iconSize);
      glTexCoord2f(0, 0);
      glVertex2f(iconX, iconY + iconSize);
      glEnd();
      glDisable(GL_TEXTURE_2D);
    }
    char number[] = {(char)('1' + i), '\0'};
    drawLabel(&numbers, number, x + (slotWidth - textWidth(&numbers, number)) / 2, height - 14, slotWidth - 4);
  }
  float baseline = height - barHeight - 22;
  const char* names[] = {"Empty", "Grass", "Dirt", "Stone", "Cobblestone", "Oak planks", "Stone bricks"};
  const char* selectedName = names[hotbarBlock(data->selectedSlot)];
  if (baseline - state->fontHeight >= height * 0.5f + 14)
    drawLabel(state, selectedName, (width - textWidth(state, selectedName)) / 2, baseline, width - 16);
  baseline -= state->fontHeight + 6;
  if (baseline - state->fontHeight >= height * 0.5f + 14)
    drawLabel(state, data->captured ? "Hold left: break | Right: place | Esc" : "Esc: capture mouse to move and edit", 8, baseline, width - 16);
  baseline -= state->fontHeight + 6;
  if (baseline - state->fontHeight >= height * 0.5f + 14)
    drawLabel(state, data->flying ? "Fly: WASD + Space/Shift | F: walk" : "Walk: WASD | Space: jump | F: fly", 8, baseline, width - 16);
  baseline -= state->fontHeight + 6;
  if (!data->flying && baseline - state->fontHeight >= height * 0.5f + 14)
    drawLabel(state, "Shift: crouch | Double-tap W: run", 8, baseline, width - 16);
}

static const char* movementStatus(const DebugData* data) {
  if (data->flying)
    return "Debug flight";
  if (data->crouched)
    return data->grounded ? "Crouching: grounded" : "Crouching: airborne";
  if (data->running)
    return data->grounded ? "Running: grounded" : "Running: airborne";
  return data->grounded ? "Walking: grounded" : "Walking: airborne";
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
  GLint activeTexture;
  glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
  glPushAttrib(GL_CURRENT_BIT | GL_LINE_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_COLOR_BUFFER_BIT | GL_POLYGON_BIT);
  beginText(&state);
  glActiveTexture(GL_TEXTURE0);
  glDisable(GL_TEXTURE_2D);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBlendEquation(GL_FUNC_ADD);
  glDisable(GL_CULL_FACE);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  float baseline = 8 + state.fontHeight;
  if (data->saveStatus)
    drawTopLabel(&state, entrySave.text, &baseline);
  char mode[112];
  const char* status = data->modeBlocked ? "No safe walk position" : movementStatus(data);
  snprintf(mode, sizeof(mode), "%s | F3: %s | F4: wireframe %s", status, data->showDebug ? "hide debug" : "debug", data->wireframe ? "on" : "off");
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
  glActiveTexture((GLenum)activeTexture);
}
static void UpdateEntries(DebugData* data) {
  snprintf(entrySave.text, sizeof(entrySave.text), "Seed: %u | F5: %s", (unsigned)worldSeed(), data->saveStatus ? data->saveStatus : "Save");
  snprintf(entryMovement.text, sizeof(entryMovement.text), "%s | Steps/frame: %d", movementStatus(data), data->simulationSteps);
  snprintf(entryFPS.text, sizeof(entryFPS.text), "FPS: %.1f", data->fps);
  snprintf(entryBiome.text, sizeof(entryBiome.text), "Current biome: %s", getCurrentBiomeText(data->camera->position.x, data->camera->position.z));
  snprintf(entryCubeCount.text, sizeof(entryCubeCount.text), "Surface blocks: %d", data->visibleBlocks);
  if (data->stats) {
    snprintf(entryChunks.text, sizeof(entryChunks.text), "Chunks: %d/%d | Draws: %d | Hidden: %d | Queries: %d", data->stats->chunksRendered, data->stats->chunksConsidered,
             data->stats->terrainDrawCalls, data->stats->chunksOccluded, data->stats->occlusionQueries);
    snprintf(entryFaces.text, sizeof(entryFaces.text), "Quads: %zu | Triangles: %zu", data->stats->submittedQuads, data->stats->submittedTriangles);
    snprintf(entryRebuilds.text, sizeof(entryRebuilds.text), "Rebuilt: %d | Update: %.2f ms", data->stats->chunksRebuilt, data->stats->meshUpdateMilliseconds);
  }

  snprintf(entryWorldCoords.text, sizeof(entryWorldCoords.text), "World coordinates: X:%.1f Y:%.1f Z:%.1f", data->camera->position.x, data->camera->position.y,
           data->camera->position.z);

  int currentChunkX = (int)floor(data->camera->position.x / (CHUNK_SIZE * CUBE_SIZE));
  int currentChunkZ = (int)floor(data->camera->position.z / (CHUNK_SIZE * CUBE_SIZE));

  snprintf(entryChunkCoords.text, sizeof(entryChunkCoords.text), "Chunk coordinates: X:%d Z:%d", currentChunkX, currentChunkZ);
}
void HUDCleanup(void) {
  glDeleteTextures(HOTBAR_SLOT_COUNT, itemTextures);
  memset(itemTextures, 0, sizeof(itemTextures));
}

bool HUDInit(const char* buildName, const char* buildVersion) {
  HUDCleanup();
  entryBiome.text[0] = '\0';
  entryFPS.text[0] = '\0';
  entryCubeCount.text[0] = '\0';
  snprintf(entryBuildInfo.text, sizeof(entryBuildInfo.text), "%s %s", buildName, buildVersion);
  const char* paths[] = {"assets/textures/grass-side.png",  "assets/textures/dirt.png",       "assets/textures/stone.png",
                         "assets/textures/cobblestone.png", "assets/textures/oak-planks.png", "assets/textures/stone-bricks.png"};
  GLint activeTexture;
  glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
  glPushAttrib(GL_TEXTURE_BIT);
  glActiveTexture(GL_TEXTURE0);
  bool ready = true;
  for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
    itemTextures[i] = loadTexture(paths[i]);
    if (!itemTextures[i] || glGetError() != GL_NO_ERROR) {
      fprintf(stderr, "Cannot load hotbar icon: %s\n", paths[i]);
      ready = false;
      break;
    }
  }
  if (!ready)
    HUDCleanup();
  glPopAttrib();
  glActiveTexture((GLenum)activeTexture);
  return ready;
}
