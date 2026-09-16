#include "graphics/player_renderer.h"
#include <stdint.h>
#include <string.h>

static int playerRenderedFrame = -1;
static unsigned liveBodyFrames;
static uint64_t punchSilhouettes[3];

static void playerViewFrame(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  if (frame == 93) {
    for (int x = -64; x <= -56; x++)
      for (int z = -65; z <= -55; z++)
        for (int y = 39; y <= 45; y++)
          CHECK(setBlock(&(Vec3i){x, y, z}, y == 39 ? BLOCK_STONE : BLOCK_AIR));
    CHECK(playerSetPosition(&input->player, (Vec3){-60.5f, 40, -60.5f}));
    input->flying = false;
    input->view = CAMERA_FIRST_PERSON;
    input->camera->position = playerEyePosition(&input->player);
    input->camera->yaw = 90;
    input->camera->pitch = -12;
    updateCameraVectors(input->camera);
    resetPlayerModelAnimation(&input->animation, input->player.position);
    input->clock.tick = 6000;
    keyCallback(window, GLFW_KEY_F6, 0, GLFW_PRESS, 0);
    CHECK(input->view == CAMERA_THIRD_PERSON_BACK);
  }
  if (frame == 94)
    pressedKey = GLFW_KEY_W;
  if (frame == 95) {
    startRunning(window, keyCallback);
    eventSeconds = -1;
  }
  if (frame == 96)
    pressedKey = GLFW_KEY_LEFT_SHIFT;
  if (frame == 97) {
    pressedKey = -1;
    keyCallback(window, GLFW_KEY_F6, 0, GLFW_PRESS, 0);
    CHECK(input->view == CAMERA_THIRD_PERSON_FRONT);
  }
  if (frame == 98)
    keyCallback(window, GLFW_KEY_SPACE, 0, GLFW_PRESS, 0);
  if (frame == 99) {
    keyCallback(window, GLFW_KEY_F6, 0, GLFW_PRESS, 0);
    CHECK(input->view == CAMERA_FIRST_PERSON);
  }
  if (frame == 100)
    setCursorCaptured(window, false);
  if (frame == 101)
    setCursorCaptured(window, true);
}

static unsigned changedPlayerPixels(const unsigned char* before, const unsigned char* after, int width, int height, uint64_t* silhouette, bool hand) {
  unsigned changed = 0;
  uint64_t hash = UINT64_C(14695981039346656037);
  for (int y = 0; y < height; y++)
    for (int x = 0; x < width; x++) {
      size_t offset = ((size_t)y * width + x) * 3;
      bool different = memcmp(before + offset, after + offset, 3) != 0;
      changed += different;
      hash = (hash ^ (unsigned)different) * UINT64_C(1099511628211);
      // The arm must leave the central aiming/selection/progress region clear.
      if (hand && abs(x - width / 2) < width / 12 && abs(y - height / 2) < height / 12)
        CHECK(!different);
    }
  if (silhouette)
    *silhouette = hash;
  return changed;
}

void __real_renderPlayerModel(const PlayerRenderer*, Vec3, const PlayerModelPose*, const Mat4, const Mat4, const DayNightState*);

void __wrap_renderPlayerModel(const PlayerRenderer* renderer, Vec3 feet, const PlayerModelPose* pose, const Mat4 view, const Mat4 projection, const DayNightState* daylight) {
  CHECK(frame >= 93 && frame <= 98 && playerRenderedFrame != frame);
  playerRenderedFrame = frame;
  liveBodyFrames++;
  InputState* input = glfwGetWindowUserPointer(glfwGetCurrentContext());
  Vec3 expectedFeet = inputBodyFeet(input);
  CHECK(feet.x == expectedFeet.x && feet.y == expectedFeet.y && feet.z == expectedFeet.z);
  Camera eye = *input->camera;
  Player body = input->player;
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  size_t bytes = (size_t)viewport[2] * viewport[3] * 3;
  unsigned char* before = malloc(bytes);
  unsigned char* after = malloc(bytes);
  CHECK(before && after);
  glReadPixels(0, 0, viewport[2], viewport[3], GL_RGB, GL_UNSIGNED_BYTE, before);
  __real_renderPlayerModel(renderer, feet, pose, view, projection, daylight);
  glReadPixels(0, 0, viewport[2], viewport[3], GL_RGB, GL_UNSIGNED_BYTE, after);
  CHECK(changedPlayerPixels(before, after, viewport[2], viewport[3], NULL, false) > 200);
  CHECK(memcmp(&eye, input->camera, sizeof(eye)) == 0 && memcmp(&body, &input->player, sizeof(body)) == 0);
  free(before);
  free(after);
}

void __real_renderPlayerHand(const PlayerRenderer*, const PlayerModelPose*, float, const DayNightState*);

void __wrap_renderPlayerHand(const PlayerRenderer* renderer, const PlayerModelPose* pose, float aspect, const DayNightState* daylight) {
  CHECK(playerRenderedFrame != frame);
  playerRenderedFrame = frame;
  bool probe = frame == 0 || frame == 1 || frame == 3 || frame == 55 || frame == 60 || frame == 65 || frame == 99;
  if (!probe) {
    __real_renderPlayerHand(renderer, pose, aspect, daylight);
    return;
  }
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  size_t pixels = (size_t)viewport[2] * viewport[3];
  unsigned char* before = malloc(pixels * 3);
  unsigned char* after = malloc(pixels * 3);
  float* depthBefore = malloc(pixels * sizeof(float));
  float* depthAfter = malloc(pixels * sizeof(float));
  CHECK(before && after && depthBefore && depthAfter);
  glReadPixels(0, 0, viewport[2], viewport[3], GL_RGB, GL_UNSIGNED_BYTE, before);
  glReadPixels(0, 0, viewport[2], viewport[3], GL_DEPTH_COMPONENT, GL_FLOAT, depthBefore);
  __real_renderPlayerHand(renderer, pose, aspect, daylight);
  glReadPixels(0, 0, viewport[2], viewport[3], GL_RGB, GL_UNSIGNED_BYTE, after);
  glReadPixels(0, 0, viewport[2], viewport[3], GL_DEPTH_COMPONENT, GL_FLOAT, depthAfter);
  uint64_t hash = 0;
  CHECK(changedPlayerPixels(before, after, viewport[2], viewport[3], &hash, true) > 200);
  CHECK(memcmp(depthBefore, depthAfter, pixels * sizeof(float)) == 0);
  if (frame == 55 || frame == 60 || frame == 65)
    punchSilhouettes[(frame - 55) / 5] = hash;
  if (frame == 65)
    CHECK(punchSilhouettes[0] != punchSilhouettes[1] && punchSilhouettes[1] != punchSilhouettes[2]);
  free(before);
  free(after);
  free(depthBefore);
  free(depthAfter);
}

static void checkPlayerRenderedFrame(const InputState* input) {
  CHECK(playerRenderedFrame == frame);
  if (frame >= 93 && frame <= 101) {
    Vec3 eye = playerEyePosition(&input->player);
    CHECK(input->camera->position.x == eye.x && input->camera->position.y == eye.y && input->camera->position.z == eye.z);
    CHECK(playerCanOccupyPosture(input->player.position, input->player.crouched));
    if (frame == 94)
      CHECK(input->animation.gaitPhase > 0 && input->animation.gaitWeight > 0);
    if (frame == 95)
      CHECK(input->player.running);
    if (frame == 96)
      CHECK(input->player.crouched && !input->player.running && fabsf(eye.y - input->player.position.y - 0.9f) < 0.00001f);
    if (frame == 98)
      CHECK(!input->player.grounded && input->player.velocity.y > 0);
    if (frame == 100)
      CHECK(input->animation.gaitWeight == 0 && input->animation.gaitPhase == 0 && !input->breakHeld);
    if (frame == 101)
      CHECK(liveBodyFrames == 6);
  }
}
