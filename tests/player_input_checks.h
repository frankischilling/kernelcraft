#ifndef PLAYER_INPUT_CHECKS_H
#define PLAYER_INPUT_CHECKS_H

#include <string.h>

typedef struct {
  Vec3i cell;
  int id;
} PlayerViewFixtureBlock;

static bool sameViewVec3(Vec3 a, Vec3 b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

static void setPlayerViewFixtureBlock(PlayerViewFixtureBlock* blocks, size_t capacity, size_t* count, Vec3i cell, int id) {
  CHECK(*count < capacity);
  const Block* block = getBlock(&cell);
  CHECK(block != NULL);
  blocks[*count] = (PlayerViewFixtureBlock){cell, block->id};
  (*count)++;
  if (block->id != id)
    CHECK(setBlock(&cell, id));
}

static void restorePlayerViewFixture(PlayerViewFixtureBlock* blocks, size_t count) {
  while (count) {
    PlayerViewFixtureBlock saved = blocks[--count];
    const Block* block = getBlock(&saved.cell);
    CHECK(block != NULL);
    if (block->id != saved.id)
      CHECK(setBlock(&saved.cell, saved.id));
  }
}

static void testPlayerViewInput(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  Camera* camera = input->camera;
  CHECK(camera != NULL);
  Camera originalCamera = *camera;
  InputState originalInput = *input;
  int originalPressedKey = pressedKey;
  int originalFocused = focused;
  bool originalIconified = iconified;
  bool originalZeroFramebuffer = zeroFramebuffer;
  int originalCursorMode = cursorMode;
  bool originalShiftHeld = shiftHeld, originalSideHeld = sideHeld, originalBackHeld = backHeld;

  GLFWkeyfun key = glfwSetKeyCallback(window, NULL);
  glfwSetKeyCallback(window, key);
  CHECK(key != NULL);

  enum { FIXTURE_CAPACITY = 512 };

  PlayerViewFixtureBlock fixture[FIXTURE_CAPACITY];
  size_t fixtureCount = 0;
  const int fixtureX = 72, fixtureY = 40, fixtureZ = -72;

  // Build a deterministic walking/camera lane in the real world while retaining
  // every overwritten block so this check leaves the shared fixture unchanged.
  for (int x = fixtureX - 2; x <= fixtureX + 2; x++)
    for (int z = fixtureZ - 5; z <= fixtureZ + 7; z++) {
      setPlayerViewFixtureBlock(fixture, FIXTURE_CAPACITY, &fixtureCount, (Vec3i){x, fixtureY - 1, z}, BLOCK_STONE);
      for (int y = fixtureY; y <= fixtureY + 4; y++)
        setPlayerViewFixtureBlock(fixture, FIXTURE_CAPACITY, &fixtureCount, (Vec3i){x, y, z}, BLOCK_AIR);
    }

  focused = GLFW_TRUE;
  iconified = zeroFramebuffer = false;
  shiftHeld = sideHeld = backHeld = false;
  pressedKey = -1;
  setCursorCaptured(window, true);
  input->chat.open = false;
  input->flying = false;
  input->view = CAMERA_FIRST_PERSON;
  input->breakHeld = true;
  input->breaking = (BlockBreaking){.target = {fixtureX, fixtureY, fixtureZ + 3}, .block = BLOCK_STONE, .elapsed = 0.2, .active = true};
  CHECK(playerSetPosition(&input->player, (Vec3){fixtureX + 0.5f, fixtureY, fixtureZ + 0.5f}));
  camera->position = playerEyePosition(&input->player);
  camera->yaw = 90;
  camera->pitch = 0;
  updateCameraVectors(camera);
  resetPlayerModelAnimation(&input->animation, input->player.position);

  Camera beforeToggleCamera = *camera;
  Player beforeTogglePlayer = input->player;
  BlockBreaking beforeToggleBreaking = input->breaking;
  SavedPlayer beforeSnapshot, afterSnapshot;
  CHECK(snapshotPlayer(input, &beforeSnapshot));
  Vec3 canonicalEye = playerEyePosition(&input->player);
  CHECK(sameViewVec3(camera->position, canonicalEye));

  // F6 cycles first -> back -> front -> first. Repeat and release are inert.
  key(window, GLFW_KEY_F6, 0, GLFW_REPEAT, 0);
  key(window, GLFW_KEY_F6, 0, GLFW_RELEASE, 0);
  CHECK(input->view == CAMERA_FIRST_PERSON);
  key(window, GLFW_KEY_F6, 0, GLFW_PRESS, 0);
  CHECK(input->view == CAMERA_THIRD_PERSON_BACK);
  key(window, GLFW_KEY_F6, 0, GLFW_PRESS, 0);
  CHECK(input->view == CAMERA_THIRD_PERSON_FRONT);
  key(window, GLFW_KEY_F6, 0, GLFW_PRESS, 0);
  CHECK(input->view == CAMERA_FIRST_PERSON);

  CHECK(!memcmp(camera, &beforeToggleCamera, sizeof(*camera)));
  CHECK(!memcmp(&input->player, &beforeTogglePlayer, sizeof(input->player)));
  CHECK(!memcmp(&input->breaking, &beforeToggleBreaking, sizeof(input->breaking)));
  CHECK(input->breakHeld);
  CHECK(snapshotPlayer(input, &afterSnapshot));
  CHECK(sameViewVec3(beforeSnapshot.feet, afterSnapshot.feet));
  CHECK(beforeSnapshot.yaw == afterSnapshot.yaw && beforeSnapshot.pitch == afterSnapshot.pitch && beforeSnapshot.selectedSlot == afterSnapshot.selectedSlot);
  CHECK(sameViewVec3(camera->position, playerEyePosition(&input->player)));

  input->view = CAMERA_THIRD_PERSON_BACK;
  focused = GLFW_FALSE;
  key(window, GLFW_KEY_F6, 0, GLFW_PRESS, 0);
  CHECK(input->view == CAMERA_THIRD_PERSON_BACK);
  focused = GLFW_TRUE;
  iconified = true;
  key(window, GLFW_KEY_F6, 0, GLFW_PRESS, 0);
  CHECK(input->view == CAMERA_THIRD_PERSON_BACK);
  iconified = false;
  zeroFramebuffer = true;
  key(window, GLFW_KEY_F6, 0, GLFW_PRESS, 0);
  CHECK(input->view == CAMERA_THIRD_PERSON_BACK);
  zeroFramebuffer = false;
  input->chat.open = true;
  key(window, GLFW_KEY_F6, 0, GLFW_PRESS, 0);
  CHECK(input->view == CAMERA_THIRD_PERSON_BACK);
  input->chat.open = false;

  // Pausing clears transient movement/breaking animation state without changing
  // the selected display view.
  input->animation.gaitPhase = 1.25;
  input->animation.gaitWeight = 0.8f;
  pauseInput(input);
  CHECK(input->view == CAMERA_THIRD_PERSON_BACK);
  CHECK(input->animation.gaitPhase == 0 && input->animation.gaitWeight == 0);

  // At 240 Hz, alternate display frames have no 120 Hz physics tick. They must
  // not turn the gait weight on/off compared with coarser display frame rates.
  double referencePhase = 0;
  float referenceWeight = 0;
  const int rates[] = {30, 60, 120, 240};
  for (size_t rate = 0; rate < sizeof(rates) / sizeof(*rates); rate++) {
    CHECK(playerSetPosition(&input->player, (Vec3){fixtureX + 0.5f, fixtureY, fixtureZ + 0.5f}));
    setCursorCaptured(window, true);
    pressedKey = GLFW_KEY_W;
    for (int step = 0; step < rates[rate] / 10; step++)
      processInput(window, input, 1.0 / rates[rate]);
    if (!rate) {
      referencePhase = input->animation.gaitPhase;
      referenceWeight = input->animation.gaitWeight;
      CHECK(referencePhase > 0 && referenceWeight > 0);
    } else {
      CHECK(fabs(input->animation.gaitPhase - referencePhase) < 0.00001);
      CHECK(fabsf(input->animation.gaitWeight - referenceWeight) < 0.00001f);
    }
  }
  pressedKey = -1;
  CHECK(playerSetPosition(&input->player, (Vec3){fixtureX + 0.5f, fixtureY, fixtureZ + 0.5f}));
  camera->position = playerEyePosition(&input->player);
  pauseInput(input);

  SavedPlayer saved;
  CHECK(snapshotPlayer(input, &saved));
  Camera restoredCamera = *camera;
  InputState restored;
  CHECK(initSavedInputs(&restored, &restoredCamera, &saved));
  CHECK(restored.view == CAMERA_FIRST_PERSON);
  CHECK(sameViewVec3(restoredCamera.position, playerEyePosition(&restored.player)));

  // Third-person cameras derive from, but never mutate, the authoritative eye.
  Camera display;
  Camera authoritativeBefore = *camera;
  CHECK(makeThirdPersonCamera(&display, camera, false, 16.0f / 9.0f));
  CHECK(!memcmp(camera, &authoritativeBefore, sizeof(*camera)));
  Camera eye = *camera;
  Camera immutableEye = eye;
  CHECK(fabsf(display.position.z - (eye.position.z - 3.0f)) < 0.0001f);
  CHECK(fabsf(display.position.x - eye.position.x) < 0.0001f && sameViewVec3(display.front, eye.front));
  CHECK(!memcmp(&eye, &immutableEye, sizeof(eye)));
  CHECK(makeThirdPersonCamera(&display, &eye, true, 16.0f / 9.0f));
  CHECK(fabsf(display.position.z - (eye.position.z + 3.0f)) < 0.0001f);
  CHECK(fabsf(display.front.z + eye.front.z) < 0.0001f);
  CHECK(!memcmp(&eye, &immutableEye, sizeof(eye)));

  // A centered wall clamps stand-off while an immediately adjacent wall falls
  // back to the authoritative eye because there is no safe third-person room.
  setPlayerViewFixtureBlock(fixture, FIXTURE_CAPACITY, &fixtureCount, (Vec3i){fixtureX, fixtureY + 1, fixtureZ - 2}, BLOCK_STONE);
  CHECK(makeThirdPersonCamera(&display, &eye, false, 16.0f / 9.0f));
  float clamped = vec3_distance(&display.position, &eye.position);
  CHECK(clamped >= 0.8f && clamped < 3.0f);
  setPlayerViewFixtureBlock(fixture, FIXTURE_CAPACITY, &fixtureCount, (Vec3i){fixtureX, fixtureY + 1, fixtureZ - 2}, BLOCK_AIR);
  setPlayerViewFixtureBlock(fixture, FIXTURE_CAPACITY, &fixtureCount, (Vec3i){fixtureX, fixtureY + 1, fixtureZ - 1}, BLOCK_STONE);
  CHECK(!makeThirdPersonCamera(&display, &eye, false, 16.0f / 9.0f));
  CHECK(!memcmp(&display, &eye, sizeof(display)));
  setPlayerViewFixtureBlock(fixture, FIXTURE_CAPACITY, &fixtureCount, (Vec3i){fixtureX, fixtureY + 1, fixtureZ - 1}, BLOCK_AIR);

  // The center ray misses this block. A wide viewport's near-plane edge must
  // still catch it and reduce camera distance.
  setPlayerViewFixtureBlock(fixture, FIXTURE_CAPACITY, &fixtureCount, (Vec3i){fixtureX - 1, fixtureY + 1, fixtureZ - 2}, BLOCK_STONE);
  CHECK(makeThirdPersonCamera(&display, &eye, false, 1.0f));
  CHECK(fabsf(vec3_distance(&display.position, &eye.position) - 3.0f) < 0.0001f);
  CHECK(makeThirdPersonCamera(&display, &eye, false, 8.0f));
  CHECK(vec3_distance(&display.position, &eye.position) < 3.0f);
  CHECK(!memcmp(&eye, &immutableEye, sizeof(eye)));
  setPlayerViewFixtureBlock(fixture, FIXTURE_CAPACITY, &fixtureCount, (Vec3i){fixtureX - 1, fixtureY + 1, fixtureZ - 2}, BLOCK_AIR);

  // Exercise animation through real input/physics: actual displacement advances
  // gait, a wall-stationary player stops phase advance, and pause resets it.
  CHECK(playerSetPosition(&input->player, (Vec3){fixtureX + 0.5f, fixtureY, fixtureZ + 0.5f}));
  input->flying = false;
  input->chat.open = false;
  input->breakHeld = false;
  resetBlockBreaking(&input->breaking);
  playerResetRunInput(&input->runInput);
  camera->position = playerEyePosition(&input->player);
  camera->yaw = 90;
  camera->pitch = 0;
  updateCameraVectors(camera);
  resetPlayerModelAnimation(&input->animation, input->player.position);
  setCursorCaptured(window, true);
  pressedKey = GLFW_KEY_W;
  processInput(window, input, 0.1);
  CHECK(input->animation.gaitPhase > 0 && input->animation.gaitWeight > 0);
  setPlayerViewFixtureBlock(fixture, FIXTURE_CAPACITY, &fixtureCount, (Vec3i){fixtureX, fixtureY, fixtureZ + 2}, BLOCK_STONE);
  setPlayerViewFixtureBlock(fixture, FIXTURE_CAPACITY, &fixtureCount, (Vec3i){fixtureX, fixtureY + 1, fixtureZ + 2}, BLOCK_STONE);
  for (int step = 0; step < 8; step++)
    processInput(window, input, 0.1);
  Vec3 stoppedFeet = input->player.position;
  double stoppedPhase = input->animation.gaitPhase;
  float stoppedWeight = input->animation.gaitWeight;
  processInput(window, input, 0.1);
  CHECK(sameViewVec3(input->player.position, stoppedFeet));
  CHECK(input->animation.gaitPhase == stoppedPhase && input->animation.gaitWeight < stoppedWeight);
  input->view = CAMERA_THIRD_PERSON_FRONT;
  setCursorCaptured(window, false);
  processInput(window, input, 0.1);
  CHECK(input->view == CAMERA_THIRD_PERSON_FRONT);
  CHECK(input->animation.gaitPhase == 0 && input->animation.gaitWeight == 0);

  restorePlayerViewFixture(fixture, fixtureCount);
  pressedKey = originalPressedKey;
  focused = originalFocused;
  iconified = originalIconified;
  zeroFramebuffer = originalZeroFramebuffer;
  shiftHeld = originalShiftHeld;
  sideHeld = originalSideHeld;
  backHeld = originalBackHeld;
  if (cursorMode != originalCursorMode)
    glfwSetInputMode(window, GLFW_CURSOR, originalCursorMode);
  *camera = originalCamera;
  *input = originalInput;
}

#endif
