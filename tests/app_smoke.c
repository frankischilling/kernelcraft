// Exercise the real application loop without taking the user's mouse or focus.
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "graphics/camera.h"
#include "graphics/hud.h"
#include "utils/inputs.h"
#include "world/world.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int cursorMode = GLFW_CURSOR_NORMAL;
static int focused = GLFW_TRUE;
static bool iconified;
static int pressedKey = -1;
static int frame = -1;
static int swaps, waits;
static bool sawCompactHUD, sawDebugHUD;
static Vec3 beforeMinimize;
static float yawBeforeMinimize, pitchBeforeMinimize;
static const Vec3i editFixture = {-1, 40, 6};
static const int sizes[][2] = {{640, 360}, {360, 640}, {0, 0}, {1280, 720}};

#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "Application smoke test: %s (line %d)\n", #condition, __LINE__); \
    exit(EXIT_FAILURE); \
  } \
} while (0)

static void testInput(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  Camera* camera = input->camera;
  CHECK(camera != NULL);
  GLFWcursorposfun mouse = glfwSetCursorPosCallback(window, NULL);
  GLFWkeyfun key = glfwSetKeyCallback(window, NULL);
  GLFWwindowfocusfun focus = glfwSetWindowFocusCallback(window, NULL);
  glfwSetCursorPosCallback(window, mouse);
  glfwSetKeyCallback(window, key);
  glfwSetWindowFocusCallback(window, focus);
  CHECK(mouse && key);
  CHECK(!input->flying && input->player.grounded && playerCanOccupy(input->player.position));
  CHECK(!input->showDebug);
  key(window, GLFW_KEY_F3, 0, GLFW_PRESS, 0);
  CHECK(input->showDebug);
  key(window, GLFW_KEY_F3, 0, GLFW_REPEAT, 0);
  CHECK(input->showDebug);
  key(window, GLFW_KEY_F3, 0, GLFW_RELEASE, 0);
  CHECK(input->showDebug);
  key(window, GLFW_KEY_F3, 0, GLFW_PRESS, 0);
  CHECK(!input->showDebug && input->player.grounded);
  key(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  CHECK(input->flying);
  key(window, GLFW_KEY_F, 0, GLFW_REPEAT, 0);
  CHECK(input->flying);

  mouse(window, 10, 10);
  mouse(window, 30, 10);
  CHECK(fabsf(camera->yaw - 91.0f) < 0.001f);
  // Changing movement mode does not pause or warp the cursor.
  key(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  CHECK(!input->flying);
  mouse(window, 50, 10);
  CHECK(fabsf(camera->yaw - 92.0f) < 0.001f);
  key(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  CHECK(input->flying);
  mouse(window, 70, 10);
  CHECK(fabsf(camera->yaw - 93.0f) < 0.001f);
  key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  CHECK(cursorMode == GLFW_CURSOR_NORMAL);
  key(window, GLFW_KEY_F3, 0, GLFW_PRESS, 0);
  CHECK(input->showDebug && cursorMode == GLFW_CURSOR_NORMAL);
  key(window, GLFW_KEY_F3, 0, GLFW_PRESS, 0);
  CHECK(!input->showDebug);
  float yaw = camera->yaw;
  mouse(window, 500, 500);
  mouse(window, 900, 900);
  CHECK(camera->yaw == yaw);
  key(window, GLFW_KEY_ESCAPE, 0, GLFW_REPEAT, 0);
  CHECK(cursorMode == GLFW_CURSOR_NORMAL);
  Vec3 position = camera->position;
  pressedKey = GLFW_KEY_W;
  processInput(window, input, 0.1f);
  CHECK(camera->position.x == position.x && camera->position.y == position.y && camera->position.z == position.z);

  key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  mouse(window, -1000, -1000);
  CHECK(camera->yaw == yaw && camera->pitch == 0);
  mouse(window, -980, -1000);
  CHECK(fabsf(camera->yaw - yaw - 1.0f) < 0.001f);
  processInput(window, input, 0.1f);
  CHECK(fabsf(vec3_distance(&camera->position, &position) - 1.0f) < 0.001f);

  CHECK(focus != NULL);
  focused = GLFW_FALSE;
  focus(window, focused);
  key(window, GLFW_KEY_F3, 0, GLFW_PRESS, 0);
  CHECK(!input->showDebug);
  CHECK(cursorMode == GLFW_CURSOR_NORMAL);
  position = camera->position;
  yaw = camera->yaw;
  mouse(window, 4000, 4000);
  processInput(window, input, 0.1f);
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

static void testIconifiedInput(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  Camera* camera = input->camera;
  Camera originalCamera = *camera;
  InputState originalInput = *input;
  GLFWkeyfun key = glfwSetKeyCallback(window, NULL);
  GLFWcursorposfun mouse = glfwSetCursorPosCallback(window, NULL);
  GLFWmousebuttonfun click = glfwSetMouseButtonCallback(window, NULL);
  glfwSetKeyCallback(window, key);
  glfwSetCursorPosCallback(window, mouse);
  glfwSetMouseButtonCallback(window, click);
  const Vec3i target = {0, 41, 0}, placement = {0, 41, -1};
  CHECK(setBlock(&target, BLOCK_STONE) && setBlock(&placement, BLOCK_AIR));
  CHECK(playerSetPosition(&input->player, (Vec3){0.5f, 40, -2.5f}));
  camera->position = playerEyePosition(&input->player);
  camera->yaw = 90;
  camera->pitch = 0;
  updateCameraVectors(camera);
  Vec3 position = camera->position;
  int material = selectedBlock();
  bool debug = input->showDebug;
  // Retain focus and framebuffer dimensions to isolate iconification from
  // platform-dependent resize/focus callback ordering.
  iconified = true;
  key(window, GLFW_KEY_F3, 0, GLFW_PRESS, 0);
  CHECK(input->showDebug == debug);
  key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  CHECK(cursorMode == GLFW_CURSOR_DISABLED);
  key(window, GLFW_KEY_3, 0, GLFW_PRESS, 0);
  CHECK(selectedBlock() == material);
  key(window, GLFW_KEY_F5, 0, GLFW_PRESS, 0);
  CHECK(!input->saveRequested);
  click(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
  CHECK(getBlock(&target)->id == BLOCK_STONE);
  click(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
  CHECK(getBlock(&placement)->id == BLOCK_AIR);
  for (int flying = 0; flying <= 1; flying++) {
    input->flying = flying != 0;
    key(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
    CHECK(input->flying == (flying != 0));
    key(window, GLFW_KEY_SPACE, 0, GLFW_PRESS, 0);
    CHECK(!input->jumpRequested);
    mouse(window, 100, 100);
    mouse(window, 300, 300);
    CHECK(camera->yaw == 90 && camera->pitch == 0);
    pressedKey = GLFW_KEY_W;
    processInput(window, input, 10);
    CHECK(input->simulationSteps == 0);
    CHECK(camera->position.x == position.x && camera->position.y == position.y && camera->position.z == position.z);
  }
  iconified = false;
  pressedKey = -1;
  CHECK(setBlock(&target, BLOCK_AIR));
  *camera = originalCamera;
  *input = originalInput;
}

static void testSavedInput(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  Camera* camera = input->camera;
  Camera originalCamera = *camera;
  InputState originalInput = *input;
  SavedPlayer saved;
  CHECK(playerSetPosition(&input->player, (Vec3){-40.5f, 40, -40.5f}));
  input->flying = false;
  camera->position = playerEyePosition(&input->player);
  camera->yaw = -810;
  CHECK(snapshotPlayer(input, &saved) && saved.yaw == 270 && saved.feet.y == 40);
  Camera restoredCamera = *camera;
  InputState restored;
  CHECK(initSavedInputs(&restored, &restoredCamera, &saved));
  CHECK(!restored.flying && restored.player.velocity.y == 0 && restored.player.position.y == 40);
  input->flying = true;
  camera->position.x = 500;
  Vec3 before = camera->position;
  CHECK(snapshotPlayer(input, &saved) && playerCanOccupy(saved.feet));
  CHECK(camera->position.x == before.x && input->flying); // Taking a snapshot must not move the current session.
  GLFWkeyfun key = glfwSetKeyCallback(window, NULL);
  glfwSetKeyCallback(window, key);
  key(window, GLFW_KEY_F5, 0, GLFW_REPEAT, 0);
  CHECK(!input->saveRequested);
  key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  key(window, GLFW_KEY_F5, 0, GLFW_PRESS, 0);
  CHECK(!input->saveRequested);
  key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  *camera = originalCamera;
  *input = originalInput;
}

static void testEditing(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  Camera* camera = input->camera;
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

static void testWalkingControls(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  Camera* camera = input->camera;
  GLFWkeyfun key = glfwSetKeyCallback(window, NULL);
  glfwSetKeyCallback(window, key);
  // A floating platform and a three-block wall isolate controls from terrain shape.
  for (int x = -2; x <= 2; x++)
    for (int z = -2; z <= 5; z++) {
      CHECK(setBlock(&(Vec3i){x, 39, z}, BLOCK_STONE));
      for (int y = 40; y <= 46; y++)
        CHECK(setBlock(&(Vec3i){x, y, z}, z == 2 && y <= 42 ? BLOCK_STONE : BLOCK_AIR));
    }
  camera->position = (Vec3){0.5f, 40 + PLAYER_EYE_HEIGHT, 0.5f};
  key(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  CHECK(!input->flying && input->player.grounded);
  // Looking almost straight up must retain full horizontal walking speed.
  camera->pitch = 89;
  updateCameraVectors(camera);
  pressedKey = GLFW_KEY_W;
  processInput(window, input, 1.0 / 30);
  CHECK(fabsf(input->player.position.z - 0.65f) < 0.0001f && input->player.position.y == 40);
  pressedKey = -1;
  key(window, GLFW_KEY_SPACE, 0, GLFW_PRESS, 0);
  key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  Vec3 paused = input->player.position;
  processInput(window, input, 10);
  CHECK(input->simulationSteps == 0 && !input->jumpRequested && !input->player.jumpPending);
  CHECK(input->player.position.y == paused.y && input->player.position.z == paused.z);
  key(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  CHECK(!input->flying);
  key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  processInput(window, input, 1.0 / 30);
  CHECK(input->player.grounded); // Pausing discarded the queued jump.
  key(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  camera->position = (Vec3){0.5f, 40.5f, 2.5f}; // Inside the wall.
  key(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  CHECK(!input->flying && playerCanOccupy(input->player.position) && input->player.grounded);
  CHECK(input->player.position.y == 43); // Return to flight's nearest safe surface.
  key(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  camera->position = (Vec3){1000, 1000, -1000};
  key(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  CHECK(!input->flying && playerCanOccupy(input->player.position));
  CHECK(setBlock(&(Vec3i){4, 0, 4}, BLOCK_STONE));
  for (int y = 1; y < 4; y++)
    CHECK(setBlock(&(Vec3i){4, y, 4}, BLOCK_AIR));
  CHECK(setBlock(&(Vec3i){4, 4, 4}, BLOCK_STONE));
  CHECK(playerSetPosition(&input->player, (Vec3){4.6f, 1, 4.5f}));
  camera->position = playerEyePosition(&input->player);
  key(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  key(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  CHECK(!input->flying && input->player.position.x == 4.6f && input->player.position.y == 1 && input->player.position.z == 4.5f);
  key(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  camera->position.z += 0.1f;
  key(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  CHECK(!input->flying && input->player.position.y == 1 && input->player.position.z == 4.6f);
  key(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  initCamera(camera);
}

static void prepareMousePause(GLFWwindow* window) {
  Camera* camera = ((InputState*)glfwGetWindowUserPointer(window))->camera;
  yawBeforeMinimize = camera->yaw;
  pitchBeforeMinimize = camera->pitch;
  setCursorCaptured(window, true);
  mouseCallback(window, 100, 100);
}

static void checkRestoredMouse(GLFWwindow* window) {
  Camera* camera = ((InputState*)glfwGetWindowUserPointer(window))->camera;
  // No cursor or focus event was delivered during the pause.
  mouseCallback(window, 5000, -5000);
  CHECK(camera->yaw == yawBeforeMinimize && camera->pitch == pitchBeforeMinimize);
  mouseCallback(window, 5020, -5020);
  CHECK(fabsf(camera->yaw - yawBeforeMinimize - 1) < 0.001f);
  CHECK(fabsf(camera->pitch - pitchBeforeMinimize - 1) < 0.001f);
  camera->yaw = yawBeforeMinimize;
  camera->pitch = pitchBeforeMinimize;
  updateCameraVectors(camera);
}

static void walkingFrame(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  Camera* camera = input->camera;
  GLFWkeyfun key = glfwSetKeyCallback(window, NULL);
  glfwSetKeyCallback(window, key);
  if (frame == 4) {
    camera->position = (Vec3){0.5f, 40 + PLAYER_EYE_HEIGHT, 0.5f};
    key(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
    CHECK(!input->flying);
    pressedKey = GLFW_KEY_W;
  }
  if (frame == 19) {
    CHECK(fabsf(input->player.position.z - 1.7f) < 0.00001f && input->player.grounded);
    pressedKey = GLFW_KEY_SPACE;
    key(window, GLFW_KEY_SPACE, 0, GLFW_PRESS, 0);
  }
  if (frame == 20)
    CHECK(input->player.position.y > 40 && !input->player.grounded);
  if (frame > 19 && frame < 45)
    key(window, GLFW_KEY_SPACE, 0, GLFW_REPEAT, 0);
  if (frame == 45) {
    CHECK(input->player.position.y == 40 && input->player.grounded); // Held jump did not repeat.
    pressedKey = -1;
    camera->pitch = -89;
    updateCameraVectors(camera);
    GLFWmousebuttonfun click = glfwSetMouseButtonCallback(window, NULL);
    glfwSetMouseButtonCallback(window, click);
    click(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
    CHECK(getBlock(&(Vec3i){0, 40, 1})->id == BLOCK_AIR); // Placement cannot overlap the actual feet/body.
    click(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
    CHECK(getBlock(&(Vec3i){0, 39, 1})->id == BLOCK_AIR);
    key(window, GLFW_KEY_SPACE, 0, GLFW_PRESS, 0);
  }
  if (frame == 46) {
    CHECK(input->player.position.y < 40 && input->player.velocity.y < 0);
    prepareMousePause(window);
  }
  if (frame == 47) {
    beforeMinimize = camera->position;
    pressedKey = GLFW_KEY_W;
    input->player.accumulator = PLAYER_STEP_SECONDS / 2;
    input->player.jumpPending = true;
  }
  if (frame == 48) {
    CHECK(camera->position.x == beforeMinimize.x && camera->position.y == beforeMinimize.y && camera->position.z == beforeMinimize.z);
    CHECK(input->player.accumulator == 0 && !input->player.jumpPending);
    checkRestoredMouse(window);
    key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  }
  if (frame == 49) {
    CHECK(camera->position.x == beforeMinimize.x && camera->position.y == beforeMinimize.y && camera->position.z == beforeMinimize.z);
    pressedKey = -1;
    key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  }
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
    testIconifiedInput(window);
    testSavedInput(window);
    testWalkingControls(window);
    testEditing(window);
    InputState* input = glfwGetWindowUserPointer(window);
    Camera* camera = input->camera;
    camera->position.x = -0.5f;
    camera->position.y = 40.5f;
    CHECK(setBlock(&editFixture, BLOCK_STONE));
  }
  if (frame == 0) {
    InputState* input = glfwGetWindowUserPointer(window);
    Camera* camera = input->camera;
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
    InputState* input = glfwGetWindowUserPointer(window);
    Camera* camera = input->camera;
    CHECK(camera->position.x == beforeMinimize.x && camera->position.y == beforeMinimize.y && camera->position.z == beforeMinimize.z);
    pressedKey = -1;
    CHECK(setBlock(&(Vec3i){-1, 40, 7}, BLOCK_STONE));
  }
  frame++;
  if (frame == 0)
    pressedKey = GLFW_KEY_W;
  if (frame == 2) {
    beforeMinimize = ((InputState*)glfwGetWindowUserPointer(window))->camera->position;
    pressedKey = GLFW_KEY_W;
  }
  if (frame < 4 && frame != 2) {
    glfwSetWindowSize(window, sizes[frame][0], sizes[frame][1]);
    glfwPollEvents();
  }
  if (frame == 1)
    prepareMousePause(window);
  if (frame == 3) {
    checkRestoredMouse(window);
    GLFWkeyfun key = glfwSetKeyCallback(window, NULL);
    glfwSetKeyCallback(window, key);
    key(window, GLFW_KEY_F3, 0, GLFW_PRESS, 0);
    GLFWmousebuttonfun click = glfwSetMouseButtonCallback(window, NULL);
    glfwSetMouseButtonCallback(window, click);
    click(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
    CHECK(getBlock(&editFixture)->id == BLOCK_GRASS);
  }
  if (frame >= 4 && frame < 50)
    walkingFrame(window);
  return frame >= 50;
}

double __wrap_glfwGetTime(void) {
  return frame < 0 ? 0.0 : frame < 4 ? (double)(frame + 1) : 4.0 + (frame - 3) / 30.0;
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
  if (attrib == GLFW_ICONIFIED)
    return iconified || frame == 47;
  return attrib == GLFW_FOCUSED ? focused : __real_glfwGetWindowAttrib(window, attrib);
}

int __wrap_glfwGetKey(GLFWwindow* window, int key) {
  (void)window;
  return key == pressedKey ? GLFW_PRESS : GLFW_RELEASE;
}

void __wrap_glfwGetFramebufferSize(GLFWwindow* window, int* width, int* height) {
  (void)window;
  int index = frame < 0 ? 0 : frame > 3 ? 3 : frame;
  *width = sizes[index][0];
  *height = sizes[index][1];
}

void __wrap_glfwWaitEvents(void) {
  CHECK(frame == 2 || frame == 47);
  GLFWkeyfun key = glfwSetKeyCallback(glfwGetCurrentContext(), NULL);
  glfwSetKeyCallback(glfwGetCurrentContext(), key);
  key(glfwGetCurrentContext(), GLFW_KEY_3, 0, GLFW_PRESS, 0);
  InputState* input = glfwGetWindowUserPointer(glfwGetCurrentContext());
  bool debug = input->showDebug;
  key(glfwGetCurrentContext(), GLFW_KEY_F3, 0, GLFW_PRESS, 0);
  CHECK(input->showDebug == debug);
  CHECK(selectedBlock() == BLOCK_GRASS);
  waits++;
}

void __real_HUDDraw(GLuint program, DebugData* data);
void __wrap_HUDDraw(GLuint program, DebugData* data) {
  InputState* input = glfwGetWindowUserPointer(glfwGetCurrentContext());
  CHECK(data->showDebug == input->showDebug);
  sawCompactHUD |= !data->showDebug;
  sawDebugHUD |= data->showDebug;
  __real_HUDDraw(program, data);
}

static void captureFrame(int width, int height, const unsigned char* pixels) {
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
}

void __real_glfwSwapBuffers(GLFWwindow* window);
void __wrap_glfwSwapBuffers(GLFWwindow* window) {
  if (frame >= 4) {
    InputState* input = glfwGetWindowUserPointer(window);
    Vec3 eye = playerEyePosition(&input->player);
    CHECK(frame < 50 && frame != 47 && !input->flying);
    CHECK(playerCanOccupy(input->player.position));
    CHECK(input->camera->position.x == eye.x && input->camera->position.y == eye.y && input->camera->position.z == eye.z);
    CHECK(input->simulationSteps == (frame == 48 ? 0 : 4));
    if ((frame == 18 || frame == 20) && getenv("KERNELCRAFT_TEST_CAPTURE")) {
      unsigned char* pixels = malloc(1280 * 720 * 3);
      CHECK(pixels);
      glReadPixels(0, 0, 1280, 720, GL_RGB, GL_UNSIGNED_BYTE, pixels);
      captureFrame(1280, 720, pixels);
      free(pixels);
    }
    CHECK(glGetError() == GL_NO_ERROR);
    swaps++;
    __real_glfwSwapBuffers(window);
    return;
  }
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

  captureFrame(width, height, pixels);
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
    CHECK(swaps == 48 && waits == 2);
    CHECK(sawCompactHUD && sawDebugHUD);
    puts("Application walking, jumping, flight, editing, selection pixels, pause, framebuffer, and shutdown tests passed");
  }
  __real_glfwDestroyWindow(window);
}
