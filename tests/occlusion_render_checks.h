// Runs through the real world renderer, mesh uploads and framebuffer.
#include "world/occlusion.h"
#include "world/mesh_visibility.h"

static bool bypassOcclusion;
static unsigned visibilityChecks;
bool __real_occlusionBoundsHidden(const OcclusionBuffer* buffer, Vec3 min, Vec3 max);

bool __wrap_occlusionBoundsHidden(const OcclusionBuffer* buffer, Vec3 min, Vec3 max) {
  visibilityChecks++;
  return !bypassOcclusion && __real_occlusionBoundsHidden(buffer, min, max);
}

bool __real_meshVisibilityIntersects(const MeshVisibility* visibility, const float planes[6][4]);

bool __wrap_meshVisibilityIntersects(const MeshVisibility* visibility, const float planes[6][4]) {
  visibilityChecks++;
  return bypassOcclusion || __real_meshVisibilityIntersects(visibility, planes);
}

static bool compareOcclusionFrame(const Camera* camera, float fov, int width, int height, bool wireframe, int expectedDraws, int hidden) {
  size_t count = (size_t)width * height;
  unsigned char* color[2] = {malloc(count * 4), malloc(count * 4)};
  float* depth[2] = {malloc(count * sizeof(float)), malloc(count * sizeof(float))};
  bool success = color[0] && color[1] && depth[0] && depth[1];
  Mat4 view, projection;
  Vec3 target;
  vec3_add(&target, &camera->position, &camera->front);
  mat4_lookAt(view, &camera->position, &target, &camera->up);
  mat4_perspective(projection, fov, (float)width / height, 0.1f, 1000);
  glViewport(0, 0, width, height);
  RenderResult result[2] = {{0}};
  for (int pass = 0; success && pass < 2; pass++) {
    // Link-time wrappers bypass only the new visibility decisions. The
    // reference submits every original distance/frustum candidate with the
    // actual terrain shader, materials, vertex buffers and depth test.
    bypassOcclusion = pass == 1;
    // The first pass must exercise normal cache invalidation. Only the
    // reference forces a rebuild so it cannot reuse a culled draw list.
    if (pass)
      getChunk(&(Vec2i){0, 0})->dirty = true;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draws = 0;
    result[pass] = renderWorld(camera, view, projection, wireframe);
    success &= result[pass].success && draws == (unsigned long)result[pass].terrainDrawCalls + 1;
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, color[pass]);
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth[pass]);
  }

  bypassOcclusion = false;
  // Leave a normal, clean cache for the next camera/viewport/mode transition.
  getChunk(&(Vec2i){0, 0})->dirty = true;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  success &= renderWorld(camera, view, projection, wireframe).success;
  if (success) {
    size_t colorDifferences = 0, depthDifferences = 0;
    for (size_t pixel = 0; pixel < count; pixel++) {
      colorDifferences += memcmp(color[0] + pixel * 4, color[1] + pixel * 4, 4) != 0;
      depthDifferences += depth[0][pixel] != depth[1][pixel];
    }

    success = !colorDifferences && !depthDifferences && (expectedDraws < 0 || result[0].terrainDrawCalls == expectedDraws) && result[0].chunksOccluded >= hidden;
    if (wireframe)
      success &= result[0].chunksOccluded == 0;
    if (!success)
      fprintf(stderr, "Occlusion comparison at %.3f %.3f %.3f fov=%.1f %dx%d wire=%d: color=%zu depth=%zu draws=%d/%d expected=%d hidden=%d/%d\n", camera->position.x,
              camera->position.y, camera->position.z, fov, width, height, wireframe, colorDifferences, depthDifferences, result[0].terrainDrawCalls, result[1].terrainDrawCalls,
              expectedDraws, result[0].chunksOccluded, hidden);
  }

  for (int pass = 0; pass < 2; pass++) {
    free(color[pass]);
    free(depth[pass]);
  }

  return success && glGetError() == GL_NO_ERROR;
}

static Camera occlusionCamera(Vec3 position, Vec3 target) {
  Camera camera = {.position = position, .up = {0, 1, 0}};
  vec3_subtract(&camera.front, &target, &position);
  vec3_normalize(&camera.front, &camera.front);
  return camera;
}

static bool testVisibilityInvalidation(Camera camera) {
  Mat4 view, projection;
  Vec3 target = {8.5f, 20.5f, -20};
  mat4_lookAt(view, &camera.position, &target, &camera.up);
  mat4_perspective(projection, 70, 960.0f / 540, 0.1f, 1000);
  glViewport(0, 0, 960, 540);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (!renderWorld(&camera, view, projection, false).success)
    return false;
  bool wireframe = false;
  for (int change = 0; change < 7; change++) {
    switch (change) {
    case 0:
      target.x += 1;
      mat4_lookAt(view, &camera.position, &target, &camera.up);
      break;
    case 1:
      mat4_perspective(projection, 80, 960.0f / 540, 0.1f, 1000);
      break;
    case 2:
      glViewport(5, 7, 640, 360);
      break;
    case 3:
    case 4:
      wireframe = !wireframe;
      break;
    case 5:
      camera.position.x += 0.1f; // Change distance filtering independently of the matrix.
      break;
    case 6:
      getChunk(&(Vec2i){0, 0})->dirty = true;
      break;
    }

    visibilityChecks = 0;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    RenderResult changed = renderWorld(&camera, view, projection, wireframe);
    if (!changed.success || !visibilityChecks || changed.chunksRebuilt != (change == 6 ? 1 : 0)) {
      fprintf(stderr, "Visibility cache failed to refresh for change %d: checks=%u rebuilt=%d\n", change, visibilityChecks, changed.chunksRebuilt);
      return false;
    }

    visibilityChecks = 0;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    RenderResult repeated = renderWorld(&camera, view, projection, wireframe);
    if (!repeated.success || visibilityChecks || repeated.chunksOccluded != changed.chunksOccluded || repeated.terrainDrawCalls != changed.terrainDrawCalls) {
      fprintf(stderr, "Visibility cache failed to reuse change %d\n", change);
      return false;
    }
  }

  glViewport(0, 0, 960, 540);
  return true;
}

static bool testMovingOcclusion(GLuint shader) {
  clearTerrainFixture();
  for (int x = 0; x < 16; x++)
    for (int y = 8; y < 32; y++)
      setBlock(&(Vec3i){x, y, 0}, BLOCK_STONE);
  setBlock(&(Vec3i){8, 20, -20}, BLOCK_DIRT);
  if (!initWorld(shader))
    return false;
  for (int frame = 0; frame < 12; frame++) {
    Vec3 target = {8.5f, 20.5f, -20};
    Camera camera = occlusionCamera((Vec3){8.5f + frame * 0.01f, 20.5f, 8}, target);
    Mat4 view, projection;
    mat4_lookAt(view, &camera.position, &target, &camera.up);
    mat4_perspective(projection, 70, 960.0f / 540, 0.1f, 1000);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    RenderResult result = renderWorld(&camera, view, projection, false);
    if (!result.success || result.terrainDrawCalls != 1) {
      fprintf(stderr, "Moving occlusion frame %d: expected one terrain draw behind a solid wall, got %d\n", frame, result.terrainDrawCalls);
      return false;
    }

    if (!compareOcclusionFrame(&camera, 70, 960, 540, false, 1, 1))
      return false;
  }

  Camera camera = occlusionCamera((Vec3){8.5f, 20.5f, 8}, (Vec3){8.5f, 20.5f, -20});
  if (!testVisibilityInvalidation(camera))
    return false;
  Mat4 cachedView, cachedProjection;
  Vec3 cachedTarget = {8.5f, 20.5f, -20};
  mat4_lookAt(cachedView, &camera.position, &cachedTarget, &camera.up);
  mat4_perspective(cachedProjection, 70, 960.0f / 540, 0.1f, 1000);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  RenderResult first = renderWorld(&camera, cachedView, cachedProjection, false);
  visibilityChecks = 0;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  RenderResult second = renderWorld(&camera, cachedView, cachedProjection, false);
  if (!first.success || !second.success || first.terrainDrawCalls != second.terrainDrawCalls || first.chunksOccluded != second.chunksOccluded || visibilityChecks) {
    fprintf(stderr, "Unchanged visibility repeated %u CPU checks\n", visibilityChecks);
    return false;
  }

  if (!compareOcclusionFrame(&camera, 100, 320, 480, false, 1, 1) || !compareOcclusionFrame(&camera, 35, 37, 23, false, 1, 1) ||
      !compareOcclusionFrame(&camera, 70, 960, 540, true, 2, 0))
    return false;
  // Opening one voxel must reveal the rear chunk in this very frame.
  setBlock(&(Vec3i){8, 20, 0}, BLOCK_AIR);
  if (!compareOcclusionFrame(&camera, 70, 960, 540, false, 2, 0))
    return false;
  // Close the opening, then view partially exposed geometry from beside it.
  setBlock(&(Vec3i){8, 20, 0}, BLOCK_STONE);
  camera = occlusionCamera((Vec3){-5, 20.5f, 8}, (Vec3){8.5f, 20.5f, -20});
  if (!compareOcclusionFrame(&camera, 70, 960, 540, false, -1, 0))
    return false;
  // Near-plane intersection and entering a solid block cannot remove pixels.
  const float distances[] = {1.05f, 0.95f, 0.5f, -0.05f};
  for (size_t i = 0; i < sizeof(distances) / sizeof(distances[0]); i++) {
    camera = occlusionCamera((Vec3){8.5f, 20.5f, distances[i]}, (Vec3){8.5f, 20.5f, -20});
    if (!compareOcclusionFrame(&camera, 70, 320, 240, false, -1, 0))
      return false;
  }

  for (int x = 0; x < 16; x++)
    for (int y = 8; y < 32; y++)
      setBlock(&(Vec3i){x, y, 0}, BLOCK_AIR);
  camera = occlusionCamera((Vec3){8.5f, 20.5f, 8}, (Vec3){8.5f, 20.5f, -20});
  if (!compareOcclusionFrame(&camera, 70, 960, 540, false, 1, 0))
    return false;
  // Rebuild across a negative chunk seam and switch framebuffer targets.
  for (int x = -16; x < 0; x++)
    for (int y = 8; y < 32; y++)
      setBlock(&(Vec3i){x, y, -1}, BLOCK_STONE);
  setBlock(&(Vec3i){-8, 20, -20}, BLOCK_DIRT);
  camera = occlusionCamera((Vec3){-7.5f, 20.5f, 8}, (Vec3){-7.5f, 20.5f, -20});
  GLuint framebuffer, color, depth;
  glGenFramebuffers(1, &framebuffer);
  glGenRenderbuffers(1, &color);
  glGenRenderbuffers(1, &depth);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, color);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 101, 73);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
  glBindRenderbuffer(GL_RENDERBUFFER, depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 101, 73);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  bool framebufferSuccess = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE && compareOcclusionFrame(&camera, 70, 101, 73, false, -1, 1);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteRenderbuffers(1, &color);
  glDeleteRenderbuffers(1, &depth);
  glDeleteFramebuffers(1, &framebuffer);
  if (!framebufferSuccess || !compareOcclusionFrame(&camera, 70, 960, 540, false, -1, 1))
    return false;
  puts("Current-view occlusion skips hidden terrain while moving");
  // Looking above terrain must not keep submitting boxes containing only air.
  cleanupWorld();
  if (!initChunksSeeded(0) || !initWorld(shader))
    return false;
  for (int frame = 0; frame < 12; frame++) {
    Camera camera;
    initCamera(&camera);
    camera.position = (Vec3){0.5f + frame * 0.0025f, 13.62f, 3.5f};
    camera.pitch = 89;
    camera.yaw += frame * 0.05f;
    updateCameraVectors(&camera);
    Vec3 target;
    vec3_add(&target, &camera.position, &camera.front);
    Mat4 view, projection;
    mat4_lookAt(view, &camera.position, &target, &camera.up);
    mat4_perspective(projection, 70, 960.0f / 540, 0.1f, 1000);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    RenderResult result = renderWorld(&camera, view, projection, false);
    if (!result.success || result.terrainDrawCalls != 0) {
      fprintf(stderr, "Moving sky frame %d: expected no terrain draws, got %d\n", frame, result.terrainDrawCalls);
      return false;
    }
  }

  // Compare changing poses in the real generated world, including underground,
  // grazing horizons, sky, negative coordinates and chunk seams.
  const float heights[] = {0.5f, 10, 13.62f, 40};
  const float pitches[] = {-70, -10, 40, 89};
  for (int pose = 0; pose < 192; pose++) {
    initCamera(&camera);
    camera.position = (Vec3){(pose % 3 - 1) * 17.15f, heights[(pose / 48) % 4], (pose % 4 - 2) * 10.1f};
    camera.yaw = (pose % 12) * 30.0f;
    camera.pitch = pitches[(pose / 12) % 4];
    updateCameraVectors(&camera);
    if (!compareOcclusionFrame(&camera, 70, 320, 180, false, -1, 0))
      return false;
  }

  glViewport(0, 0, 960, 540);
  puts("Occlusion matches unculled color/depth: moving walls, aperture, edits, near plane, wireframe, resize, framebuffer, and 192 terrain poses");
  return glGetError() == GL_NO_ERROR;
}
