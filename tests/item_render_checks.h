#ifndef ITEM_RENDER_CHECKS_H
#define ITEM_RENDER_CHECKS_H

#include "graphics/item_renderer.h"
#include "../libs/stb_image.h"

typedef struct {
  GLuint framebuffer, color, depth;
  int width, height;
} ItemTestTarget;

static void itemTestRead(int x, int y, int width, int height, GLenum format, GLenum type, void* pixels) {
  const GLenum fields[] = {GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_SKIP_ROWS, GL_PACK_SKIP_PIXELS, GL_PACK_SWAP_BYTES};
  GLint previous[5], buffer;
  glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &buffer);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  for (int i = 0; i < 5; i++) {
    glGetIntegerv(fields[i], &previous[i]);
    glPixelStorei(fields[i], i ? 0 : 1);
  }
  glReadPixels(x, y, width, height, format, type, pixels);
  for (int i = 0; i < 5; i++)
    glPixelStorei(fields[i], previous[i]);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)buffer);
}

static void itemTestDestroyTarget(ItemTestTarget* target) {
  glDeleteFramebuffers(1, &target->framebuffer);
  glDeleteTextures(1, &target->color);
  glDeleteRenderbuffers(1, &target->depth);
  *target = (ItemTestTarget){0};
}

static bool itemTestTarget(ItemTestTarget* target, int width, int height) {
  itemTestDestroyTarget(target);
  target->width = width;
  target->height = height;
  glGenFramebuffers(1, &target->framebuffer);
  glGenTextures(1, &target->color);
  glGenRenderbuffers(1, &target->depth);
  GLint unpack;
  glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, target->color);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glBindRenderbuffer(GL_RENDERBUFFER, target->depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
  glBindFramebuffer(GL_FRAMEBUFFER, target->framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target->color, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, target->depth);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)unpack);
  glViewport(0, 0, width, height);
  return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE && glGetError() == GL_NO_ERROR;
}

static void itemTestClear(double depth) {
  glDisable(GL_SCISSOR_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDepthMask(GL_TRUE);
  glClearDepth(depth);
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static DayNightState itemTestNeutralLight(void) {
  return (DayNightState){.lightDirection = {0, 1, 0}, .skyFill = {1, 1, 1}, .groundFill = {1, 1, 1}};
}

// Expected UVs are independently evaluated from the face's world-space axes.
// No item-model vertices, renderer layer tables, or cached icons supply goldens.
static bool itemTestFaces(const ItemRenderer* renderer, ItemTestTarget* target) {
  if (!itemTestTarget(target, 256, 256))
    return false;
  const char* paths[13] = {"assets/textures/stone.png",
                           "assets/textures/dirt.png",
                           "assets/textures/grass-top.png",
                           "assets/textures/grass-side.png",
                           NULL,
                           NULL,
                           NULL,
                           "assets/textures/cobblestone.png",
                           "assets/textures/oak-planks.png",
                           "assets/textures/stone-bricks.png",
                           "assets/textures/oak-log-side.png",
                           "assets/textures/oak-log-top.png",
                           "assets/textures/oak-leaves.png"};
  unsigned char* images[13] = {0};
  bool ok = true;
  for (int layer = 0; layer < 13; layer++) {
    if (!paths[layer])
      continue;
    int width, height, channels;
    images[layer] = stbi_load(paths[layer], &width, &height, &channels, 4);
    ok &= images[layer] && width == 16 && height == 16;
  }
  const Vec3 normals[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
  const uint16_t items[] = {1, 2, 3, 4, 5, 6, 11, 12};
  const int layers[8][6] = {{3, 3, 2, 1, 3, 3}, {1, 1, 1, 1, 1, 1}, {0, 0, 0, 0, 0, 0},       {7, 7, 7, 7, 7, 7},
                            {8, 8, 8, 8, 8, 8}, {9, 9, 9, 9, 9, 9}, {10, 10, 11, 11, 10, 10}, {12, 12, 12, 12, 12, 12}};
  DayNightState light = itemTestNeutralLight();
  Mat4 model, projection;
  mat4_identity(model);
  mat4_identity(projection);
  projection[0] = projection[5] = 2;
  projection[10] = -0.2f;
  unsigned checked = 0;
  for (size_t itemIndex = 0; ok && itemIndex < sizeof(items) / sizeof(items[0]); itemIndex++)
    for (int face = 0; ok && face < 6; face++) {
      uint16_t item = items[itemIndex];
      Vec3 normal = normals[face], eye, center = {0}, up = {0, 1, 0};
      vec3_scale(&eye, &normal, 3);
      if (face == 2)
        up = (Vec3){0, 0, -1};
      if (face == 3)
        up = (Vec3){0, 0, 1};
      Vec3 forward = {-normal.x, -normal.y, -normal.z}, right, vertical;
      vec3_cross(&right, &forward, &up);
      vec3_cross(&vertical, &right, &forward);
      Mat4 view;
      mat4_lookAt(view, &eye, &center, &up);
      itemTestClear(1);
      renderItemModel(renderer, item, model, view, projection, &light);
      unsigned char pixels[256 * 256 * 4];
      itemTestRead(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
      for (int y = 0; ok && y < 16; y++)
        for (int x = 0; ok && x < 16; x++) {
          int px = x * 16 + 8, py = y * 16 + 8;
          float sx = (px + 0.5f) / 256 - 0.5f, sy = (py + 0.5f) / 256 - 0.5f;
          Vec3 p = {normal.x * .5f + right.x * sx + vertical.x * sy, normal.y * .5f + right.y * sx + vertical.y * sy, normal.z * .5f + right.z * sx + vertical.z * sy};
          float u = face < 2 ? p.z + .5f : p.x + .5f;
          float v = face == 2 ? p.z + .5f : face == 3 ? .5f - p.z : .5f - p.y;
          int tx = (int)floorf(u * 16), ty = (int)floorf(v * 16);
          const unsigned char* expected = images[layers[itemIndex][face]] + (ty * 16 + tx) * 4;
          const unsigned char* actual = pixels + (py * 256 + px) * 4;
          if (!expected[3]) {
            // Cutout leaves discard transparent texels. Their source RGB is not
            // observable after the clear, so only alpha is a meaningful golden.
            ok &= actual[3] == 0;
          } else {
            for (int c = 0; c < 4; c++)
              ok &= abs((int)actual[c] - expected[c]) <= 1;
          }
          checked++;
        }
      if (!ok)
        fprintf(stderr, "3D item face image mismatch: item=%u face=%d\n", item, face);
    }
  for (int layer = 0; layer < 13; layer++)
    stbi_image_free(images[layer]);
  if (ok)
    printf("3D block faces match %u independent source-image samples\n", checked);
  return ok;
}

static bool itemTestIcons(const ItemRenderer* renderer) {
  GLuint framebuffer;
  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  unsigned char pixels[ITEM_ICON_SIZE * ITEM_ICON_SIZE * 4];
  bool ok = !itemRendererIcon(renderer, 0) && !itemRendererIcon(renderer, ITEM_ID_LAST + 1);
  for (uint16_t item = 1; item <= ITEM_ID_LAST; item++) {
    GLuint icon = itemRendererIcon(renderer, item);
    ok &= icon && glIsTexture(icon);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, icon, 0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      ok = false;
      break;
    }
    itemTestRead(0, 0, ITEM_ICON_SIZE, ITEM_ICON_SIZE, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    size_t visible = 0, green = 0, brown = 0;
    for (size_t i = 0; i < sizeof(pixels); i += 4) {
      if (!pixels[i + 3])
        continue;
      visible++;
      green += pixels[i + 1] > pixels[i] + 10;
      brown += pixels[i] > pixels[i + 1] + 10;
    }
    ok &= visible > 500 && visible < ITEM_ICON_SIZE * ITEM_ICON_SIZE * 9 / 10;
    ok &= pixels[3] == 0 && pixels[(ITEM_ICON_SIZE - 1) * 4 + 3] == 0 && pixels[sizeof(pixels) - 1] == 0;
    if (item == ITEM_GRASS_BLOCK)
      ok &= green > 100 && brown > 100;
  }
  glDeleteFramebuffers(1, &framebuffer);
  if (!ok)
    fprintf(stderr, "3D GUI silhouettes or grass face colors failed\n");
  return ok;
}

static bool itemTestDroppedVolume(const ItemRenderer* renderer, ItemTestTarget* target) {
  if (!itemTestTarget(target, 256, 256))
    return false;
  DroppedItems drops = {0};
  drops.items[0] = (DroppedItem){.active = true, .stack = {ITEM_OAK_PLANKS, 1}};
  DayNightState light = itemTestNeutralLight();
  Mat4 projection;
  mat4_identity(projection);
  projection[0] = projection[5] = 1 / .35f;
  projection[10] = -.2f;
  const Vec3 eyes[] = {{0, .05f, 3}, {3, .05f, 0}, {0, 3.05f, 0}};
  bool ok = true;
  for (int side = 0; side < 3; side++) {
    Vec3 center = {0, .05f, 0}, up = side == 2 ? (Vec3){0, 0, -1} : (Vec3){0, 1, 0};
    Mat4 view;
    mat4_lookAt(view, &eyes[side], &center, &up);
    unsigned char pixels[256 * 256 * 4];
    itemTestClear(1);
    renderDroppedItems(renderer, &drops, view, projection, &light);
    itemTestRead(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    unsigned visible = 0;
    for (size_t i = 3; i < sizeof(pixels); i += 4)
      visible += pixels[i] != 0;
    ok &= visible > 7000;
    itemTestClear(0);
    renderDroppedItems(renderer, &drops, view, projection, &light);
    itemTestRead(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    for (size_t i = 3; i < sizeof(pixels); i += 4)
      ok &= pixels[i] == 0;
  }
  if (!ok)
    fprintf(stderr, "Dropped cubes lost volume or ignored foreground depth\n");
  return ok;
}

static void itemTestFillSkinRect(unsigned char* pixels, int x, int y, int width, int height, const unsigned char color[4]) {
  for (int row = y; row < y + height; row++)
    for (int col = x; col < x + width; col++)
      memcpy(pixels + ((size_t)row * PLAYER_SKIN_SIZE + col) * 4, color, 4);
}

static bool itemTestWriteHeldSkin(const char* path) {
  unsigned char pixels[PLAYER_SKIN_SIZE * PLAYER_SKIN_SIZE * 4] = {0};
  static const int rightBase[PLAYER_MODEL_FACE_COUNT][4] = {{40, 20, 4, 12}, {48, 20, 4, 12}, {44, 16, 4, 4}, {48, 16, 4, 4}, {44, 20, 4, 12}, {52, 20, 4, 12}};
  static const int rightOuter[PLAYER_MODEL_FACE_COUNT][4] = {{40, 36, 4, 12}, {48, 36, 4, 12}, {44, 32, 4, 4}, {48, 32, 4, 4}, {44, 36, 4, 12}, {52, 36, 4, 12}};
  static const int leftBase[PLAYER_MODEL_FACE_COUNT][4] = {{32, 52, 4, 12}, {40, 52, 4, 12}, {36, 48, 4, 4}, {40, 48, 4, 4}, {36, 52, 4, 12}, {44, 52, 4, 12}};
  static const int leftOuter[PLAYER_MODEL_FACE_COUNT][4] = {{48, 52, 4, 12}, {56, 52, 4, 12}, {52, 48, 4, 4}, {56, 48, 4, 4}, {52, 52, 4, 12}, {60, 52, 4, 12}};
  const unsigned char colors[4][4] = {{241, 30, 189, 255}, {242, 132, 22, 255}, {24, 212, 232, 255}, {40, 80, 240, 255}};
  for (int face = 0; face < PLAYER_MODEL_FACE_COUNT; face++) {
    itemTestFillSkinRect(pixels, rightBase[face][0], rightBase[face][1], rightBase[face][2], rightBase[face][3], colors[0]);
    itemTestFillSkinRect(pixels, rightOuter[face][0], rightOuter[face][1], rightOuter[face][2] / 2, rightOuter[face][3], colors[1]);
    itemTestFillSkinRect(pixels, leftBase[face][0], leftBase[face][1], leftBase[face][2], leftBase[face][3], colors[2]);
    itemTestFillSkinRect(pixels, leftOuter[face][0], leftOuter[face][1], leftOuter[face][2] / 2, leftOuter[face][3], colors[3]);
  }

  unsigned char header[18] = {0};
  header[2] = 2;
  header[12] = PLAYER_SKIN_SIZE;
  header[14] = PLAYER_SKIN_SIZE;
  header[16] = 32;
  header[17] = 0x28;
  FILE* file = fopen(path, "wb");
  if (!file)
    return false;
  bool ok = fwrite(header, 1, sizeof(header), file) == sizeof(header);
  for (int y = 0; y < PLAYER_SKIN_SIZE && ok; y++)
    for (int x = 0; x < PLAYER_SKIN_SIZE && ok; x++) {
      const unsigned char* rgba = pixels + ((size_t)y * PLAYER_SKIN_SIZE + x) * 4;
      unsigned char bgra[4] = {rgba[2], rgba[1], rgba[0], rgba[3]};
      ok = fwrite(bgra, 1, sizeof(bgra), file) == sizeof(bgra);
    }
  if (fclose(file) != 0)
    ok = false;
  return ok;
}

static bool itemTestHeldArmColor(const unsigned char* pixel, int hand) {
  static const unsigned char colors[4][3] = {{241, 30, 189}, {242, 132, 22}, {24, 212, 232}, {40, 80, 240}};
  int first = hand ? 2 : 0;
  for (int color = first; color < first + 2; color++)
    if (abs((int)pixel[0] - colors[color][0]) <= 2 && abs((int)pixel[1] - colors[color][1]) <= 2 && abs((int)pixel[2] - colors[color][2]) <= 2)
      return true;
  return false;
}

static bool itemTestHeldArmLayerColor(const unsigned char* pixel, int hand, int outer) {
  static const unsigned char colors[4][3] = {{241, 30, 189}, {242, 132, 22}, {24, 212, 232}, {40, 80, 240}};
  const unsigned char* color = colors[hand * 2 + outer];
  return abs((int)pixel[0] - color[0]) <= 2 && abs((int)pixel[1] - color[1]) <= 2 && abs((int)pixel[2] - color[2]) <= 2;
}

static unsigned itemTestCountHeldArm(const unsigned char* pixels, int width, int height, int hand) {
  unsigned count = 0;
  for (int y = 0; y < height; y++)
    for (int x = 0; x < width; x++) {
      if ((!hand && x < width / 2) || (hand && x >= width / 2))
        continue;
      count += itemTestHeldArmColor(pixels + ((size_t)y * width + x) * 4, hand);
    }
  return count;
}

static unsigned itemTestCountHeldArmLayer(const unsigned char* pixels, int width, int height, int hand, int outer) {
  unsigned count = 0;
  for (int y = 0; y < height; y++)
    for (int x = 0; x < width; x++) {
      if ((!hand && x < width / 2) || (hand && x >= width / 2))
        continue;
      count += itemTestHeldArmLayerColor(pixels + ((size_t)y * width + x) * 4, hand, outer);
    }
  return count;
}

static bool itemTestHeld(ItemRenderer* renderer, const PlayerRenderer* playerRenderer, ItemTestTarget* target) {
  const int sizes[][2] = {{320, 240}, {180, 320}, {960, 540}};
  DayNightState light = itemTestNeutralLight();
  PlayerModelPose pose;
  playerModelPose(&pose, &(PlayerPoseInput){.grounded = true});
  bool ok = true;
  for (size_t size = 0; ok && size < sizeof(sizes) / sizeof(*sizes); size++) {
    int width = sizes[size][0], height = sizes[size][1];
    if (!itemTestTarget(target, width, height))
      return false;
    size_t bytes = (size_t)width * height * 4;
    unsigned char* pixels = malloc(bytes);
    unsigned char* other = malloc(bytes);
    float* before = malloc(bytes);
    float* after = malloc(bytes);
    if (!pixels || !other || !before || !after) {
      free(pixels);
      free(other);
      free(before);
      free(after);
      return false;
    }
    itemTestClear(.375);
    itemTestRead(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, before);
    ItemStack primary = {ITEM_GRASS_BLOCK, 4}, secondary = {ITEM_STONE, 3};
    ok &= renderHeldItems(renderer, playerRenderer, primary, secondary, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    itemTestRead(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, after);
    ok &= !memcmp(before, after, bytes);
    unsigned rightArm = itemTestCountHeldArm(pixels, width, height, 0), leftArm = itemTestCountHeldArm(pixels, width, height, 1);
    ok &= rightArm == 0 && leftArm == 0;
    ok &= itemTestCountHeldArmLayer(pixels, width, height, 0, 0) == 0 && itemTestCountHeldArmLayer(pixels, width, height, 0, 1) == 0;
    ok &= itemTestCountHeldArmLayer(pixels, width, height, 1, 0) == 0 && itemTestCountHeldArmLayer(pixels, width, height, 1, 1) == 0;
    // A block-only viewmodel must not leave hidden arm fragments in the private
    // target, and compositing that target must continue to preserve world depth.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->heldTarget.framebuffer);
    itemTestRead(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, after);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, target->framebuffer);
    unsigned armDepth = 0;
    for (size_t pixel = 0; pixel < (size_t)width * height; pixel++)
      if ((itemTestHeldArmColor(pixels + pixel * 4, 0) || itemTestHeldArmColor(pixels + pixel * 4, 1)) && after[pixel] < 0.999f)
        armDepth++;
    ok &= armDepth == 0;
    unsigned left = 0, right = 0;
    for (int y = 0; y < height; y++)
      for (int x = 0; x < width; x++) {
        bool opaque = pixels[((size_t)y * width + x) * 4 + 3] != 0;
        if (x < width / 2)
          left += opaque;
        else
          right += opaque;
        if (abs(x - width / 2) < width / 12 && abs(y - height / 2) < height / 12)
          ok &= !opaque;
      }
    ok &= left > 100 && right > 100;
    GLuint retained = renderer->heldTarget.framebuffer;
    itemTestClear(.375);
    primary.item = ITEM_STONE;
    ok &= renderHeldItems(renderer, playerRenderer, primary, (ItemStack){0}, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, other);
    unsigned different = 0;
    for (size_t i = 0; i < bytes; i += 4)
      different += memcmp(pixels + i, other + i, 3) != 0;
    ok &= different > 100 && retained == renderer->heldTarget.framebuffer && itemTestCountHeldArm(other, width, height, 0) == 0;

    // Walking bob still moves the block even though no first-person arm is drawn.
    itemTestClear(.375);
    pose.gaitWeight = 1;
    pose.gaitPhase = 1.5707963267948966;
    ok &= renderHeldItems(renderer, playerRenderer, primary, (ItemStack){0}, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    different = 0;
    for (size_t i = 0; i < bytes; i += 4)
      different += memcmp(pixels + i, other + i, 3) != 0;
    ok &= different > 20 && itemTestCountHeldArm(pixels, width, height, 0) == 0;
    pose.gaitWeight = 0;
    pose.gaitPhase = 0;

    // Breaking must use a true strike/recovery path. The previous symmetric
    // sin(pi * progress) transform made quarter and three-quarter phases
    // identical, so a held block retraced the exact strike poses backward.
    itemTestClear(.375);
    pose.punch = 0.25f;
    ok &= renderHeldItems(renderer, playerRenderer, primary, (ItemStack){0}, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    ok &= itemTestCountHeldArm(pixels, width, height, 0) == 0;
    itemTestClear(.375);
    pose.punch = 0.75f;
    ok &= renderHeldItems(renderer, playerRenderer, primary, (ItemStack){0}, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, other);
    ok &= itemTestCountHeldArm(other, width, height, 0) == 0;
    different = 0;
    for (size_t i = 0; i < bytes; i += 4)
      different += memcmp(pixels + i, other + i, 3) != 0;
    ok &= different > 100;

    // Both ends of the normalized cycle are the same resting item pose.
    itemTestClear(.375);
    pose.punch = 0;
    ok &= renderHeldItems(renderer, playerRenderer, primary, (ItemStack){0}, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    itemTestClear(.375);
    pose.punch = 1;
    ok &= renderHeldItems(renderer, playerRenderer, primary, (ItemStack){0}, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, other);
    ok &= !memcmp(pixels, other, bytes);

    // Successful placement has its own one-shot held-model motion and can
    // animate the offhand independently when placement falls back to that slot.
    itemTestClear(.375);
    pose.punch = 0;
    pose.placeMain = 0.5f;
    ok &= renderHeldItems(renderer, playerRenderer, primary, (ItemStack){0}, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, other);
    ok &= itemTestCountHeldArm(other, width, height, 0) == 0;
    different = 0;
    for (size_t i = 0; i < bytes; i += 4)
      different += memcmp(pixels + i, other + i, 3) != 0;
    ok &= different > 100;
    pose.placeMain = 0;

    itemTestClear(.375);
    ok &= renderHeldItems(renderer, playerRenderer, (ItemStack){0}, secondary, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    ok &= itemTestCountHeldArm(pixels, width, height, 1) == 0;
    itemTestClear(.375);
    pose.placeOffhand = 0.5f;
    ok &= renderHeldItems(renderer, playerRenderer, (ItemStack){0}, secondary, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, other);
    ok &= itemTestCountHeldArm(other, width, height, 1) == 0;
    different = 0;
    for (size_t i = 0; i < bytes; i += 4)
      different += memcmp(pixels + i, other + i, 3) != 0;
    ok &= different > 100;
    pose.placeOffhand = 0;

    // Suppression is per hand: a block has no arm while a non-placeable held
    // equipment item still uses its own skinned arm and sleeve.
    ItemStack equipment = {ITEM_LEATHER_BOOTS, 1};
    itemTestClear(.375);
    ok &= renderHeldItems(renderer, playerRenderer, primary, equipment, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, other);
    ok &= itemTestCountHeldArm(other, width, height, 0) == 0 && itemTestCountHeldArm(other, width, height, 1) > 40;
    ok &= itemTestCountHeldArmLayer(other, width, height, 0, 0) == 0 && itemTestCountHeldArmLayer(other, width, height, 0, 1) == 0;
    ok &= itemTestCountHeldArmLayer(other, width, height, 1, 0) > 10 && itemTestCountHeldArmLayer(other, width, height, 1, 1) > 5;
    itemTestClear(.375);
    ok &= renderHeldItems(renderer, playerRenderer, equipment, secondary, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, other);
    ok &= itemTestCountHeldArm(other, width, height, 0) > 40 && itemTestCountHeldArm(other, width, height, 1) == 0;
    ok &= itemTestCountHeldArmLayer(other, width, height, 0, 0) > 10 && itemTestCountHeldArmLayer(other, width, height, 0, 1) > 5;
    ok &= itemTestCountHeldArmLayer(other, width, height, 1, 0) == 0 && itemTestCountHeldArmLayer(other, width, height, 1, 1) == 0;

    // A block must remain arm-free through the complete breaking and placement
    // cycles in both hands, including the rest endpoints.
    for (int action = 0; ok && action < 3; action++) {
      for (int step = 0; ok && step <= 20; step++) {
        pose.punch = pose.placeMain = pose.placeOffhand = 0;
        float phase = step / 20.0f;
        if (action == 0)
          pose.punch = phase;
        else if (action == 1)
          pose.placeMain = phase;
        else
          pose.placeOffhand = phase;
        itemTestClear(.375);
        ok &= renderHeldItems(renderer, playerRenderer, action == 2 ? (ItemStack){0} : primary, action == 2 ? secondary : (ItemStack){0}, &pose, (float)width / height, &light);
        itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, other);
        unsigned arm = itemTestCountHeldArm(other, width, height, action == 2);
        unsigned visible = 0;
        for (size_t i = 3; i < bytes; i += 4)
          visible += other[i] != 0;
        if (arm != 0 || visible <= 100) {
          fprintf(stderr, "Held block arm-free check failed: %dx%d action=%d phase=%.2f arm=%u visible=%u\n", width, height, action, phase, arm, visible);
          ok = false;
        }
      }
      printf("Held block arm-free sweep: %dx%d action=%d\n", width, height, action);
    }
    pose.punch = pose.placeMain = pose.placeOffhand = 0;

    itemTestClear(.375);
    ok &= renderHeldItems(renderer, playerRenderer, (ItemStack){0}, (ItemStack){0}, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, other);
    for (size_t i = 3; i < bytes; i += 4)
      ok &= other[i] == 0;
    free(pixels);
    free(other);
    free(before);
    free(after);
  }
  if (!ok)
    fprintf(stderr, "Held item visibility, item selection, depth isolation, or reuse failed\n");
  return ok;
}

static GLuint itemFailedFramebuffer, itemFailedTexture, itemFailedDepth;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC itemRealFramebufferStatus;

static GLenum GLAPIENTRY itemFailFramebuffer(GLenum target) {
  (void)target;
  GLint framebuffer, texture, depth;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
  glGetIntegerv(GL_RENDERBUFFER_BINDING, &depth);
  itemFailedFramebuffer = (GLuint)framebuffer;
  itemFailedTexture = (GLuint)texture;
  itemFailedDepth = (GLuint)depth;
  return GL_FRAMEBUFFER_UNSUPPORTED;
}

static bool itemFailedObjectsReleased(void) {
  return itemFailedFramebuffer && itemFailedTexture && itemFailedDepth && !glIsFramebuffer(itemFailedFramebuffer) && !glIsTexture(itemFailedTexture) &&
         !glIsRenderbuffer(itemFailedDepth);
}

static bool itemTestAllocationFailure(ItemRenderer* renderer, const PlayerRenderer* playerRenderer) {
  ItemRenderer failed = {0};
  InventoryCheckGLState before, after;
  inventoryCheckCaptureState(&before);
  itemRealFramebufferStatus = __glewCheckFramebufferStatus;
  __glewCheckFramebufferStatus = itemFailFramebuffer;
  bool initialized = initItemRenderer(&failed);
  __glewCheckFramebufferStatus = itemRealFramebufferStatus;
  inventoryCheckCaptureState(&after);
  bool ok = !initialized && !failed.program && !failed.compositeProgram && !failed.vbo && !failed.vao && !failed.materials && itemFailedObjectsReleased() &&
            inventoryCheckStateEqual(&before, &after);
  for (int item = 0; item <= ITEM_ID_LAST; item++)
    ok &= failed.icons[item] == 0;
  cleanupItemRenderer(&failed);

  // Force a new held target size to fail after a good target already exists.
  // The existing target must survive and drawing must recover on the next call.
  ItemRenderTarget retained = renderer->heldTarget;
  glViewport(0, 0, retained.width - 1, retained.height - 1);
  inventoryCheckCaptureState(&before);
  PlayerModelPose pose;
  playerModelPose(&pose, &(PlayerPoseInput){.grounded = true});
  DayNightState light = itemTestNeutralLight();
  __glewCheckFramebufferStatus = itemFailFramebuffer;
  bool drawn = renderHeldItems(renderer, playerRenderer, (ItemStack){ITEM_STONE, 1}, (ItemStack){0}, &pose, (float)(retained.width - 1) / (retained.height - 1), &light);
  __glewCheckFramebufferStatus = itemRealFramebufferStatus;
  inventoryCheckCaptureState(&after);
  ok &= !drawn && itemFailedObjectsReleased() && inventoryCheckStateEqual(&before, &after);
  ok &= renderer->heldTarget.framebuffer == retained.framebuffer && renderer->heldTarget.color == retained.color && renderer->heldTarget.depth == retained.depth &&
        renderer->heldTarget.width == retained.width && renderer->heldTarget.height == retained.height;
  ok &= glIsFramebuffer(retained.framebuffer) && glIsTexture(retained.color) && glIsRenderbuffer(retained.depth);
  ok &= renderHeldItems(renderer, playerRenderer, (ItemStack){ITEM_STONE, 1}, (ItemStack){0}, &pose, (float)(retained.width - 1) / (retained.height - 1), &light);
  if (!ok)
    fprintf(stderr, "Item initialization/held-target allocation rollback or recovery failed\n");
  return ok;
}

#include "held_arm_transparency_checks.h"

static bool testItemRendering(void) {
  InventoryCheckGLState original;
  inventoryCheckCaptureState(&original);
  GLint arrayBuffer, textureArray, renderbuffer;
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
  glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &textureArray);
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  ItemTestTarget target = {0};
  ItemRenderer* renderer = HUDItems();
  const char* heldSkinPath = "test-held-arms.tga";
  PlayerRenderer heldPlayer = {0};
  bool ok = itemTestWriteHeldSkin(heldSkinPath) && initPlayerRenderer(&heldPlayer, heldSkinPath);
  ok &= itemTestFaces(renderer, &target) && itemTestIcons(renderer) && itemTestDroppedVolume(renderer, &target) && itemTestHeld(renderer, &heldPlayer, &target);
  if (ok)
    ok = itemTestArmTransparency(renderer, &heldPlayer, &target);
  if (ok) {
    glViewport(3, 5, 240, 220);
    glScissor(11, 13, 17, 19);
    glEnable(GL_SCISSOR_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
    glPolygonMode(GL_FRONT, GL_LINE);
    glPolygonMode(GL_BACK, GL_POINT);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_DST_COLOR, GL_ONE, GL_ZERO, GL_ONE);
    glActiveTexture(GL_TEXTURE3);
    InventoryCheckGLState expected, actual;
    inventoryCheckCaptureState(&expected);
    PlayerModelPose pose;
    playerModelPose(&pose, &(PlayerPoseInput){.grounded = true});
    DayNightState light = itemTestNeutralLight();
    ok &= renderHeldItems(renderer, &heldPlayer, (ItemStack){ITEM_LEATHER_BOOTS, 1}, (ItemStack){0}, &pose, 240.0f / 220, &light);
    inventoryCheckCaptureState(&actual);
    ok &= inventoryCheckStateEqual(&expected, &actual);
    ok &= itemTestAllocationFailure(renderer, &heldPlayer);
  }
  cleanupPlayerRenderer(&heldPlayer);
  remove(heldSkinPath);
  itemTestDestroyTarget(&target);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)original.drawFramebuffer);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)original.readFramebuffer);
  glPopAttrib();
  inventoryCheckRestoreState(&original);
  glBindBuffer(GL_ARRAY_BUFFER, (GLuint)arrayBuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)renderbuffer);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, (GLuint)textureArray);
  glActiveTexture((GLenum)original.activeTexture);
  ok &= glGetError() == GL_NO_ERROR;
  if (ok)
    puts("3D item face images, GUI silhouettes, dropped volume, held items, depth isolation, and state checks passed");
  else
    fprintf(stderr, "3D item rendering checks failed\n");
  return ok;
}

#endif
