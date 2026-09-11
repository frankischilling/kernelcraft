static int shadowPixelLuma(const unsigned char pixel[3]) {
  return pixel[0] + pixel[1] + pixel[2];
}

static bool shadowScreenPoint(const Mat4 combined, Vec3 point, int width, int height, int* x, int* y) {
  float w = combined[3] * point.x + combined[7] * point.y + combined[11] * point.z + combined[15];
  if (!isfinite(w) || fabsf(w) < 0.00001f)
    return false;
  float ndcX = (combined[0] * point.x + combined[4] * point.y + combined[8] * point.z + combined[12]) / w;
  float ndcY = (combined[1] * point.x + combined[5] * point.y + combined[9] * point.z + combined[13]) / w;
  *x = (int)((ndcX + 1.0f) * 0.5f * width);
  *y = (int)((ndcY + 1.0f) * 0.5f * height);
  return *x >= 0 && *x < width && *y >= 0 && *y < height;
}

static bool readShadowProbe(const Mat4 combined, Vec3 point, unsigned char pixel[3]) {
  int x, y;
  if (!shadowScreenPoint(combined, point, 960, 540, &x, &y))
    return false;
  glReadPixels(x, y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);
  return glGetError() == GL_NO_ERROR;
}

static bool testTerrainShadows(GLuint shader) {
  clearTerrainFixture();
  for (int x = -10; x <= 10; x++)
    for (int z = -5; z <= 5; z++)
      setBlock(&(Vec3i){x, 20, z}, BLOCK_STONE);
  for (int y = 21; y < 27; y++)
    setBlock(&(Vec3i){0, y, 0}, BLOCK_STONE);
  if (!initWorld(shader))
    return false;

  unsigned char tile[10 * 4];
  for (int layer = 0; layer < 10; layer++) {
    tile[layer * 4] = 180;
    tile[layer * 4 + 1] = 180;
    tile[layer * 4 + 2] = 180;
    tile[layer * 4 + 3] = 255;
  }
  glActiveTexture(GL_TEXTURE0);
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 1, 1, 10, 0, GL_RGBA, GL_UNSIGNED_BYTE, tile);

  Camera camera = {.position = {0, 31, 18}, .up = {0, 1, 0}, .fov = 70};
  Vec3 target = {0, 20.5f, 0.5f};
  Vec3 leftProbe = {-5.5f, 21.01f, 0.5f};
  Vec3 rightProbe = {5.5f, 21.01f, 0.5f};
  Mat4 view, projection, combined;
  mat4_lookAt(view, &camera.position, &target, &camera.up);
  mat4_perspective(projection, camera.fov, 960.0f / 540, 0.1f, 1000);
  mat4_multiply(combined, projection, view);

  DayNightState positive = sampleDayNight(0.125);
  setWorldDayNight(&positive);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ZERO);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glEnable(GL_SCISSOR_TEST);
  glScissor(0, 0, 960, 540);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(3, 7);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glClearDepth(0.25);
  RenderResult first = renderWorld(&camera, view, projection, false);
  unsigned char positiveLeft[3], positiveRight[3];
  bool success = first.success && first.chunksRebuilt == 0 && readShadowProbe(combined, leftProbe, positiveLeft) && readShadowProbe(combined, rightProbe, positiveRight);
  GLint framebuffer, depthFunction, cullFace, polygonMode[2];
  GLfloat polygonFactor, polygonUnits, clearDepth;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
  glGetIntegerv(GL_DEPTH_FUNC, &depthFunction);
  glGetIntegerv(GL_CULL_FACE_MODE, &cullFace);
  glGetIntegerv(GL_POLYGON_MODE, polygonMode);
  glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &polygonFactor);
  glGetFloatv(GL_POLYGON_OFFSET_UNITS, &polygonUnits);
  glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
  success &= framebuffer == 0 && glIsEnabled(GL_BLEND) && glIsEnabled(GL_CULL_FACE) && glIsEnabled(GL_SCISSOR_TEST) && glIsEnabled(GL_POLYGON_OFFSET_FILL) &&
             depthFunction == GL_LESS && cullFace == GL_BACK && polygonMode[0] == GL_FILL && polygonMode[1] == GL_FILL && polygonFactor == 3 && polygonUnits == 7 &&
             fabsf(clearDepth - 0.25f) < 0.0001f && first.shadowDrawCalls > 0;
  int positiveDifference = shadowPixelLuma(positiveRight) - shadowPixelLuma(positiveLeft);
  if (!success || positiveDifference < 18) {
    fprintf(stderr, "Positive-light terrain shadow was not darker on the left receiver: difference=%d\n", positiveDifference);
    return false;
  }

  glClearDepth(1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  RenderResult stable = renderWorld(&camera, view, projection, false);
  success = stable.success && stable.shadowDrawCalls == 0;
  if (!success)
    return false;

  for (int y = 21; y < 27; y++)
    setBlock(&(Vec3i){0, y, 0}, BLOCK_AIR);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  RenderResult removed = renderWorld(&camera, view, projection, false);
  unsigned char removedLeft[3];
  success = removed.success && removed.chunksRebuilt > 0 && removed.shadowDrawCalls > 0 && readShadowProbe(combined, leftProbe, removedLeft);
  int removedDifference = shadowPixelLuma(removedLeft) - shadowPixelLuma(positiveLeft);
  if (!success || removedDifference < 12) {
    fprintf(stderr, "Removing the occluder did not clear the shadow: difference=%d\n", removedDifference);
    return false;
  }

  for (int y = 21; y < 27; y++)
    setBlock(&(Vec3i){0, y, 0}, BLOCK_STONE);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (!renderWorld(&camera, view, projection, false).success)
    return false;
  DayNightState negative = sampleDayNight(0.375);
  setWorldDayNight(&negative);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  RenderResult reversed = renderWorld(&camera, view, projection, false);
  unsigned char negativeLeft[3], negativeRight[3];
  success = reversed.success && reversed.chunksRebuilt == 0 && reversed.shadowDrawCalls > 0 && readShadowProbe(combined, leftProbe, negativeLeft) && readShadowProbe(combined, rightProbe, negativeRight);
  int negativeDifference = shadowPixelLuma(negativeLeft) - shadowPixelLuma(negativeRight);
  if (!success || negativeDifference < 18) {
    fprintf(stderr, "Reversed-light terrain shadow did not move to the right receiver: difference=%d\n", negativeDifference);
    return false;
  }

  puts("Terrain cast shadows, occluder edits, and day/night direction updates passed");
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_POLYGON_OFFSET_FILL);
  return glGetError() == GL_NO_ERROR;
}
