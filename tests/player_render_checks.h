#ifndef PLAYER_RENDER_CHECKS_H
#define PLAYER_RENDER_CHECKS_H

#include "graphics/player_renderer.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PLAYER_CHECK_WIDTH = 960, PLAYER_CHECK_HEIGHT = 540 };

typedef struct {
  unsigned char r, g, b, a;
} PlayerCheckColor;

typedef struct {
  GLint program, vao, arrayBuffer, activeTexture, texture0, texture3;
  GLint sampler0, pixelUnpackBuffer;
  GLint unpackAlignment, unpackRowLength, unpackSkipRows, unpackSkipPixels;
  GLint depthFunc, cullFace, frontFace, polygonMode[2];
  GLint blendSrcRGB, blendDstRGB, blendSrcAlpha, blendDstAlpha;
  GLint blendEquationRGB, blendEquationAlpha;
  GLboolean depthTest, depthMask, blend, cull;
  GLdouble depthRange[2];
} PlayerCheckGLState;

typedef struct {
  GLuint vao, buffer, textures[2], sampler;
} PlayerCheckSentinels;

static void playerCheckCaptureState(PlayerCheckGLState* state) {
  glGetIntegerv(GL_CURRENT_PROGRAM, &state->program);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state->vao);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &state->arrayBuffer);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &state->activeTexture);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &state->texture0);
  glGetIntegerv(GL_SAMPLER_BINDING, &state->sampler0);
  glActiveTexture(GL_TEXTURE3);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &state->texture3);
  glActiveTexture((GLenum)state->activeTexture);
  glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &state->pixelUnpackBuffer);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &state->unpackAlignment);
  glGetIntegerv(GL_UNPACK_ROW_LENGTH, &state->unpackRowLength);
  glGetIntegerv(GL_UNPACK_SKIP_ROWS, &state->unpackSkipRows);
  glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &state->unpackSkipPixels);
  glGetIntegerv(GL_DEPTH_FUNC, &state->depthFunc);
  glGetIntegerv(GL_CULL_FACE_MODE, &state->cullFace);
  glGetIntegerv(GL_FRONT_FACE, &state->frontFace);
  glGetIntegerv(GL_POLYGON_MODE, state->polygonMode);
  glGetIntegerv(GL_BLEND_SRC_RGB, &state->blendSrcRGB);
  glGetIntegerv(GL_BLEND_DST_RGB, &state->blendDstRGB);
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &state->blendSrcAlpha);
  glGetIntegerv(GL_BLEND_DST_ALPHA, &state->blendDstAlpha);
  glGetIntegerv(GL_BLEND_EQUATION_RGB, &state->blendEquationRGB);
  glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &state->blendEquationAlpha);
  state->depthTest = glIsEnabled(GL_DEPTH_TEST);
  state->blend = glIsEnabled(GL_BLEND);
  state->cull = glIsEnabled(GL_CULL_FACE);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &state->depthMask);
  glGetDoublev(GL_DEPTH_RANGE, state->depthRange);
}

static bool playerCheckStateEqual(const PlayerCheckGLState* left, const PlayerCheckGLState* right) {
  return left->program == right->program && left->vao == right->vao && left->arrayBuffer == right->arrayBuffer && left->activeTexture == right->activeTexture &&
         left->texture0 == right->texture0 && left->texture3 == right->texture3 && left->sampler0 == right->sampler0 && left->pixelUnpackBuffer == right->pixelUnpackBuffer &&
         left->unpackAlignment == right->unpackAlignment && left->unpackRowLength == right->unpackRowLength && left->unpackSkipRows == right->unpackSkipRows &&
         left->unpackSkipPixels == right->unpackSkipPixels && left->depthFunc == right->depthFunc && left->cullFace == right->cullFace && left->frontFace == right->frontFace &&
         left->polygonMode[0] == right->polygonMode[0] && left->polygonMode[1] == right->polygonMode[1] && left->blendSrcRGB == right->blendSrcRGB &&
         left->blendDstRGB == right->blendDstRGB && left->blendSrcAlpha == right->blendSrcAlpha && left->blendDstAlpha == right->blendDstAlpha &&
         left->blendEquationRGB == right->blendEquationRGB && left->blendEquationAlpha == right->blendEquationAlpha && left->depthTest == right->depthTest &&
         left->depthMask == right->depthMask && left->blend == right->blend && left->cull == right->cull && fabs(left->depthRange[0] - right->depthRange[0]) < 1e-12 &&
         fabs(left->depthRange[1] - right->depthRange[1]) < 1e-12;
}

static void playerCheckRestoreState(const PlayerCheckGLState* state) {
  glUseProgram((GLuint)state->program);
  glBindVertexArray((GLuint)state->vao);
  glBindBuffer(GL_ARRAY_BUFFER, (GLuint)state->arrayBuffer);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)state->pixelUnpackBuffer);
  glPixelStorei(GL_UNPACK_ALIGNMENT, state->unpackAlignment);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, state->unpackRowLength);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, state->unpackSkipRows);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, state->unpackSkipPixels);
  glActiveTexture(GL_TEXTURE0);
  glBindSampler(0, (GLuint)state->sampler0);
  glBindTexture(GL_TEXTURE_2D, (GLuint)state->texture0);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, (GLuint)state->texture3);
  glActiveTexture((GLenum)state->activeTexture);
  glDepthFunc((GLenum)state->depthFunc);
  glDepthMask(state->depthMask);
  glDepthRange(state->depthRange[0], state->depthRange[1]);
  glCullFace((GLenum)state->cullFace);
  glFrontFace((GLenum)state->frontFace);
  glBlendFuncSeparate((GLenum)state->blendSrcRGB, (GLenum)state->blendDstRGB, (GLenum)state->blendSrcAlpha, (GLenum)state->blendDstAlpha);
  glBlendEquationSeparate((GLenum)state->blendEquationRGB, (GLenum)state->blendEquationAlpha);
  glPolygonMode(GL_FRONT, (GLenum)state->polygonMode[0]);
  glPolygonMode(GL_BACK, (GLenum)state->polygonMode[1]);
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
}

static bool playerCheckSetSentinels(GLuint shader, PlayerCheckSentinels* sentinels, PlayerCheckGLState* state) {
  memset(sentinels, 0, sizeof(*sentinels));
  glGenVertexArrays(1, &sentinels->vao);
  glGenBuffers(1, &sentinels->buffer);
  glGenTextures(2, sentinels->textures);
  glGenSamplers(1, &sentinels->sampler);
  if (!sentinels->vao || !sentinels->buffer || !sentinels->textures[0] || !sentinels->textures[1] || !sentinels->sampler)
    return false;
  glUseProgram(shader);
  glBindVertexArray(sentinels->vao);
  glBindBuffer(GL_ARRAY_BUFFER, sentinels->buffer);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sentinels->buffer);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 71);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, 2);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, 3);
  glActiveTexture(GL_TEXTURE0);
  glSamplerParameteri(sentinels->sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glSamplerParameteri(sentinels->sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glSamplerParameteri(sentinels->sampler, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
  glSamplerParameteri(sentinels->sampler, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
  glBindSampler(0, sentinels->sampler);
  glBindTexture(GL_TEXTURE_2D, sentinels->textures[0]);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, sentinels->textures[1]);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDepthFunc(GL_GREATER);
  glDepthRange(0.2, 0.8);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_DST_ALPHA, GL_SRC_ALPHA);
  glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_SUBTRACT);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);
  glFrontFace(GL_CW);
  glPolygonMode(GL_FRONT, GL_LINE);
  glPolygonMode(GL_BACK, GL_POINT);
  playerCheckCaptureState(state);
  return glGetError() == GL_NO_ERROR;
}

static void playerCheckDeleteSentinels(const PlayerCheckSentinels* sentinels) {
  glDeleteSamplers(1, &sentinels->sampler);
  glDeleteTextures(2, sentinels->textures);
  glDeleteBuffers(1, &sentinels->buffer);
  glDeleteVertexArrays(1, &sentinels->vao);
}

static void playerCheckNeutralPose(PlayerModelPose* pose) {
  *pose = (PlayerModelPose){.rootScale = 1};
  for (int part = 0; part < PLAYER_MODEL_PART_COUNT; part++)
    pose->parts[part].scale = (Vec3){1, 1, 1};
}

static DayNightState playerCheckWhiteLight(void) {
  return (DayNightState){.lightDirection = {0, 1, 0}, .skyFill = {1, 1, 1}, .groundFill = {1, 1, 1}};
}

static bool playerCheckTextureUpload(const PlayerRenderer* renderer) {
  GLint previousActive, previousTexture;
  glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActive);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
  glBindTexture(GL_TEXTURE_2D, renderer->texture);
  GLint width = 0, height = 0, format = 0, minFilter = 0, magFilter = 0, wrapS = 0, wrapT = 0;
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
  glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &minFilter);
  glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &magFilter);
  glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrapS);
  glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &wrapT);
  unsigned char pixels[PLAYER_SKIN_SIZE * PLAYER_SKIN_SIZE * 4];
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  glBindTexture(GL_TEXTURE_2D, (GLuint)previousTexture);
  glActiveTexture((GLenum)previousActive);
  if (width != PLAYER_SKIN_SIZE || height != PLAYER_SKIN_SIZE || format != GL_RGBA8 || minFilter != GL_NEAREST || magFilter != GL_NEAREST || wrapS != GL_CLAMP_TO_EDGE ||
      wrapT != GL_CLAMP_TO_EDGE || glGetError() != GL_NO_ERROR)
    return false;

  static const struct {
    int x, y;
    PlayerCheckColor color;
  } witnesses[] = {
      {10, 10, {187, 140, 71, 255}}, {22, 20, {225, 207, 184, 255}}, {44, 24, {255, 240, 222, 255}}, {36, 52, {255, 240, 222, 255}},
      {4, 20, {36, 165, 36, 255}},   {20, 52, {36, 165, 36, 255}},   {40, 8, {58, 53, 46, 255}},     {44, 36, {174, 126, 68, 255}},
      {42, 10, {0, 0, 0, 0}},        {44, 40, {0, 0, 0, 0}},         {52, 52, {0, 0, 0, 0}},         {4, 52, {0, 0, 0, 0}},
  };

  for (size_t sample = 0; sample < sizeof(witnesses) / sizeof(*witnesses); sample++) {
    const unsigned char* actual = pixels + ((size_t)witnesses[sample].y * PLAYER_SKIN_SIZE + witnesses[sample].x) * 4;
    const PlayerCheckColor* expected = &witnesses[sample].color;
    if (actual[0] != expected->r || actual[1] != expected->g || actual[2] != expected->b || actual[3] != expected->a)
      return false;
  }
  return true;
}

static void playerCheckFillRect(unsigned char pixels[PLAYER_SKIN_SIZE * PLAYER_SKIN_SIZE * 4], int x, int y, int width, int height, PlayerCheckColor color) {
  for (int row = y; row < y + height; row++)
    for (int column = x; column < x + width; column++) {
      unsigned char* pixel = pixels + ((size_t)row * PLAYER_SKIN_SIZE + column) * 4;
      pixel[0] = color.r;
      pixel[1] = color.g;
      pixel[2] = color.b;
      pixel[3] = color.a;
    }
}

static bool playerCheckWriteTga(const char* path, int channels, const unsigned char pixels[PLAYER_SKIN_SIZE * PLAYER_SKIN_SIZE * 4]) {
  unsigned char header[18] = {0};
  header[2] = 2;
  header[12] = PLAYER_SKIN_SIZE;
  header[14] = PLAYER_SKIN_SIZE;
  header[16] = (unsigned char)(channels * 8);
  header[17] = (unsigned char)(0x20 | (channels == 4 ? 8 : 0));
  FILE* file = fopen(path, "wb");
  if (!file)
    return false;
  bool ok = fwrite(header, 1, sizeof(header), file) == sizeof(header);
  for (int y = 0; y < PLAYER_SKIN_SIZE && ok; y++)
    for (int x = 0; x < PLAYER_SKIN_SIZE && ok; x++) {
      const unsigned char* rgba = pixels + ((size_t)y * PLAYER_SKIN_SIZE + x) * 4;
      unsigned char bgra[4] = {rgba[2], rgba[1], rgba[0], rgba[3]};
      ok = fwrite(bgra, 1, (size_t)channels, file) == (size_t)channels;
    }
  if (fclose(file) != 0)
    ok = false;
  return ok;
}

static bool playerCheckMakeRgbSkin(const char* path) {
  unsigned char pixels[PLAYER_SKIN_SIZE * PLAYER_SKIN_SIZE * 4];
  for (size_t i = 0; i < sizeof(pixels); i += 4) {
    pixels[i] = 70;
    pixels[i + 1] = 120;
    pixels[i + 2] = 180;
    pixels[i + 3] = 255;
  }
  return playerCheckWriteTga(path, 3, pixels);
}

static bool playerCheckMakeMappedSkin(const char* path) {
  unsigned char pixels[PLAYER_SKIN_SIZE * PLAYER_SKIN_SIZE * 4] = {0};

  static const struct {
    int x, y, width, height;
    PlayerCheckColor color;
  } headFaces[] = {
      {0, 8, 8, 8, {240, 80, 20, 255}},   {16, 8, 8, 8, {20, 160, 240, 255}}, {8, 0, 8, 8, {220, 220, 40, 255}},
      {16, 0, 8, 8, {100, 50, 180, 255}}, {24, 8, 8, 8, {40, 200, 130, 255}},
  };

  // Right, left, top, bottom and back use solid colors; front gets four
  // independently colored quadrants to catch horizontal or vertical UV flips.
  for (size_t face = 0; face < sizeof(headFaces) / sizeof(*headFaces); face++)
    playerCheckFillRect(pixels, headFaces[face].x, headFaces[face].y, headFaces[face].width, headFaces[face].height, headFaces[face].color);
  playerCheckFillRect(pixels, 8, 8, 4, 4, (PlayerCheckColor){245, 20, 30, 255});
  playerCheckFillRect(pixels, 12, 8, 4, 4, (PlayerCheckColor){20, 220, 40, 255});
  playerCheckFillRect(pixels, 8, 12, 4, 4, (PlayerCheckColor){30, 40, 230, 255});
  playerCheckFillRect(pixels, 12, 12, 4, 4, (PlayerCheckColor){230, 200, 20, 255});
  playerCheckFillRect(pixels, 20, 20, 8, 12, (PlayerCheckColor){180, 60, 200, 255});
  playerCheckFillRect(pixels, 44, 20, 4, 12, (PlayerCheckColor){30, 190, 190, 255});
  playerCheckFillRect(pixels, 36, 52, 4, 12, (PlayerCheckColor){210, 110, 30, 255});
  playerCheckFillRect(pixels, 4, 20, 4, 12, (PlayerCheckColor){80, 200, 60, 255});
  playerCheckFillRect(pixels, 20, 52, 4, 12, (PlayerCheckColor){200, 60, 90, 255});
  return playerCheckWriteTga(path, 4, pixels);
}

static bool playerCheckMakeArmSkin(const char* path, unsigned char outerAlpha) {
  unsigned char pixels[PLAYER_SKIN_SIZE * PLAYER_SKIN_SIZE * 4] = {0};
  static const int base[PLAYER_MODEL_FACE_COUNT][4] = {{40, 20, 4, 12}, {48, 20, 4, 12}, {44, 16, 4, 4}, {48, 16, 4, 4}, {44, 20, 4, 12}, {52, 20, 4, 12}};
  static const int outer[PLAYER_MODEL_FACE_COUNT][4] = {{40, 36, 4, 12}, {48, 36, 4, 12}, {44, 32, 4, 4}, {48, 32, 4, 4}, {44, 36, 4, 12}, {52, 36, 4, 12}};
  for (int face = 0; face < PLAYER_MODEL_FACE_COUNT; face++) {
    playerCheckFillRect(pixels, base[face][0], base[face][1], base[face][2], base[face][3], (PlayerCheckColor){40, 80, 200, 255});
    playerCheckFillRect(pixels, outer[face][0], outer[face][1], outer[face][2], outer[face][3], (PlayerCheckColor){200, 180, 20, outerAlpha});
  }
  return playerCheckWriteTga(path, 4, pixels);
}

static bool playerCheckProject(const Mat4 matrix, Vec3 point, int* x, int* y) {
  float w = matrix[3] * point.x + matrix[7] * point.y + matrix[11] * point.z + matrix[15];
  if (w <= 0)
    return false;
  float clipX = matrix[0] * point.x + matrix[4] * point.y + matrix[8] * point.z + matrix[12];
  float clipY = matrix[1] * point.x + matrix[5] * point.y + matrix[9] * point.z + matrix[13];
  *x = (int)((clipX / w + 1) * (PLAYER_CHECK_WIDTH * 0.5f));
  *y = (int)((clipY / w + 1) * (PLAYER_CHECK_HEIGHT * 0.5f));
  return *x >= 0 && *x < PLAYER_CHECK_WIDTH && *y >= 0 && *y < PLAYER_CHECK_HEIGHT;
}

static bool playerCheckFrontTexel(PlayerModelPart part, int atlasX, int atlasY, int rectX, int rectY, int rectWidth, int rectHeight, const Mat4 matrix, PlayerCheckColor expected) {
  const PlayerPartSpec* spec = playerModelPartSpec(part);
  if (!spec)
    return false;
  float u = (atlasX + 0.5f - rectX) / rectWidth;
  float v = (atlasY + 0.5f - rectY) / rectHeight;
  Vec3 center = {spec->pivot.x + spec->centerOffset.x, spec->pivot.y + spec->centerOffset.y, spec->pivot.z + spec->centerOffset.z};
  Vec3 point = {center.x + (0.5f - u) * spec->size.x, center.y + (0.5f - v) * spec->size.y, center.z - spec->size.z * 0.5f};
  int x, y;
  if (!playerCheckProject(matrix, point, &x, &y))
    return false;
  unsigned char actual[3];
  glReadPixels(x, y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, actual);
  return abs((int)actual[0] - expected.r) <= 2 && abs((int)actual[1] - expected.g) <= 2 && abs((int)actual[2] - expected.b) <= 2;
}

static int playerCheckLitPixels(const unsigned char* pixels, int x0, int y0, int x1, int y1) {
  int lit = 0;
  for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++) {
      const unsigned char* pixel = pixels + ((size_t)y * PLAYER_CHECK_WIDTH + x) * 3;
      lit += pixel[0] || pixel[1] || pixel[2];
    }
  return lit;
}

static bool playerCheckMappedModel(const PlayerRenderer* renderer) {
  bool ok = true;
  PlayerModelPose pose;
  playerCheckNeutralPose(&pose);
  DayNightState daylight = playerCheckWhiteLight();
  Camera camera = {.position = {0, 0.9f, -4}, .up = {0, 1, 0}};
  Vec3 target = {0, 0.9f, 0};
  Mat4 view, projection, combined;
  mat4_lookAt(view, &camera.position, &target, &camera.up);
  mat4_perspective(projection, 35, (float)PLAYER_CHECK_WIDTH / PLAYER_CHECK_HEIGHT, 0.1f, 20);
  mat4_multiply(combined, projection, view);
  glViewport(0, 0, PLAYER_CHECK_WIDTH, PLAYER_CHECK_HEIGHT);
  glClearColor(0, 0, 0, 1);
  glClearDepth(1);
  glDepthMask(GL_TRUE);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  renderPlayerModel(renderer, (Vec3){0}, &pose, view, projection, &daylight);
  ok &= playerCheckFrontTexel(PLAYER_MODEL_HEAD, 9, 9, 8, 8, 8, 8, combined, (PlayerCheckColor){245, 20, 30, 255});
  ok &= playerCheckFrontTexel(PLAYER_MODEL_HEAD, 14, 9, 8, 8, 8, 8, combined, (PlayerCheckColor){20, 220, 40, 255});
  ok &= playerCheckFrontTexel(PLAYER_MODEL_HEAD, 9, 14, 8, 8, 8, 8, combined, (PlayerCheckColor){30, 40, 230, 255});
  ok &= playerCheckFrontTexel(PLAYER_MODEL_HEAD, 14, 14, 8, 8, 8, 8, combined, (PlayerCheckColor){230, 200, 20, 255});
  ok &= playerCheckFrontTexel(PLAYER_MODEL_TORSO, 24, 26, 20, 20, 8, 12, combined, (PlayerCheckColor){180, 60, 200, 255});
  ok &= playerCheckFrontTexel(PLAYER_MODEL_RIGHT_ARM, 46, 26, 44, 20, 4, 12, combined, (PlayerCheckColor){30, 190, 190, 255});
  ok &= playerCheckFrontTexel(PLAYER_MODEL_LEFT_ARM, 38, 58, 36, 52, 4, 12, combined, (PlayerCheckColor){210, 110, 30, 255});
  ok &= playerCheckFrontTexel(PLAYER_MODEL_RIGHT_LEG, 6, 26, 4, 20, 4, 12, combined, (PlayerCheckColor){80, 200, 60, 255});
  ok &= playerCheckFrontTexel(PLAYER_MODEL_LEFT_LEG, 22, 58, 20, 52, 4, 12, combined, (PlayerCheckColor){200, 60, 90, 255});

  static const struct {
    Vec3 normal, up;
    PlayerCheckColor color;
  } headViews[] = {
      {{1, 0, 0}, {0, 1, 0}, {240, 80, 20, 255}},   {{-1, 0, 0}, {0, 1, 0}, {20, 160, 240, 255}}, {{0, 1, 0}, {0, 0, -1}, {220, 220, 40, 255}},
      {{0, -1, 0}, {0, 0, 1}, {100, 50, 180, 255}}, {{0, 0, 1}, {0, 1, 0}, {40, 200, 130, 255}},
  };

  const Vec3 head = {0, 1.575f, 0};
  for (size_t face = 0; face < sizeof(headViews) / sizeof(*headViews); face++) {
    camera.position = (Vec3){head.x + headViews[face].normal.x * 2.5f, head.y + headViews[face].normal.y * 2.5f, head.z + headViews[face].normal.z * 2.5f};
    camera.up = headViews[face].up;
    mat4_lookAt(view, &camera.position, &head, &camera.up);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderPlayerModel(renderer, (Vec3){0}, &pose, view, projection, &daylight);
    unsigned char pixel[3];
    glReadPixels(PLAYER_CHECK_WIDTH / 2, PLAYER_CHECK_HEIGHT / 2, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);
    ok &= abs((int)pixel[0] - headViews[face].color.r) <= 2 && abs((int)pixel[1] - headViews[face].color.g) <= 2 && abs((int)pixel[2] - headViews[face].color.b) <= 2;
  }

  camera = (Camera){.position = {0, 0.9f, -4}, .up = {0, 1, 0}};
  mat4_lookAt(view, &camera.position, &target, &camera.up);
  size_t bytes = (size_t)PLAYER_CHECK_WIDTH * PLAYER_CHECK_HEIGHT * 3;
  unsigned char* visible = malloc(bytes);
  unsigned char* blocked = malloc(bytes);
  if (!visible || !blocked)
    ok = false;
  if (visible && blocked) {
    glClearDepth(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderPlayerModel(renderer, (Vec3){0}, &pose, view, projection, &daylight);
    glReadPixels(0, 0, PLAYER_CHECK_WIDTH, PLAYER_CHECK_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, visible);
    glClearDepth(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderPlayerModel(renderer, (Vec3){0}, &pose, view, projection, &daylight);
    glReadPixels(0, 0, PLAYER_CHECK_WIDTH, PLAYER_CHECK_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, blocked);
    ok &= playerCheckLitPixels(visible, 0, 0, PLAYER_CHECK_WIDTH, PLAYER_CHECK_HEIGHT) > 1000;
    ok &= playerCheckLitPixels(blocked, 0, 0, PLAYER_CHECK_WIDTH, PLAYER_CHECK_HEIGHT) == 0;
  }
  free(visible);
  free(blocked);
  glClearDepth(1);
  glClearColor(0, 0, 0, 0);
  return ok && glGetError() == GL_NO_ERROR;
}

static bool playerCheckBodyAlphaDepth(const PlayerRenderer* transparent, const PlayerRenderer* half, const PlayerRenderer* opaque) {
  PlayerModelPose pose;
  playerCheckNeutralPose(&pose);
  DayNightState daylight = playerCheckWhiteLight();
  Camera camera = {.position = {0, 0.9f, -4}, .up = {0, 1, 0}};
  Vec3 target = {0, 0.9f, 0};
  Mat4 view, projection, combined;
  mat4_lookAt(view, &camera.position, &target, &camera.up);
  mat4_perspective(projection, 35, (float)PLAYER_CHECK_WIDTH / PLAYER_CHECK_HEIGHT, 0.1f, 20);
  mat4_multiply(combined, projection, view);
  const PlayerPartSpec* arm = playerModelPartSpec(PLAYER_MODEL_RIGHT_ARM);
  if (!arm)
    return false;
  Vec3 center = {arm->pivot.x + arm->centerOffset.x, arm->pivot.y + arm->centerOffset.y, arm->pivot.z + arm->centerOffset.z - arm->size.z * 0.5f};
  int x, y;
  if (!playerCheckProject(combined, center, &x, &y))
    return false;

  const PlayerRenderer* renderers[] = {transparent, half, opaque};
  unsigned char colors[3][3] = {{0}};
  float depths[3] = {0};
  glViewport(0, 0, PLAYER_CHECK_WIDTH, PLAYER_CHECK_HEIGHT);
  glClearColor(0, 0, 0, 1);
  glClearDepth(1);
  glDepthMask(GL_TRUE);
  for (int pass = 0; pass < 3; pass++) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderPlayerModel(renderers[pass], (Vec3){0}, &pose, view, projection, &daylight);
    glReadPixels(x, y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, colors[pass]);
    glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depths[pass]);
  }

  bool ok = depths[0] == depths[1] && depths[2] < depths[0];
  int colorDifference = 0;
  for (int channel = 0; channel < 3; channel++) {
    colorDifference += abs((int)colors[2][channel] - colors[0][channel]);
    int expected = (colors[0][channel] * 127 + colors[2][channel] * 128 + 127) / 255;
    ok &= abs((int)colors[1][channel] - expected) <= 3;
  }
  ok &= colorDifference > 60;
  glClearColor(0, 0, 0, 0);
  return ok && glGetError() == GL_NO_ERROR;
}

static bool playerCheckHandAlphaAndDepth(const PlayerRenderer* transparent, const PlayerRenderer* half, const PlayerRenderer* opaque) {
  const size_t colorBytes = (size_t)PLAYER_CHECK_WIDTH * PLAYER_CHECK_HEIGHT * 3;
  const size_t depthBytes = (size_t)PLAYER_CHECK_WIDTH * PLAYER_CHECK_HEIGHT * sizeof(float);
  unsigned char* zeroPixels = malloc(colorBytes);
  unsigned char* halfPixels = malloc(colorBytes);
  unsigned char* fullPixels = malloc(colorBytes);
  unsigned char* punchedPixels = malloc(colorBytes);
  float* depthBefore = malloc(depthBytes);
  float* depthAfter = malloc(depthBytes);
  bool ok = zeroPixels && halfPixels && fullPixels && punchedPixels && depthBefore && depthAfter;
  PlayerModelPose idle;
  playerCheckNeutralPose(&idle);
  DayNightState daylight = playerCheckWhiteLight();
  glViewport(0, 0, PLAYER_CHECK_WIDTH, PLAYER_CHECK_HEIGHT);
  glClearColor(0, 0, 0, 1);
  glClearDepth(0.375);
  glDepthMask(GL_TRUE);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (ok) {
    glReadPixels(0, 0, PLAYER_CHECK_WIDTH, PLAYER_CHECK_HEIGHT, GL_DEPTH_COMPONENT, GL_FLOAT, depthBefore);
    renderPlayerHand(transparent, &idle, (float)PLAYER_CHECK_WIDTH / PLAYER_CHECK_HEIGHT, &daylight);
    glReadPixels(0, 0, PLAYER_CHECK_WIDTH, PLAYER_CHECK_HEIGHT, GL_DEPTH_COMPONENT, GL_FLOAT, depthAfter);
    ok &= memcmp(depthBefore, depthAfter, depthBytes) == 0;
  }

  if (ok) {
    glClearDepth(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderPlayerHand(transparent, &idle, (float)PLAYER_CHECK_WIDTH / PLAYER_CHECK_HEIGHT, &daylight);
    glReadPixels(0, 0, PLAYER_CHECK_WIDTH, PLAYER_CHECK_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, zeroPixels);
    glClearDepth(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderPlayerHand(transparent, &idle, (float)PLAYER_CHECK_WIDTH / PLAYER_CHECK_HEIGHT, &daylight);
    glReadPixels(0, 0, PLAYER_CHECK_WIDTH, PLAYER_CHECK_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, fullPixels);
    ok &= memcmp(zeroPixels, fullPixels, colorBytes) == 0;
  }

  if (ok) {
    memcpy(zeroPixels, fullPixels, colorBytes);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderPlayerHand(half, &idle, (float)PLAYER_CHECK_WIDTH / PLAYER_CHECK_HEIGHT, &daylight);
    glReadPixels(0, 0, PLAYER_CHECK_WIDTH, PLAYER_CHECK_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, halfPixels);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderPlayerHand(opaque, &idle, (float)PLAYER_CHECK_WIDTH / PLAYER_CHECK_HEIGHT, &daylight);
    glReadPixels(0, 0, PLAYER_CHECK_WIDTH, PLAYER_CHECK_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, fullPixels);
    size_t blended = 0;
    for (size_t pixel = 0; pixel < colorBytes; pixel += 3) {
      int difference =
          abs((int)fullPixels[pixel] - zeroPixels[pixel]) + abs((int)fullPixels[pixel + 1] - zeroPixels[pixel + 1]) + abs((int)fullPixels[pixel + 2] - zeroPixels[pixel + 2]);
      if (difference < 30)
        continue;
      blended++;
      for (int channel = 0; channel < 3; channel++) {
        int expected = (zeroPixels[pixel + channel] * 127 + fullPixels[pixel + channel] * 128 + 127) / 255;
        if (abs((int)halfPixels[pixel + channel] - expected) > 3)
          ok = false;
      }
    }
    ok &= blended > 100;
  }

  if (ok) {
    int centerLit = playerCheckLitPixels(zeroPixels, PLAYER_CHECK_WIDTH / 2 - 96, PLAYER_CHECK_HEIGHT / 2 - 54, PLAYER_CHECK_WIDTH / 2 + 96, PLAYER_CHECK_HEIGHT / 2 + 54);
    int lowerRight = playerCheckLitPixels(zeroPixels, PLAYER_CHECK_WIDTH / 2, 0, PLAYER_CHECK_WIDTH, PLAYER_CHECK_HEIGHT / 2);
    ok &= centerLit == 0 && lowerRight > 100;
    PlayerModelPose punching;
    PlayerPoseInput input = {.yaw = -90, .punch = 1, .grounded = true};
    playerModelPose(&punching, &input);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderPlayerHand(transparent, &punching, (float)PLAYER_CHECK_WIDTH / PLAYER_CHECK_HEIGHT, &daylight);
    glReadPixels(0, 0, PLAYER_CHECK_WIDTH, PLAYER_CHECK_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, punchedPixels);
    size_t changed = 0;
    for (size_t pixel = 0; pixel < colorBytes; pixel += 3)
      changed += memcmp(zeroPixels + pixel, punchedPixels + pixel, 3) != 0;
    ok &= changed > 100;
    ok &= playerCheckLitPixels(punchedPixels, PLAYER_CHECK_WIDTH / 2 - 96, PLAYER_CHECK_HEIGHT / 2 - 54, PLAYER_CHECK_WIDTH / 2 + 96, PLAYER_CHECK_HEIGHT / 2 + 54) == 0;
  }

  free(zeroPixels);
  free(halfPixels);
  free(fullPixels);
  free(punchedPixels);
  free(depthBefore);
  free(depthAfter);
  glClearDepth(1);
  glClearColor(0, 0, 0, 0);
  return ok && glGetError() == GL_NO_ERROR;
}

static bool playerCheckRenderState(GLuint shader, const PlayerRenderer* renderer) {
  PlayerCheckGLState original, expected, actual;
  PlayerCheckSentinels sentinels;
  playerCheckCaptureState(&original);
  if (!playerCheckSetSentinels(shader, &sentinels, &expected)) {
    playerCheckRestoreState(&original);
    playerCheckDeleteSentinels(&sentinels);
    return false;
  }
  PlayerModelPose pose;
  playerCheckNeutralPose(&pose);
  DayNightState daylight = playerCheckWhiteLight();
  Camera camera = {.position = {0, 0.9f, -4}, .up = {0, 1, 0}};
  Vec3 target = {0, 0.9f, 0};
  Mat4 view, projection;
  mat4_lookAt(view, &camera.position, &target, &camera.up);
  mat4_perspective(projection, 35, (float)PLAYER_CHECK_WIDTH / PLAYER_CHECK_HEIGHT, 0.1f, 20);
  renderPlayerModel(renderer, (Vec3){0}, &pose, view, projection, &daylight);
  playerCheckCaptureState(&actual);
  bool ok = playerCheckStateEqual(&actual, &expected) && glGetError() == GL_NO_ERROR;
  renderPlayerHand(renderer, &pose, (float)PLAYER_CHECK_WIDTH / PLAYER_CHECK_HEIGHT, &daylight);
  playerCheckCaptureState(&actual);
  ok &= playerCheckStateEqual(&actual, &expected) && glGetError() == GL_NO_ERROR;
  playerCheckRestoreState(&original);
  playerCheckDeleteSentinels(&sentinels);
  return ok;
}

static bool playerCheckInvalidSkin(const char* path) {
  PlayerRenderer renderer = {0};
  bool rejected = !initPlayerRenderer(&renderer, path);
  bool empty = !renderer.program && !renderer.texture && !renderer.vao && !renderer.vbo;
  cleanupPlayerRenderer(&renderer);
  return rejected && empty && glGetError() == GL_NO_ERROR;
}

static bool testPlayerRendering(GLuint shader) {
  const char* mappedPath = "test-player-map.tga";
  const char* transparentPath = "test-player-alpha0.tga";
  const char* halfPath = "test-player-alpha128.tga";
  const char* opaquePath = "test-player-alpha255.tga";
  const char* rgbPath = "test-player-rgb.tga";
  bool ok = playerCheckMakeMappedSkin(mappedPath) && playerCheckMakeArmSkin(transparentPath, 0) && playerCheckMakeArmSkin(halfPath, 128) &&
            playerCheckMakeArmSkin(opaquePath, 255) && playerCheckMakeRgbSkin(rgbPath);
  PlayerRenderer supplied = {0}, mapped = {0}, transparent = {0}, half = {0}, opaque = {0};
  PlayerCheckGLState original, expected, actual;
  PlayerCheckSentinels sentinels = {0};
  playerCheckCaptureState(&original);
  if (ok)
    ok = playerCheckSetSentinels(shader, &sentinels, &expected);
  if (ok) {
    ok = initPlayerRenderer(&supplied, "assets/player/skin.png");
    playerCheckCaptureState(&actual);
    ok &= playerCheckStateEqual(&actual, &expected);
  }
  playerCheckRestoreState(&original);
  playerCheckDeleteSentinels(&sentinels);
  if (ok)
    ok = playerCheckTextureUpload(&supplied);
  if (ok)
    ok = !supplied.fractionalAlpha;
  if (ok)
    ok = playerCheckInvalidSkin("missing-player-skin.png") && playerCheckInvalidSkin("assets/textures/dirt.png") && playerCheckInvalidSkin(rgbPath);
  if (ok)
    ok = initPlayerRenderer(&mapped, mappedPath) && initPlayerRenderer(&transparent, transparentPath) && initPlayerRenderer(&half, halfPath) &&
         initPlayerRenderer(&opaque, opaquePath);
  if (ok)
    ok = !transparent.fractionalAlpha && half.fractionalAlpha && !opaque.fractionalAlpha;
  if (ok)
    ok = playerCheckMappedModel(&mapped);
  if (ok)
    ok = playerCheckBodyAlphaDepth(&transparent, &half, &opaque);
  if (ok)
    ok = playerCheckHandAlphaAndDepth(&transparent, &half, &opaque);
  if (ok)
    ok = playerCheckRenderState(shader, &supplied);

  cleanupPlayerRenderer(&opaque);
  cleanupPlayerRenderer(&half);
  cleanupPlayerRenderer(&transparent);
  cleanupPlayerRenderer(&mapped);
  cleanupPlayerRenderer(&supplied);
  remove(mappedPath);
  remove(transparentPath);
  remove(halfPath);
  remove(opaquePath);
  remove(rgbPath);
  glViewport(0, 0, PLAYER_CHECK_WIDTH, PLAYER_CHECK_HEIGHT);
  glClearColor(0, 0, 0, 0);
  glClearDepth(1);
  glDepthMask(GL_TRUE);
  if (!ok)
    fprintf(stderr, "Player skin/model rendering, alpha, depth, or GL state checks failed\n");
  else
    puts("Player skin atlas, model faces, hand alpha, depth isolation, and GL state checks passed");
  return ok && glGetError() == GL_NO_ERROR;
}

#endif
