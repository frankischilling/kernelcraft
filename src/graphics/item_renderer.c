#include "item_renderer.h"
#include "shader.h"
#include "texture.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  GLint program, vao, buffer, active, texture, arrayTexture, sampler;
  GLint drawFramebuffer, readFramebuffer, renderbuffer, unpackBuffer;
} ItemRenderState;

static void saveState(ItemRenderState* state) {
  glGetIntegerv(GL_CURRENT_PROGRAM, &state->program);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state->vao);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &state->buffer);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &state->active);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &state->texture);
  glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &state->arrayTexture);
  glGetIntegerv(GL_SAMPLER_BINDING, &state->sampler);
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &state->drawFramebuffer);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &state->readFramebuffer);
  glGetIntegerv(GL_RENDERBUFFER_BINDING, &state->renderbuffer);
  glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &state->unpackBuffer);
  glPushAttrib(GL_ALL_ATTRIB_BITS);
}

static void restoreState(const ItemRenderState* state) {
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)state->drawFramebuffer);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)state->readFramebuffer);
  glPopAttrib();
  glUseProgram((GLuint)state->program);
  glBindVertexArray((GLuint)state->vao);
  glBindBuffer(GL_ARRAY_BUFFER, (GLuint)state->buffer);
  glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)state->renderbuffer);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)state->unpackBuffer);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, (GLuint)state->texture);
  glBindTexture(GL_TEXTURE_2D_ARRAY, (GLuint)state->arrayTexture);
  glBindSampler(0, (GLuint)state->sampler);
  glActiveTexture((GLenum)state->active);
}

static void destroyTarget(ItemRenderTarget* target) {
  glDeleteFramebuffers(1, &target->framebuffer);
  glDeleteTextures(1, &target->color);
  glDeleteRenderbuffers(1, &target->depth);
  *target = (ItemRenderTarget){0};
}

// Called inside a saved render state. Build a replacement before releasing the
// previous target, so an allocation failure cannot publish mismatched storage.
static bool resizeTarget(ItemRenderTarget* target, int width, int height) {
  if (width <= 0 || height <= 0)
    return false;
  if (target->framebuffer && target->width == width && target->height == height)
    return true;
  GLint limit;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &limit);
  if (width > limit || height > limit)
    return false;
  ItemRenderTarget next = {.width = width, .height = height};
  glGenFramebuffers(1, &next.framebuffer);
  glGenTextures(1, &next.color);
  glGenRenderbuffers(1, &next.depth);
  if (!next.framebuffer || !next.color || !next.depth) {
    destroyTarget(&next);
    return false;
  }
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, next.color);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glBindRenderbuffer(GL_RENDERBUFFER, next.depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
  glBindFramebuffer(GL_FRAMEBUFFER, next.framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, next.color, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, next.depth);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
  GLenum error = glGetError();
  if (!complete || error != GL_NO_ERROR) {
    destroyTarget(&next);
    return false;
  }
  destroyTarget(target);
  *target = next;
  return true;
}

static bool ready(const ItemRenderer* renderer) {
  return renderer && renderer->program && renderer->compositeProgram && renderer->vao && renderer->vbo && renderer->materials;
}

static void beginItems(const ItemRenderer* renderer, const Mat4 viewProjection, const DayNightState* light) {
  glUseProgram(renderer->program);
  glBindVertexArray(renderer->vao);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, renderer->materials);
  glBindSampler(0, 0);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDepthRange(0, 1);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDisable(GL_BLEND);
  glDisable(GL_ALPHA_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_COLOR_LOGIC_OP);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glDisable(GL_FRAMEBUFFER_SRGB);
  glUniformMatrix4fv(renderer->viewProjectionLocation, 1, GL_FALSE, viewProjection);
  glUniform3f(renderer->lightDirectionLocation, light->lightDirection.x, light->lightDirection.y, light->lightDirection.z);
  glUniform3f(renderer->lightColorLocation, light->lightColor.x, light->lightColor.y, light->lightColor.z);
  glUniform3f(renderer->skyColorLocation, light->skyFill.x, light->skyFill.y, light->skyFill.z);
  glUniform3f(renderer->groundColorLocation, light->groundFill.x, light->groundFill.y, light->groundFill.z);
}

static void drawModel(const ItemRenderer* renderer, uint16_t item, const Mat4 model) {
  if (!item || item > ITEM_ID_LAST || !renderer->count[item])
    return;
  uint32_t color = inventoryItemColor(item);
  glUniform3f(renderer->colorLocation, ((color >> 16) & 255) / 255.0f, ((color >> 8) & 255) / 255.0f, (color & 255) / 255.0f);
  glUniformMatrix4fv(renderer->modelLocation, 1, GL_FALSE, model);
  glDrawArrays(GL_TRIANGLES, renderer->first[item], renderer->count[item]);
}

static void appendTransform(Mat4 model, const Mat4 transform) {
  Mat4 next;
  mat4_multiply(next, model, transform);
  memcpy(model, next, sizeof(next));
}

static void translate(Mat4 model, Vec3 offset) {
  Mat4 translation;
  mat4_identity(translation);
  translation[12] = offset.x;
  translation[13] = offset.y;
  translation[14] = offset.z;
  appendTransform(model, translation);
}

static void scale(Mat4 model, Vec3 amount) {
  Mat4 scaling;
  mat4_identity(scaling);
  scaling[0] = amount.x;
  scaling[5] = amount.y;
  scaling[10] = amount.z;
  appendTransform(model, scaling);
}

static void rotate(Mat4 model, int axis, float radians) {
  Mat4 rotation;
  mat4_identity(rotation);
  int a = (axis + 1) % 3, b = (axis + 2) % 3;
  rotation[a * 4 + a] = rotation[b * 4 + b] = cosf(radians);
  rotation[a * 4 + b] = sinf(radians);
  rotation[b * 4 + a] = -sinf(radians);
  appendTransform(model, rotation);
}

static void clearTarget(const ItemRenderTarget* target) {
  glBindFramebuffer(GL_FRAMEBUFFER, target->framebuffer);
  glViewport(0, 0, target->width, target->height);
  glDisable(GL_SCISSOR_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDepthMask(GL_TRUE);
  glClearColor(0, 0, 0, 0);
  glClearDepth(1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static bool bakeIcons(ItemRenderer* renderer) {
  ItemRenderTarget target = {0};
  if (!resizeTarget(&target, ITEM_ICON_SIZE, ITEM_ICON_SIZE))
    return false;
  Vec3 eye = {2, 1.6f, 2.5f}, origin = {0}, up = {0, 1, 0};
  Mat4 view, projection, viewProjection, model;
  mat4_lookAt(view, &eye, &origin, &up);
  mat4_identity(projection);
  projection[0] = projection[5] = 1.0f / 0.83f;
  projection[10] = -0.2f;
  mat4_multiply(viewProjection, projection, view);
  mat4_identity(model);
  DayNightState light = {.lightDirection = {-0.35f, 0.85f, 0.45f}, .lightColor = {0.55f, 0.55f, 0.55f}, .skyFill = {0.5f, 0.5f, 0.5f}, .groundFill = {0.3f, 0.3f, 0.3f}};
  for (uint16_t item = 1; item <= ITEM_ID_LAST; item++) {
    clearTarget(&target);
    beginItems(renderer, viewProjection, &light);
    drawModel(renderer, item, model);
    glGenTextures(1, &renderer->icons[item]);
    glBindTexture(GL_TEXTURE_2D, renderer->icons[item]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, ITEM_ICON_SIZE, ITEM_ICON_SIZE, 0);
    if (!renderer->icons[item] || glGetError() != GL_NO_ERROR) {
      destroyTarget(&target);
      return false;
    }
  }
  destroyTarget(&target);
  return true;
}

bool initItemRenderer(ItemRenderer* renderer) {
  if (!renderer)
    return false;
  *renderer = (ItemRenderer){0};
  ItemModelVertex vertices[ITEM_ID_LAST * ITEM_MODEL_VERTEX_CAPACITY + 6];
  size_t count = 0;
  for (uint16_t item = 1; item <= ITEM_ID_LAST; item++) {
    ItemModel model;
    if (!itemModelBuild(item, &model) || model.count > ITEM_MODEL_VERTEX_CAPACITY)
      return false;
    renderer->first[item] = (GLint)count;
    renderer->count[item] = (GLsizei)model.count;
    memcpy(vertices + count, model.vertices, model.count * sizeof(*vertices));
    count += model.count;
  }
  renderer->quadFirst = (GLint)count;
  const float corners[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};
  for (int i = 0; i < 6; i++)
    vertices[count++] =
        (ItemModelVertex){.position = {corners[i][0], corners[i][1], 0}, .normal = {0, 0, 1}, .u = (corners[i][0] + 1) * 0.5f, .v = (corners[i][1] + 1) * 0.5f, .layer = -1};
  ItemRenderState state;
  saveState(&state);
  const GLenum unpackFields[] = {GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH, GL_UNPACK_SKIP_ROWS, GL_UNPACK_SKIP_PIXELS, GL_UNPACK_IMAGE_HEIGHT, GL_UNPACK_SKIP_IMAGES};
  GLint unpack[6];
  for (size_t i = 0; i < 6; i++) {
    glGetIntegerv(unpackFields[i], &unpack[i]);
    glPixelStorei(unpackFields[i], i == 0 ? 1 : 0);
  }
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  bool success = false;
  renderer->program = loadShaders("assets/shaders/item_vertex.glsl", "assets/shaders/item_fragment.glsl");
  if (!renderer->program)
    goto finish;
  renderer->compositeProgram = loadShaders("assets/shaders/item_composite_vertex.glsl", "assets/shaders/item_composite_fragment.glsl");
  if (!renderer->compositeProgram)
    goto finish;
  glUseProgram(renderer->compositeProgram);
  GLint image = glGetUniformLocation(renderer->compositeProgram, "image");
  if (image < 0)
    goto finish;
  glUniform1i(image, 0);
  const char* paths[] = {"assets/textures/stone.png",        "assets/textures/dirt.png",         "assets/textures/grass-top.png",
                         "assets/textures/grass-side.png",   "assets/textures/dirt-rocks.png",   "assets/textures/grass-top-leaves.png",
                         "assets/textures/grass-bug.png",    "assets/textures/cobblestone.png",  "assets/textures/oak-planks.png",
                         "assets/textures/stone-bricks.png", "assets/textures/oak-log-side.png", "assets/textures/oak-log-top.png",
                         "assets/textures/oak-leaves.png"};
  renderer->materials = loadTextureArray(paths, (int)(sizeof(paths) / sizeof(paths[0])));
  if (!renderer->materials)
    goto finish;
  glGenVertexArrays(1, &renderer->vao);
  glGenBuffers(1, &renderer->vbo);
  glBindVertexArray(renderer->vao);
  glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(count * sizeof(*vertices)), vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ItemModelVertex), (void*)offsetof(ItemModelVertex, position));
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ItemModelVertex), (void*)offsetof(ItemModelVertex, normal));
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(ItemModelVertex), (void*)offsetof(ItemModelVertex, u));
  glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(ItemModelVertex), (void*)offsetof(ItemModelVertex, layer));
  for (GLuint attribute = 0; attribute < 4; attribute++)
    glEnableVertexAttribArray(attribute);

  const struct {
    GLint* location;
    const char* name;
  } uniforms[] = {{&renderer->modelLocation, "model"},
                  {&renderer->viewProjectionLocation, "viewProjection"},
                  {&renderer->colorLocation, "itemColor"},
                  {&renderer->lightDirectionLocation, "lightDirection"},
                  {&renderer->lightColorLocation, "lightColor"},
                  {&renderer->skyColorLocation, "skyColor"},
                  {&renderer->groundColorLocation, "groundColor"}};

  glUseProgram(renderer->program);
  for (size_t i = 0; i < sizeof(uniforms) / sizeof(*uniforms); i++) {
    *uniforms[i].location = glGetUniformLocation(renderer->program, uniforms[i].name);
    if (*uniforms[i].location < 0)
      goto finish;
  }
  GLint materials = glGetUniformLocation(renderer->program, "materials");
  if (materials < 0)
    goto finish;
  glUniform1i(materials, 0);
  if (!ready(renderer) || glGetError() != GL_NO_ERROR)
    goto finish;
  success = bakeIcons(renderer);
finish:
  for (size_t i = 0; i < 6; i++)
    glPixelStorei(unpackFields[i], unpack[i]);
  restoreState(&state);
  if (!success) {
    cleanupItemRenderer(renderer);
    fprintf(stderr, "Failed to initialize 3D item rendering\n");
  }
  return success;
}

void cleanupItemRenderer(ItemRenderer* renderer) {
  if (!renderer)
    return;
  destroyTarget(&renderer->heldTarget);
  glDeleteTextures(ITEM_ID_LAST + 1, renderer->icons);
  glDeleteTextures(1, &renderer->materials);
  glDeleteBuffers(1, &renderer->vbo);
  glDeleteVertexArrays(1, &renderer->vao);
  if (renderer->program)
    glDeleteProgram(renderer->program);
  if (renderer->compositeProgram)
    glDeleteProgram(renderer->compositeProgram);
  *renderer = (ItemRenderer){0};
}

GLuint itemRendererIcon(const ItemRenderer* renderer, uint16_t item) {
  return ready(renderer) && item && item <= ITEM_ID_LAST ? renderer->icons[item] : 0;
}

void renderItemModel(const ItemRenderer* renderer, uint16_t item, const Mat4 model, const Mat4 view, const Mat4 projection, const DayNightState* daylight) {
  if (!ready(renderer) || !item || item > ITEM_ID_LAST || !model || !view || !projection || !daylight)
    return;
  ItemRenderState state;
  saveState(&state);
  Mat4 viewProjection;
  mat4_multiply(viewProjection, projection, view);
  beginItems(renderer, viewProjection, daylight);
  drawModel(renderer, item, model);
  restoreState(&state);
}

void renderDroppedItems(const ItemRenderer* renderer, const DroppedItems* drops, const Mat4 view, const Mat4 projection, const DayNightState* daylight) {
  if (!ready(renderer) || !drops || !view || !projection || !daylight)
    return;
  bool any = false;
  for (size_t i = 0; i < DROPPED_ITEM_CAPACITY; i++)
    any |= drops->items[i].active;
  if (!any)
    return;
  ItemRenderState state;
  saveState(&state);
  Mat4 viewProjection;
  mat4_multiply(viewProjection, projection, view);
  beginItems(renderer, viewProjection, daylight);
  for (size_t i = 0; i < DROPPED_ITEM_CAPACITY; i++) {
    const DroppedItem* item = &drops->items[i];
    if (!item->active)
      continue;
    float phase = (float)drops->animationSeconds + (float)i * 0.4f;
    Mat4 model;
    mat4_identity(model);
    translate(model, (Vec3){item->position.x, item->position.y + 0.05f + 0.04f * sinf(phase * 3), item->position.z});
    rotate(model, 1, phase * 1.570796327f);
    scale(model, (Vec3){0.25f, 0.25f, 0.25f});
    drawModel(renderer, item->stack.item, model);
  }
  restoreState(&state);
}

static bool hasItem(ItemStack stack) {
  return stack.count && stack.item && inventoryStackValid(stack);
}

void renderPlayerHeldItems(const ItemRenderer* renderer, Vec3 feet, const PlayerModelPose* pose, ItemStack mainHand, ItemStack offhand, const Mat4 view, const Mat4 projection,
                           const DayNightState* daylight) {
  if (!ready(renderer) || !pose || !daylight || (!hasItem(mainHand) && !hasItem(offhand)))
    return;
  ItemRenderState state;
  saveState(&state);
  Mat4 viewProjection;
  mat4_multiply(viewProjection, projection, view);
  beginItems(renderer, viewProjection, daylight);
  ItemStack hands[] = {mainHand, offhand};
  for (int hand = 0; hand < 2; hand++) {
    if (!hasItem(hands[hand]))
      continue;
    PlayerModelPart part = hand ? PLAYER_MODEL_LEFT_ARM : PLAYER_MODEL_RIGHT_ARM;
    const PlayerPartSpec* spec = playerModelPartSpec(part);
    const PlayerPartPose* joint = &pose->parts[part];
    Mat4 model;
    mat4_identity(model);
    translate(model, feet);
    rotate(model, 1, pose->rootYaw);
    scale(model, (Vec3){pose->rootScale, pose->rootScale, pose->rootScale});
    translate(model, (Vec3){spec->pivot.x + joint->translation.x, spec->pivot.y + joint->translation.y, spec->pivot.z + joint->translation.z});
    rotate(model, 2, joint->rotation.z);
    rotate(model, 1, joint->rotation.y);
    rotate(model, 0, joint->rotation.x);
    scale(model, joint->scale);
    translate(model, (Vec3){spec->centerOffset.x, spec->centerOffset.y - spec->size.y * 0.5f, spec->centerOffset.z - 0.12f});
    scale(model, (Vec3){0.32f, 0.32f, 0.32f});
    drawModel(renderer, hands[hand].item, model);
  }
  restoreState(&state);
}

static void compositeHeld(const ItemRenderer* renderer, GLuint texture, const GLint viewport[4]) {
  glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
  glUseProgram(renderer->compositeProgram);
  glBindVertexArray(renderer->vao);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glActiveTexture(GL_TEXTURE0);
  glBindSampler(0, 0);
  glBindTexture(GL_TEXTURE_2D, texture);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glBlendEquation(GL_FUNC_ADD);
  glDrawArrays(GL_TRIANGLES, renderer->quadFirst, 6);
}

static void heldGripTransform(Mat4 model, int hand, const PlayerModelPose* pose, float aspect) {
  float fit = fminf(1, aspect / 1.3f);
  PlayerModelSwing attack = playerModelSwing(pose->punch);
  PlayerModelSwing mainPlacement = playerModelSwing(pose->placeMain);
  PlayerModelSwing offhandPlacement = playerModelSwing(pose->placeOffhand);
  float bob = sinf((float)pose->gaitPhase) * pose->gaitWeight * 0.018f;
  float side = hand ? -1.0f : 1.0f;
  PlayerModelSwing strike = hand ? (PlayerModelSwing){0} : attack;
  PlayerModelSwing placement = hand ? offhandPlacement : mainPlacement;
  mat4_identity(model);
  translate(model, (Vec3){side * (0.42f * aspect - (0.18f * strike.reach + 0.10f * placement.reach) * fit), -0.25f + bob + 0.12f * strike.lift - 0.10f * placement.arc,
                          -1.2f - 0.18f * strike.arc - 0.20f * placement.arc});
  rotate(model, 0, 0.20f - 0.48f * strike.arc + 0.35f * placement.arc);
  rotate(model, 1, side * (-0.55f + 0.35f * strike.reach + 0.15f * placement.reach));
  rotate(model, 2, side * (-0.12f - 0.30f * strike.roll + 0.12f * placement.roll));
  scale(model, (Vec3){0.5f * fit, 0.5f * fit, 0.5f * fit});
}

static void heldArmTransform(Mat4 arm, const Mat4 grip, int hand) {
  memcpy(arm, grip, sizeof(Mat4));
  // Turn the body arm around so its wrist reaches the item while its shoulder
  // continues down toward the corresponding lower screen corner. The offset is
  // in the same grip space as the item, keeping both locked through animation.
  // The palm meets the lower front corner, leaving the forearm exposed below
  // the item throughout its strike and placement arcs.
  translate(arm, (Vec3){hand ? -0.28f : 0.28f, -0.48f, 0.50f});
  rotate(arm, 0, 3.141592654f);
  rotate(arm, 2, hand ? -0.18f : 0.18f);
  const PlayerPartSpec* spec = playerModelPartSpec(hand ? PLAYER_MODEL_LEFT_ARM : PLAYER_MODEL_RIGHT_ARM);
  translate(arm, (Vec3){-spec->centerOffset.x, -spec->centerOffset.y + spec->size.y * 0.5f, -spec->centerOffset.z});
}

bool renderHeldItems(ItemRenderer* renderer, const PlayerRenderer* playerRenderer, ItemStack mainHand, ItemStack offhand, const PlayerModelPose* pose, float aspect,
                     const DayNightState* daylight) {
  if (!hasItem(mainHand) && !hasItem(offhand))
    return true;
  if (!ready(renderer) || !playerRenderer || !playerRenderer->program || !playerRenderer->texture || !playerRenderer->vao || !playerRenderer->vbo || !pose || !daylight ||
      !isfinite(aspect) || aspect <= 0)
    return false;
  ItemRenderState state;
  saveState(&state);
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  if (!resizeTarget(&renderer->heldTarget, viewport[2], viewport[3])) {
    restoreState(&state);
    return false;
  }
  clearTarget(&renderer->heldTarget);
  Mat4 projection;
  mat4_perspective(projection, 70, aspect, 0.05f, 10);
  // Establish the same private-pass raster state before the first arm as the
  // item draws use. The arm helper preserves this state while swapping shaders,
  // so caller alpha/stencil/logic/sRGB settings cannot affect either hand.
  beginItems(renderer, projection, daylight);
  ItemStack hands[] = {mainHand, offhand};
  for (int hand = 0; hand < 2; hand++) {
    if (!hasItem(hands[hand]))
      continue;
    Mat4 grip, arm;
    heldGripTransform(grip, hand, pose, aspect);
    heldArmTransform(arm, grip, hand);
    renderPlayerViewArm(playerRenderer, hand ? PLAYER_MODEL_LEFT_ARM : PLAYER_MODEL_RIGHT_ARM, projection, arm, daylight, false);
    beginItems(renderer, projection, daylight);
    drawModel(renderer, hands[hand].item, grip);
  }
  // Fractional sleeve texels blend only after every opaque arm/item has depth.
  if (playerRenderer->fractionalAlpha)
    for (int hand = 0; hand < 2; hand++) {
      if (!hasItem(hands[hand]))
        continue;
      Mat4 grip, arm;
      heldGripTransform(grip, hand, pose, aspect);
      heldArmTransform(arm, grip, hand);
      renderPlayerViewArm(playerRenderer, hand ? PLAYER_MODEL_LEFT_ARM : PLAYER_MODEL_RIGHT_ARM, projection, arm, daylight, true);
    }
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)state.drawFramebuffer);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)state.readFramebuffer);
  compositeHeld(renderer, renderer->heldTarget.color, viewport);
  bool success = glGetError() == GL_NO_ERROR;
  restoreState(&state);
  return success;
}
