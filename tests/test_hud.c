#include <GL/glew.h>
#include <GL/freeglut.h>
#include <GLFW/glfw3.h>
#include "graphics/hud.h"
#include "utils/text.h"
#include "world/cube.h"
#include "../libs/stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures, labels;
static bool sawSaveFailure, sawModeBlocked, sawFPS, sawDebugHint;
static const char* expectedMovement;
static bool sawMovement, sawMaterial;
static const char* expectedMaterial;
static const char* expectedWireframe;
static bool sawWireframe;
static const char* failTexture;
static GLuint partialTextures[6];
static int partialCount;
static float rectangles[32][4];
static float materialX[9];
static GLuint iconTextures[6];
#define CHECK(c)                                                                                                                                                                   \
  do {                                                                                                                                                                             \
    if (!(c)) {                                                                                                                                                                    \
      fprintf(stderr, "HUD check failed: %s at %d\n", #c, __LINE__);                                                                                                               \
      failures++;                                                                                                                                                                  \
    }                                                                                                                                                                              \
  } while (0)

GLuint __real_loadTexture(const char* path);
GLuint __wrap_loadTexture(const char* path) {
  if (failTexture && !strcmp(path, failTexture))
    return 0;
  GLuint texture = __real_loadTexture(path);
  if (failTexture && partialCount < 6)
    partialTextures[partialCount++] = texture;
  return texture;
}

void __real_renderText(const TextState* state, const char* text, float x, float y);
void __wrap_renderText(const TextState* state, const char* text, float x, float y) {
  CHECK(state->fontHeight == glutBitmapHeight(state->font));
  sawSaveFailure |= strstr(text, "Save failed") != NULL;
  sawModeBlocked |= strstr(text, "No safe walk position") != NULL;
  sawFPS |= strstr(text, "FPS:") != NULL;
  sawDebugHint |= strstr(text, "F3:") != NULL;
  sawMaterial |= expectedMaterial && !strcmp(text, expectedMaterial);
  sawMovement |= expectedMovement && strstr(text, expectedMovement) != NULL;
  sawWireframe |= expectedWireframe && strstr(text, expectedWireframe) != NULL;
  if (text[0] >= '1' && text[0] <= '9' && (text[1] == ' ' || text[1] == '\0'))
    materialX[text[0] - '1'] = x + glutBitmapWidth(state->font, text[0]) * 0.5f;
  if (text[0] >= '1' && text[0] <= '6' && text[1] == '\0') {
    GLint texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    int slot = text[0] - '1';
    CHECK(texture && glIsTexture((GLuint)texture));
    CHECK(!iconTextures[slot] || iconTextures[slot] == (GLuint)texture);
    iconTextures[slot] = (GLuint)texture;
  }
  int width = glutBitmapLength(state->font, (const unsigned char*)text);
  float top = y - state->fontHeight, bottom = y + 4;
  CHECK(x >= 0 && x + width <= state->viewport[2]);
  CHECK(top >= 0 && bottom <= state->viewport[3]);
  float cx = state->viewport[2] * 0.5f, cy = state->viewport[3] * 0.5f;
  CHECK(x + width <= cx - 12 || x >= cx + 12 || bottom <= cy - 12 || top >= cy + 12);
  for (int i = 0; i < labels; i++)
    CHECK(x + width <= rectangles[i][0] || x >= rectangles[i][2] || bottom <= rectangles[i][1] || top >= rectangles[i][3]);
  CHECK(labels < 32);
  if (labels < 32) {
    rectangles[labels][0] = x;
    rectangles[labels][1] = top;
    rectangles[labels][2] = x + width;
    rectangles[labels++][3] = bottom;
  }
  __real_renderText(state, text, x, y);
}

static void capture(int width, int height, int index, int debug, const unsigned char* pixels) {
  const char* prefix = getenv("KERNELCRAFT_TEST_CAPTURE");
  if (!prefix)
    return;
  char path[1024];
  int length = snprintf(path, sizeof(path), "%s-%d-%d.ppm", prefix, index, debug);
  CHECK(length > 0 && (size_t)length < sizeof(path));
  FILE* file = fopen(path, "wb");
  CHECK(file);
  if (!file)
    return;
  fprintf(file, "P6\n%d %d\n255\n", width, height);
  for (int row = height - 1; row >= 0; row--)
    CHECK(fwrite(pixels + (size_t)row * width * 3, 1, (size_t)width * 3, file) == (size_t)width * 3);
  CHECK(fclose(file) == 0);
}

// Find the complete upright PNG, enlarged by two with no color filtering.
// Search inside the slot instead of sharing HUD layout or UV calculations.
static bool hasIcon(const unsigned char* pixels, int width, int height, int slot, const unsigned char* reference) {
  for (int bottom = 16; bottom + 32 < height && bottom < 80; bottom++)
    for (int left = (int)materialX[slot] - 24; left <= (int)materialX[slot]; left++) {
      if (left < 0 || left + 32 > width)
        continue;
      bool match = true;
      for (int y = 0; match && y < 32; y++)
        for (int x = 0; match && x < 32; x++) {
          const unsigned char* actual = pixels + ((size_t)(bottom + y) * width + left + x) * 3;
          const unsigned char* expected = reference + ((15 - y / 2) * 16 + x / 2) * 4;
          for (int channel = 0; channel < 3; channel++)
            if (abs((int)actual[channel] - expected[channel]) > 1)
              match = false;
        }
      if (match)
        return true;
    }
  return false;
}

int main(int argc, char** argv) {
  glutInit(&argc, argv);
  if (!glfwInit())
    return 1;
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  GLFWwindow* window = glfwCreateWindow(640, 480, "HUD test", NULL, NULL);
  if (!window)
    return 1;
  glfwMakeContextCurrent(window);
  if (glewInit() != GLEW_OK)
    return 1;
  while (glGetError() != GL_NO_ERROR) {
  }
  Camera camera;
  initCamera(&camera);
  RenderResult stats = {.submittedQuads = 100000, .submittedTriangles = 200000};
  DebugData data = {.camera = &camera,
                    .fps = 60,
                    .selectedSlot = 0,
                    .captured = true,
                    .flying = true,
                    .modeBlocked = true,
                    .saveStatus = "Save failed; see console",
                    .stats = &stats,
                    .selection = {.hit = true, .blockCoords = {-128, 63, -128}}};
  GLuint sentinel;
  glGenTextures(1, &sentinel);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, sentinel);
  glActiveTexture(GL_TEXTURE1);
  CHECK(HUDInit("kernelcraft", "HUD test"));
  GLint activeBeforeDraw, textureBeforeDraw;
  glGetIntegerv(GL_ACTIVE_TEXTURE, &activeBeforeDraw);
  CHECK(activeBeforeDraw == GL_TEXTURE1);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBeforeDraw);
  CHECK(textureBeforeDraw == (GLint)sentinel);
  const char* paths[] = {"assets/textures/grass-side.png",  "assets/textures/dirt.png",       "assets/textures/stone.png",
                         "assets/textures/cobblestone.png", "assets/textures/oak-planks.png", "assets/textures/stone-bricks.png"};
  unsigned char* reference[6];
  for (int slot = 0; slot < 6; slot++) {
    int width, height, channels;
    reference[slot] = stbi_load(paths[slot], &width, &height, &channels, 4);
    CHECK(reference[slot] && width == 16 && height == 16);
    if (!reference[slot] || width != 16 || height != 16)
      return 1;
    for (int pixel = 0; pixel < 16 * 16; pixel++)
      CHECK(reference[slot][pixel * 4 + 3] == 255);
  }
  const int sizes[][2] = {{320, 240}, {240, 320}, {640, 360}, {1280, 720}, {1920, 1080}, {192, 120}, {640, 120}, {1280, 120}, {96, 120}, {64, 64}, {1, 1}, {0, 0}};
  for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
    int width = sizes[i][0], height = sizes[i][1];
    if (width && height) {
      glfwSetWindowSize(window, width, height);
      glfwPollEvents();
      // Hidden GLX windows resize their back buffer at the next swap.
      glfwSwapBuffers(window);
      glfwGetFramebufferSize(window, &width, &height);
    }
    glViewport(0, 0, width, height);
    for (int debug = 0; debug <= 1; debug++) {
      labels = 0;
      for (int slot = 0; slot < 9; slot++)
        materialX[slot] = -1;
      sawSaveFailure = sawModeBlocked = sawFPS = sawDebugHint = false;
      data.showDebug = debug;
      data.wireframe = debug != 0;
      expectedWireframe = debug ? "F4: wireframe on" : "F4: wireframe off";
      sawWireframe = false;
      data.captured = debug == 0;
      data.breakingProgress = width >= 192 && height >= 180 ? 0.5f : 0;
      data.selectedSlot = ((int)i * 2 + debug) % 9;
      glClearColor(0.3f, 0.4f, 0.5f, 1);
      glClear(GL_COLOR_BUFFER_BIT);
      glEnable(GL_DEPTH_TEST);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, sentinel);
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
      glEnable(GL_TEXTURE_2D);
      glActiveTexture(GL_TEXTURE1);
      glDisable(GL_TEXTURE_2D);
      glDisable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ZERO);
      glBlendEquation(GL_FUNC_SUBTRACT);
      glEnable(GL_CULL_FACE);
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      glDepthMask(GL_FALSE);
      glLineWidth(2);
      glColor4f(0.2f, 0.3f, 0.4f, 0.5f);
      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      glScalef(2, 3, 4);
      HUDDraw(0, &data);
      GLint active, texture, environment, blendSource, blendDestination, blendEquation, polygonMode[2];
      glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
      CHECK(active == GL_TEXTURE1 && !glIsEnabled(GL_TEXTURE_2D));
      glActiveTexture(GL_TEXTURE0);
      glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
      glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &environment);
      CHECK(texture == (GLint)sentinel && environment == GL_REPLACE && glIsEnabled(GL_TEXTURE_2D));
      glActiveTexture(GL_TEXTURE1);
      glGetIntegerv(GL_BLEND_SRC_RGB, &blendSource);
      glGetIntegerv(GL_BLEND_DST_RGB, &blendDestination);
      glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquation);
      glGetIntegerv(GL_POLYGON_MODE, polygonMode);
      CHECK(!glIsEnabled(GL_BLEND) && blendSource == GL_ONE && blendDestination == GL_ZERO && blendEquation == GL_FUNC_SUBTRACT);
      CHECK(glIsEnabled(GL_CULL_FACE) && polygonMode[0] == GL_LINE && polygonMode[1] == GL_LINE);
      CHECK(glIsEnabled(GL_DEPTH_TEST));
      GLint matrixMode, program;
      GLfloat lineWidth, color[4], matrix[16];
      GLboolean depthMask;
      glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
      glGetIntegerv(GL_CURRENT_PROGRAM, &program);
      glGetFloatv(GL_LINE_WIDTH, &lineWidth);
      glGetFloatv(GL_CURRENT_COLOR, color);
      glGetFloatv(GL_PROJECTION_MATRIX, matrix);
      glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
      CHECK(matrixMode == GL_PROJECTION && program == 0 && lineWidth == 2 && !depthMask);
      CHECK(color[0] == 0.2f && color[1] == 0.3f && color[2] == 0.4f && color[3] == 0.5f);
      CHECK(matrix[0] == 2 && matrix[5] == 3 && matrix[10] == 4 && matrix[15] == 1);
      if (width >= 240 && height >= 240) {
        CHECK(labels >= 5 && sawSaveFailure && sawModeBlocked && sawDebugHint);
        CHECK(sawFPS == (bool)debug);
      }
      if (width >= 1280 && height >= 240)
        CHECK(sawWireframe);
      if (!width || !height) {
        CHECK(labels == 0);
        CHECK(glGetError() == GL_NO_ERROR);
        continue;
      }
      unsigned char* pixels = malloc((size_t)width * height * 3);
      CHECK(pixels);
      if (!pixels)
        return 1;
      glPixelStorei(GL_PACK_ALIGNMENT, 1);
      glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
      if (width >= 192 && height >= 180) {
        const unsigned char* filled = pixels + ((size_t)(height / 2 + 12) * width + width / 2 - 12) * 3;
        const unsigned char* empty = pixels + ((size_t)(height / 2 + 12) * width + width / 2 + 12) * 3;
        CHECK(debug ? filled[0] < 240 : filled[0] > 240 && filled[1] > 180 && filled[2] < 100);
        CHECK(debug ? empty[0] < 240 : empty[0] < 30 && empty[1] < 30 && empty[2] < 30);
      }
      if (width >= 192 && height >= 120) {
        for (int slot = 0; slot < 9; slot++)
          CHECK(materialX[slot] >= 0 && (!slot || materialX[slot] > materialX[slot - 1]));
        unsigned char* crosshair = pixels + ((size_t)(height / 2) * width + width / 2 + 4) * 3;
        printf("HUD %dx%d debug=%d center=%u,%u,%u\n", width, height, debug, crosshair[0], crosshair[1], crosshair[2]);
        CHECK(debug ? crosshair[0] >= 150 && crosshair[0] <= 155 : crosshair[0] > 240);
        int gold[9] = {0};
        for (int y = 0; y < 80 && y < height; y++)
          for (int x = 0; x < width; x++) {
            const unsigned char* pixel = pixels + ((size_t)y * width + x) * 3;
            if (pixel[0] > 240 && pixel[1] > 180 && pixel[2] < 100) {
              int slot = 0;
              while (slot < 8 && x >= (materialX[slot] + materialX[slot + 1]) / 2 - 0.5f)
                slot++;
              gold[slot]++;
            }
          }
        // Each slot must occupy its own region, with exactly one selected border.
        int selected = data.selectedSlot;
        CHECK(gold[selected] > 10);
        for (int slot = 0; slot < 9; slot++)
          if (slot != selected)
            CHECK(gold[slot] == 0);
        if (width >= 640 && height >= 180) {
          for (int slot = 0; slot < 6; slot++)
            CHECK(hasIcon(pixels, width, height, slot, reference[slot]));
          for (int slot = 6; slot < 9; slot++) {
            const unsigned char* empty = pixels + ((size_t)40 * width + (int)materialX[slot]) * 3;
            CHECK(abs((int)empty[0] - 31) <= 1 && abs((int)empty[1] - 31) <= 1 && abs((int)empty[2] - 31) <= 1);
          }
        }
      } else {
        for (int slot = 0; slot < 9; slot++)
          CHECK(materialX[slot] == -1);
      }
      capture(width, height, (int)i, debug, pixels);
      free(pixels);
      CHECK(glGetError() == GL_NO_ERROR);
    }
  }
  glfwSetWindowSize(window, 640, 480);
  glfwPollEvents();
  glfwSwapBuffers(window);
  glViewport(0, 0, 640, 480);
  const char* states[] = {"Crouching: grounded", "Crouching: airborne", "Running: grounded", "Running: airborne", "Debug flight"};
  data.modeBlocked = false;
  for (int mode = 0; mode < 5; mode++) {
    labels = 0;
    sawMovement = false;
    expectedMovement = states[mode];
    data.crouched = mode < 2;
    data.running = mode >= 2;
    data.grounded = mode % 2 == 0;
    data.flying = mode == 4;
    HUDDraw(0, &data);
    CHECK(sawMovement && glGetError() == GL_NO_ERROR);
  }
  const char* names[] = {"Cobblestone", "Oak planks", "Stone bricks"};
  for (int material = 0; material < 3; material++) {
    labels = 0;
    sawMaterial = false;
    expectedMaterial = names[material];
    data.selectedSlot = material + 3;
    HUDDraw(0, &data);
    CHECK(sawMaterial);
  }
  HUDCleanup();
  for (int slot = 0; slot < 6; slot++)
    CHECK(iconTextures[slot] && !glIsTexture(iconTextures[slot]));
  HUDCleanup();
  for (int failedSlot = 3; failedSlot < 6; failedSlot++) {
    partialCount = 0;
    failTexture = paths[failedSlot];
    CHECK(!HUDInit("kernelcraft", "missing material icon"));
    CHECK(partialCount == failedSlot);
    for (int slot = 0; slot < partialCount; slot++)
      CHECK(partialTextures[slot] && !glIsTexture(partialTextures[slot]));
    HUDCleanup();
  }
  failTexture = NULL;
  memset(iconTextures, 0, sizeof(iconTextures));
  CHECK(HUDInit("kernelcraft", "recovered material icons"));
  labels = 0;
  HUDDraw(0, &data);
  HUDCleanup();
  for (int slot = 0; slot < 6; slot++)
    CHECK(iconTextures[slot] && !glIsTexture(iconTextures[slot]));
  glDeleteTextures(1, &sentinel);
  for (int slot = 0; slot < 6; slot++)
    stbi_image_free(reference[slot]);
  CHECK(glGetError() == GL_NO_ERROR);
  glfwDestroyWindow(window);
  glfwTerminate();
  if (failures)
    return 1;
  puts("HUD layout tests passed");
  return 0;
}
