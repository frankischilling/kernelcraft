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
  GLint activeTexture;
  GLint texture0;
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
} InventoryCheckGLState;

static void inventoryCheckCaptureState(InventoryCheckGLState* state) {
  glGetIntegerv(GL_CURRENT_PROGRAM, &state->program);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state->vao);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &state->arrayBuffer);
  glGetIntegerv(GL_VIEWPORT, state->viewport);
  glGetIntegerv(GL_SCISSOR_BOX, state->scissorBox);
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &state->drawFramebuffer);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &state->readFramebuffer);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &state->activeTexture);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &state->texture0);
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
}

static bool inventoryCheckStateEqual(const InventoryCheckGLState* left, const InventoryCheckGLState* right) {
  return left->program == right->program && left->vao == right->vao && left->arrayBuffer == right->arrayBuffer &&
         !memcmp(left->viewport, right->viewport, sizeof(left->viewport)) && !memcmp(left->scissorBox, right->scissorBox, sizeof(left->scissorBox)) &&
         left->drawFramebuffer == right->drawFramebuffer && left->readFramebuffer == right->readFramebuffer && left->activeTexture == right->activeTexture &&
         left->texture0 == right->texture0 && left->sampler0 == right->sampler0 && left->polygonMode[0] == right->polygonMode[0] && left->polygonMode[1] == right->polygonMode[1] &&
         left->depthFunc == right->depthFunc && left->scissor == right->scissor && left->cullFace == right->cullFace && left->frontFace == right->frontFace &&
         left->blendSrcRGB == right->blendSrcRGB && left->blendDstRGB == right->blendDstRGB && left->blendSrcAlpha == right->blendSrcAlpha &&
         left->blendDstAlpha == right->blendDstAlpha && left->blendEquationRGB == right->blendEquationRGB && left->blendEquationAlpha == right->blendEquationAlpha &&
         left->depthTest == right->depthTest && left->depthMask == right->depthMask && left->blend == right->blend && left->cull == right->cull &&
         !memcmp(left->colorMask, right->colorMask, sizeof(left->colorMask)) && left->clearDepth == right->clearDepth;
}

static void inventoryCheckRestoreState(const InventoryCheckGLState* state) {
  glUseProgram((GLuint)state->program);
  glBindVertexArray((GLuint)state->vao);
  glBindBuffer(GL_ARRAY_BUFFER, (GLuint)state->arrayBuffer);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)state->drawFramebuffer);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)state->readFramebuffer);
  glViewport(state->viewport[0], state->viewport[1], state->viewport[2], state->viewport[3]);
  glScissor(state->scissorBox[0], state->scissorBox[1], state->scissorBox[2], state->scissorBox[3]);
  glPolygonMode(GL_FRONT, (GLenum)state->polygonMode[0]);
  glPolygonMode(GL_BACK, (GLenum)state->polygonMode[1]);
  glDepthFunc((GLenum)state->depthFunc);
  glDepthMask(state->depthMask);
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

static bool inventoryCheckPreviewPixels(const InventoryUILayout* layout) {
  InventoryUIRect preview = {
      layout->panel.x + (int)lroundf(26 * layout->scale),
      layout->panel.y + (int)lroundf(8 * layout->scale),
      (int)lroundf(75 * layout->scale) - (int)lroundf(26 * layout->scale),
      (int)lroundf(78 * layout->scale) - (int)lroundf(8 * layout->scale),
  };
  size_t bytes = (size_t)preview.width * preview.height * 3;
  unsigned char* pixels = malloc(bytes);
  if (!pixels)
    return false;
  inventoryReadPixels(preview.x, layout->framebufferHeight - preview.y - preview.height, preview.width, preview.height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
  size_t modelPixels = 0;
  for (size_t i = 0; i < bytes; i += 3) {
    int difference = abs((int)pixels[i] - 38) + abs((int)pixels[i + 1] - 38) + abs((int)pixels[i + 2] - 38);
    modelPixels += difference > 24;
  }
  free(pixels);
  return modelPixels > 100;
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
  InventoryUI ui = {0};
  bool ok = initPlayerRenderer(&renderer, "assets/player/skin.png") && inventoryUIInit(&ui);
  Inventory inventory;
  inventoryInit(&inventory);
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
  GLuint textures[INVENTORY_UI_BLOCK_TEXTURE_COUNT] = {0};
  InventoryUILayout layout;
  ok &= inventoryUILayout(original.viewport[2], original.viewport[3], &layout);

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
  glEnable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
  glActiveTexture(GL_TEXTURE3);
  InventoryCheckGLState expected, actual;
  inventoryCheckCaptureState(&expected);

  inventoryUIDraw(&ui, &inventory, &renderer, &pose, &daylight, textures, original.viewport[2], original.viewport[3], original.viewport[2] / 2, original.viewport[3] / 3);
  inventoryCheckCaptureState(&actual);
  ok &= inventoryCheckStateEqual(&expected, &actual);
  inventoryReadPixels(sampleX, sampleY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depthAfter);
  ok &= depthBefore == depthAfter;
  ok &= inventoryCheckPreviewPixels(&layout);

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
  cleanupPlayerRenderer(&renderer);
  inventoryCheckRestoreState(&original);
  if (!ok)
    fprintf(stderr, "Inventory preview, equipment, world-depth isolation, or GL state check failed\n");
  else
    puts("Inventory layout, preview, equipment, world-depth isolation, and GL state checks passed");
  return ok && glGetError() == GL_NO_ERROR;
}

#endif
