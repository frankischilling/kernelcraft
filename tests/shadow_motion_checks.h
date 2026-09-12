static Vec3 shadowTestDirection(double cell) {
  double angle = cell * 6.283185307179586 / 1024;
  return (Vec3){(float)cos(angle), (float)sin(angle), 0};
}

static bool testShadowCacheTransitions(void) {
  ShadowCache cache;
  if (!initShadowCache(&cache, (Vec3){0, 0, 0}, 10))
    return false;
  GLint previousVAO, previousBuffer;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousBuffer);
  GLuint vao, buffers[2];
  glGenVertexArrays(1, &vao);
  glGenBuffers(2, buffers);
  glBindVertexArray(vao);
  const float vertices[] = {-1, 0, 0, 1, 0, 0, 0, 2, 0};
  const unsigned indices[] = {0, 1, 2};
  glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[1]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
  ShadowGeometry geometry = {vao, 3};
  const double directions[] = {10.25, 10.75, 11.25, 10.75, 1023.25, 0.25, 1023.75};
  const int expectedDraws[] = {2, 0, 1, 1, 2, 1, 1};
  bool ok = glGetError() == GL_NO_ERROR;
  int calls = 0;
  for (size_t step = 0; step < sizeof(directions) / sizeof(*directions); step++) {
    bool updated = updateShadowCache(&cache, shadowTestDirection(directions[step]), false, &geometry, 1, &calls);
    if (!updated || calls != expectedDraws[step]) {
      fprintf(stderr, "Shadow cache transition %zu: draws=%d expected=%d success=%d\n", step, calls, expectedDraws[step], updated);
      ok = false;
    }
  }
  Vec3 light = shadowTestDirection(1023.75);
  ok &= updateShadowCache(&cache, light, true, &geometry, 1, &calls) && calls == 2;
  // A rejected GL operation safely simulates a pass reporting an error after
  // submission. Neither old endpoint may survive an unsuccessful edit refresh.
  glEnable(GL_NONE);
  bool failed = !updateShadowCache(&cache, light, true, &geometry, 1, &calls);
  if (!failed || calls != 1) {
    fprintf(stderr, "Shadow cache failed refresh: failed=%d submitted=%d expected=1\n", failed, calls);
    ok = false;
  }
  ok &= updateShadowCache(&cache, light, false, &geometry, 1, &calls) && calls == 2;
  ok &= updateShadowCache(&cache, light, false, &geometry, 1, &calls) && calls == 0;
  // /time set day and /time set night put their active light at the same
  // zenith. Roundoff must not select a different neighboring cache interval.
  DayNightState noon = sampleDayNight(0.25), midnight = sampleDayNight(0.75);
  ok &= updateShadowCache(&cache, noon.lightDirection, false, &geometry, 1, &calls);
  for (int toggle = 0; toggle < 4; toggle++) {
    Vec3 direction = toggle % 2 ? noon.lightDirection : midnight.lightDirection;
    bool updated = updateShadowCache(&cache, direction, false, &geometry, 1, &calls);
    if (!updated || calls) {
      fprintf(stderr, "Equivalent day/night shadow direction rebuilt %d casters\n", calls);
      ok = false;
    }
  }
  glBindVertexArray(previousVAO);
  glBindBuffer(GL_ARRAY_BUFFER, previousBuffer);
  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(2, buffers);
  cleanupShadowCache(&cache);
  if (ok)
    puts("Shadow cache forward/reverse/wrap, edits, and failed-refresh recovery passed");
  return ok && glGetError() == GL_NO_ERROR;
}

// Fixed camera and uniform albedo distinguish raster flicker from texture
// motion. The second scene deliberately includes a moving cast-shadow edge.
static bool testShadowMotion(GLuint shader) {
  bool ok = testShadowCacheTransitions();
  for (int roof = 0; roof < 2; roof++) {
    clearTerrainFixture();
    for (int x = -8; x <= 8; x++)
      for (int z = -8; z <= 8; z++)
        ok &= setBlock(&(Vec3i){x, 10, z}, BLOCK_STONE);
    if (roof)
      for (int x = 3; x <= 5; x++)
        for (int z = -2; z <= 2; z++)
          ok &= setBlock(&(Vec3i){x, 14, z}, BLOCK_STONE);
    if (!ok || !initWorld(shader))
      return false;
    unsigned char tile[40];
    memset(tile, 200, sizeof(tile));
    for (int i = 3; i < 40; i += 4)
      tile[i] = 255;
    glActiveTexture(GL_TEXTURE0);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 1, 1, 10, 0, GL_RGBA, GL_UNSIGNED_BYTE, tile);
    Camera camera = {.position = {0.5f, 13.5f, 0.5f}, .front = {0, -1, 0}, .up = {0, 0, 1}};
    Vec3 target = {0.5f, 11, 0.5f};
    Mat4 view, projection;
    mat4_lookAt(view, &camera.position, &target, &camera.up);
    mat4_perspective(projection, 70, 960.0f / 540, 0.1f, 1000);
    unsigned char pixels[480 * 3], previous[sizeof(pixels)], first[sizeof(pixels)];
    int maxStep = 0, maxRange = 0, maxError = 0, totalMovement = 0, movedPixels = 0;
    unsigned long shadowCalls = 0;
    for (int frame = 0; frame < 180; frame++) {
      DayNightState state = sampleDayNight(0.125 + frame / 72000.0);
      setWorldDayNight(&state);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      RenderResult result = renderWorld(&camera, view, projection, false);
      ok &= result.success;
      shadowCalls += result.shadowDrawCalls;
      glReadPixels(240, 270, 480, 1, GL_RGB, GL_UNSIGNED_BYTE, pixels);
      int low = 255, high = 0;
      // At this phase daylight is fully established: top-face red irradiance
      // is 0.36 sky fill plus 0.62 times the sine of the solar elevation.
      double linear = displayToLinear(200) * (0.36 + 0.62 * sin((0.125 + frame / 72000.0) * 6.283185307179586));
      int expected = (int)lround(255 * (1.055 * pow(linear, 1 / 2.4) - 0.055));
      for (int i = 0; i < 480; i++) {
        int value = pixels[i * 3];
        if (value < low)
          low = value;
        if (value > high)
          high = value;
        int error = abs(value - expected);
        if (error > maxError)
          maxError = error;
        if (frame) {
          int step = abs(value - previous[i * 3]);
          if (step > maxStep)
            maxStep = step;
        }
      }
      if (high - low > maxRange)
        maxRange = high - low;
      if (!frame)
        memcpy(first, pixels, sizeof(first));
      memcpy(previous, pixels, sizeof(previous));
    }
    for (int i = 0; i < 480; i++) {
      int change = abs(pixels[i * 3] - first[i * 3]);
      totalMovement += change;
      movedPixels += change > 3;
    }
    printf("Shadow motion roof=%d: spatial_range=%d max_frame_step=%d expected_error=%d total_movement=%d moved_pixels=%d shadow_draws=%lu\n", roof, maxRange, maxStep, maxError,
           totalMovement, movedPixels, shadowCalls);
    // A one-byte quantization transition is legitimate. Moving cast edges
    // may darken or lighten, but should not jump several shades in one frame.
    ok &= maxStep <= 3;
    // This covers three seconds of the live clock, not a paused benchmark.
    // Rebuilding every frame previously submitted 720 caster draws per scene.
    ok &= shadowCalls > 0 && shadowCalls <= 32;
    ok &= roof ? maxRange >= 25 && movedPixels >= 5 : maxRange <= 2 && maxError <= 2;
    // Opening chat leaves the light fixed. Both color and shadow workload
    // should remain stable; resuming must not require unrelated mesh uploads.
    for (int frame = 0; frame < 20; frame++) {
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      RenderResult paused = renderWorld(&camera, view, projection, false);
      glReadPixels(240, 270, 480, 1, GL_RGB, GL_UNSIGNED_BYTE, pixels);
      ok &= paused.success && paused.shadowDrawCalls == 0 && paused.chunksRebuilt == 0 && memcmp(previous, pixels, sizeof(pixels)) == 0;
    }
  }
  if (!ok)
    fprintf(stderr, "Stationary terrain pulsed or moving cast shadow lost continuity\n");
  return ok && glGetError() == GL_NO_ERROR;
}
