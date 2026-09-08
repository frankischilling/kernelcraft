// Exercise the real application loop without taking the user's mouse or focus.
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "graphics/camera.h"
#include "utils/inputs.h"
#include "world/world.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int cursorMode = GLFW_CURSOR_NORMAL;
static int focused = GLFW_TRUE;
static int pressedKey = -1;
static int frame = -1;
static int swaps, waits;
static Vec3 beforeMinimize;
static const Vec3i editFixture = {-1, 40, 6};
static const int sizes[][2] = {{640, 360}, {360, 640}, {0, 0}, {1280, 720}};

#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "Application smoke test: %s (line %d)\n", #condition, __LINE__); \
    exit(EXIT_FAILURE); \
  } \
} while (0)

static void testInput(GLFWwindow* window) {
  Camera* camera = glfwGetWindowUserPointer(window);
  CHECK(camera != NULL);
  GLFWcursorposfun mouse = glfwSetCursorPosCallback(window, NULL);
  GLFWkeyfun key = glfwSetKeyCallback(window, NULL);
  GLFWwindowfocusfun focus = glfwSetWindowFocusCallback(window, NULL);
  glfwSetCursorPosCallback(window, mouse);
  glfwSetKeyCallback(window, key);
  glfwSetWindowFocusCallback(window, focus);
  CHECK(mouse && key);

  mouse(window, 10, 10);
  mouse(window, 30, 10);
  CHECK(fabsf(camera->yaw - 91.0f) < 0.001f);
  key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  CHECK(cursorMode == GLFW_CURSOR_NORMAL);
  float yaw = camera->yaw;
  mouse(window, 500, 500);
  mouse(window, 900, 900);
  CHECK(camera->yaw == yaw);
  key(window, GLFW_KEY_ESCAPE, 0, GLFW_REPEAT, 0);
  CHECK(cursorMode == GLFW_CURSOR_NORMAL);
  Vec3 position = camera->position;
  pressedKey = GLFW_KEY_W;
  processInput(window, camera, 0.1f);
  CHECK(camera->position.x == position.x && camera->position.y == position.y && camera->position.z == position.z);

  key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  mouse(window, -1000, -1000);
  CHECK(camera->yaw == yaw && camera->pitch == 0);
  mouse(window, -980, -1000);
  CHECK(fabsf(camera->yaw - yaw - 1.0f) < 0.001f);
  processInput(window, camera, 0.1f);
  CHECK(fabsf(vec3_distance(&camera->position, &position) - 1.0f) < 0.001f);

  CHECK(focus != NULL);
  focused = GLFW_FALSE;
  focus(window, focused);
  CHECK(cursorMode == GLFW_CURSOR_NORMAL);
  position = camera->position;
  yaw = camera->yaw;
  mouse(window, 4000, 4000);
  processInput(window, camera, 0.1f);
  CHECK(camera->yaw == yaw);
  CHECK(camera->position.x == position.x && camera->position.y == position.y && camera->position.z == position.z);
  key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  CHECK(cursorMode == GLFW_CURSOR_NORMAL);
  focused = GLFW_TRUE;
  focus(window, focused);
  CHECK(cursorMode == GLFW_CURSOR_NORMAL);
  key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  mouse(window, 5000, 5000);
  CHECK(camera->yaw == yaw);
  mouse(window, 5020, 5000);
  CHECK(fabsf(camera->yaw - yaw - 1.0f) < 0.001f);
  pressedKey = -1;
  initCamera(camera);
}

static void testEditing(GLFWwindow* window) {
  Camera* camera = glfwGetWindowUserPointer(window);
  Camera saved = *camera;
  GLFWmousebuttonfun click = glfwSetMouseButtonCallback(window, NULL);
  GLFWkeyfun key = glfwSetKeyCallback(window, NULL);
  glfwSetMouseButtonCallback(window, click);
  glfwSetKeyCallback(window, key);
  CHECK(click && key);
  camera->position = (Vec3){0.5f, 40.5f, -2.5f};
  camera->front = (Vec3){0, 0, 1};
  Vec3i target = {0, 40, 0}, placement = {0, 40, -1};
  CHECK(setBlock(&target, BLOCK_STONE));
  CHECK(setBlock(&placement, BLOCK_AIR));
  key(window, GLFW_KEY_2, 0, GLFW_PRESS, 0);
  CHECK(selectedBlock() == BLOCK_DIRT);
  key(window, GLFW_KEY_3, 0, GLFW_REPEAT, 0);
  CHECK(selectedBlock() == BLOCK_DIRT);
  click(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_RELEASE, 0);
  CHECK(getBlock(&placement)->id == BLOCK_AIR);
  click(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
  CHECK(getBlock(&placement)->id == BLOCK_DIRT);
  click(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
  CHECK(getBlock(&placement)->id == BLOCK_AIR && getBlock(&target)->id == BLOCK_STONE);
  click(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_REPEAT, 0);
  CHECK(getBlock(&target)->id == BLOCK_STONE);
  key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  click(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
  key(window, GLFW_KEY_3, 0, GLFW_PRESS, 0);
  CHECK(getBlock(&target)->id == BLOCK_STONE && selectedBlock() == BLOCK_DIRT);
  key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  focused = GLFW_FALSE;
  click(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
  CHECK(getBlock(&target)->id == BLOCK_STONE);
  focused = GLFW_TRUE;
  camera->position.z = -1.1f;
  click(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
  CHECK(getBlock(&placement)->id == BLOCK_AIR);
  click(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
  CHECK(getBlock(&target)->id == BLOCK_AIR);
  key(window, GLFW_KEY_1, 0, GLFW_PRESS, 0);
  *camera = saved;
}

GLFWwindow* __real_glfwCreateWindow(int width, int height, const char* title, GLFWmonitor* monitor, GLFWwindow* share);
GLFWwindow* __wrap_glfwCreateWindow(int width, int height, const char* title, GLFWmonitor* monitor, GLFWwindow* share) {
  (void)width;
  (void)height;
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
  return __real_glfwCreateWindow(640, 360, title, monitor, share);
}

int __wrap_glfwWindowShouldClose(GLFWwindow* window) {
  if (frame == -1) {
    testInput(window);
    testEditing(window);
    Camera* camera = glfwGetWindowUserPointer(window);
    camera->position.x = -0.5f;
    camera->position.y = 40.5f;
    CHECK(setBlock(&editFixture, BLOCK_STONE));
  }
  if (frame == 0) {
    Camera* camera = glfwGetWindowUserPointer(window);
    // A one-second stall must move one unit after the 0.1-second flight clamp.
    // This also catches disconnecting keyboard input from the application loop.
    CHECK(fabsf(camera->position.z - 4.0f) < 0.001f);
    CHECK(!getChunk(&(Vec2i){7, 8})->dirty);
    GLFWmousebuttonfun click = glfwSetMouseButtonCallback(window, NULL);
    glfwSetMouseButtonCallback(window, click);
    click(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
    CHECK(getBlock(&editFixture)->id == BLOCK_AIR && getChunk(&(Vec2i){7, 8})->dirty);
    pressedKey = -1;
  }
  if (frame == 2) {
    Camera* camera = glfwGetWindowUserPointer(window);
    CHECK(camera->position.x == beforeMinimize.x && camera->position.y == beforeMinimize.y && camera->position.z == beforeMinimize.z);
    pressedKey = -1;
    CHECK(setBlock(&(Vec3i){-1, 40, 7}, BLOCK_STONE));
  }
  frame++;
  if (frame == 0)
    pressedKey = GLFW_KEY_W;
  if (frame == 2) {
    beforeMinimize = ((Camera*)glfwGetWindowUserPointer(window))->position;
    pressedKey = GLFW_KEY_W;
  }
  if (frame < 4 && frame != 2) {
    glfwSetWindowSize(window, sizes[frame][0], sizes[frame][1]);
    glfwPollEvents();
  }
  if (frame == 3) {
    GLFWmousebuttonfun click = glfwSetMouseButtonCallback(window, NULL);
    glfwSetMouseButtonCallback(window, click);
    click(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
    CHECK(getBlock(&editFixture)->id == BLOCK_GRASS);
  }
  return frame >= 4;
}

double __wrap_glfwGetTime(void) {
  return frame < 0 ? 0.0 : (double)(frame + 1);
}

void __wrap_glfwSetInputMode(GLFWwindow* window, int mode, int value) {
  (void)window;
  if (mode == GLFW_CURSOR)
    cursorMode = value;
}

int __real_glfwGetInputMode(GLFWwindow* window, int mode);
int __wrap_glfwGetInputMode(GLFWwindow* window, int mode) {
  return mode == GLFW_CURSOR ? cursorMode : __real_glfwGetInputMode(window, mode);
}

int __real_glfwGetWindowAttrib(GLFWwindow* window, int attrib);
int __wrap_glfwGetWindowAttrib(GLFWwindow* window, int attrib) {
  return attrib == GLFW_FOCUSED ? focused : __real_glfwGetWindowAttrib(window, attrib);
}

int __wrap_glfwGetKey(GLFWwindow* window, int key) {
  (void)window;
  return key == pressedKey ? GLFW_PRESS : GLFW_RELEASE;
}

void __wrap_glfwGetFramebufferSize(GLFWwindow* window, int* width, int* height) {
  (void)window;
  int index = frame < 0 ? 0 : frame;
  CHECK(index < 4);
  *width = sizes[index][0];
  *height = sizes[index][1];
}

void __wrap_glfwWaitEvents(void) {
  CHECK(frame == 2);
  GLFWkeyfun key = glfwSetKeyCallback(glfwGetCurrentContext(), NULL);
  glfwSetKeyCallback(glfwGetCurrentContext(), key);
  key(glfwGetCurrentContext(), GLFW_KEY_3, 0, GLFW_PRESS, 0);
  CHECK(selectedBlock() == BLOCK_GRASS);
  waits++;
}

void __real_glfwSwapBuffers(GLFWwindow* window);
void __wrap_glfwSwapBuffers(GLFWwindow* window) {
  CHECK(frame >= 0 && frame < 4 && frame != 2);
  GLint viewport[4], program;
  glGetIntegerv(GL_VIEWPORT, viewport);
  CHECK(viewport[2] == sizes[frame][0] && viewport[3] == sizes[frame][1]);
  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  CHECK(program != 0);
  GLfloat matrix[16];
  glGetUniformfv((GLuint)program, glGetUniformLocation((GLuint)program, "viewProjection"), matrix);
  // Initial camera faces +Z without pitch. The two diagonal magnitudes recover
  // the aspect ratio independently of the production projection helper.
  CHECK(fabsf(fabsf(matrix[5] / matrix[0]) - (float)sizes[frame][0] / sizes[frame][1]) < 0.001f);
  // The same centered crosshair must be present after every framebuffer resize.
  unsigned char pixel[4] = {0};
  glReadPixels(sizes[frame][0] / 2 + 4, sizes[frame][1] / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
  if (pixel[0] <= 240 || pixel[1] <= 240 || pixel[2] <= 240)
    fprintf(stderr, "Frame %d crosshair pixel: %u %u %u\n", frame, pixel[0], pixel[1], pixel[2]);
  CHECK(pixel[0] > 240 && pixel[1] > 240 && pixel[2] > 240);
  glReadPixels(sizes[frame][0] / 2 + 16, sizes[frame][1] / 2 + 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
  CHECK(!getChunk(&(Vec2i){7, 8})->dirty);
  int width = sizes[frame][0], height = sizes[frame][1];
  unsigned char* pixels = malloc((size_t)width * height * 3);
  CHECK(pixels);
  glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
  int outlinePixels = 0;
  for (int y = height / 3; y < height * 2 / 3; y++)
    for (int x = width / 3; x < width * 2 / 3; x++) {
      unsigned char* p = pixels + ((size_t)y * width + x) * 3;
      outlinePixels += p[0] > 240 && p[1] > 180 && p[2] < 100;
    }

  const char* capturePrefix = getenv("KERNELCRAFT_TEST_CAPTURE");
  if (capturePrefix) {
    char path[1024];
    int length = snprintf(path, sizeof(path), "%s-%d.ppm", capturePrefix, frame);
    CHECK(length > 0 && (size_t)length < sizeof(path));
    FILE* capture = fopen(path, "wb");
    CHECK(capture);
    fprintf(capture, "P6\n%d %d\n255\n", width, height);
    for (int row = height - 1; row >= 0; row--)
      CHECK(fwrite(pixels + (size_t)row * width * 3, 1, (size_t)width * 3, capture) == (size_t)width * 3);
    CHECK(fclose(capture) == 0);
  }
  free(pixels);
  if (frame == 1)
    CHECK(pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0);
  else
    CHECK(pixel[0] || pixel[1] || pixel[2]);
  CHECK(frame == 1 ? outlinePixels == 0 : outlinePixels > 10);
  swaps++;
  __real_glfwSwapBuffers(window);
}

void __real_glfwDestroyWindow(GLFWwindow* window);
void __wrap_glfwDestroyWindow(GLFWwindow* window) {
  CHECK(glfwGetCurrentContext() == window);
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    fprintf(stderr, "OpenGL startup/shutdown error: %u\n", error);
    exit(EXIT_FAILURE);
  }
  if (frame >= 0) {
    CHECK(swaps == 3 && waits == 1);
    puts("Application editing, selection pixels, input, framebuffer, and shutdown tests passed");
  }
  __real_glfwDestroyWindow(window);
}
