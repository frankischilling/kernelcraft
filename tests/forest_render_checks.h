#ifndef FOREST_RENDER_CHECKS_H
#define FOREST_RENDER_CHECKS_H

// Render the generated forest through the normal terrain, shadow and sky passes.
// Material tests separately compare every tree face against the source PNGs.
static bool testForestRendering(GLuint shader, const char* capturePrefix) {
  if (!initChunksSeeded(0) || !initWorld(shader))
    return false;
  Vec3 tree = {0};
  int closest = WORLD_SIZE * 2;
  size_t roots = 0, leaves = 0, litter = 0;
  for (int x = -WORLD_SIZE / 2; x < WORLD_SIZE / 2; x++)
    for (int z = -WORLD_SIZE / 2; z < WORLD_SIZE / 2; z++)
      for (int y = 1; y < WORLD_HEIGHT; y++) {
        int id = getBlock(&(Vec3i){x, y, z})->id;
        leaves += id == 8;
        litter += id == 9;
        if (id != 7 || getBlock(&(Vec3i){x, y - 1, z})->id == 7)
          continue;
        roots++;
        if (abs(x) + abs(z) < closest) {
          closest = abs(x) + abs(z);
          tree = (Vec3){x + 0.5f, y + 3.0f, z + 0.5f};
        }
      }
  if (roots < 10 || leaves < roots * 10 || litter < roots)
    return false;
  SkyRenderer sky = {0};
  if (!initSky(&sky))
    return false;
  Camera camera = {.position = {tree.x + 12, tree.y + 8, tree.z + 20}, .up = {0, 1, 0}};
  vec3_subtract(&camera.front, &tree, &camera.position);
  vec3_normalize(&camera.front, &camera.front);
  Mat4 view, projection;
  mat4_lookAt(view, &camera.position, &tree, &camera.up);
  mat4_perspective(projection, 70, 960.0f / 540, 0.1f, 1000);
  DayNightState daylight = sampleDayNight(0.2);
  setWorldDayNight(&daylight);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  RenderResult result = renderWorld(&camera, view, projection, false);
  renderSky(&sky, &camera, 960.0f / 540, &daylight);
  size_t bytes = 960 * 540 * 3;
  unsigned char* pixels = malloc(bytes);
  bool success = pixels && result.success && result.chunksRendered > 0 && result.surfaceBlocks > 100;
  if (success) {
    GLint packAlignment;
    glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);
    size_t green = 0;
    for (size_t p = 0; p < bytes; p += 3)
      green += pixels[p + 1] > pixels[p] + 15 && pixels[p + 1] > pixels[p + 2] + 10;
    success &= green > 1000;
    printf("Generated oak forest: %zu roots, %zu leaf blocks, %zu leafy grass blocks, %zu green pixels, %d drawn chunks\n", roots, leaves, litter, green, result.chunksRendered);
    if (capturePrefix) {
      char path[1024];
      int length = snprintf(path, sizeof(path), "%s-forest.ppm", capturePrefix);
      FILE* image = length > 0 && (size_t)length < sizeof(path) ? fopen(path, "wb") : NULL;
      if (!image) {
        success = false;
      } else {
        success &= fprintf(image, "P6\n960 540\n255\n") > 0;
        for (int row = 539; row >= 0; row--)
          success &= fwrite(pixels + (size_t)row * 960 * 3, 1, 960 * 3, image) == 960 * 3;
        success &= fclose(image) == 0;
      }
    }
  }
  free(pixels);
  cleanupSky(&sky);
  return success && glGetError() == GL_NO_ERROR;
}

#endif
