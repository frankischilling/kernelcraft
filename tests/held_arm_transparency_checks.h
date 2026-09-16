#ifndef HELD_ARM_TRANSPARENCY_CHECKS_H
#define HELD_ARM_TRANSPARENCY_CHECKS_H

// This header is included by item_render_checks.h after its framebuffer/read
// helpers and synthetic held-player skin have been defined.

static bool heldArmTransparencyInRect(int x, int y, const int rect[4]) {
  return x >= rect[0] && x < rect[0] + rect[2] && y >= rect[1] && y < rect[1] + rect[3];
}

static bool heldArmTransparencyOuterPixel(const unsigned char* pixel, int x, int y) {
  static const int rightOuter[PLAYER_MODEL_FACE_COUNT][4] = {
      {40, 36, 4, 12}, {48, 36, 4, 12}, {44, 32, 4, 4}, {48, 32, 4, 4}, {44, 36, 4, 12}, {52, 36, 4, 12},
  };
  static const int leftOuter[PLAYER_MODEL_FACE_COUNT][4] = {
      {48, 52, 4, 12}, {56, 52, 4, 12}, {52, 48, 4, 4}, {56, 48, 4, 4}, {52, 52, 4, 12}, {60, 52, 4, 12},
  };
  bool right = false, left = false;
  for (int face = 0; face < PLAYER_MODEL_FACE_COUNT; face++) {
    right |= heldArmTransparencyInRect(x, y, rightOuter[face]);
    left |= heldArmTransparencyInRect(x, y, leftOuter[face]);
  }
  // itemTestWriteHeldSkin paints these exact opaque outer-layer colors. Check
  // both atlas region and color so overlapping/unpainted skin texels are never
  // changed by this regression fixture.
  bool rightPaint = pixel[0] == 242 && pixel[1] == 132 && pixel[2] == 22;
  bool leftPaint = pixel[0] == 40 && pixel[1] == 80 && pixel[2] == 240;
  return pixel[3] == 255 && ((right && rightPaint) || (left && leftPaint));
}

static size_t heldArmTransparencySetOuterAlpha(unsigned char* destination, const unsigned char* original, unsigned char alpha) {
  memcpy(destination, original, (size_t)PLAYER_SKIN_SIZE * PLAYER_SKIN_SIZE * 4);
  size_t changed = 0;
  for (int y = 0; y < PLAYER_SKIN_SIZE; y++)
    for (int x = 0; x < PLAYER_SKIN_SIZE; x++) {
      size_t offset = ((size_t)y * PLAYER_SKIN_SIZE + x) * 4;
      if (!heldArmTransparencyOuterPixel(original + offset, x, y))
        continue;
      destination[offset + 3] = alpha;
      changed++;
    }
  return changed;
}

static void heldArmTransparencyUpload(PlayerRenderer* playerRenderer, const unsigned char* pixels, bool fractional) {
  glBindTexture(GL_TEXTURE_2D, playerRenderer->texture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, PLAYER_SKIN_SIZE, PLAYER_SKIN_SIZE, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  playerRenderer->fractionalAlpha = fractional;
}

static bool heldArmTransparencyCapture(ItemRenderer* items, PlayerRenderer* playerRenderer, ItemTestTarget* target, ItemStack mainHand, ItemStack offhand,
                                       const PlayerModelPose* pose, const DayNightState* light, unsigned char* rgba, float* depthBefore, float* depthAfter) {
  const int width = target->width, height = target->height;
  size_t pixels = (size_t)width * height;
  glBindFramebuffer(GL_FRAMEBUFFER, target->framebuffer);
  glViewport(0, 0, width, height);
  itemTestClear(.375);
  itemTestRead(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depthBefore);
  bool rendered = renderHeldItems(items, playerRenderer, mainHand, offhand, pose, (float)width / height, light);
  itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  itemTestRead(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depthAfter);
  return rendered && memcmp(depthBefore, depthAfter, pixels * sizeof(float)) == 0;
}

static bool itemTestArmTransparency(ItemRenderer* items, PlayerRenderer* playerRenderer, ItemTestTarget* target) {
  if (!items || !playerRenderer || !playerRenderer->texture || !target || !target->framebuffer || target->width <= 0 || target->height <= 0)
    return false;

  const size_t skinBytes = (size_t)PLAYER_SKIN_SIZE * PLAYER_SKIN_SIZE * 4;
  const size_t pixels = (size_t)target->width * target->height;
  const size_t imageBytes = pixels * 4;
  unsigned char* original = malloc(skinBytes);
  unsigned char* transparentSkin = malloc(skinBytes);
  unsigned char* opaqueSkin = malloc(skinBytes);
  unsigned char* halfSkin = malloc(skinBytes);
  unsigned char* transparent = malloc(imageBytes);
  unsigned char* opaque = malloc(imageBytes);
  unsigned char* half = malloc(imageBytes);
  float* depthBefore = malloc(pixels * sizeof(float));
  float* depthAfter = malloc(pixels * sizeof(float));
  if (!original || !transparentSkin || !opaqueSkin || !halfSkin || !transparent || !opaque || !half || !depthBefore || !depthAfter) {
    free(original);
    free(transparentSkin);
    free(opaqueSkin);
    free(halfSkin);
    free(transparent);
    free(opaque);
    free(half);
    free(depthBefore);
    free(depthAfter);
    return false;
  }

  GLint previousActive, previousTexture, previousPackBuffer, previousUnpackBuffer;
  const GLenum packFields[] = {GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_SKIP_ROWS, GL_PACK_SKIP_PIXELS};
  const GLenum unpackFields[] = {GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH, GL_UNPACK_SKIP_ROWS, GL_UNPACK_SKIP_PIXELS};
  GLint previousPack[4], previousUnpack[4];
  glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActive);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
  glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &previousPackBuffer);
  glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &previousUnpackBuffer);
  for (size_t i = 0; i < 4; i++) {
    glGetIntegerv(packFields[i], &previousPack[i]);
    glGetIntegerv(unpackFields[i], &previousUnpack[i]);
    glPixelStorei(packFields[i], i ? 0 : 1);
    glPixelStorei(unpackFields[i], i ? 0 : 1);
  }
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, playerRenderer->texture);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, original);

  bool originalFractional = playerRenderer->fractionalAlpha;
  size_t transparentChanged = heldArmTransparencySetOuterAlpha(transparentSkin, original, 0);
  size_t opaqueChanged = heldArmTransparencySetOuterAlpha(opaqueSkin, original, 255);
  size_t halfChanged = heldArmTransparencySetOuterAlpha(halfSkin, original, 128);
  bool ok = transparentChanged > 0 && transparentChanged == opaqueChanged && opaqueChanged == halfChanged && glGetError() == GL_NO_ERROR;
  size_t compared = 0;
  DayNightState light = itemTestNeutralLight();
  const float phases[] = {0.133f, 0.5f, 0.9f};

  for (int hand = 0; ok && hand < 2; hand++)
    for (size_t phase = 0; ok && phase < sizeof(phases) / sizeof(*phases); phase++) {
      PlayerModelPose pose;
      playerModelPose(&pose, &(PlayerPoseInput){.grounded = true});
      if (hand)
        pose.placeOffhand = phases[phase];
      else
        pose.placeMain = phases[phase];
      ItemStack mainHand = hand ? (ItemStack){0} : (ItemStack){ITEM_LEATHER_HELMET, 1};
      ItemStack offhand = hand ? (ItemStack){ITEM_LEATHER_BOOTS, 1} : (ItemStack){0};

      heldArmTransparencyUpload(playerRenderer, transparentSkin, false);
      ok &= heldArmTransparencyCapture(items, playerRenderer, target, mainHand, offhand, &pose, &light, transparent, depthBefore, depthAfter);
      heldArmTransparencyUpload(playerRenderer, opaqueSkin, false);
      ok &= heldArmTransparencyCapture(items, playerRenderer, target, mainHand, offhand, &pose, &light, opaque, depthBefore, depthAfter);
      heldArmTransparencyUpload(playerRenderer, halfSkin, true);
      ok &= heldArmTransparencyCapture(items, playerRenderer, target, mainHand, offhand, &pose, &light, half, depthBefore, depthAfter);

      size_t caseCompared = 0;
      for (size_t pixel = 0; ok && pixel < pixels; pixel++) {
        const unsigned char* base = transparent + pixel * 4;
        const unsigned char* sleeve = opaque + pixel * 4;
        const unsigned char* actual = half + pixel * 4;
        if (base[3] < 250 || sleeve[3] < 250)
          continue;
        int delta = abs((int)base[0] - sleeve[0]) + abs((int)base[1] - sleeve[1]) + abs((int)base[2] - sleeve[2]);
        if (delta < 12)
          continue;
        caseCompared++;
        compared++;
        for (int channel = 0; channel < 3; channel++) {
          int expected = ((int)sleeve[channel] * 128 + (int)base[channel] * 127 + 127) / 255;
          ok &= abs((int)actual[channel] - expected) <= 3;
        }
        ok &= actual[3] >= 252;
      }
      ok &= caseCompared > 20;
    }

  // Restore the exact test-owned GPU skin and renderer metadata even after a
  // failed comparison so later item/player regressions see their original fixture.
  heldArmTransparencyUpload(playerRenderer, original, originalFractional);
  glBindTexture(GL_TEXTURE_2D, (GLuint)previousTexture);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)previousPackBuffer);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)previousUnpackBuffer);
  for (size_t i = 0; i < 4; i++) {
    glPixelStorei(packFields[i], previousPack[i]);
    glPixelStorei(unpackFields[i], previousUnpack[i]);
  }
  glActiveTexture((GLenum)previousActive);

  ok &= compared > 120 && glGetError() == GL_NO_ERROR;
  if (ok)
    printf("Held equipment arm fractional sleeves: %zu blended samples\n", compared);
  else
    fprintf(stderr, "Held equipment arm fractional sleeve ordering/depth regression failed: compared=%zu\n", compared);

  free(original);
  free(transparentSkin);
  free(opaqueSkin);
  free(halfSkin);
  free(transparent);
  free(opaque);
  free(half);
  free(depthBefore);
  free(depthAfter);
  return ok;
}

#endif
