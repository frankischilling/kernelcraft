static PFNGLGETQUERYOBJECTUIVPROC realQueryObject;
static bool holdOcclusionResults;
static int prematureQueryReads;

static void GLAPIENTRY delayOcclusionResult(GLuint query, GLenum parameter, GLuint* value) {
  if (holdOcclusionResults && parameter == GL_QUERY_RESULT_AVAILABLE) {
    *value = GL_FALSE;
    return;
  }
  if (holdOcclusionResults && parameter == GL_QUERY_RESULT)
    prematureQueryReads++;
  realQueryObject(query, parameter, value);
}

static RenderResult occlusionFrame(const Camera* camera, const Mat4 view, const Mat4 projection, bool wireframe) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  RenderResult result = renderWorld(camera, view, projection, wireframe);
  glFinish(); // Test only: make asynchronous results deterministic.
  return result;
}

// Real GPU regression: removing culling must fail the submission assertion;
// reusing stale visibility must fail the edit/movement pixel comparisons.
static bool testOcclusion(GLuint shader) {
  clearTerrainFixture();
  for (int x = 1; x < 15; x++)
    for (int y = 16; y < 32; y++)
      setBlock(&(Vec3i){x, y, -1}, BLOCK_STONE);
  for (int x = 6; x < 10; x++)
    for (int y = 22; y < 26; y++)
      setBlock(&(Vec3i){x, y, -20}, BLOCK_DIRT);
  if (!initWorld(shader))
    return false;
  Camera camera = {.position = {8, 24, 12}, .front = {0, 0, -1}, .up = {0, 1, 0}};
  Vec3 target = {8, 24, -20};
  Mat4 view, projection;
  mat4_lookAt(view, &camera.position, &target, &camera.up);
  mat4_perspective(projection, 70, 960.0f / 540, 0.1f, 1000);
  size_t bytes = 960 * 540 * 3;
  unsigned char* reference = malloc(bytes);
  unsigned char* culled = malloc(bytes);
  if (!reference || !culled) {
    free(reference);
    free(culled);
    return false;
  }
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  RenderResult first = renderWorld(&camera, view, projection, false), settled = {0};
  glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, reference);
  for (int frame = 0; frame < 8; frame++) {
    settled = occlusionFrame(&camera, view, projection, false);
  }
  glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, culled);
  bool success = first.success && settled.success && first.terrainDrawCalls == 2 && settled.terrainDrawCalls == 1 && settled.submittedTriangles < first.submittedTriangles &&
                 memcmp(reference, culled, bytes) == 0;
  printf("Occlusion wall: terrain draws %d -> %d, triangles %zu -> %zu, identical pixels %d\n", first.terrainDrawCalls, settled.terrainDrawCalls, first.submittedTriangles,
         settled.submittedTriangles, memcmp(reference, culled, bytes) == 0);
  success &= first.occlusionQueries == 2 && settled.chunksOccluded == 1 && settled.occlusionQueries == 0;

  GLuint framebuffer, attachments[2];
  glGenFramebuffers(1, &framebuffer);
  glGenRenderbuffers(2, attachments);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, attachments[0]);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 960, 540);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, attachments[0]);
  glBindRenderbuffer(GL_RENDERBUFFER, attachments[1]);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 960, 540);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, attachments[1]);
  success &= glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

  // Each transition starts with a proven-hidden rear chunk. Its very first
  // frame must match a fresh renderer with no cached query results.
  for (int transition = 0; transition < 9; transition++) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    camera.position = (Vec3){8, 24, 12};
    mat4_lookAt(view, &camera.position, &target, &camera.up);
    mat4_perspective(projection, 70, 960.0f / 540, 0.1f, 1000);
    glViewport(0, 0, 960, 540);
    setBlock(&(Vec3i){8, 24, -1}, BLOCK_STONE);
    if (!initWorld(shader)) {
      success = false;
      break;
    }
    for (int frame = 0; frame < 3; frame++)
      settled = occlusionFrame(&camera, view, projection, false);
    success &= settled.chunksOccluded == 1;
    switch (transition) {
    case 0: // Opening a one-block aperture at a negative-coordinate seam.
      setBlock(&(Vec3i){8, 24, -1}, BLOCK_AIR);
      break;
    case 1: // A side view reveals the rear chunk.
      camera.position.x = 28;
      mat4_lookAt(view, &camera.position, &target, &camera.up);
      break;
    case 2: // Projection changes include the running FOV transition.
      mat4_perspective(projection, 80, 960.0f / 540, 0.1f, 1000);
      break;
    case 3: // Viewport-only changes must also discard visibility.
      glViewport(0, 0, 480, 270);
      break;
    case 4: // Wireframe ignores solid occlusion evidence.
      break;
    case 5: // Free flight inside the foreground block.
      camera.position.z = -0.5f;
      mat4_lookAt(view, &camera.position, &target, &camera.up);
      break;
    case 6: // The foreground crosses the near plane.
      camera.position.z = 0.05f;
      mat4_lookAt(view, &camera.position, &target, &camera.up);
      break;
    case 7: // Rotation without changing the camera position.
      mat4_lookAt(view, &camera.position, &(Vec3){14, 24, -20}, &camera.up);
      break;
    case 8: // Switching render targets must discard prior sample evidence.
      glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
      break;
    }
    RenderResult changed = occlusionFrame(&camera, view, projection, transition == 4);
    glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, culled);
    success &= changed.success && changed.chunksOccluded == 0;
    if (transition == 0)
      success &= changed.chunksRebuilt == 2;
    if (transition == 4) {
      success &= changed.terrainDrawCalls == 2 && changed.occlusionQueries == 0;
      changed = occlusionFrame(&camera, view, projection, false);
      success &= changed.chunksOccluded == 0;
      glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, culled);
    }
    if (!initWorld(shader)) {
      success = false;
      break;
    }
    RenderResult fresh = occlusionFrame(&camera, view, projection, false);
    glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, reference);
    bool matches = fresh.success && changed.terrainDrawCalls == fresh.terrainDrawCalls && memcmp(reference, culled, bytes) == 0;
    success &= matches;
    // Repeat on the settled query path, including partially exposed chunks.
    for (int frame = 0; frame < 4; frame++)
      settled = occlusionFrame(&camera, view, projection, false);
    glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, culled);
    success &= settled.success && memcmp(reference, culled, bytes) == 0;
    if (transition == 0 || transition == 1)
      success &= settled.terrainDrawCalls == 2;
    printf("Occlusion transition %d: fresh pixels match %d, settled draws %d\n", transition, matches, settled.terrainDrawCalls);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  glDeleteFramebuffers(1, &framebuffer);
  glDeleteRenderbuffers(2, attachments);

  // A delayed query must neither block nor hide terrain. If an edit arrives
  // while that query is pending, its eventual zero-sample result is stale.
  camera.position = (Vec3){8, 24, 12};
  mat4_lookAt(view, &camera.position, &target, &camera.up);
  mat4_perspective(projection, 70, 960.0f / 540, 0.1f, 1000);
  glViewport(0, 0, 960, 540);
  setBlock(&(Vec3i){8, 24, -1}, BLOCK_STONE);
  success &= initWorld(shader);
  realQueryObject = __glewGetQueryObjectuiv;
  __glewGetQueryObjectuiv = delayOcclusionResult;
  holdOcclusionResults = true;
  for (int frame = 0; frame < 3; frame++) {
    RenderResult pending = occlusionFrame(&camera, view, projection, false);
    success &= pending.success && pending.terrainDrawCalls == 2 && pending.chunksOccluded == 0;
  }
  setBlock(&(Vec3i){8, 24, -1}, BLOCK_AIR);
  settled = occlusionFrame(&camera, view, projection, false);
  success &= settled.success && settled.terrainDrawCalls == 2;
  holdOcclusionResults = false;
  for (int frame = 0; frame < 4; frame++) {
    settled = occlusionFrame(&camera, view, projection, false);
    success &= settled.success && settled.terrainDrawCalls == 2 && settled.chunksOccluded == 0;
  }
  __glewGetQueryObjectuiv = realQueryObject;
  success &= prematureQueryReads == 0;
  free(reference);
  free(culled);
  if (!success)
    fprintf(stderr, "Hidden terrain must stop submitting geometry without changing pixels\n");
  return success && glGetError() == GL_NO_ERROR;
}
