#include "graphics/item_renderer.h"
#include "graphics/player_renderer.h"
#include <stdint.h>
#include <string.h>

static int playerRenderedFrame = -1;
static int inventoryPreviewFrame = -1;
static unsigned liveBodyFrames;
static uint64_t punchSilhouettes[3];
static float punchPhases[3];
static uint64_t heldItemColorHashes[3];
static uint64_t placementSilhouettes[2];
static float placementPhases[2];

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
    input->selectedSlot = 8;
    input->inventory.carried[8] = (ItemStack){0};
    input->inventory.offhand = (ItemStack){0};
  }
  if (frame == 100)
    setCursorCaptured(window, false);
  if (frame == 101)
    setCursorCaptured(window, true);
}

static void heldItemFrame(GLFWwindow* window) {
  InputState* input = glfwGetWindowUserPointer(window);
  if (frame < 111 || frame > 114)
    return;
  CHECK(!input->inventoryOpen && input->view == CAMERA_FIRST_PERSON && cursorMode == GLFW_CURSOR_DISABLED);
  input->selectedSlot = 2;
  input->inventory.offhand = (ItemStack){0};
  if (frame == 111)
    input->inventory.carried[2] = (ItemStack){ITEM_STONE, 1};
  else if (frame == 112)
    input->inventory.carried[2] = (ItemStack){0};
  else if (frame == 113)
    input->inventory.carried[2] = (ItemStack){ITEM_LEATHER_HELMET, 1};
  else {
    input->inventory.carried[2] = (ItemStack){0};
    input->inventory.offhand = (ItemStack){ITEM_STONE_BRICKS, 1};
  }
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
      // The resting arm leaves aim clear. An active strike sweeps inward, with
      // the crosshair and breaking bar subsequently drawn above it by the HUD.
      if (hand && abs(x - width / 2) < width / 12 && abs(y - height / 2) < height / 12)
        CHECK(!different);
    }
  if (silhouette)
    *silhouette = hash;
  return changed;
}

static unsigned changedHeldPixels(const unsigned char* before, const unsigned char* after, size_t pixels, uint64_t* silhouette, uint64_t* colorHash, unsigned* skinPixels) {
  unsigned changed = 0;
  unsigned skin = 0;
  uint64_t shape = UINT64_C(14695981039346656037), color = UINT64_C(14695981039346656037);
  for (size_t pixel = 0; pixel < pixels; pixel++) {
    size_t offset = pixel * 3;
    bool different = memcmp(before + offset, after + offset, 3) != 0;
    shape = (shape ^ (unsigned)different) * UINT64_C(1099511628211);
    if (!different)
      continue;
    changed++;
    unsigned r = after[offset], g = after[offset + 1], b = after[offset + 2];
    // Both arm fronts in the supplied skin use warm flesh pixels. Keep the
    // threshold viable under moonlight; gray stone/brick held items cannot meet
    // the ordered-channel test, so an item-only draw still fails this witness.
    skin += r > 60 && r > g + 2 && g > b + 2;
    for (int channel = 0; channel < 3; channel++)
      color = (color ^ after[offset + channel]) * UINT64_C(1099511628211);
  }
  if (silhouette)
    *silhouette = shape;
  if (colorHash)
    *colorHash = color;
  if (skinPixels)
    *skinPixels = skin;
  return changed;
}

void __real_renderPlayerModel(const PlayerRenderer*, Vec3, const PlayerModelPose*, const Mat4, const Mat4, const DayNightState*);

void __wrap_renderPlayerModel(const PlayerRenderer* renderer, Vec3 feet, const PlayerModelPose* pose, const Mat4 view, const Mat4 projection, const DayNightState* daylight) {
  if (frame >= 102) {
    GLint framebuffer;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
    if (framebuffer) {
      CHECK(inventoryPreviewFrame != frame);
      inventoryPreviewFrame = frame;
      CHECK(feet.x == 0 && feet.y == 0 && feet.z == 0);
    } else {
      CHECK(playerRenderedFrame != frame);
      playerRenderedFrame = frame;
    }
    __real_renderPlayerModel(renderer, feet, pose, view, projection, daylight);
    return;
  }
  CHECK(frame >= 93 && frame <= 98 && playerRenderedFrame != frame);
  playerRenderedFrame = frame;
  liveBodyFrames++;
  InputState* input = glfwGetWindowUserPointer(glfwGetCurrentContext());
  Vec3 expectedFeet = inputBodyFeet(input);
  CHECK(feet.x == expectedFeet.x && feet.y == expectedFeet.y && feet.z == expectedFeet.z);
  if (frame == 96) {
    // Crouching lowers the authoritative first-person eye, but the selected
    // third-person camera keeps its standing-height anchor. The fixture around
    // the player is clear enough for the full three-block rear camera distance.
    Vec3 direction = input->camera->front;
    vec3_normalize(&direction, &direction);
    Vec3 expectedPosition = {input->player.position.x - direction.x * 3, input->player.position.y + PLAYER_EYE_HEIGHT - direction.y * 3,
                             input->player.position.z - direction.z * 3};
    Vec3 expectedTarget;
    vec3_add(&expectedTarget, &expectedPosition, &direction);
    Mat4 expectedView;
    mat4_lookAt(expectedView, &expectedPosition, &expectedTarget, &input->camera->up);
    for (int element = 0; element < 16; element++)
      CHECK(fabsf(view[element] - expectedView[element]) < 0.00001f);
    CHECK(fabsf(input->camera->position.y - (input->player.position.y + PLAYER_CROUCH_EYE_HEIGHT)) < 0.00001f);
  }
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
  bool probe = frame == 99 || frame == 112;
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
  CHECK(changedPlayerPixels(before, after, viewport[2], viewport[3], NULL, pose->punch <= 0 || pose->punch >= 1) > 200);
  CHECK(memcmp(depthBefore, depthAfter, pixels * sizeof(float)) == 0);
  free(before);
  free(after);
  free(depthBefore);
  free(depthAfter);
}

bool __real_renderHeldItems(ItemRenderer*, const PlayerRenderer*, ItemStack, ItemStack, const PlayerModelPose*, float, const DayNightState*);

bool __wrap_renderHeldItems(ItemRenderer* renderer, const PlayerRenderer* playerRenderer, ItemStack mainHand, ItemStack offhand, const PlayerModelPose* pose, float aspect,
                            const DayNightState* daylight) {
  InputState* input = glfwGetWindowUserPointer(glfwGetCurrentContext());
  CHECK(input && playerRenderer && playerRenderer->texture);
  ItemStack expectedMain, expectedOffhand;
  inputHeldItems(input, &expectedMain, &expectedOffhand);
  CHECK(mainHand.item == expectedMain.item && mainHand.count == expectedMain.count);
  CHECK(offhand.item == expectedOffhand.item && offhand.count == expectedOffhand.count);
  bool offhandBlockWithoutMain = !mainHand.count && offhand.count && inventoryItemBlock(offhand.item) != BLOCK_AIR;
  if (mainHand.count || offhandBlockWithoutMain) {
    CHECK(playerRenderedFrame != frame);
    playerRenderedFrame = frame;
  }

  bool probePunch = frame == 55 || frame == 60 || frame == 65 || frame == 69;
  bool probeItem = frame == 111 || frame == 113 || frame == 114;
  bool probePlacement = frame == 115 || frame == 119 || frame == 122;
  if (frame == 123) {
    CHECK(!input->placement.active && !mainHand.count && !input->inventory.carried[input->selectedSlot].count);
  }
  if (!probePunch && !probeItem && !probePlacement)
    return __real_renderHeldItems(renderer, playerRenderer, mainHand, offhand, pose, aspect, daylight);

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
  bool rendered = __real_renderHeldItems(renderer, playerRenderer, mainHand, offhand, pose, aspect, daylight);
  CHECK(rendered);
  glReadPixels(0, 0, viewport[2], viewport[3], GL_RGB, GL_UNSIGNED_BYTE, after);
  glReadPixels(0, 0, viewport[2], viewport[3], GL_DEPTH_COMPONENT, GL_FLOAT, depthAfter);
  uint64_t silhouette = 0, colorHash = 0;
  unsigned skinPixels = 0;
  CHECK(changedHeldPixels(before, after, pixels, &silhouette, &colorHash, &skinPixels) > 100);
  CHECK(memcmp(depthBefore, depthAfter, pixels * sizeof(float)) == 0);

  printf("Application held frame %d: skin=%u punch=%.3f place=%.3f/%.3f\n", frame, skinPixels, pose->punch, pose->placeMain, pose->placeOffhand);
  const char* capturePrefix = getenv("KERNELCRAFT_TEST_CAPTURE");
  if (capturePrefix) {
    char path[1024];
    snprintf(path, sizeof(path), "%s-held-%d.ppm", capturePrefix, frame);
    FILE* capture = fopen(path, "wb");
    CHECK(capture);
    fprintf(capture, "P6\n%d %d\n255\n", viewport[2], viewport[3]);
    for (int row = viewport[3] - 1; row >= 0; row--)
      CHECK(fwrite(after + (size_t)row * viewport[2] * 3, 1, (size_t)viewport[2] * 3, capture) == (size_t)viewport[2] * 3);
    CHECK(fclose(capture) == 0);
  }

  if (probePunch) {
    CHECK(mainHand.item == ITEM_STONE && mainHand.count == 1);
    CHECK(skinPixels == 0);
    if (frame == 69) {
      // Dirt completes between swing boundaries. Visual motion must survive the
      // gameplay timer reset on the removal frame instead of snapping to rest.
      CHECK(!input->breaking.active && pose->punch > 0 && pose->punch < 1);
    } else {
      int index = (frame - 55) / 5;
      punchSilhouettes[index] = silhouette;
      punchPhases[index] = pose->punch;
      if (frame == 65) {
        CHECK(punchPhases[0] != punchPhases[1] && punchPhases[1] != punchPhases[2] && punchPhases[0] != punchPhases[2]);
        CHECK(punchSilhouettes[0] != punchSilhouettes[1] && punchSilhouettes[1] != punchSilhouettes[2] && punchSilhouettes[0] != punchSilhouettes[2]);
      }
    }
  } else if (probeItem) {
    int index = frame == 111 ? 0 : frame == 113 ? 1 : 2;
    heldItemColorHashes[index] = colorHash;
    if (frame == 111)
      CHECK(mainHand.item == ITEM_STONE && mainHand.count == 1 && !offhand.count && skinPixels == 0);
    if (frame == 113)
      CHECK(mainHand.item == ITEM_LEATHER_HELMET && mainHand.count == 1 && !offhand.count);
    if (frame == 114) {
      CHECK(!mainHand.count && offhand.item == ITEM_STONE_BRICKS && offhand.count == 1 && skinPixels == 0);
      CHECK(heldItemColorHashes[0] != heldItemColorHashes[1] && heldItemColorHashes[1] != heldItemColorHashes[2] && heldItemColorHashes[0] != heldItemColorHashes[2]);
    }
  } else {
    CHECK(input->placement.active && input->placement.hand == BLOCK_PLACEMENT_HAND_MAIN && !input->inventory.carried[input->selectedSlot].count);
    CHECK(mainHand.item == ITEM_STONE && mainHand.count == 1 && !offhand.count);
    CHECK(skinPixels == 0);
    if (frame == 122) {
      CHECK(pose->placeMain == 1 && pose->placeOffhand == 0);
    } else {
      int index = frame == 115 ? 0 : 1;
      placementSilhouettes[index] = silhouette;
      placementPhases[index] = pose->placeMain;
      CHECK(placementPhases[index] > 0 && placementPhases[index] < 1);
      if (frame == 119)
        CHECK(placementPhases[0] != placementPhases[1] && placementSilhouettes[0] != placementSilhouettes[1]);
    }
  }

  free(before);
  free(after);
  free(depthBefore);
  free(depthAfter);
  return rendered;
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
