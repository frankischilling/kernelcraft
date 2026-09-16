// Independent render-distance and GPU-index regressions included by
// render_benchmark.c after the hidden OpenGL context and draw wrappers exist.

static bool chunkRenderClearWorld(void) {
  for (int x = 0; x < CHUNKS_PER_AXIS; x++)
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Chunk* chunk = getChunk(&(Vec2i){x, z});
      if (!chunk)
        return false;
      memset(chunk->blocks, 0, sizeof(chunk->blocks));
      chunk->dirty = true;
    }
  return true;
}

static RenderResult chunkRenderFrame(Camera* camera, Vec3 target) {
  vec3_subtract(&camera->front, &target, &camera->position);
  vec3_normalize(&camera->front, &camera->front);
  Mat4 view, projection;
  mat4_lookAt(view, &camera->position, &target, &camera->up);
  mat4_perspective(projection, 70, 960.0f / 540.0f, 0.1f, 1000);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  return renderWorld(camera, view, projection, false);
}

static bool chunkRenderDistanceChecks(GLuint shader) {
  if (!chunkRenderClearWorld())
    return false;
  Vec3i nearBlock = {16, 20, 8};
  if (!setBlock(&nearBlock, BLOCK_STONE) || !setWorldRenderDistance(1) || !initWorld(shader))
    return false;

  Camera camera = {.position = {8, 20.5f, 8}, .up = {0, 1, 0}};
  Vec3 target = {16.5f, 20.5f, 8.5f};
  RenderResult first = chunkRenderFrame(&camera, target);
  unsigned char nearPixel[3];
  glReadPixels(480, 270, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, nearPixel);
  if (!first.success || first.visibilityChunksScanned != 9 || first.visibilityCandidates != 1 || first.chunksRendered != 1 || !(nearPixel[0] || nearPixel[1] || nearPixel[2])) {
    fprintf(stderr, "Radius-one boundary failed: scanned=%d candidates=%d rendered=%d pixel=(%u,%u,%u)\n", first.visibilityChunksScanned, first.visibilityCandidates,
            first.chunksRendered, nearPixel[0], nearPixel[1], nearPixel[2]);
    return false;
  }

  RenderResult cached = chunkRenderFrame(&camera, target);
  if (!cached.success || cached.visibilityChunksScanned || cached.visibilityCandidates != first.visibilityCandidates || cached.chunksRendered != first.chunksRendered)
    return false;

  // Setting the existing value or rejecting an invalid value must not evict an
  // otherwise reusable visibility list.
  if (!setWorldRenderDistance(1) || setWorldRenderDistance(0) || setWorldRenderDistance(WORLD_RENDER_DISTANCE_MAX + 1) || getWorldRenderDistance() != 1)
    return false;
  cached = chunkRenderFrame(&camera, target);
  if (!cached.success || cached.visibilityChunksScanned)
    return false;

  if (!setWorldRenderDistance(2))
    return false;
  RenderResult expanded = chunkRenderFrame(&camera, target);
  if (!expanded.success || expanded.visibilityChunksScanned != 25 || expanded.visibilityCandidates != 1)
    return false;

  // The distance test is still based on chunk centers. At X=8, the +1 chunk
  // center is exactly 16 blocks away while the +2 center is 32 blocks away.
  if (!setWorldRenderDistance(1) || !setBlock(&nearBlock, BLOCK_AIR))
    return false;
  Vec3i farBlock = {32, 20, 8};
  if (!setBlock(&farBlock, BLOCK_STONE))
    return false;
  target = (Vec3){32.5f, 20.5f, 8.5f};
  RenderResult distant = chunkRenderFrame(&camera, target);
  unsigned char farPixel[3];
  glReadPixels(480, 270, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, farPixel);
  if (!distant.success || distant.visibilityChunksScanned != 9 || distant.visibilityCandidates || distant.chunksRendered || farPixel[0] || farPixel[1] || farPixel[2]) {
    fprintf(stderr, "Radius-one far chunk leaked: scanned=%d candidates=%d rendered=%d pixel=(%u,%u,%u)\n", distant.visibilityChunksScanned, distant.visibilityCandidates,
            distant.chunksRendered, farPixel[0], farPixel[1], farPixel[2]);
    return false;
  }

  // Signed finite-world edges clip the square candidate neighborhood before
  // the exact circular test. These counts are derived only from 16 chunks/axis
  // and radius two: 3x5 at each edge, then 1x5 just outside the loaded world.
  if (!setWorldRenderDistance(2))
    return false;
  const float positions[] = {-127.5f, 127.5f, 159.9f, 160.1f};
  const int expectedScans[] = {15, 15, 5, 0};
  for (size_t i = 0; i < sizeof(positions) / sizeof(positions[0]); i++) {
    camera.position = (Vec3){positions[i], 20.5f, 8};
    RenderResult edge = chunkRenderFrame(&camera, (Vec3){0, 20.5f, 8});
    if (!edge.success || edge.visibilityChunksScanned != expectedScans[i]) {
      fprintf(stderr, "World-edge candidate scan at x=%.1f: %d, expected %d\n", positions[i], edge.visibilityChunksScanned, expectedScans[i]);
      return false;
    }
  }

  // The new default must reveal actual geometry beyond the previous 96-block
  // radius. This chunk center is 112 blocks from the camera.
  Vec3i expandedBlock = {120, 20, 8};
  if (!setBlock(&farBlock, BLOCK_AIR) || !setBlock(&expandedBlock, BLOCK_STONE) || !setWorldRenderDistance(6))
    return false;
  camera.position = (Vec3){8, 20.5f, 8};
  target = (Vec3){120.5f, 20.5f, 8.5f};
  RenderResult previousRadius = chunkRenderFrame(&camera, target);
  glReadPixels(480, 270, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, farPixel);
  if (!previousRadius.success || previousRadius.chunksRendered || farPixel[0] || farPixel[1] || farPixel[2] || !setWorldRenderDistance(12))
    return false;
  RenderResult newRadius = chunkRenderFrame(&camera, target);
  glReadPixels(480, 270, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, farPixel);
  if (!newRadius.success || newRadius.chunksRendered != 1 || newRadius.visibilityChunksScanned != 256 || !(farPixel[0] || farPixel[1] || farPixel[2]))
    return false;

  puts("Chunk render distance, cache invalidation, signed edges, and exact radius checks passed");
  return glGetError() == GL_NO_ERROR;
}

static bool chunkRenderShortIndexChecks(GLuint shader, unsigned char* before, unsigned char* after, size_t pixelBytes) {
  if (!chunkRenderClearWorld())
    return false;
  Vec3i block = {1, 20, 1};
  if (!setBlock(&block, BLOCK_STONE) || !setWorldRenderDistance(2) || !initWorld(shader))
    return false;

  Camera camera = {.position = {1.5f, 20.5f, 8}, .up = {0, 1, 0}};
  Vec3 target = {1.5f, 20.5f, 1.5f};
  RenderResult first = chunkRenderFrame(&camera, target);
  glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, before);
  // One isolated cube is six quads, six indices per quad. Its maximum vertex
  // index is 23, so the retained element payload is exactly 36 uint16_t values.
  const size_t expectedBytes = 6u * 6u * sizeof(uint16_t);
  if (!first.success || first.submittedQuads != 6 || first.indexBytesRetained != expectedBytes || first.shadowDrawCalls <= 0)
    return false;

  if (!setBlock(&block, BLOCK_AIR) || !setBlock(&block, BLOCK_STONE))
    return false;
  RenderResult rebuilt = chunkRenderFrame(&camera, target);
  glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, after);
  if (!rebuilt.success || rebuilt.chunksRebuilt != 1 || rebuilt.shadowDrawCalls <= 0 || rebuilt.indexBytesUploaded != expectedBytes ||
      rebuilt.indexBytesRetained != expectedBytes || memcmp(before, after, pixelBytes) != 0) {
    fprintf(stderr, "16-bit chunk EBO failed: rebuilt=%d uploaded=%zu retained=%zu pixels_equal=%d\n", rebuilt.chunksRebuilt, rebuilt.indexBytesUploaded,
            rebuilt.indexBytesRetained, memcmp(before, after, pixelBytes) == 0);
    return false;
  }

  return glGetError() == GL_NO_ERROR;
}

static bool chunkRenderWideIndexChecks(GLuint shader, unsigned char* before, unsigned char* after, size_t pixelBytes) {
  if (!chunkRenderClearWorld())
    return false;
  Chunk* chunk = getChunk(&(Vec2i){CHUNKS_PER_AXIS / 2, CHUNKS_PER_AXIS / 2});
  if (!chunk)
    return false;
  for (int x = 0; x < CHUNK_SIZE; x++)
    for (int y = 0; y < CHUNK_HEIGHT; y++)
      for (int z = 0; z < CHUNK_SIZE; z++)
        if ((x + y + z) % 2 == 0)
          chunk->blocks[x][y][z].id = BLOCK_STONE;
  chunk->dirty = true;
  if (!setWorldRenderDistance(2) || !initWorld(shader))
    return false;

  Camera camera = {.position = {8, 32.5f, 40}, .up = {0, 1, 0}};
  Vec3 target = {8, 32.5f, 8};
  RenderResult first = chunkRenderFrame(&camera, target);
  glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, before);

  // A 16x64x16 parity checkerboard contains 8192 isolated blocks. No exposed
  // unit faces can merge, yielding 49,152 quads and 196,608 vertices. The last
  // vertex exceeds UINT16_MAX, so all 294,912 indices must remain uint32_t.
  const size_t expectedQuads = (size_t)CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE / 2 * 6;
  const size_t expectedBytes = expectedQuads * 6 * sizeof(uint32_t);
  if (!first.success || first.chunksRendered != 1 || first.submittedQuads != expectedQuads || first.indexBytesRetained != expectedBytes || first.shadowDrawCalls <= 0) {
    fprintf(stderr, "32-bit checkerboard setup failed: rendered=%d quads=%zu/%zu retained=%zu/%zu\n", first.chunksRendered, first.submittedQuads, expectedQuads,
            first.indexBytesRetained, expectedBytes);
    return false;
  }

  Vec3i interior = {1, 1, 2};
  if (!setBlock(&interior, BLOCK_AIR) || !setBlock(&interior, BLOCK_STONE))
    return false;
  RenderResult rebuilt = chunkRenderFrame(&camera, target);
  glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, after);
  if (!rebuilt.success || rebuilt.chunksRebuilt != 1 || rebuilt.shadowDrawCalls <= 0 || rebuilt.indexBytesUploaded != expectedBytes ||
      rebuilt.indexBytesRetained != expectedBytes || rebuilt.submittedQuads != expectedQuads || memcmp(before, after, pixelBytes) != 0) {
    fprintf(stderr, "32-bit checkerboard EBO failed: rebuilt=%d uploaded=%zu/%zu retained=%zu/%zu quads=%zu pixels_equal=%d\n", rebuilt.chunksRebuilt, rebuilt.indexBytesUploaded,
            expectedBytes, rebuilt.indexBytesRetained, expectedBytes, rebuilt.submittedQuads, memcmp(before, after, pixelBytes) == 0);
    return false;
  }

  puts("Chunk 16-bit EBO, checked 32-bit fallback, shadow redraw, and pixel identity checks passed");
  return glGetError() == GL_NO_ERROR;
}

static bool testChunkRenderStorageAndDistance(GLuint shader) {
  const int savedDistance = getWorldRenderDistance();
  GLint savedViewport[4];
  GLfloat savedClear[4];
  GLboolean savedDepth = glIsEnabled(GL_DEPTH_TEST);
  glGetIntegerv(GL_VIEWPORT, savedViewport);
  glGetFloatv(GL_COLOR_CLEAR_VALUE, savedClear);
  glViewport(0, 0, 960, 540);
  glClearColor(0, 0, 0, 1);
  glEnable(GL_DEPTH_TEST);

  const size_t pixelBytes = 960u * 540u * 3u;
  unsigned char* before = malloc(pixelBytes);
  unsigned char* after = malloc(pixelBytes);
  bool success = before && after;
  if (success)
    success = chunkRenderDistanceChecks(shader);
  if (success)
    success = chunkRenderShortIndexChecks(shader, before, after, pixelBytes);
  if (success)
    success = chunkRenderWideIndexChecks(shader, before, after, pixelBytes);
  free(before);
  free(after);

  // Leave later render fixtures on the ordinary deterministic seed-zero world,
  // while preserving the caller's configured render distance and GL setup.
  cleanupWorld();
  if (!initChunksSeeded(0) || !setWorldRenderDistance(savedDistance) || !initWorld(shader))
    success = false;
  glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);
  glClearColor(savedClear[0], savedClear[1], savedClear[2], savedClear[3]);
  if (savedDepth)
    glEnable(GL_DEPTH_TEST);
  else
    glDisable(GL_DEPTH_TEST);
  return success && glGetError() == GL_NO_ERROR;
}
