#include "player_renderer.h"
#include "shader.h"
#include "../../libs/stb_image.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

enum { VERTICES_PER_FACE = 6, FACES_PER_CUBE = 6, VERTICES_PER_CUBE = VERTICES_PER_FACE * FACES_PER_CUBE, FLOATS_PER_VERTEX = 8 };

typedef struct {
  float position[3];
  float normal[3];
  float uv[2];
} PlayerVertex;

typedef struct {
  GLint program;
  GLint vao;
  GLint activeTexture;
  GLint texture0;
  GLint sampler0;
  GLint depthFunc;
  GLint cullFace;
  GLint frontFace;
  GLint polygonMode[2];
  GLint blendSrcRGB, blendDstRGB, blendSrcAlpha, blendDstAlpha;
  GLint blendEquationRGB, blendEquationAlpha;
  GLfloat polygonOffsetFactor, polygonOffsetUnits;
  GLboolean depthTest, depthMask, blend, cull, polygonOffsetFill;
} PlayerRenderState;

static const float faceCorners[PLAYER_MODEL_FACE_COUNT][4][3] = {
    [PLAYER_MODEL_FACE_RIGHT] = {{0.5f, 0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}},
    [PLAYER_MODEL_FACE_LEFT] = {{-0.5f, 0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}},
    [PLAYER_MODEL_FACE_TOP] = {{0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, 0.5f}},
    [PLAYER_MODEL_FACE_BOTTOM] = {{0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, -0.5f}},
    [PLAYER_MODEL_FACE_FRONT] = {{0.5f, 0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}},
    [PLAYER_MODEL_FACE_BACK] = {{-0.5f, 0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}},
};

static const float faceNormals[PLAYER_MODEL_FACE_COUNT][3] = {
    [PLAYER_MODEL_FACE_RIGHT] = {1, 0, 0},   [PLAYER_MODEL_FACE_LEFT] = {-1, 0, 0},  [PLAYER_MODEL_FACE_TOP] = {0, 1, 0},
    [PLAYER_MODEL_FACE_BOTTOM] = {0, -1, 0}, [PLAYER_MODEL_FACE_FRONT] = {0, 0, -1}, [PLAYER_MODEL_FACE_BACK] = {0, 0, 1},
};

static int cubeFirstVertex(PlayerModelPart part, PlayerSkinLayer layer) {
  return ((int)part * PLAYER_SKIN_LAYER_COUNT + (int)layer) * VERTICES_PER_CUBE;
}

static void emitFace(PlayerVertex* vertices, size_t* count, PlayerModelFace face, const PlayerSkinRect* rect) {
  const int corners[] = {0, 1, 2, 0, 2, 3};
  const float u0 = rect->x / (float)PLAYER_SKIN_SIZE;
  const float v0 = rect->y / (float)PLAYER_SKIN_SIZE;
  const float u1 = (rect->x + rect->width) / (float)PLAYER_SKIN_SIZE;
  const float v1 = (rect->y + rect->height) / (float)PLAYER_SKIN_SIZE;
  const float uv[4][2] = {{u0, v0}, {u0, v1}, {u1, v1}, {u1, v0}};
  for (int vertex = 0; vertex < VERTICES_PER_FACE; vertex++) {
    int corner = corners[vertex];
    PlayerVertex* out = &vertices[(*count)++];
    memcpy(out->position, faceCorners[face][corner], sizeof(out->position));
    memcpy(out->normal, faceNormals[face], sizeof(out->normal));
    memcpy(out->uv, uv[corner], sizeof(out->uv));
    // The skin net folds the bottom back toward the front; its V direction is
    // opposite the side quads. Keep the rectangle and horizontal order intact.
    if (face == PLAYER_MODEL_FACE_BOTTOM)
      out->uv[1] = v0 + v1 - out->uv[1];
  }
}

static bool buildVertices(PlayerVertex* vertices) {
  size_t count = 0;
  for (int part = 0; part < PLAYER_MODEL_PART_COUNT; part++) {
    if (!playerModelPartSpec((PlayerModelPart)part))
      return false;
    for (int layer = 0; layer < PLAYER_SKIN_LAYER_COUNT; layer++)
      for (int face = 0; face < PLAYER_MODEL_FACE_COUNT; face++) {
        const PlayerSkinRect* rect = playerModelSkinRect((PlayerModelPart)part, (PlayerSkinLayer)layer, (PlayerModelFace)face);
        if (!rect || !rect->width || !rect->height || rect->x + rect->width > PLAYER_SKIN_SIZE || rect->y + rect->height > PLAYER_SKIN_SIZE)
          return false;
        emitFace(vertices, &count, (PlayerModelFace)face, rect);
      }
  }
  return count == (size_t)PLAYER_MODEL_PART_COUNT * PLAYER_SKIN_LAYER_COUNT * VERTICES_PER_CUBE;
}

static void snapshotRenderState(PlayerRenderState* state) {
  glGetIntegerv(GL_CURRENT_PROGRAM, &state->program);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state->vao);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &state->activeTexture);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &state->texture0);
  glGetIntegerv(GL_SAMPLER_BINDING, &state->sampler0);
  glGetIntegerv(GL_DEPTH_FUNC, &state->depthFunc);
  glGetIntegerv(GL_CULL_FACE_MODE, &state->cullFace);
  glGetIntegerv(GL_FRONT_FACE, &state->frontFace);
  glGetIntegerv(GL_POLYGON_MODE, state->polygonMode);
  glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &state->polygonOffsetFactor);
  glGetFloatv(GL_POLYGON_OFFSET_UNITS, &state->polygonOffsetUnits);
  glGetIntegerv(GL_BLEND_SRC_RGB, &state->blendSrcRGB);
  glGetIntegerv(GL_BLEND_DST_RGB, &state->blendDstRGB);
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &state->blendSrcAlpha);
  glGetIntegerv(GL_BLEND_DST_ALPHA, &state->blendDstAlpha);
  glGetIntegerv(GL_BLEND_EQUATION_RGB, &state->blendEquationRGB);
  glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &state->blendEquationAlpha);
  state->depthTest = glIsEnabled(GL_DEPTH_TEST);
  state->blend = glIsEnabled(GL_BLEND);
  state->cull = glIsEnabled(GL_CULL_FACE);
  state->polygonOffsetFill = glIsEnabled(GL_POLYGON_OFFSET_FILL);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &state->depthMask);
}

static void restoreRenderState(const PlayerRenderState* state) {
  glUseProgram((GLuint)state->program);
  glBindVertexArray((GLuint)state->vao);
  glDepthFunc((GLenum)state->depthFunc);
  glDepthMask(state->depthMask);
  glCullFace((GLenum)state->cullFace);
  glFrontFace((GLenum)state->frontFace);
  glBlendFuncSeparate((GLenum)state->blendSrcRGB, (GLenum)state->blendDstRGB, (GLenum)state->blendSrcAlpha, (GLenum)state->blendDstAlpha);
  glBlendEquationSeparate((GLenum)state->blendEquationRGB, (GLenum)state->blendEquationAlpha);
  glPolygonMode(GL_FRONT, (GLenum)state->polygonMode[0]);
  glPolygonMode(GL_BACK, (GLenum)state->polygonMode[1]);
  glPolygonOffset(state->polygonOffsetFactor, state->polygonOffsetUnits);
  if (state->polygonOffsetFill)
    glEnable(GL_POLYGON_OFFSET_FILL);
  else
    glDisable(GL_POLYGON_OFFSET_FILL);
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

static void setLighting(const PlayerRenderer* renderer, const DayNightState* daylight) {
  glUniform3f(renderer->lightDirectionLocation, daylight->lightDirection.x, daylight->lightDirection.y, daylight->lightDirection.z);
  glUniform3f(renderer->lightColorLocation, daylight->lightColor.x, daylight->lightColor.y, daylight->lightColor.z);
  glUniform3f(renderer->skyColorLocation, daylight->skyFill.x, daylight->skyFill.y, daylight->skyFill.z);
  glUniform3f(renderer->groundColorLocation, daylight->groundFill.x, daylight->groundFill.y, daylight->groundFill.z);
}

static bool validUniforms(const PlayerRenderer* renderer) {
  const GLint locations[] = {renderer->viewProjectionLocation,     renderer->feetLocation,           renderer->rootYawLocation,        renderer->rootScaleLocation,
                             renderer->partSizeLocation,           renderer->partPivotLocation,      renderer->partCenterLocation,     renderer->partTranslationLocation,
                             renderer->partRotationLocation,       renderer->partScaleLocation,      renderer->shellInflationLocation, renderer->viewModelLocation,
                             renderer->viewModelTransformLocation, renderer->lightDirectionLocation, renderer->lightColorLocation,     renderer->skyColorLocation,
                             renderer->groundColorLocation,        renderer->outerLayerLocation,     renderer->outerPassLocation,      renderer->skinLocation};
  for (size_t i = 0; i < sizeof(locations) / sizeof(*locations); i++)
    if (locations[i] < 0)
      return false;
  return true;
}

bool initPlayerRenderer(PlayerRenderer* renderer, const char* skinPath) {
  if (!renderer || !skinPath)
    return false;
  *renderer = (PlayerRenderer){0};

  int width, height, channels;
  if (!stbi_info(skinPath, &width, &height, &channels) || width != PLAYER_SKIN_SIZE || height != PLAYER_SKIN_SIZE || channels != 4) {
    fprintf(stderr, "Player skin must be a 64x64 RGBA image: %s\n", skinPath);
    return false;
  }
  unsigned char* pixels = stbi_load(skinPath, &width, &height, &channels, STBI_rgb_alpha);
  if (!pixels || width != PLAYER_SKIN_SIZE || height != PLAYER_SKIN_SIZE) {
    fprintf(stderr, "Failed to decode player skin: %s\n", skinPath);
    stbi_image_free(pixels);
    return false;
  }

  PlayerVertex vertices[PLAYER_MODEL_PART_COUNT * PLAYER_SKIN_LAYER_COUNT * VERTICES_PER_CUBE];
  if (!buildVertices(vertices)) {
    fprintf(stderr, "Invalid player model skin mapping\n");
    stbi_image_free(pixels);
    return false;
  }

  GLint previousProgram, previousVAO, previousBuffer, previousActive, previousTexture;
  GLint previousUnpackBuffer, previousUnpackAlignment, previousUnpackRowLength, previousUnpackSkipRows, previousUnpackSkipPixels;
  glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousBuffer);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActive);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
  glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &previousUnpackBuffer);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
  glGetIntegerv(GL_UNPACK_ROW_LENGTH, &previousUnpackRowLength);
  glGetIntegerv(GL_UNPACK_SKIP_ROWS, &previousUnpackSkipRows);
  glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &previousUnpackSkipPixels);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);

  bool ready = false;
  renderer->program = loadShaders("assets/shaders/player_vertex.glsl", "assets/shaders/player_fragment.glsl");
  if (!renderer->program)
    goto restore;

  glGenTextures(1, &renderer->texture);
  glBindTexture(GL_TEXTURE_2D, renderer->texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, PLAYER_SKIN_SIZE, PLAYER_SKIN_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

  glGenVertexArrays(1, &renderer->vao);
  glGenBuffers(1, &renderer->vbo);
  glBindVertexArray(renderer->vao);
  glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PlayerVertex), (void*)offsetof(PlayerVertex, position));
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(PlayerVertex), (void*)offsetof(PlayerVertex, normal));
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(PlayerVertex), (void*)offsetof(PlayerVertex, uv));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);

  glUseProgram(renderer->program);
#define PLAYER_UNIFORM(field, name) renderer->field = glGetUniformLocation(renderer->program, name)
  PLAYER_UNIFORM(viewProjectionLocation, "viewProjection");
  PLAYER_UNIFORM(feetLocation, "feet");
  PLAYER_UNIFORM(rootYawLocation, "rootYaw");
  PLAYER_UNIFORM(rootScaleLocation, "rootScale");
  PLAYER_UNIFORM(partSizeLocation, "partSize");
  PLAYER_UNIFORM(partPivotLocation, "partPivot");
  PLAYER_UNIFORM(partCenterLocation, "partCenter");
  PLAYER_UNIFORM(partTranslationLocation, "partTranslation");
  PLAYER_UNIFORM(partRotationLocation, "partRotation");
  PLAYER_UNIFORM(partScaleLocation, "partScale");
  PLAYER_UNIFORM(shellInflationLocation, "shellInflation");
  PLAYER_UNIFORM(viewModelLocation, "viewModel");
  PLAYER_UNIFORM(viewModelTransformLocation, "viewModelTransform");
  PLAYER_UNIFORM(lightDirectionLocation, "lightDirection");
  PLAYER_UNIFORM(lightColorLocation, "lightColor");
  PLAYER_UNIFORM(skyColorLocation, "skyColor");
  PLAYER_UNIFORM(groundColorLocation, "groundColor");
  PLAYER_UNIFORM(outerLayerLocation, "outerLayer");
  PLAYER_UNIFORM(outerPassLocation, "outerPass");
  PLAYER_UNIFORM(skinLocation, "skin");
#undef PLAYER_UNIFORM
  glUniform1i(renderer->skinLocation, 0);
  renderer->fractionalAlpha = false;
  for (size_t pixel = 0; pixel < (size_t)PLAYER_SKIN_SIZE * PLAYER_SKIN_SIZE; pixel++) {
    unsigned char alpha = pixels[pixel * 4 + 3];
    if (alpha && alpha != 255) {
      renderer->fractionalAlpha = true;
      break;
    }
  }
  ready = renderer->program && renderer->texture && renderer->vao && renderer->vbo && validUniforms(renderer) && glGetError() == GL_NO_ERROR;

restore:
  stbi_image_free(pixels);
  glUseProgram((GLuint)previousProgram);
  glBindVertexArray((GLuint)previousVAO);
  glBindBuffer(GL_ARRAY_BUFFER, (GLuint)previousBuffer);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)previousUnpackBuffer);
  glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, previousUnpackRowLength);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, previousUnpackSkipRows);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, previousUnpackSkipPixels);
  glBindTexture(GL_TEXTURE_2D, (GLuint)previousTexture);
  glActiveTexture((GLenum)previousActive);
  if (!ready) {
    glDeleteBuffers(1, &renderer->vbo);
    glDeleteVertexArrays(1, &renderer->vao);
    glDeleteTextures(1, &renderer->texture);
    if (renderer->program)
      glDeleteProgram(renderer->program);
    *renderer = (PlayerRenderer){0};
    fprintf(stderr, "Failed to initialize player rendering\n");
  }
  return ready;
}

void cleanupPlayerRenderer(PlayerRenderer* renderer) {
  if (!renderer)
    return;
  glDeleteBuffers(1, &renderer->vbo);
  glDeleteVertexArrays(1, &renderer->vao);
  glDeleteTextures(1, &renderer->texture);
  if (renderer->program)
    glDeleteProgram(renderer->program);
  *renderer = (PlayerRenderer){0};
}

static void setPart(const PlayerRenderer* renderer, PlayerModelPart part, const PlayerModelPose* pose, PlayerSkinLayer layer, bool viewModel, int outerPass) {
  const PlayerPartSpec* spec = playerModelPartSpec(part);
  const PlayerPartPose* partPose = &pose->parts[part];
  // Classic joints overlap on coplanar front/back faces. Give those seams a
  // stable per-part order in depth-buffer units without moving the geometry.
  // No slope factor: the bias stays bounded as the camera orbits the player.
  if (!viewModel)
    glPolygonOffset(0, -8.0f * (part + 1));
  glUniform3f(renderer->partSizeLocation, spec->size.x, spec->size.y, spec->size.z);
  glUniform3f(renderer->partPivotLocation, spec->pivot.x, spec->pivot.y, spec->pivot.z);
  glUniform3f(renderer->partCenterLocation, spec->centerOffset.x, spec->centerOffset.y, spec->centerOffset.z);
  glUniform3f(renderer->partTranslationLocation, partPose->translation.x, partPose->translation.y, partPose->translation.z);
  glUniform3f(renderer->partRotationLocation, partPose->rotation.x, partPose->rotation.y, partPose->rotation.z);
  glUniform3f(renderer->partScaleLocation, partPose->scale.x, partPose->scale.y, partPose->scale.z);
  glUniform1f(renderer->shellInflationLocation, layer == PLAYER_SKIN_OUTER ? playerModelOuterInflation(part) : 0);
  glUniform1i(renderer->outerLayerLocation, layer == PLAYER_SKIN_OUTER);
  glUniform1i(renderer->outerPassLocation, outerPass);
  glUniform1i(renderer->viewModelLocation, viewModel);
}

static void drawPart(const PlayerRenderer* renderer, PlayerModelPart part, const PlayerModelPose* pose, PlayerSkinLayer layer, bool viewModel, int outerPass) {
  setPart(renderer, part, pose, layer, viewModel, outerPass);
  glDrawArrays(GL_TRIANGLES, cubeFirstVertex(part, layer), VERTICES_PER_CUBE);
}

static bool rendererReady(const PlayerRenderer* renderer, const PlayerModelPose* pose, const DayNightState* daylight) {
  return renderer && renderer->program && renderer->texture && renderer->vao && renderer->vbo && pose && daylight;
}

static void beginPlayerDraw(const PlayerRenderer* renderer, const DayNightState* daylight) {
  glUseProgram(renderer->program);
  glBindVertexArray(renderer->vao);
  glActiveTexture(GL_TEXTURE0);
  glBindSampler(0, 0);
  glBindTexture(GL_TEXTURE_2D, renderer->texture);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  setLighting(renderer, daylight);
}

void renderPlayerModel(const PlayerRenderer* renderer, Vec3 feet, const PlayerModelPose* pose, const Mat4 view, const Mat4 projection, const DayNightState* daylight) {
  if (!rendererReady(renderer, pose, daylight))
    return;
  PlayerRenderState state;
  snapshotRenderState(&state);
  beginPlayerDraw(renderer, daylight);

  Mat4 viewProjection;
  mat4_multiply(viewProjection, projection, view);
  glUniformMatrix4fv(renderer->viewProjectionLocation, 1, GL_FALSE, viewProjection);
  glUniform3f(renderer->feetLocation, feet.x, feet.y, feet.z);
  glUniform1f(renderer->rootYawLocation, pose->rootYaw);
  glUniform1f(renderer->rootScaleLocation, pose->rootScale);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  for (int part = 0; part < PLAYER_MODEL_PART_COUNT; part++)
    drawPart(renderer, (PlayerModelPart)part, pose, PLAYER_SKIN_BASE, false, 0);

  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  for (int part = 0; part < PLAYER_MODEL_PART_COUNT; part++)
    drawPart(renderer, (PlayerModelPart)part, pose, PLAYER_SKIN_OUTER, false, 1);

  if (renderer->fractionalAlpha) {
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glDepthMask(GL_FALSE);
    for (int part = 0; part < PLAYER_MODEL_PART_COUNT; part++)
      drawPart(renderer, (PlayerModelPart)part, pose, PLAYER_SKIN_OUTER, false, 2);
  }

  glDepthMask(GL_FALSE);

  restoreRenderState(&state);
}

void renderPlayerHand(const PlayerRenderer* renderer, const PlayerModelPose* pose, float aspect, const DayNightState* daylight) {
  if (!rendererReady(renderer, pose, daylight) || !isfinite(aspect) || aspect <= 0)
    return;
  PlayerRenderState state;
  snapshotRenderState(&state);
  beginPlayerDraw(renderer, daylight);

  Mat4 projection;
  mat4_perspective(projection, 70.0f, aspect, 0.05f, 10.0f);
  glUniformMatrix4fv(renderer->viewProjectionLocation, 1, GL_FALSE, projection);
  glUniform3f(renderer->feetLocation, 0, 0, 0);
  glUniform1f(renderer->rootYawLocation, 0);
  glUniform1f(renderer->rootScaleLocation, 1);
  Mat4 handTransform;
  playerModelHandTransform(handTransform, pose, aspect);
  glUniformMatrix4fv(renderer->viewModelTransformLocation, 1, GL_FALSE, handTransform);
  // The foreground arm shares its mesh/skin with the body but has an independent
  // bare-hand swing. Its shoulder extends offscreen, leaving the fist in view.
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_BLEND);
  drawPart(renderer, PLAYER_MODEL_RIGHT_ARM, pose, PLAYER_SKIN_BASE, true, 0);
  drawPart(renderer, PLAYER_MODEL_RIGHT_ARM, pose, PLAYER_SKIN_OUTER, true, 1);
  if (renderer->fractionalAlpha) {
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    drawPart(renderer, PLAYER_MODEL_RIGHT_ARM, pose, PLAYER_SKIN_OUTER, true, 2);
  }

  restoreRenderState(&state);
}
