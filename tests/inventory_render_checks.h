#ifndef INVENTORY_RENDER_CHECKS_H
#define INVENTORY_RENDER_CHECKS_H

#include "graphics/inventory_ui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  GLint program;
  GLint vao;
  GLint arrayBuffer;
  GLint viewport[4];
  GLint scissorBox[4];
  GLint drawFramebuffer;
  GLint readFramebuffer;
  GLint renderbuffer;
  GLint pixelUnpackBuffer;
  GLint activeTexture;
  GLint texture0;
  GLint textureArray0;
  GLint sampler0;
  GLint polygonMode[2];
  GLint depthFunc;
  GLint cullFace;
  GLint frontFace;
  GLint blendSrcRGB, blendDstRGB, blendSrcAlpha, blendDstAlpha;
  GLint blendEquationRGB, blendEquationAlpha;
  GLboolean scissor;
  GLboolean depthTest;
  GLboolean depthMask;
  GLboolean blend;
  GLboolean cull;
  GLboolean colorMask[4];
  GLdouble clearDepth;
  GLdouble depthRange[2];
} InventoryCheckGLState;

static void inventoryCheckCaptureState(InventoryCheckGLState* state) {
  glGetIntegerv(GL_CURRENT_PROGRAM, &state->program);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state->vao);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &state->arrayBuffer);
  glGetIntegerv(GL_VIEWPORT, state->viewport);
  glGetIntegerv(GL_SCISSOR_BOX, state->scissorBox);
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &state->drawFramebuffer);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &state->readFramebuffer);
  glGetIntegerv(GL_RENDERBUFFER_BINDING, &state->renderbuffer);
  glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &state->pixelUnpackBuffer);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &state->activeTexture);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &state->texture0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &state->textureArray0);
  glGetIntegerv(GL_SAMPLER_BINDING, &state->sampler0);
  glActiveTexture((GLenum)state->activeTexture);
  glGetIntegerv(GL_POLYGON_MODE, state->polygonMode);
  glGetIntegerv(GL_DEPTH_FUNC, &state->depthFunc);
  glGetIntegerv(GL_CULL_FACE_MODE, &state->cullFace);
  glGetIntegerv(GL_FRONT_FACE, &state->frontFace);
  glGetIntegerv(GL_BLEND_SRC_RGB, &state->blendSrcRGB);
  glGetIntegerv(GL_BLEND_DST_RGB, &state->blendDstRGB);
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &state->blendSrcAlpha);
  glGetIntegerv(GL_BLEND_DST_ALPHA, &state->blendDstAlpha);
  glGetIntegerv(GL_BLEND_EQUATION_RGB, &state->blendEquationRGB);
  glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &state->blendEquationAlpha);
  state->scissor = glIsEnabled(GL_SCISSOR_TEST);
  state->depthTest = glIsEnabled(GL_DEPTH_TEST);
  state->blend = glIsEnabled(GL_BLEND);
  state->cull = glIsEnabled(GL_CULL_FACE);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &state->depthMask);
  glGetBooleanv(GL_COLOR_WRITEMASK, state->colorMask);
  glGetDoublev(GL_DEPTH_CLEAR_VALUE, &state->clearDepth);
  glGetDoublev(GL_DEPTH_RANGE, state->depthRange);
}

static bool inventoryCheckStateEqual(const InventoryCheckGLState* left, const InventoryCheckGLState* right) {
  return left->program == right->program && left->vao == right->vao && left->arrayBuffer == right->arrayBuffer &&
         !memcmp(left->viewport, right->viewport, sizeof(left->viewport)) && !memcmp(left->scissorBox, right->scissorBox, sizeof(left->scissorBox)) &&
         left->drawFramebuffer == right->drawFramebuffer && left->readFramebuffer == right->readFramebuffer && left->renderbuffer == right->renderbuffer &&
         left->pixelUnpackBuffer == right->pixelUnpackBuffer && left->activeTexture == right->activeTexture && left->texture0 == right->texture0 &&
         left->textureArray0 == right->textureArray0 && left->sampler0 == right->sampler0 && left->polygonMode[0] == right->polygonMode[0] &&
         left->polygonMode[1] == right->polygonMode[1] && left->depthFunc == right->depthFunc && left->scissor == right->scissor && left->cullFace == right->cullFace &&
         left->frontFace == right->frontFace && left->blendSrcRGB == right->blendSrcRGB && left->blendDstRGB == right->blendDstRGB && left->blendSrcAlpha == right->blendSrcAlpha &&
         left->blendDstAlpha == right->blendDstAlpha && left->blendEquationRGB == right->blendEquationRGB && left->blendEquationAlpha == right->blendEquationAlpha &&
         left->depthTest == right->depthTest && left->depthMask == right->depthMask && left->blend == right->blend && left->cull == right->cull &&
         !memcmp(left->colorMask, right->colorMask, sizeof(left->colorMask)) && left->clearDepth == right->clearDepth && left->depthRange[0] == right->depthRange[0] &&
         left->depthRange[1] == right->depthRange[1];
}

static void inventoryCheckRestoreState(const InventoryCheckGLState* state) {
  glUseProgram((GLuint)state->program);
  glBindVertexArray((GLuint)state->vao);
  glBindBuffer(GL_ARRAY_BUFFER, (GLuint)state->arrayBuffer);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)state->drawFramebuffer);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)state->readFramebuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)state->renderbuffer);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)state->pixelUnpackBuffer);
  glViewport(state->viewport[0], state->viewport[1], state->viewport[2], state->viewport[3]);
  glScissor(state->scissorBox[0], state->scissorBox[1], state->scissorBox[2], state->scissorBox[3]);
  glPolygonMode(GL_FRONT, (GLenum)state->polygonMode[0]);
  glPolygonMode(GL_BACK, (GLenum)state->polygonMode[1]);
  glDepthFunc((GLenum)state->depthFunc);
  glDepthMask(state->depthMask);
  glDepthRange(state->depthRange[0], state->depthRange[1]);
  glColorMask(state->colorMask[0], state->colorMask[1], state->colorMask[2], state->colorMask[3]);
  glClearDepth(state->clearDepth);
  glCullFace((GLenum)state->cullFace);
  glFrontFace((GLenum)state->frontFace);
  glBlendFuncSeparate((GLenum)state->blendSrcRGB, (GLenum)state->blendDstRGB, (GLenum)state->blendSrcAlpha, (GLenum)state->blendDstAlpha);
  glBlendEquationSeparate((GLenum)state->blendEquationRGB, (GLenum)state->blendEquationAlpha);
  if (state->scissor)
    glEnable(GL_SCISSOR_TEST);
  else
    glDisable(GL_SCISSOR_TEST);
  if (state->depthTest)
    glEnable(GL_DEPTH_TEST);
  else
    glDisable(GL_DEPTH_TEST);
  if (state->blend)
    glEnable(GL_BLEND);
  else
    glDisable(GL_BLEND);
  if (state->cull)
    glEnable(GL_CULL_FACE);
  else
    glDisable(GL_CULL_FACE);
  glActiveTexture(GL_TEXTURE0);
  glBindSampler(0, (GLuint)state->sampler0);
  glBindTexture(GL_TEXTURE_2D, (GLuint)state->texture0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, (GLuint)state->textureArray0);
  glActiveTexture((GLenum)state->activeTexture);
}

static bool inventoryCheckSlot(const InventoryUILayout* layout, InventorySlotRef slot, int x, int y, int size) {
  InventoryUIRect rect;
  if (!inventoryUISlotRect(layout, slot, &rect) || rect.x != x || rect.y != y || rect.width != size || rect.height != size)
    return false;
  InventorySlotRef hit = {0};
  return inventoryUIHitTest(layout, rect.x + rect.width / 2, rect.y + rect.height / 2, &hit) && hit.kind == slot.kind && hit.index == slot.index;
}

static bool inventoryCheckLayout(void) {
  InventoryUILayout layout;
  if (!inventoryUILayout(1280, 720, &layout) || layout.scale != 3.0f || layout.panel.x != 376 || layout.panel.y != 111 || layout.panel.width != 528 || layout.panel.height != 498)
    return false;
  if (!inventoryCheckSlot(&layout, (InventorySlotRef){INVENTORY_SLOT_ARMOR, INVENTORY_ARMOR_HEAD}, 400, 135, 54) ||
      !inventoryCheckSlot(&layout, (InventorySlotRef){INVENTORY_SLOT_OFFHAND, 0}, 607, 297, 54) ||
      !inventoryCheckSlot(&layout, (InventorySlotRef){INVENTORY_SLOT_CRAFTING, 3}, 724, 219, 54) ||
      !inventoryCheckSlot(&layout, (InventorySlotRef){INVENTORY_SLOT_RESULT, 0}, 838, 195, 54) ||
      !inventoryCheckSlot(&layout, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 0}, 400, 537, 54) ||
      !inventoryCheckSlot(&layout, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 9}, 400, 363, 54) ||
      !inventoryCheckSlot(&layout, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 35}, 832, 471, 54))
    return false;

  if (!inventoryUILayout(1920, 1080, &layout) || layout.scale != 4.0f || layout.panel.x != 608 || layout.panel.y != 208 || layout.panel.width != 704 || layout.panel.height != 664)
    return false;

  if (!inventoryUILayout(120, 100, &layout) || layout.scale >= 1.0f || layout.panel.x < 0 || layout.panel.y < 0 || layout.panel.x + layout.panel.width > 120 ||
      layout.panel.y + layout.panel.height > 100)
    return false;
  InventoryUIRect last;
  return inventoryUISlotRect(&layout, (InventorySlotRef){INVENTORY_SLOT_CARRIED, 8}, &last) && last.x >= 0 && last.y >= 0 && last.x + last.width <= 120 &&
         last.y + last.height <= 100;
}

static void inventoryReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* destination) {
  const GLenum settings[] = {GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_SKIP_ROWS, GL_PACK_SKIP_PIXELS, GL_PACK_SWAP_BYTES};
  GLint saved[5], buffer;
  glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &buffer);
  for (size_t i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
    glGetIntegerv(settings[i], &saved[i]);
    glPixelStorei(settings[i], i == 0 ? 1 : 0);
  }
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  glReadPixels(x, y, width, height, format, type, destination);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)buffer);
  for (size_t i = 0; i < sizeof(settings) / sizeof(settings[0]); i++)
    glPixelStorei(settings[i], saved[i]);
}

typedef struct {
  int left, top, right, bottom;
  size_t pixels;
  unsigned long long headHash;
} InventoryCheckBounds;

static bool inventoryAvatarBounds(int framebufferHeight, int left, int top, int right, int bottom, int rowThreshold, int headBottom, InventoryCheckBounds* bounds) {
  int width = right - left, height = bottom - top;
  if (!bounds || width <= 0 || height <= 0)
    return false;
  size_t bytes = (size_t)width * height * 3;
  unsigned char* pixels = malloc(bytes);
  if (!pixels)
    return false;
  inventoryReadPixels(left, framebufferHeight - bottom, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

  *bounds = (InventoryCheckBounds){.left = right, .top = bottom, .right = left - 1, .bottom = top - 1, .headHash = 1469598103934665603ULL};
  for (int sourceRow = 0; sourceRow < height; sourceRow++) {
    int screenY = bottom - 1 - sourceRow;
    int rowPixels = 0;
    for (int x = 0; x < width; x++) {
      const unsigned char* pixel = pixels + ((size_t)sourceRow * width + x) * 3;
      if (screenY < headBottom)
        for (int channel = 0; channel < 3; channel++) {
          bounds->headHash ^= pixel[channel];
          bounds->headHash *= 1099511628211ULL;
        }
      int difference = abs((int)pixel[0] - 38) + abs((int)pixel[1] - 38) + abs((int)pixel[2] - 38);
      if (difference <= 24)
        continue;
      rowPixels++;
      bounds->pixels++;
      int screenX = left + x;
      if (screenX < bounds->left)
        bounds->left = screenX;
      if (screenX > bounds->right)
        bounds->right = screenX;
    }
    if (rowPixels >= rowThreshold) {
      if (screenY < bounds->top)
        bounds->top = screenY;
      if (screenY > bounds->bottom)
        bounds->bottom = screenY;
    }
  }
  free(pixels);
  return bounds->pixels > 100 && bounds->right >= bounds->left && bounds->bottom >= bounds->top;
}

static bool inventoryCheckAvatarCase(InventoryUI* ui, ItemRenderer* items, const PlayerRenderer* playerRenderer, const Inventory* inventory, const PlayerModelPose* livePose,
                                     const DayNightState* liveDaylight, int framebufferWidth, int framebufferHeight, int scale, int panelX, int panelY, int mouseX, int mouseY,
                                     int selectedSlot, const char* label, InventoryCheckBounds* outputBounds) {
  glViewport(0, 0, framebufferWidth, framebufferHeight);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glClearDepth(1);
  glDepthMask(GL_TRUE);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  inventoryUIDraw(ui, inventory, playerRenderer, livePose, liveDaylight, items, selectedSlot, framebufferWidth, framebufferHeight, mouseX, mouseY);

  // Independent classic-layout goldens: armor begins at logical y=8 and the
  // boots slot ends at y=80. The avatar column itself is logical x=26..75.
  int armorTop = panelY + 8 * scale;
  int armorBottom = panelY + 80 * scale;
  int previewLeft = panelX + 26 * scale;
  int previewRight = panelX + 75 * scale;
  InventoryCheckBounds bounds;
  if (!inventoryAvatarBounds(framebufferHeight, previewLeft, armorTop, previewRight, armorBottom, scale > 1 ? scale * 2 : 2, armorTop + 20 * scale, &bounds)) {
    fprintf(stderr, "Inventory avatar %s has no stable visible bounds\n", label);
    return false;
  }

  int inset = 2 * scale;
  int visibleHeight = bounds.bottom - bounds.top + 1;
  bool ok =
      bounds.top >= armorTop + inset - 1 && bounds.bottom <= armorBottom - inset && visibleHeight >= 54 * scale && bounds.left > previewLeft && bounds.right < previewRight - 1;
  if (!ok)
    fprintf(stderr, "Inventory avatar %s bounds %d,%d..%d,%d height %d outside armor band y=%d..%d scale=%d\n", label, bounds.left, bounds.top, bounds.right, bounds.bottom,
            visibleHeight, armorTop, armorBottom, scale);
  if (outputBounds)
    *outputBounds = bounds;
  return ok;
}

static bool testInventoryRendering(void) {
  if (!inventoryCheckLayout()) {
    fprintf(stderr, "Inventory UI layout or hit testing check failed\n");
    return false;
  }

  InventoryCheckGLState original;
  inventoryCheckCaptureState(&original);
  if (original.viewport[2] < INVENTORY_UI_LOGICAL_WIDTH || original.viewport[3] < INVENTORY_UI_LOGICAL_HEIGHT)
    return true;

  PlayerRenderer renderer = {0};
  ItemRenderer items = {0};
  InventoryUI ui = {0};
  bool ok = initPlayerRenderer(&renderer, "assets/player/skin.png") && initItemRenderer(&items) && inventoryUIInit(&ui);
  Inventory unarmored;
  inventoryInit(&unarmored);
  Inventory inventory = unarmored;
  for (int armor = 0; armor < INVENTORY_ARMOR_SLOT_COUNT; armor++) {
    inventory.armor[armor] = inventory.carried[INVENTORY_HOTBAR_SLOT_COUNT + armor];
    inventory.carried[INVENTORY_HOTBAR_SLOT_COUNT + armor] = (ItemStack){0};
  }
  PlayerModelPose pose;
  PlayerPoseInput poseInput = {.yaw = -90, .grounded = true};
  playerModelPose(&pose, &poseInput);
  DayNightState daylight = {
      .lightDirection = {-0.35f, 0.80f, 0.45f},
      .lightColor = {0.70f, 0.70f, 0.70f},
      .skyFill = {0.45f, 0.45f, 0.45f},
      .groundFill = {0.30f, 0.30f, 0.30f},
  };
  PlayerModelPose disturbedPose;
  PlayerPoseInput disturbedInput = {.yaw = 17, .pitch = 81, .gaitPhase = 2.4, .gaitWeight = 1, .punch = 0.35f, .crouched = true, .running = true, .grounded = false};
  playerModelPose(&disturbedPose, &disturbedInput);
  DayNightState darkWorld = {.lightDirection = {0, -1, 0}, .lightColor = {0.01f, 0.01f, 0.01f}, .skyFill = {0.01f, 0.01f, 0.01f}, .groundFill = {0.01f, 0.01f, 0.01f}};

  // Fixed framebuffer cases keep expectations independent of inventoryUILayout.
  InventoryCheckBounds unarmoredBounds = {0}, equippedBounds = {0};
  if (ok)
    ok = inventoryCheckAvatarCase(&ui, &items, &renderer, &unarmored, &pose, &daylight, 960, 540, 2, 304, 104, 480, 270, 8, "unarmored-center", &unarmoredBounds);
  if (ok)
    ok = inventoryCheckAvatarCase(&ui, &items, &renderer, &inventory, &disturbedPose, &darkWorld, 960, 540, 2, 304, 104, 480, 270, 8, "equipped-center", &equippedBounds);
  if (ok && (equippedBounds.top > unarmoredBounds.top || equippedBounds.bottom < unarmoredBounds.bottom + 1 || equippedBounds.pixels <= unarmoredBounds.pixels ||
             equippedBounds.headHash == unarmoredBounds.headHash)) {
    fprintf(stderr, "Equipped inventory avatar does not visibly add cap/boots: bare y=%d..%d pixels=%zu head=%llu, equipped y=%d..%d pixels=%zu head=%llu\n", unarmoredBounds.top,
            unarmoredBounds.bottom, unarmoredBounds.pixels, unarmoredBounds.headHash, equippedBounds.top, equippedBounds.bottom, equippedBounds.pixels, equippedBounds.headHash);
    ok = false;
  }
  if (ok)
    ok = inventoryCheckAvatarCase(&ui, &items, &renderer, &inventory, &disturbedPose, &darkWorld, 960, 540, 2, 304, 104, 0, 0, 0, "equipped-mouse-upper-left", NULL);
  if (ok)
    ok = inventoryCheckAvatarCase(&ui, &items, &renderer, &inventory, &disturbedPose, &darkWorld, 960, 540, 2, 304, 104, 959, 539, 0, "equipped-mouse-lower-right", NULL);
  if (ok)
    ok = inventoryCheckAvatarCase(&ui, &items, &renderer, &inventory, &disturbedPose, &darkWorld, 360, 540, 1, 92, 187, 359, 0, 0, "equipped-portrait", NULL);

  // Exercise shared 3D icons through both derived-result and cursor paths while
  // keeping ownership valid: four carried stones move into crafting and three
  // dirt move onto the cursor.
  inventory.carried[2].count -= INVENTORY_CRAFTING_SLOT_COUNT;
  for (int slot = 0; slot < INVENTORY_CRAFTING_SLOT_COUNT; slot++)
    inventory.crafting[slot] = (ItemStack){ITEM_STONE, 1};
  inventory.carried[1].count -= 3;
  inventory.cursor = (ItemStack){ITEM_DIRT, 3};

  glClearDepth(0.375);
  glDepthMask(GL_TRUE);
  glClear(GL_DEPTH_BUFFER_BIT);
  float depthBefore = 0, depthAfter = 0;
  int sampleX = original.viewport[2] / 2;
  int sampleY = original.viewport[3] / 2;
  inventoryReadPixels(sampleX, sampleY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depthBefore);

  glViewport(3, 5, original.viewport[2] - 7, original.viewport[3] - 11);
  glEnable(GL_SCISSOR_TEST);
  glScissor(7, 9, original.viewport[2] / 2, original.viewport[3] / 2);
  glPolygonMode(GL_FRONT, GL_LINE);
  glPolygonMode(GL_BACK, GL_POINT);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDepthFunc(GL_GREATER);
  glDepthRange(0.2, 0.8);
  glEnable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
  glActiveTexture(GL_TEXTURE3);
  InventoryCheckGLState expected, actual;
  inventoryCheckCaptureState(&expected);

  inventoryUIDraw(&ui, &inventory, &renderer, &disturbedPose, &darkWorld, &items, 0, original.viewport[2], original.viewport[3], original.viewport[2] / 2,
                  original.viewport[3] / 3);
  inventoryCheckCaptureState(&actual);
  ok &= inventoryCheckStateEqual(&expected, &actual);
  inventoryReadPixels(sampleX, sampleY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depthAfter);
  ok &= depthBefore == depthAfter;

  PlayerEquipmentVisuals equipment = {.helmet = true, .chestplate = true, .leggings = true, .boots = true};
  Vec3 eye = {0, 0.9f, -4};
  Vec3 center = {0, 0.9f, 0};
  Vec3 up = {0, 1, 0};
  Mat4 view, projection;
  mat4_lookAt(view, &eye, &center, &up);
  mat4_perspective(projection, 35, (float)original.viewport[2] / original.viewport[3], 0.1f, 20);
  renderPlayerEquipment(&renderer, (Vec3){0}, &pose, &equipment, view, projection, &daylight);
  inventoryCheckCaptureState(&actual);
  ok &= inventoryCheckStateEqual(&expected, &actual);

  inventoryUICleanup(&ui);
  cleanupItemRenderer(&items);
  cleanupPlayerRenderer(&renderer);
  inventoryCheckRestoreState(&original);
  if (!ok)
    fprintf(stderr, "Inventory preview, equipment, world-depth isolation, or GL state check failed\n");
  else
    puts("Inventory layout, preview, equipment, world-depth isolation, and GL state checks passed");
  return ok && glGetError() == GL_NO_ERROR;
}

#endif
