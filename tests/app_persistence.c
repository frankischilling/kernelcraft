// Run the real application in separate save/load processes with registered inputs.
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "utils/inputs.h"
#include "graphics/hud.h"
#include "world/world.h"
#include "world/save.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c)                                                                                                                                                                   \
  do {                                                                                                                                                                             \
    if (!(c)) {                                                                                                                                                                    \
      fprintf(stderr, "Persistence application test: %s (line %d)\n", #c, __LINE__);                                                                                               \
      exit(EXIT_FAILURE);                                                                                                                                                          \
    }                                                                                                                                                                              \
  } while (0)
static int frame = -1, swaps, cursorMode = GLFW_CURSOR_NORMAL;
static bool failureShown;
static const Vec3 feet = {-0.5f, 40, 0.5f};
static const Vec3i removed = {-1, 41, 2}, placed = {-1, 41, 3}, exitEdit = {0, 42, 4}, cobblestone = {0, 41, 3};
static bool crouchScenario(void) {
  const char* phase = getenv("KERNELCRAFT_TEST_RESTART");
  return phase && !strncmp(phase, "crouch-", 7);
}
static bool failing(void) {
  const char* phase = getenv("KERNELCRAFT_TEST_RESTART");
  return phase && !strcmp(phase, "fail");
}
static bool saving(void) {
  const char* phase = getenv("KERNELCRAFT_TEST_RESTART");
  CHECK(phase);
  return !strcmp(phase, "save") || !strcmp(phase, "crouch-save") || failing();
}
static int id(Vec3i cell) {
  const Block* block = getBlock(&cell);
  CHECK(block);
  return block->id;
}

GLFWwindow* __real_glfwCreateWindow(int width, int height, const char* title, GLFWmonitor* monitor, GLFWwindow* share);
GLFWwindow* __wrap_glfwCreateWindow(int width, int height, const char* title, GLFWmonitor* monitor, GLFWwindow* share) {
  (void)width;
  (void)height;
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
  return __real_glfwCreateWindow(640, 480, title, monitor, share);
}
void __wrap_glfwSetInputMode(GLFWwindow* window, int mode, int value) {
  (void)window;
  CHECK(mode == GLFW_CURSOR);
  cursorMode = value;
}
int __wrap_glfwGetInputMode(GLFWwindow* window, int mode) {
  (void)window;
  CHECK(mode == GLFW_CURSOR);
  return cursorMode;
}
int __real_glfwGetWindowAttrib(GLFWwindow* window, int attrib);
int __wrap_glfwGetWindowAttrib(GLFWwindow* window, int attrib) {
  return attrib == GLFW_FOCUSED ? GLFW_TRUE : __real_glfwGetWindowAttrib(window, attrib);
}
int __wrap_glfwGetKey(GLFWwindow* window, int key) {
  (void)window;
  (void)key;
  return GLFW_RELEASE;
}
double __wrap_glfwGetTime(void) {
  return 1.0;
} // Freeze physics so saved feet compare exactly.

int __wrap_glfwWindowShouldClose(GLFWwindow* window) {
  frame++;
  InputState* input = glfwGetWindowUserPointer(window);
  CHECK(input && !input->flying && playerCanOccupyPosture(input->player.position, input->player.crouched));
  GLFWkeyfun key = glfwSetKeyCallback(window, NULL);
  glfwSetKeyCallback(window, key);
  GLFWmousebuttonfun mouse = glfwSetMouseButtonCallback(window, NULL);
  glfwSetMouseButtonCallback(window, mouse);
  CHECK(key && mouse && worldSeed() == 42);
  if (frame == 0 && saving()) {
    for (int x = -2; x <= 1; x++)
      for (int z = 0; z <= 4; z++) {
        CHECK(setBlock(&(Vec3i){x, 39, z}, BLOCK_STONE));
        for (int y = 40; y <= 44; y++)
          CHECK(setBlock(&(Vec3i){x, y, z}, BLOCK_AIR));
      }
    CHECK(setBlock(&removed, BLOCK_DIRT));
    CHECK(setBlock(&(Vec3i){-1, 41, 4}, BLOCK_GRASS));
    CHECK(playerSetPosition(&input->player, feet));
    input->camera->position = playerEyePosition(&input->player);
    input->camera->yaw = 90;
    input->camera->pitch = 0;
    updateCameraVectors(input->camera);
    mouse(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
    CHECK(id(removed) == BLOCK_DIRT);
    for (int i = 0; i < 5; i++)
      processBlockBreaking(window, input, 0.1);
    CHECK(id(removed) == BLOCK_AIR);
    mouse(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
    key(window, GLFW_KEY_3, 0, GLFW_PRESS, 0);
    mouse(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
    CHECK(id(placed) == BLOCK_STONE);
    CHECK(setBlock(&(Vec3i){0, 41, 4}, BLOCK_GRASS));
    input->camera->position.x = 0.5f;
    key(window, GLFW_KEY_4, 0, GLFW_PRESS, 0);
    CHECK(selectedHotbarSlot() == 3 && selectedBlock() == BLOCK_COBBLESTONE);
    mouse(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
    CHECK(id(cobblestone) == BLOCK_COBBLESTONE);
    input->camera->position = playerEyePosition(&input->player);
    CHECK(getChunk(&(Vec2i){7, 8})->dirty && getChunk(&(Vec2i){8, 8})->dirty);
    if (crouchScenario()) {
      CHECK(playerAdvance(&input->player, (PlayerMotion){.crouch = true}, PLAYER_STEP_SECONDS) == 1);
      CHECK(setBlock(&(Vec3i){-1, 41, 0}, BLOCK_STONE));
      input->camera->position = playerEyePosition(&input->player);
      input->camera->pitch = -35;
      updateCameraVectors(input->camera);
    }
    key(window, GLFW_KEY_F5, 0, GLFW_REPEAT, 0);
    CHECK(!input->saveRequested);
    key(window, GLFW_KEY_F5, 0, GLFW_PRESS, 0);
    CHECK(input->saveRequested);
  } else if (frame == 0) {
    CHECK(id(removed) == BLOCK_AIR && id(placed) == BLOCK_STONE && id(exitEdit) == BLOCK_DIRT && id(cobblestone) == BLOCK_COBBLESTONE);
    Vec3 expectedFeet = feet;
    if (crouchScenario()) {
      expectedFeet.y = 42;
      CHECK(id((Vec3i){-1, 41, 0}) == BLOCK_STONE);
    }
    CHECK(!memcmp(&input->player.position, &expectedFeet, sizeof(feet)));
    CHECK(!input->player.crouched && !input->player.running && !input->runInput.tapPending);
    CHECK(!input->breakHeld && !input->breaking.active);
    CHECK(input->player.velocity.x == 0 && input->player.velocity.y == 0 && input->player.velocity.z == 0);
    CHECK(input->camera->yaw == 90 && input->camera->pitch == (crouchScenario() ? -35 : 0) && selectedBlock() == BLOCK_AIR);
    CHECK(selectedHotbarSlot() == 8);
  }
  if (frame == 1 && saving()) {
    CHECK(!input->saveRequested);
    const char* path = getenv("KERNELCRAFT_TEST_WORLD");
    CHECK(path);
    FILE* file = fopen(path, "rb");
    if (failing()) {
      CHECK(!file && failureShown);
    } else {
      CHECK(file);
      CHECK(fseek(file, 0, SEEK_END) == 0 && ftell(file) == 72 + 4194304);
      unsigned char selection[4];
      CHECK(fseek(file, 60, SEEK_SET) == 0 && fread(selection, 1, 4, file) == 4);
      CHECK(selection[0] == 4 && !selection[1] && !selection[2] && !selection[3]);
      if (crouchScenario()) {
        float savedY;
        CHECK(fseek(file, 44, SEEK_SET) == 0 && fread(&savedY, sizeof(savedY), 1, file) == 1);
        CHECK(savedY == 42 && input->player.position.y == 40 && input->player.crouched);
      }
      CHECK(fclose(file) == 0);
    }
    // A second edit after F5 must be included by the normal-exit save.
    CHECK(setBlock(&exitEdit, BLOCK_DIRT));
    key(window, GLFW_KEY_9, 0, GLFW_PRESS, 0);
    CHECK(selectedHotbarSlot() == 8 && selectedBlock() == BLOCK_AIR);
  }
  return frame >= 2;
}

void __real_HUDDraw(GLuint program, DebugData* data);
void __wrap_HUDDraw(GLuint program, DebugData* data) {
  if (failing()) {
    CHECK(data->saveStatus && strstr(data->saveStatus, "Save failed"));
    failureShown = true;
  }
  __real_HUDDraw(program, data);
}

void __real_glfwSwapBuffers(GLFWwindow* window);
void __wrap_glfwSwapBuffers(GLFWwindow* window) {
  CHECK(glGetError() == GL_NO_ERROR);
  CHECK(!getChunk(&(Vec2i){7, 8})->dirty && !getChunk(&(Vec2i){8, 8})->dirty);
  unsigned char pixel[4] = {0};
  // The crouch-save camera is below its roof; the restarted camera is above it.
  // The standard fixture also verifies the placed stone off the crosshair.
  glReadPixels(320 + 16, 240 + 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
  if (!crouchScenario() || !saving())
    CHECK(pixel[0] || pixel[1] || pixel[2]);
  swaps++;
  __real_glfwSwapBuffers(window);
}
void __real_glfwDestroyWindow(GLFWwindow* window);
void __wrap_glfwDestroyWindow(GLFWwindow* window) {
  CHECK(glfwGetCurrentContext() == window && glGetError() == GL_NO_ERROR);
  if (frame >= 0) {
    CHECK(swaps == 2);
    puts(failing() ? "Application save failure status and cleanup checks passed" : "Application edit, F5, exit save, restart state, and rendered chunk checks passed");
  }
  __real_glfwDestroyWindow(window);
}
