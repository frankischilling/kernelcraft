#include <GL/glew.h>
#include <GL/freeglut.h>
#include <GLFW/glfw3.h>
#include "graphics/hud.h"
#include "utils/text.h"
#include "world/cube.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures, labels;
static bool sawSaveFailure, sawModeBlocked, sawFPS, sawDebugHint;
static float rectangles[32][4];
static float materialX[3];
#define CHECK(c)                                                                                                                                                                   \
  do {                                                                                                                                                                             \
    if (!(c)) {                                                                                                                                                                    \
      fprintf(stderr, "HUD check failed: %s at %d\n", #c, __LINE__);                                                                                                               \
      failures++;                                                                                                                                                                  \
    }                                                                                                                                                                              \
  } while (0)

void __real_renderText(const TextState* state, const char* text, float x, float y);
void __wrap_renderText(const TextState* state, const char* text, float x, float y) {
  CHECK(state->fontHeight == glutBitmapHeight(state->font));
  sawSaveFailure |= strstr(text, "Save failed") != NULL;
  sawModeBlocked |= strstr(text, "No safe walk position") != NULL;
  sawFPS |= strstr(text, "FPS:") != NULL;
  sawDebugHint |= strstr(text, "F3:") != NULL;
  if (text[0] >= '1' && text[0] <= '3' && (text[1] == ' ' || text[1] == '\0'))
    materialX[text[0] - '1'] = x;
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
                    .selectedBlock = BLOCK_GRASS,
                    .captured = true,
                    .flying = true,
                    .modeBlocked = true,
                    .saveStatus = "Save failed; see console",
                    .stats = &stats,
                    .selection = {.hit = true, .blockCoords = {-128, 63, -128}}};
  HUDInit("kernelcraft", "HUD test");
  const int sizes[][2] = {{320, 240}, {240, 320}, {640, 360}, {1280, 720}, {1920, 1080}, {96, 120}, {64, 64}, {1, 1}, {0, 0}};
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
      materialX[0] = materialX[1] = materialX[2] = -1;
      sawSaveFailure = sawModeBlocked = sawFPS = sawDebugHint = false;
      data.showDebug = debug;
      data.captured = debug == 0;
      data.selectedBlock = BLOCK_GRASS + (int)i % 3;
      glClearColor(0.3f, 0.4f, 0.5f, 1);
      glClear(GL_COLOR_BUFFER_BIT);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_TEXTURE_2D);
      glDepthMask(GL_FALSE);
      glLineWidth(2);
      glColor4f(0.2f, 0.3f, 0.4f, 0.5f);
      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      glScalef(2, 3, 4);
      HUDDraw(0, &data);
      CHECK(glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_TEXTURE_2D));
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
      if (width >= 96 && height >= 120) {
        CHECK(materialX[0] >= 0 && materialX[1] > materialX[0] && materialX[2] > materialX[1]);
        unsigned char* crosshair = pixels + ((size_t)(height / 2) * width + width / 2 + 4) * 3;
        printf("HUD %dx%d debug=%d center=%u,%u,%u\n", width, height, debug, crosshair[0], crosshair[1], crosshair[2]);
        CHECK(debug ? crosshair[0] >= 150 && crosshair[0] <= 155 : crosshair[0] > 240);
        int gold[3] = {0};
        for (int y = 0; y < 50 && y < height; y++)
          for (int x = 0; x < width; x++) {
            const unsigned char* pixel = pixels + ((size_t)y * width + x) * 3;
            if (pixel[0] > 240 && pixel[1] > 180 && pixel[2] < 100)
              gold[x >= materialX[2] - 8 ? 2 : x >= materialX[1] - 8 ? 1 : 0]++;
          }
        // Each slot must occupy its own region, with exactly one selected border.
        int selected = data.selectedBlock - BLOCK_GRASS;
        CHECK(gold[selected] > 10);
        for (int slot = 0; slot < 3; slot++)
          if (slot != selected)
            CHECK(gold[slot] == 0);
      }
      capture(width, height, (int)i, debug, pixels);
      free(pixels);
      CHECK(glGetError() == GL_NO_ERROR);
    }
  }
  glfwDestroyWindow(window);
  glfwTerminate();
  if (failures)
    return 1;
  puts("HUD layout tests passed");
  return 0;
}
