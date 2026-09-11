#include "graphics/clouds.h"

#define CLOUD_CHECK(condition)                                                                                                                                                     \
  do {                                                                                                                                                                             \
    if (!(condition)) {                                                                                                                                                            \
      fprintf(stderr, "Cloud rendering: %s (line %d)\n", #condition, __LINE__);                                                                                                    \
      cleanupClouds(&clouds);                                                                                                                                                      \
      return false;                                                                                                                                                                \
    }                                                                                                                                                                              \
  } while (0)

static bool captureCloudViews(const CloudRenderer* clouds, GLuint shader) {
  const char* prefix = getenv("KERNELCRAFT_CLOUD_CAPTURE");
  if (!prefix)
    return true;
  SkyRenderer sky = {0};
  if (!initSky(&sky))
    return false;
  bool ok = initChunksSeeded(42) && initWorld(shader);
  const char* names[] = {"day", "dusk", "night", "above"};
  const double phases[] = {0.125, 0.5, 0.625, 0.25};
  static unsigned char pixels[960 * 540 * 3];
  for (int i = 0; i < 4 && ok; i++) {
    Camera camera = {.position = {-45, 24, 0}, .front = {0.94f, 0.342f, 0}, .up = {0, 1, 0}, .fov = 80};
    if (i == 3) {
      camera.position.y = 145;
      camera.front.y = -0.342f;
    }
    vec3_normalize(&camera.front, &camera.front);
    Vec3 target;
    vec3_add(&target, &camera.position, &camera.front);
    Mat4 view, projection;
    mat4_lookAt(view, &camera.position, &target, &camera.up);
    mat4_perspective(projection, camera.fov, 960.0f / 540, 0.1f, 1000);
    DayNightState state = sampleDayNight(phases[i]);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderSky(&sky, &camera, 960.0f / 540, &state);
    setWorldDayNight(&state);
    ok = renderWorld(&camera, view, projection, false).success;
    renderClouds(clouds, &camera, 960.0f / 540, projection, &state);
    glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    char path[1024];
    int length = snprintf(path, sizeof(path), "%s-%s.ppm", prefix, names[i]);
    if (length < 0 || (size_t)length >= sizeof(path)) {
      ok = false;
      break;
    }
    FILE* file = fopen(path, "wb");
    if (!file) {
      ok = false;
      break;
    }
    ok = fprintf(file, "P6\n960 540\n255\n") > 0 && ok;
    for (int row = 539; row >= 0 && ok; row--)
      ok = fwrite(pixels + row * 960 * 3, 1, 960 * 3, file) == 960 * 3;
    if (fclose(file) != 0)
      ok = false;
  }
  cleanupSky(&sky);
  return ok;
}

static bool testCloudRendering(GLuint shader) {
  CloudRenderer clouds = {0};
  CLOUD_CHECK(initClouds(&clouds));
  // View every cell in one period straight down. This fingerprint was recorded
  // from the original GPU noise implementation before caching its occupancy.
  Camera maskCamera = {.position = {384, 508, 384}, .front = {0, -1, 0}, .up = {0, 0, 1}, .fov = 90};
  Mat4 maskProjection;
  mat4_perspective(maskProjection, 90, 1, 0.1f, 1000);
  DayNightState maskDay = sampleDayNight(0.25);
  unsigned char maskPixels[64 * 64 * 3];
  glViewport(0, 0, 64, 64);
  glClearColor(0, 0, 0, 1);
  glDepthMask(GL_TRUE);
  glClearDepth(1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  renderClouds(&clouds, &maskCamera, 1, maskProjection, &maskDay);
  glReadPixels(0, 0, 64, 64, GL_RGB, GL_UNSIGNED_BYTE, maskPixels);
  uint64_t maskHash = UINT64_C(14695981039346656037);
  for (size_t i = 0; i < sizeof(maskPixels); i += 3)
    maskHash = (maskHash ^ (maskPixels[i] != 0)) * UINT64_C(1099511628211);
  printf("Cloud mask fingerprint: %llu\n", (unsigned long long)maskHash);
  CLOUD_CHECK(maskHash == UINT64_C(12639864285259775753));
  Camera camera = {.position = {0, 40, 0}, .front = {0.8f, 0.6f, 0}, .up = {0, 1, 0}, .fov = 90};
  Mat4 projection;
  mat4_perspective(projection, camera.fov, 960.0f / 540, 0.1f, 1000);
  DayNightState day = sampleDayNight(0.25), night = sampleDayNight(0.75);
  static unsigned char pixels[960 * 540 * 3], reference[sizeof(pixels)];
  glViewport(0, 0, 960, 540);
  glClearColor(0, 0, 0, 1);
  glDepthMask(GL_TRUE);
  glClearDepth(1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  renderClouds(&clouds, &camera, 960.0f / 540, projection, &day);
  glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, reference);
  unsigned covered = 0, gaps = 0;
  for (size_t i = 0; i < sizeof(reference); i += 3) {
    covered += reference[i] > 100;
    gaps += reference[i] == 0;
  }
  CLOUD_CHECK(covered > 20000 && gaps > 20000);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  renderClouds(&clouds, &camera, 960.0f / 540, projection, &night);
  glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, pixels);
  for (size_t i = 0; i < sizeof(pixels); i += 3)
    CLOUD_CHECK(pixels[i] <= reference[i]);
  // A depth surface in front of the entire layer must hide every cloud.
  glClearDepth(0.1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  renderClouds(&clouds, &camera, 960.0f / 540, projection, &day);
  glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, pixels);
  for (size_t i = 0; i < sizeof(pixels); i++)
    CLOUD_CHECK(pixels[i] == 0);
  glClearDepth(1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  camera.position.x = -768;
  renderClouds(&clouds, &camera, 960.0f / 540, projection, &day);
  glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, pixels);
  CLOUD_CHECK(memcmp(pixels, reference, sizeof(pixels)) == 0);
  camera.position.x = -24;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  renderClouds(&clouds, &camera, 960.0f / 540, projection, &day);
  glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, pixels);
  CLOUD_CHECK(memcmp(pixels, reference, sizeof(pixels)) != 0);
  float depth;
  glReadPixels(480, 270, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
  CLOUD_CHECK(depth == 1);
  // Cloud drift moves the same world pattern as an opposite camera translation.
  camera.position.x = 0;
  clouds.offset = 24;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  renderClouds(&clouds, &camera, 960.0f / 540, projection, &day);
  glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, reference);
  CLOUD_CHECK(memcmp(pixels, reference, sizeof(pixels)) == 0);
  clouds.offset = 0;
  // Looking down from above the layer reveals brighter tops and the same gaps.
  camera.position.y = 204;
  camera.front.y = -0.6f;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  renderClouds(&clouds, &camera, 960.0f / 540, projection, &day);
  glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, pixels);
  covered = 0;
  for (size_t i = 0; i < sizeof(pixels); i += 3)
    covered += pixels[i] > 220;
  CLOUD_CHECK(covered > 20000);
  // No cloud is visible when facing away from the layer.
  camera.front.y = 0.6f;
  camera.fov = 20;
  mat4_perspective(projection, camera.fov, 960.0f / 540, 0.1f, 1000);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  renderClouds(&clouds, &camera, 960.0f / 540, projection, &day);
  glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, pixels);
  for (size_t i = 0; i < sizeof(pixels); i++)
    CLOUD_CHECK(pixels[i] == 0);

  // Locate a filled cell viewed straight down, exactly 80 blocks below the eye.
  camera = (Camera){.position = {6, 204, 6}, .front = {0, -1, 0}, .up = {0, 0, 1}, .fov = 1};
  mat4_perspective(projection, camera.fov, 960.0f / 540, 0.1f, 1000);
  unsigned char center[3] = {0};
  for (int cell = 0; cell < 64 && center[0] < 220; cell++) {
    camera.position.x = cell * 12 + 6;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderClouds(&clouds, &camera, 960.0f / 540, projection, &day);
    glReadPixels(480, 270, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, center);
  }
  CLOUD_CHECK(center[0] > 220);
  // Depth planes half a block before/after that surface discriminate the actual
  // reconstructed depth, rather than just checking a near plane hides everything.
  for (int behind = 0; behind < 2; behind++) {
    double distance = behind ? 80.5 : 79.5;
    glClearDepth((1000.0 - 100.0 / distance) / 999.9);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderClouds(&clouds, &camera, 960.0f / 540, projection, &day);
    glReadPixels(480, 270, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, center);
    CLOUD_CHECK(behind ? center[0] > 220 : center[0] == 0);
  }
  glClearDepth(1);
  camera.position.y = 122;
  camera.front = (Vec3){1, 0, 0};
  camera.up = (Vec3){0, 1, 0};
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  renderClouds(&clouds, &camera, 960.0f / 540, projection, &day);
  glReadPixels(480, 270, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, center);
  CLOUD_CHECK(center[0] > 100); // Flying into a filled cell retains its cloud cover.
  camera.position.y = 40;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  renderClouds(&clouds, &camera, 960.0f / 540, projection, &day);
  glReadPixels(480, 270, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, center);
  CLOUD_CHECK(center[0] == 0); // Horizontal rays below the layer miss it.

  // Restore all state even when called from a nonstandard rendering pass.
  glUseProgram(shader);
  glDisable(GL_DEPTH_TEST);
  glDepthFunc(GL_GREATER);
  glDepthMask(GL_FALSE);
  glEnable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_DST_ALPHA, GL_SRC_ALPHA);
  glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_SUBTRACT);
  glPolygonMode(GL_FRONT, GL_LINE);
  glPolygonMode(GL_BACK, GL_POINT);
  GLint oldVao;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
  renderClouds(&clouds, &camera, 960.0f / 540, projection, &day);
  GLint value, modes[2];
  GLboolean mask;
  glGetIntegerv(GL_CURRENT_PROGRAM, &value);
  CLOUD_CHECK(value == (GLint)shader);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &value);
  CLOUD_CHECK(value == oldVao);
  CLOUD_CHECK(!glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_BLEND) && glIsEnabled(GL_CULL_FACE));
  glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);
  CLOUD_CHECK(mask == GL_FALSE);
  glGetIntegerv(GL_DEPTH_FUNC, &value);
  CLOUD_CHECK(value == GL_GREATER);
  glGetIntegerv(GL_BLEND_SRC_RGB, &value);
  CLOUD_CHECK(value == GL_ONE);
  glGetIntegerv(GL_BLEND_DST_RGB, &value);
  CLOUD_CHECK(value == GL_ZERO);
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &value);
  CLOUD_CHECK(value == GL_DST_ALPHA);
  glGetIntegerv(GL_BLEND_DST_ALPHA, &value);
  CLOUD_CHECK(value == GL_SRC_ALPHA);
  glGetIntegerv(GL_BLEND_EQUATION_RGB, &value);
  CLOUD_CHECK(value == GL_FUNC_REVERSE_SUBTRACT);
  glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &value);
  CLOUD_CHECK(value == GL_FUNC_SUBTRACT);
  glGetIntegerv(GL_POLYGON_MODE, modes);
  CLOUD_CHECK(modes[0] == GL_LINE && modes[1] == GL_POINT);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  advanceClouds(&clouds, 20, true);
  CLOUD_CHECK(clouds.offset == 0);
  advanceClouds(&clouds, 1, true);
  CLOUD_CHECK(fabs(clouds.offset - 0.06) < 1e-9);
  advanceClouds(&clouds, 1, false);
  advanceClouds(&clouds, 20, true);
  advanceClouds(&clouds, NAN, true);
  advanceClouds(&clouds, -1, true);
  CLOUD_CHECK(fabs(clouds.offset - 0.06) < 1e-9);
  clouds.offset = 767.99;
  advanceClouds(&clouds, 0.1, true);
  CLOUD_CHECK(fabs(clouds.offset - 0.05) < 1e-9);
  clouds.offset = 0;
  CLOUD_CHECK(captureCloudViews(&clouds, shader));
  cleanupClouds(&clouds);
  CLOUD_CHECK(!clouds.vao && !clouds.program && glGetError() == GL_NO_ERROR);
  puts("Cloud coverage, gaps, night lighting, terrain depth, wrapping, parallax, and pause checks passed");
  return true;
}

#undef CLOUD_CHECK
