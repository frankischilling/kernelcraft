#include "graphics/selection.h"

static unsigned liveCrackPixels[3];
static unsigned liveCrackFrames;
static unsigned char tintBeforeCracks[3];

void __real_drawBlockBreaking(const BlockBreaking*, const Ray*, const Mat4, const Mat4);

void __wrap_drawBlockBreaking(const BlockBreaking* breaking, const Ray* selection, const Mat4 view, const Mat4 projection) {
  InputState* input = glfwGetWindowUserPointer(glfwGetCurrentContext());
  CHECK(input && breaking == &input->breaking && !input->inventoryOpen);
  if (frame == 60)
    glReadPixels(660, 375, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, tintBeforeCracks);
  bool sample = frame == 55 || frame == 60 || frame == 65 || frame == 69;
  if (!sample) {
    __real_drawBlockBreaking(breaking, selection, view, projection);
    return;
  }
  int width, height;
  glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
  size_t count = (size_t)width * height;
  unsigned char* before = malloc(count * 3);
  unsigned char* after = malloc(count * 3);
  float* depthBefore = malloc(count * sizeof(float));
  float* depthAfter = malloc(count * sizeof(float));
  CHECK(before && after && depthBefore && depthAfter);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, before);
  glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depthBefore);
  __real_drawBlockBreaking(breaking, selection, view, projection);
  glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, after);
  glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depthAfter);
  unsigned changed = 0;
  for (size_t pixel = 0; pixel < count; pixel++)
    changed += memcmp(before + pixel * 3, after + pixel * 3, 3) != 0;
  CHECK(!memcmp(depthBefore, depthAfter, count * sizeof(float)));
  if (frame == 69) {
    CHECK(!breaking->active && changed == 0 && liveCrackFrames == 3);
  } else {
    int stage = (frame - 55) / 5;
    CHECK(selection->hit && breaking->active && breaking->block == BLOCK_DIRT);
    CHECK(selection->blockCoords.x == heldTarget.x && selection->blockCoords.y == heldTarget.y && selection->blockCoords.z == heldTarget.z);
    CHECK(fabsf(blockBreakingProgress(breaking) - (frame - 54) / 15.0f) < 0.00001f);
    CHECK(changed > 5);
    liveCrackPixels[stage] = changed;
    if (stage)
      CHECK(liveCrackPixels[stage] > liveCrackPixels[stage - 1]);
    liveCrackFrames++;
  }
  printf("Application cracks frame %d: %u changed pixels\n", frame, changed);
  free(before);
  free(after);
  free(depthBefore);
  free(depthAfter);
}
