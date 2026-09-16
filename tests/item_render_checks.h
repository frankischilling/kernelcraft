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
  const char* paths[10] = {
      "assets/textures/stone.png",       "assets/textures/dirt.png",       "assets/textures/grass-top.png",   "assets/textures/grass-side.png", NULL, NULL, NULL,
      "assets/textures/cobblestone.png", "assets/textures/oak-planks.png", "assets/textures/stone-bricks.png"};
  unsigned char* images[10] = {0};
  bool ok = true;
  for (int layer = 0; layer < 10; layer++) {
    if (!paths[layer])
      continue;
    int width, height, channels;
    images[layer] = stbi_load(paths[layer], &width, &height, &channels, 4);
    ok &= images[layer] && width == 16 && height == 16;
  }
  const Vec3 normals[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
  const int layers[6][6] = {{3, 3, 2, 1, 3, 3}, {1, 1, 1, 1, 1, 1}, {0, 0, 0, 0, 0, 0}, {7, 7, 7, 7, 7, 7}, {8, 8, 8, 8, 8, 8}, {9, 9, 9, 9, 9, 9}};
  DayNightState light = itemTestNeutralLight();
  Mat4 model, projection;
  mat4_identity(model);
  mat4_identity(projection);
  projection[0] = projection[5] = 2;
  projection[10] = -0.2f;
  unsigned checked = 0;
  for (uint16_t item = 1; ok && item <= 6; item++)
    for (int face = 0; ok && face < 6; face++) {
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
          const unsigned char* expected = images[layers[item - 1][face]] + (ty * 16 + tx) * 4;
          const unsigned char* actual = pixels + (py * 256 + px) * 4;
          for (int c = 0; c < 4; c++)
            ok &= abs((int)actual[c] - expected[c]) <= 1;
          checked++;
        }
      if (!ok)
        fprintf(stderr, "3D item face image mismatch: item=%u face=%d\n", item, face);
    }
  for (int layer = 0; layer < 10; layer++)
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

static bool itemTestHeld(ItemRenderer* renderer, ItemTestTarget* target) {
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
    ok &= renderHeldItems(renderer, primary, secondary, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    itemTestRead(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, after);
    ok &= !memcmp(before, after, bytes);
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
    ok &= renderHeldItems(renderer, primary, (ItemStack){0}, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, other);
    unsigned different = 0;
    for (size_t i = 0; i < bytes; i += 4)
      different += memcmp(pixels + i, other + i, 3) != 0;
    ok &= different > 100 && retained == renderer->heldTarget.framebuffer;

    // Breaking must use a true strike/recovery path. The previous symmetric
    // sin(pi * progress) transform made quarter and three-quarter phases
    // identical, so a held block retraced the exact strike poses backward.
    itemTestClear(.375);
    pose.punch = 0.25f;
    ok &= renderHeldItems(renderer, primary, (ItemStack){0}, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    itemTestClear(.375);
    pose.punch = 0.75f;
    ok &= renderHeldItems(renderer, primary, (ItemStack){0}, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, other);
    different = 0;
    for (size_t i = 0; i < bytes; i += 4)
      different += memcmp(pixels + i, other + i, 3) != 0;
    ok &= different > 100;

    // Both ends of the normalized cycle are the same resting item pose.
    itemTestClear(.375);
    pose.punch = 0;
    ok &= renderHeldItems(renderer, primary, (ItemStack){0}, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    itemTestClear(.375);
    pose.punch = 1;
    ok &= renderHeldItems(renderer, primary, (ItemStack){0}, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, other);
    ok &= !memcmp(pixels, other, bytes);

    // Successful placement has its own one-shot hand motion and can animate the
    // offhand independently when placement falls back to that slot.
    itemTestClear(.375);
    pose.punch = 0;
    pose.placeMain = 0.5f;
    ok &= renderHeldItems(renderer, primary, (ItemStack){0}, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, other);
    different = 0;
    for (size_t i = 0; i < bytes; i += 4)
      different += memcmp(pixels + i, other + i, 3) != 0;
    ok &= different > 100;
    pose.placeMain = 0;

    itemTestClear(.375);
    ok &= renderHeldItems(renderer, (ItemStack){0}, secondary, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    itemTestClear(.375);
    pose.placeOffhand = 0.5f;
    ok &= renderHeldItems(renderer, (ItemStack){0}, secondary, &pose, (float)width / height, &light);
    itemTestRead(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, other);
    different = 0;
    for (size_t i = 0; i < bytes; i += 4)
      different += memcmp(pixels + i, other + i, 3) != 0;
    ok &= different > 100;
    pose.placeOffhand = 0;

    itemTestClear(.375);
    ok &= renderHeldItems(renderer, (ItemStack){0}, (ItemStack){0}, &pose, (float)width / height, &light);
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

static bool itemTestAllocationFailure(ItemRenderer* renderer) {
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
  bool drawn = renderHeldItems(renderer, (ItemStack){ITEM_STONE, 1}, (ItemStack){0}, &pose, (float)(retained.width - 1) / (retained.height - 1), &light);
  __glewCheckFramebufferStatus = itemRealFramebufferStatus;
  inventoryCheckCaptureState(&after);
  ok &= !drawn && itemFailedObjectsReleased() && inventoryCheckStateEqual(&before, &after);
  ok &= renderer->heldTarget.framebuffer == retained.framebuffer && renderer->heldTarget.color == retained.color && renderer->heldTarget.depth == retained.depth &&
        renderer->heldTarget.width == retained.width && renderer->heldTarget.height == retained.height;
  ok &= glIsFramebuffer(retained.framebuffer) && glIsTexture(retained.color) && glIsRenderbuffer(retained.depth);
  ok &= renderHeldItems(renderer, (ItemStack){ITEM_STONE, 1}, (ItemStack){0}, &pose, (float)(retained.width - 1) / (retained.height - 1), &light);
  if (!ok)
    fprintf(stderr, "Item initialization/held-target allocation rollback or recovery failed\n");
  return ok;
}

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
  bool ok = itemTestFaces(renderer, &target) && itemTestIcons(renderer) && itemTestDroppedVolume(renderer, &target) && itemTestHeld(renderer, &target);
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
    ok &= renderHeldItems(renderer, (ItemStack){ITEM_LEATHER_BOOTS, 1}, (ItemStack){0}, &pose, 240.0f / 220, &light);
    inventoryCheckCaptureState(&actual);
    ok &= inventoryCheckStateEqual(&expected, &actual);
    ok &= itemTestAllocationFailure(renderer);
  }
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
