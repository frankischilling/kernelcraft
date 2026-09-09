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
static bool shiftHeld, sideHeld, backHeld, zeroFramebuffer;
static double eventSeconds = -1;
static float previousProjectionScale;
static int frame = -1;
static int swaps, waits;
static bool sawCompactHUD, sawDebugHUD;
static Vec3 beforeMinimize;
static float yawBeforeMinimize, pitchBeforeMinimize;
static const Vec3i editFixture = {-1, 40, 6};
static const int sizes[][2] = {{640, 360}, {360, 640}, {0, 0}, {1280, 720}};

#define CHECK(condition)                                                                                                                                                           \
  do {                                                                                                                                                                             \
    if (!(condition)) {                                                                                                                                                            \
      fprintf(stderr, "Application smoke test: %s (line %d)\n", #condition, __LINE__);                                                                                             \
      exit(EXIT_FAILURE);                                                                                                                                                          \
    }                                                                                                                                                                              \
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
  int slot = selectedHotbarSlot();
  bool debug = input->showDebug;
  // Retain focus and framebuffer dimensions to isolate iconification from
  // platform-dependent resize/focus callback ordering.
  iconified = true;
  key(window, GLFW_KEY_F3, 0, GLFW_PRESS, 0);
  CHECK(input->showDebug == debug);
  key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  CHECK(cursorMode == GLFW_CURSOR_DISABLED);
  key(window, GLFW_KEY_9, 0, GLFW_PRESS, 0);
  CHECK(selectedBlock() == material && selectedHotbarSlot() == slot);
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
  input->wireframe = true;
  Camera restoredCamera = *camera;
  InputState restored;
  CHECK(initSavedInputs(&restored, &restoredCamera, &saved));
  CHECK(!restored.flying && restored.player.velocity.y == 0 && restored.player.position.y == 40);
  CHECK(!restored.wireframe && input->wireframe);
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

static void testWireframeInput(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  CHECK(!input->wireframe);
  Camera camera = *input->camera;
  Player player = input->player;
  int slot = selectedHotbarSlot();
  keyCallback(window, GLFW_KEY_F4, 0, GLFW_PRESS, 0);
  CHECK(input->wireframe);
  keyCallback(window, GLFW_KEY_F4, 0, GLFW_REPEAT, 0);
  keyCallback(window, GLFW_KEY_F4, 0, GLFW_RELEASE, 0);
  CHECK(input->wireframe);
  setCursorCaptured(window, false);
  CHECK(input->wireframe);
  keyCallback(window, GLFW_KEY_F4, 0, GLFW_PRESS, 0);
  CHECK(!input->wireframe && cursorMode == GLFW_CURSOR_NORMAL);
  keyCallback(window, GLFW_KEY_F4, 0, GLFW_PRESS, 0);
  CHECK(input->wireframe && cursorMode == GLFW_CURSOR_NORMAL);
  for (int state = 0; state < 3; state++) {
    focused = state == 0 ? GLFW_FALSE : GLFW_TRUE;
    iconified = state == 1;
    zeroFramebuffer = state == 2;
    keyCallback(window, GLFW_KEY_F4, 0, GLFW_PRESS, 0);
    CHECK(input->wireframe);
  }
  focused = GLFW_TRUE;
  iconified = zeroFramebuffer = false;
  setCursorCaptured(window, true);
  CHECK(input->wireframe);
  keyCallback(window, GLFW_KEY_F4, 0, GLFW_PRESS, 0);
  CHECK(!input->wireframe && !input->saveRequested && selectedHotbarSlot() == slot);
  CHECK(input->camera->position.x == camera.position.x && input->camera->position.y == camera.position.y && input->camera->position.z == camera.position.z);
  CHECK(input->player.position.x == player.position.x && input->player.position.y == player.position.y && input->player.position.z == player.position.z);
  bool flying = input->flying;
  keyCallback(window, GLFW_KEY_F4, 0, GLFW_PRESS, 0);
  keyCallback(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  CHECK(input->wireframe && input->flying != flying);
  keyCallback(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  CHECK(input->wireframe && input->flying == flying);
  keyCallback(window, GLFW_KEY_F4, 0, GLFW_PRESS, 0);
  CHECK(!input->wireframe);
}

static void testCrouchControl(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  Camera* camera = input->camera;
  Camera originalCamera = *camera;
  InputState originalInput = *input;
  GLFWkeyfun key = glfwSetKeyCallback(window, NULL);
  GLFWmousebuttonfun click = glfwSetMouseButtonCallback(window, NULL);
  glfwSetKeyCallback(window, key);
  glfwSetMouseButtonCallback(window, click);
  for (int z = -42; z <= -32; z++) {
    CHECK(setBlock(&(Vec3i){-41, 39, z}, BLOCK_STONE));
    for (int y = 40; y <= 44; y++)
      CHECK(setBlock(&(Vec3i){-41, y, z}, BLOCK_AIR));
  }
  CHECK(playerSetPosition(&input->player, (Vec3){-40.5f, 40, -40.5f}));
  input->flying = false;
  camera->yaw = 90;
  camera->pitch = 0;
  updateCameraVectors(camera);
  pressedKey = GLFW_KEY_LEFT_SHIFT;
  processInput(window, input, PLAYER_STEP_SECONDS);
  CHECK(fabsf(camera->position.y - input->player.position.y - 0.9f) < 0.00001f);
  // Placement uses the crouched head instead of a stale standing exclusion box.
  CHECK(setBlock(&(Vec3i){-41, 42, -41}, BLOCK_STONE));
  camera->front = (Vec3){0, 1, 0};
  click(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
  CHECK(getBlock(&(Vec3i){-41, 41, -41})->id == BLOCK_GRASS);
  camera->front = (Vec3){0, -1, 0};
  click(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
  CHECK(getBlock(&(Vec3i){-41, 40, -41})->id == BLOCK_AIR);
  SavedPlayer saved;
  Vec3 liveFeet = input->player.position;
  CHECK(snapshotPlayer(input, &saved));
  CHECK(saved.feet.x == -40.5f && saved.feet.y == 43 && saved.feet.z == -40.5f);
  CHECK(input->player.position.y == liveFeet.y && input->player.crouched);
  Camera restoredCamera = *camera;
  InputState restored;
  CHECK(initSavedInputs(&restored, &restoredCamera, &saved));
  CHECK(!restored.player.crouched && !restored.player.running && restored.player.position.y == 43);
  // Released Shift cannot force a stand, including after capture/focus changes.
  pressedKey = -1;
  processInput(window, input, PLAYER_STEP_SECONDS);
  CHECK(input->player.crouched && input->player.position.y == 40);
  for (int pause = 0; pause < 4; pause++) {
    if (pause == 0)
      setCursorCaptured(window, false);
    if (pause == 1) {
      focused = GLFW_FALSE;
      windowFocusCallback(window, focused);
    }
    iconified = pause == 2;
    zeroFramebuffer = pause == 3;
    float eye = camera->position.y;
    processInput(window, input, 10);
    CHECK(input->player.crouched && input->simulationSteps == 0 && camera->position.y == eye);
    focused = GLFW_TRUE;
    iconified = zeroFramebuffer = false;
    setCursorCaptured(window, true);
    processInput(window, input, PLAYER_STEP_SECONDS);
    CHECK(input->player.crouched && input->player.position.y == 40);
  }
  updateCameraVectors(camera);
  pressedKey = GLFW_KEY_W;
  for (int i = 0; i < 100; i++)
    processInput(window, input, PLAYER_STEP_SECONDS);
  CHECK(!input->player.crouched && input->player.position.z > -39.7f);
  CHECK(fabsf(camera->position.y - 41.62f) < 0.00001f);
  pressedKey = GLFW_KEY_RIGHT_SHIFT;
  processInput(window, input, PLAYER_STEP_SECONDS);
  CHECK(input->player.crouched); // Both Shift keys crouch when walking.
  Vec3 eye = camera->position;
  key(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  CHECK(input->flying && camera->position.y == eye.y && !input->runInput.running);
  pressedKey = GLFW_KEY_LEFT_SHIFT;
  processInput(window, input, 0.01);
  CHECK(fabsf(camera->position.y - eye.y + 0.1f) < 0.00001f);
  key(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  CHECK(!input->flying && !input->player.crouched && playerCanOccupy(input->player.position));
  CHECK(setBlock(&(Vec3i){-45, 39, -45}, BLOCK_STONE));
  CHECK(playerSetPosition(&input->player, (Vec3){-44.5f, 40, -44.5f}));
  camera->yaw = 90;
  camera->pitch = 0;
  updateCameraVectors(camera);
  shiftHeld = true;
  pressedKey = GLFW_KEY_W;
  for (int i = 0; i < 60; i++) {
    processInput(window, input, 1.0 / 30);
    CHECK(input->player.grounded && input->player.position.y == 40);
  }
  CHECK(input->player.position.z > -44 && input->player.position.z < -43.69f);
  CHECK(setBlock(&(Vec3i){-45, 39, -45}, BLOCK_AIR));
  processInput(window, input, 1.0 / 30);
  CHECK(input->player.position.y < 40 && !input->player.grounded);
  shiftHeld = false;
  pressedKey = -1;
  *camera = originalCamera;
  *input = originalInput;
}

static void startRunning(GLFWwindow* window, GLFWkeyfun key) {
  InputState* input = glfwGetWindowUserPointer(window);
  playerResetRunInput(&input->runInput);
  pressedKey = GLFW_KEY_W;
  eventSeconds = 10;
  key(window, GLFW_KEY_W, 0, GLFW_PRESS, 0);
  eventSeconds = 10.1;
  key(window, GLFW_KEY_W, 0, GLFW_RELEASE, 0);
  eventSeconds = 10.25;
  key(window, GLFW_KEY_W, 0, GLFW_PRESS, 0);
  CHECK(input->runInput.running);
}

static void testRunningControls(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  Camera* camera = input->camera;
  Camera originalCamera = *camera;
  InputState originalInput = *input;
  GLFWkeyfun key = glfwSetKeyCallback(window, NULL);
  glfwSetKeyCallback(window, key);
  input->flying = false;
  camera->yaw = 90;
  camera->pitch = 89;
  updateCameraVectors(camera);
  CHECK(playerSetPosition(&input->player, (Vec3){-30.5f, 40, -30.5f}));
  startRunning(window, key);
  processInput(window, input, 1.0 / 30);
  CHECK(input->player.running && fabsf(input->player.position.z + 30.5f - 7.0f / 30) < 0.00001f);
  Vec3 before = input->player.position;
  sideHeld = true;
  processInput(window, input, 1.0 / 30);
  CHECK(fabsf(hypotf(input->player.position.x - before.x, input->player.position.z - before.z) - 7.0f / 30) < 0.00001f);
  sideHeld = false;
  key(window, GLFW_KEY_W, 0, GLFW_RELEASE, 0);
  CHECK(!input->runInput.running && !input->player.running);
  // Expired taps and key repeats cannot create a run.
  key(window, GLFW_KEY_W, 0, GLFW_PRESS, 0);
  key(window, GLFW_KEY_W, 0, GLFW_REPEAT, 0);
  CHECK(!input->runInput.running);
  key(window, GLFW_KEY_W, 0, GLFW_RELEASE, 0);
  eventSeconds += 0.251;
  key(window, GLFW_KEY_W, 0, GLFW_PRESS, 0);
  CHECK(!input->runInput.running);
  // S or crouch cancels even if the key is pressed and released between frames.
  const int cancelKeys[] = {GLFW_KEY_S, GLFW_KEY_LEFT_SHIFT, GLFW_KEY_RIGHT_SHIFT};
  for (int i = 0; i < 3; i++) {
    startRunning(window, key);
    key(window, cancelKeys[i], 0, GLFW_PRESS, 0);
    key(window, cancelKeys[i], 0, GLFW_RELEASE, 0);
    CHECK(!input->runInput.running && !input->runInput.tapPending);
    processInput(window, input, PLAYER_STEP_SECONDS);
    CHECK(!input->player.running);
  }
  // Held S/Shift also cancels when event delivery was absent.
  startRunning(window, key);
  backHeld = true;
  processInput(window, input, PLAYER_STEP_SECONDS);
  CHECK(!input->player.running && !input->runInput.tapPending);
  backHeld = false;
  startRunning(window, key);
  shiftHeld = true;
  processInput(window, input, PLAYER_STEP_SECONDS);
  CHECK(input->player.crouched && !input->player.running && !input->runInput.tapPending);
  shiftHeld = false;
  processInput(window, input, PLAYER_STEP_SECONDS);
  // Every pause path resets an active run and the pending first tap.
  for (int pending = 0; pending < 2; pending++)
    for (int pause = 0; pause < 4; pause++) {
      startRunning(window, key);
      if (pending) {
        playerResetRunInput(&input->runInput);
        key(window, GLFW_KEY_W, 0, GLFW_PRESS, 0);
        key(window, GLFW_KEY_W, 0, GLFW_RELEASE, 0);
        CHECK(input->runInput.tapPending);
      }
      updateCameraFov(camera, true, 0.1);
      CHECK(camera->fov > 70);
      before = input->player.position;
      if (pause == 0)
        key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
      if (pause == 1) {
        focused = GLFW_FALSE;
        windowFocusCallback(window, focused);
      }
      iconified = pause == 2;
      zeroFramebuffer = pause == 3;
      processInput(window, input, 10);
      CHECK(!input->player.running && !input->runInput.running && !input->runInput.tapPending);
      CHECK(camera->fov == 70);
      CHECK(input->player.position.x == before.x && input->player.position.y == before.y && input->player.position.z == before.z);
      focused = GLFW_TRUE;
      iconified = zeroFramebuffer = false;
      setCursorCaptured(window, true);
      processInput(window, input, PLAYER_STEP_SECONDS);
      CHECK(!input->player.running);
      key(window, GLFW_KEY_W, 0, GLFW_RELEASE, 0);
      key(window, GLFW_KEY_W, 0, GLFW_PRESS, 0);
      CHECK(!input->runInput.running); // A fresh first tap after resume stays walking.
    }
  startRunning(window, key);
  key(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
  CHECK(input->flying && !input->runInput.running && !input->runInput.tapPending && !input->player.running);
  CHECK(camera->fov == 70);
  key(window, GLFW_KEY_W, 0, GLFW_RELEASE, 0);
  key(window, GLFW_KEY_W, 0, GLFW_PRESS, 0);
  CHECK(!input->runInput.tapPending);
  pressedKey = -1;
  eventSeconds = -1;
  *camera = originalCamera;
  *input = originalInput;
}

static void testCameraCue(void) {
  Camera fine, coarse;
  initCamera(&fine);
  coarse = fine;
  for (int i = 0; i < 60; i++)
    updateCameraFov(&fine, true, 1.0 / 60);
  for (int i = 0; i < 30; i++)
    updateCameraFov(&coarse, true, 1.0 / 30);
  CHECK(fine.fov > 79.9f && fine.fov <= 80 && fabsf(fine.fov - coarse.fov) < 0.0001f);
  for (int i = 0; i < 60; i++)
    updateCameraFov(&fine, false, 1.0 / 60);
  CHECK(fine.fov >= 70 && fine.fov < 70.01f);
  float before = fine.fov;
  updateCameraFov(&fine, true, NAN);
  updateCameraFov(&fine, true, -1);
  updateCameraFov(&fine, true, 0);
  CHECK(fine.fov == before);
  initCamera(&fine);
  coarse = fine;
  updateCameraFov(&fine, true, 1000);
  updateCameraFov(&coarse, true, 0.1);
  CHECK(fine.fov == coarse.fov && fine.fov < 80);
}

// Complete a held press without advancing physics or changing the test camera.
static void finishHandBreak(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  Ray target = rayCast(input->camera->position, input->camera->front, EDIT_REACH);
  CHECK(target.hit && getBlock(&target.blockCoords)->id != BLOCK_AIR);
  for (int frame = 0; frame < 120 && getBlock(&target.blockCoords)->id != BLOCK_AIR; frame++)
    processBlockBreaking(window, input, 1.0 / 60);
  CHECK(getBlock(&target.blockCoords)->id == BLOCK_AIR);
  mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
}

static void testBreakingCancellation(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  InputState saved = *input;
  Camera camera = *input->camera;
  int slot = selectedHotbarSlot();
  Vec3i target = {50, 41, 53};
  CHECK(setBlock(&(Vec3i){50, 39, 50}, BLOCK_STONE));
  for (int y = 40; y <= 42; y++)
    for (int z = 50; z <= 55; z++)
      CHECK(setBlock(&(Vec3i){50, y, z}, BLOCK_AIR));
  CHECK(playerSetPosition(&input->player, (Vec3){50.5f, 40, 50.5f}));
  input->camera->position = playerEyePosition(&input->player);
  input->camera->front = (Vec3){0, 0, 1};
  for (int flying = 0; flying <= 1; flying++) {
    for (int reason = 0; reason < 8; reason++) {
      input->flying = flying != 0;
      CHECK(setBlock(&target, BLOCK_DIRT));
      mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
      processBlockBreaking(window, input, 0.1);
      CHECK(input->breakHeld && blockBreakingProgress(&input->breaking) > 0);
      if (reason == 0)
        mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
      if (reason == 1)
        keyCallback(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
      if (reason == 2) {
        focused = GLFW_FALSE;
        windowFocusCallback(window, GLFW_FALSE);
      }
      iconified = reason == 3;
      zeroFramebuffer = reason == 4;
      if (reason == 5)
        keyCallback(window, selectedHotbarSlot() == 8 ? GLFW_KEY_1 : GLFW_KEY_9, 0, GLFW_PRESS, 0);
      if (reason == 6)
        mouseButtonCallback(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
      if (reason == 7)
        keyCallback(window, GLFW_KEY_F, 0, GLFW_PRESS, 0);
      processBlockBreaking(window, input, 10);
      CHECK(!input->breakHeld && !input->breaking.active && getBlock(&target)->id == BLOCK_DIRT);
      focused = GLFW_TRUE;
      iconified = zeroFramebuffer = false;
      if (cursorMode != GLFW_CURSOR_DISABLED)
        setCursorCaptured(window, true);
      CHECK(setBlock(&(Vec3i){50, 41, 52}, BLOCK_AIR)); // Clear a possible right-click placement.
      for (int i = 0; i < 20; i++)
        processBlockBreaking(window, input, 0.1);
      CHECK(getBlock(&target)->id == BLOCK_DIRT && blockBreakingProgress(&input->breaking) == 0);
      mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
      mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_REPEAT, 0);
      finishHandBreak(window);
    }
  }
  keyCallback(window, GLFW_KEY_1 + slot, 0, GLFW_PRESS, 0);
  *input->camera = camera;
  *input = saved;
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
  for (int number = GLFW_KEY_4; number <= GLFW_KEY_6; number++) {
    int material = number - GLFW_KEY_1 + 1;
    key(window, number, 0, GLFW_PRESS, 0);
    CHECK(selectedHotbarSlot() == material - 1 && selectedBlock() == material);
    key(window, GLFW_KEY_1, 0, GLFW_REPEAT, 0);
    CHECK(selectedHotbarSlot() == material - 1 && selectedBlock() == material);
    click(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
    CHECK(getBlock(&placement)->id == material);
    click(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
    // A press must start hand breaking without removing the block immediately.
    CHECK(getBlock(&placement)->id == material);
    finishHandBreak(window);
    CHECK(getBlock(&placement)->id == BLOCK_AIR && getBlock(&target)->id == BLOCK_STONE);
  }
  for (int number = GLFW_KEY_7; number <= GLFW_KEY_9; number++) {
    key(window, number, 0, GLFW_PRESS, 0);
    CHECK(selectedBlock() == BLOCK_AIR);
    CHECK(selectedHotbarSlot() == number - GLFW_KEY_1);
    key(window, GLFW_KEY_1, 0, GLFW_REPEAT, 0);
    CHECK(selectedHotbarSlot() == number - GLFW_KEY_1);
    click(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
    CHECK(getBlock(&placement)->id == BLOCK_AIR);
  }
  click(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
  finishHandBreak(window);
  CHECK(getBlock(&target)->id == BLOCK_AIR);
  CHECK(setBlock(&target, BLOCK_STONE));
  key(window, GLFW_KEY_2, 0, GLFW_PRESS, 0);
  CHECK(selectedBlock() == BLOCK_DIRT);
  key(window, GLFW_KEY_3, 0, GLFW_REPEAT, 0);
  CHECK(selectedBlock() == BLOCK_DIRT);
  click(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_RELEASE, 0);
  CHECK(getBlock(&placement)->id == BLOCK_AIR);
  click(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
  CHECK(getBlock(&placement)->id == BLOCK_DIRT);
  click(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
  finishHandBreak(window);
  CHECK(getBlock(&placement)->id == BLOCK_AIR && getBlock(&target)->id == BLOCK_STONE);
  click(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_REPEAT, 0);
  CHECK(getBlock(&target)->id == BLOCK_STONE);
  key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  click(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
  key(window, GLFW_KEY_9, 0, GLFW_PRESS, 0);
  CHECK(getBlock(&target)->id == BLOCK_STONE && selectedBlock() == BLOCK_DIRT && selectedHotbarSlot() == 1);
  key(window, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  focused = GLFW_FALSE;
  click(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
  CHECK(getBlock(&target)->id == BLOCK_STONE);
  focused = GLFW_TRUE;
  camera->position.z = -1.1f;
  click(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS, 0);
  CHECK(getBlock(&placement)->id == BLOCK_AIR);
  click(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
  finishHandBreak(window);
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
    finishHandBreak(window);
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

static Vec3 beforeMovementFrame;
static void movementFrame(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  Camera* camera = input->camera;
  GLFWkeyfun key = glfwSetKeyCallback(window, NULL);
  glfwSetKeyCallback(window, key);
  if (frame == 50) {
    CHECK(setBlock(&(Vec3i){-31, 39, -31}, BLOCK_STONE));
    CHECK(playerSetPosition(&input->player, (Vec3){-30.5f, 40, -30.5f}));
    camera->yaw = 90;
    camera->pitch = 0;
    updateCameraVectors(camera);
    pressedKey = GLFW_KEY_LEFT_SHIFT;
  }
  if (frame == 51) {
    CHECK(input->player.crouched && fabsf(camera->position.y - 40.9f) < 0.00001f);
    pressedKey = -1;
  }
  if (frame == 52) {
    CHECK(!input->player.crouched && fabsf(camera->position.y - 41.62f) < 0.00001f);
    startRunning(window, key);
    eventSeconds = -1;
    beforeMovementFrame = input->player.position;
  }
  if (frame == 53) {
    CHECK(input->player.running && fabsf(input->player.position.z - beforeMovementFrame.z - 7.0f / 30) < 0.00001f);
    beforeMovementFrame = input->player.position;
    sideHeld = true;
  }
  if (frame == 54) {
    CHECK(fabsf(hypotf(input->player.position.x - beforeMovementFrame.x, input->player.position.z - beforeMovementFrame.z) - 7.0f / 30) < 0.00001f);
    sideHeld = false;
    pressedKey = -1;
    key(window, GLFW_KEY_W, 0, GLFW_RELEASE, 0);
  }
  if (frame == 55)
    CHECK(!input->player.running && input->player.velocity.x == 0 && input->player.velocity.z == 0);
}

// Drive a whole held break through main's frame loop, mesh upload, and HUD.
static const Vec3i heldTarget = {-32, 41, -29};
static void breakingFrame(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  if (frame == 55) {
    for (int y = 40; y <= 42; y++)
      for (int z = -32; z <= -28; z++)
        CHECK(setBlock(&(Vec3i){-32, y, z}, BLOCK_AIR));
    CHECK(setBlock(&(Vec3i){-32, 39, -32}, BLOCK_STONE));
    CHECK(playerSetPosition(&input->player, (Vec3){-31.5f, 40, -31.5f}));
    input->camera->position = playerEyePosition(&input->player);
    input->camera->yaw = 90;
    input->camera->pitch = 0;
    updateCameraVectors(input->camera);
    CHECK(setBlock(&heldTarget, BLOCK_DIRT));
    mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
    keyCallback(window, GLFW_KEY_F4, 0, GLFW_PRESS, 0);
    CHECK(input->wireframe && input->breakHeld);
  }
  if (frame < 70)
    CHECK(getBlock(&heldTarget)->id == BLOCK_DIRT);
  else {
    CHECK(getBlock(&heldTarget)->id == BLOCK_AIR);
    CHECK(!input->breaking.active);
    mouseButtonCallback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
  }
}

static void wireframeFrame(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  if (frame == 70) {
    keyCallback(window, GLFW_KEY_F4, 0, GLFW_PRESS, 0);
    CHECK(!input->wireframe);
    input->flying = true;
    input->camera->position = (Vec3){0, 52, -72};
    input->camera->yaw = 90;
    input->camera->pitch = 0;
    updateCameraVectors(input->camera);
    for (int x = -4; x < 4; x++)
      for (int y = 48; y < 56; y++)
        CHECK(setBlock(&(Vec3i){x, y, -64}, BLOCK_STONE));
  }
  if (frame == 71 || frame == 73)
    keyCallback(window, GLFW_KEY_F4, 0, GLFW_PRESS, 0);
  if (frame == 72) {
    keyCallback(window, GLFW_KEY_F4, 0, GLFW_REPEAT, 0);
    keyCallback(window, GLFW_KEY_F4, 0, GLFW_RELEASE, 0);
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
    testCameraCue();
    testInput(window);
    testWireframeInput(window);
    testIconifiedInput(window);
    testSavedInput(window);
    testCrouchControl(window);
    testRunningControls(window);
    testWalkingControls(window);
    testBreakingCancellation(window);
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
    finishHandBreak(window);
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
  if (frame >= 50 && frame <= 55)
    movementFrame(window);
  if (frame >= 55 && frame <= 70)
    breakingFrame(window);
  if (frame >= 70)
    wireframeFrame(window);
  return frame >= 74;
}

double __wrap_glfwGetTime(void) {
  if (eventSeconds >= 0)
    return eventSeconds;
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
  return key == pressedKey || (shiftHeld && key == GLFW_KEY_LEFT_SHIFT) || (sideHeld && key == GLFW_KEY_D) || (backHeld && key == GLFW_KEY_S) ? GLFW_PRESS : GLFW_RELEASE;
}

void __wrap_glfwGetFramebufferSize(GLFWwindow* window, int* width, int* height) {
  (void)window;
  int index = frame < 0 ? 0 : frame > 3 ? 3 : frame;
  *width = zeroFramebuffer ? 0 : sizes[index][0];
  *height = zeroFramebuffer ? 0 : sizes[index][1];
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
  bool wireframe = input->wireframe;
  key(glfwGetCurrentContext(), GLFW_KEY_F4, 0, GLFW_PRESS, 0);
  CHECK(input->wireframe == wireframe);
  CHECK(selectedBlock() == BLOCK_GRASS);
  waits++;
}

void __real_HUDDraw(GLuint program, DebugData* data);
void __wrap_HUDDraw(GLuint program, DebugData* data) {
  InputState* input = glfwGetWindowUserPointer(glfwGetCurrentContext());
  CHECK(data->showDebug == input->showDebug);
  CHECK(data->wireframe == input->wireframe);
  CHECK(data->crouched == input->player.crouched && data->running == input->player.running);
  sawCompactHUD |= !data->showDebug;
  sawDebugHUD |= data->showDebug;
  if (frame == 60) {
    // This selected face interior is away from mesh diagonals and the outline.
    // The gold tint must still fill it when the terrain itself is unfilled.
    unsigned char tint[3];
    glReadPixels(660, 375, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, tint);
    CHECK(input->wireframe && tint[0] > 30 && tint[0] < 60 && tint[1] > 25 && tint[1] < 50 && tint[2] < 15);
  }
  if (frame >= 70) {
    static RenderResult solidStats;
    CHECK(input->wireframe == (frame == 71 || frame == 72));
    // The wall is eight units away, beyond selection reach. Unfilled terrain
    // must expose its interior without a face tint or HUD covering the probe.
    unsigned char pixels[96 * 96 * 3];
    glReadPixels(592, 312, 96, 96, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    int lit = 0;
    for (size_t i = 0; i < sizeof(pixels); i += 3)
      lit += pixels[i] || pixels[i + 1] || pixels[i + 2];
    bool wireframe = frame == 71 || frame == 72;
    printf("Application terrain frame %d: %d/9216 lit pixels\n", frame, lit);
    CHECK(wireframe ? lit > 50 && lit < 1500 : lit > 9000);
    if (frame == 70)
      solidStats = *data->stats;
    else {
      CHECK(data->stats->chunksRebuilt == 0);
      CHECK(data->stats->terrainDrawCalls == solidStats.terrainDrawCalls && data->stats->submittedTriangles == solidStats.submittedTriangles);
    }
  }
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
  if (frame >= 70) {
    CHECK(frame < 74 && glGetError() == GL_NO_ERROR);
    unsigned char crosshair[3];
    glReadPixels(644, 360, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, crosshair);
    CHECK(crosshair[0] > 240 && crosshair[1] > 240 && crosshair[2] > 240);
    if (getenv("KERNELCRAFT_TEST_CAPTURE")) {
      unsigned char* pixels = malloc(1280 * 720 * 3);
      CHECK(pixels);
      glReadPixels(0, 0, 1280, 720, GL_RGB, GL_UNSIGNED_BYTE, pixels);
      captureFrame(1280, 720, pixels);
      free(pixels);
    }
    swaps++;
    __real_glfwSwapBuffers(window);
    return;
  }
  if (frame >= 4) {
    InputState* input = glfwGetWindowUserPointer(window);
    Vec3 eye = playerEyePosition(&input->player);
    CHECK(frame < 70 && frame != 47 && !input->flying);
    CHECK(playerCanOccupyPosture(input->player.position, input->player.crouched));
    CHECK(input->camera->position.x == eye.x && input->camera->position.y == eye.y && input->camera->position.z == eye.z);
    CHECK(input->simulationSteps == (frame == 48 ? 0 : 4));
    if (frame >= 55) {
      CHECK(getBlock(&heldTarget)->id == (frame < 69 ? BLOCK_DIRT : BLOCK_AIR));
      CHECK(!getChunk(&(Vec2i){6, 6})->dirty && !getChunk(&(Vec2i){5, 6})->dirty);
      if (frame < 69) {
        CHECK(fabs(blockBreakingProgress(&input->breaking) - (frame - 54) / 15.0) < 0.00001);
        unsigned char pixel[3];
        glReadPixels(1280 / 2 - 23, 720 / 2 + 12, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);
        CHECK(pixel[0] > 240 && pixel[1] > 180 && pixel[2] < 100);
      }
    }
    if (frame >= 52 && frame <= 54) {
      GLint program;
      GLfloat matrix[16];
      glGetIntegerv(GL_CURRENT_PROGRAM, &program);
      glGetUniformfv((GLuint)program, glGetUniformLocation((GLuint)program, "viewProjection"), matrix);
      if (frame == 52)
        CHECK(matrix[5] < 1.4f && matrix[5] > 1.3f);
      else if (frame == 53)
        CHECK(matrix[5] < previousProjectionScale);
      else
        CHECK(matrix[5] > previousProjectionScale && matrix[5] < 1.429f);
      previousProjectionScale = matrix[5];
    }
    if ((frame == 18 || frame == 20 || frame == 50 || frame == 52 || frame == 60) && getenv("KERNELCRAFT_TEST_CAPTURE")) {
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
    CHECK(swaps == 72 && waits == 2);
    CHECK(sawCompactHUD && sawDebugHUD);
    puts("Application walking, crouching, running, jumping, flight, editing, wireframe, selection pixels, pause, framebuffer, and shutdown tests passed");
  }
  __real_glfwDestroyWindow(window);
}
