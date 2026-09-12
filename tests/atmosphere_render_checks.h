#include "graphics/shadows.h"

static bool testShadowState(void) {
  ShadowMap map;
  if (!initShadowMap(&map, (Vec3){0, 32, 0}, 185))
    return false;
  // Use distinct caller framebuffers and unusual raster state. A depth pass
  // must not leak any of these changes into sky, terrain, or HUD rendering.
  GLuint framebuffers[2];
  glGenFramebuffers(2, framebuffers);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffers[0]);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffers[1]);
  glViewport(7, 9, 123, 87);
  glEnable(GL_SCISSOR_TEST);
  glScissor(11, 13, 21, 23);
  glEnable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glEnable(GL_RASTERIZER_DISCARD);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDepthFunc(GL_GREATER);
  glClearDepth(0.375);
  glPolygonMode(GL_FRONT, GL_LINE);
  glPolygonMode(GL_BACK, GL_POINT);
  GLint program, vao;
  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
  int calls;
  const Vec3 lights[] = {{0, 1, 0}, {1, 0.001f, 0}, {-1, 1, 0}, {0, 0, 1}};
  bool ok = true;
  for (size_t i = 0; i < sizeof(lights) / sizeof(*lights); i++) {
    ok &= updateShadowMap(&map, lights[i], false, NULL, 0, &calls) && calls == 0;
    // Every finite-world corner must remain covered at noon and the horizon.
    for (int x = -128; x <= 128; x += 256)
      for (int y = 0; y <= 64; y += 64)
        for (int z = -128; z <= 128; z += 256)
          for (int axis = 0; axis < 3; axis++) {
            float projected = map.transform[axis] * x + map.transform[4 + axis] * y + map.transform[8 + axis] * z + map.transform[12 + axis];
            ok &= isfinite(projected) && fabsf(projected) < 1;
          }
  }
  GLint value, viewport[4], box[4], polygon[2];
  GLboolean mask;
  GLdouble clear;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &value);
  ok &= value == (GLint)framebuffers[0];
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &value);
  ok &= value == (GLint)framebuffers[1];
  glGetIntegerv(GL_CURRENT_PROGRAM, &value);
  ok &= value == program;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &value);
  ok &= value == vao;
  glGetIntegerv(GL_VIEWPORT, viewport);
  glGetIntegerv(GL_SCISSOR_BOX, box);
  glGetIntegerv(GL_POLYGON_MODE, polygon);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);
  glGetIntegerv(GL_DEPTH_FUNC, &value);
  glGetDoublev(GL_DEPTH_CLEAR_VALUE, &clear);
  ok &= viewport[0] == 7 && viewport[1] == 9 && viewport[2] == 123 && viewport[3] == 87;
  ok &= box[0] == 11 && box[1] == 13 && box[2] == 21 && box[3] == 23;
  ok &= polygon[0] == GL_LINE && polygon[1] == GL_POINT && !mask && value == GL_GREATER && clear == 0.375;
  ok &= !glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_BLEND) && glIsEnabled(GL_CULL_FACE) && glIsEnabled(GL_SCISSOR_TEST) && glIsEnabled(GL_POLYGON_OFFSET_FILL) &&
        glIsEnabled(GL_RASTERIZER_DISCARD);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(2, framebuffers);
  glViewport(0, 0, 960, 540);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glDisable(GL_RASTERIZER_DISCARD);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LESS);
  glClearDepth(1);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  cleanupShadowMap(&map);
  ok &= map.program == 0 && map.depth == 0 && map.framebuffer == 0 && glGetError() == GL_NO_ERROR;
  if (!ok)
    fprintf(stderr, "Shadow coverage, resource cleanup, or caller state restoration failed\n");
  return ok;
}

// A receiver viewed from below an off-screen roof's height separates camera
// visibility from light visibility. Only the roof can explain a darker pixel.
static RenderResult atmosphereProbe(double phase, unsigned char rgb[3]) {
  DayNightState state = sampleDayNight(phase);
  setWorldDayNight(&state);
  Camera camera = {.position = {0.5f, 13, 0.5f}, .front = {0, -1, 0}, .up = {0, 0, 1}, .fov = 20};
  Vec3 target = {0.5f, 11, 0.5f};
  Mat4 view, projection;
  mat4_lookAt(view, &camera.position, &target, &camera.up);
  mat4_perspective(projection, camera.fov, 960.0f / 540, 0.1f, 1000);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  RenderResult result = renderWorld(&camera, view, projection, false);
  glReadPixels(480, 270, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, rgb);
  result.success &= glGetError() == GL_NO_ERROR;
  return result;
}

static bool testAtmosphereLighting(GLuint shader) {
  if (!testShadowState())
    return false;
  clearTerrainFixture();
  if (!setBlock(&(Vec3i){0, 10, 0}, BLOCK_STONE) || !initWorld(shader))
    return false;
  unsigned char tile[10 * 4];
  memset(tile, 200, sizeof(tile));
  for (int layer = 0; layer < 10; layer++)
    tile[layer * 4 + 3] = 255;
  glActiveTexture(GL_TEXTURE0);
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 1, 1, 10, 0, GL_RGBA, GL_UNSIGNED_BYTE, tile);
  unsigned char clear[3], covered[3], reversed[3], removed[3];
  RenderResult first = atmosphereProbe(0.125, clear);
  RenderResult cached = atmosphereProbe(0.125, clear);
  if (!first.success || !cached.success || first.shadowDrawCalls != 1 || cached.shadowDrawCalls != 0 || cached.chunksRebuilt != 0)
    return false;
  for (int x = 3; x <= 5; x++)
    for (int z = -1; z <= 1; z++)
      if (!setBlock(&(Vec3i){x, 14, z}, BLOCK_STONE))
        return false;
  RenderResult edited = atmosphereProbe(0.125, covered);
  RenderResult turned = atmosphereProbe(0.375, reversed);
  if (!edited.success || !turned.success || edited.chunksRebuilt == 0 || edited.shadowDrawCalls == 0 || turned.shadowDrawCalls == 0 || turned.chunksRebuilt != 0)
    return false;
  unsigned char moonCovered[3], moonClear[3];
  if (!atmosphereProbe(0.625, moonCovered).success || !atmosphereProbe(0.875, moonClear).success)
    return false;
  for (int x = 3; x <= 5; x++)
    for (int z = -1; z <= 1; z++)
      setBlock(&(Vec3i){x, 14, z}, BLOCK_AIR);
  if (!atmosphereProbe(0.125, removed).success)
    return false;
  printf("Off-camera roof: clear=%u shadow=%u reversed=%u removed=%u\n", clear[0], covered[0], reversed[0], removed[0]);
  bool ok = covered[0] > 60 && clear[0] > covered[0] + 25 && abs(clear[0] - reversed[0]) <= 2 && abs(clear[0] - removed[0]) <= 2;
  ok &= moonCovered[0] >= 55 && moonClear[0] > moonCovered[0] + 10;
  // Low sun creates a long shadow from a roof crossing a chunk seam. Check
  // actual pixels, since finite projection bounds alone cannot catch bad bias.
  unsigned char lowClear[3], lowCovered[3], lowReverse[3];
  ok &= atmosphereProbe(0.035, lowClear).success;
  for (int x = 14; x <= 18; x++)
    for (int z = -1; z <= 1; z++)
      ok &= setBlock(&(Vec3i){x, 14, z}, BLOCK_STONE);
  ok &= atmosphereProbe(0.035, lowCovered).success && atmosphereProbe(0.465, lowReverse).success;
  printf("Low-sun roof: clear=%u shadow=%u reversed=%u\n", lowClear[0], lowCovered[0], lowReverse[0]);
  ok &= lowClear[0] > lowCovered[0] + 10 && abs(lowClear[0] - lowReverse[0]) <= 2;
  for (int x = 14; x <= 18; x++)
    for (int z = -1; z <= 1; z++)
      setBlock(&(Vec3i){x, 14, z}, BLOCK_AIR);
  // Readability includes undersides, which receive only fill at midnight.
  DayNightState night = sampleDayNight(0.75);
  setWorldDayNight(&night);
  unsigned char top[3], side[3], bottom[3];
  ok &= lightingProbe((Vec3i){0, 10, 0}, TOP, 0, 180, top);
  ok &= lightingProbe((Vec3i){0, 10, 0}, FRONT, 0, 180, side);
  ok &= lightingProbe((Vec3i){0, 10, 0}, BOTTOM, 0, 180, bottom);
  printf("Midnight readability: top=%u side=%u underside=%u\n", top[0], side[0], bottom[0]);
  ok &= top[0] >= 80 && side[0] >= 55 && bottom[0] >= 40 && top[0] < 130;
  if (!ok)
    fprintf(stderr, "Cast shadows or midnight face readability failed\n");
  return ok;
}
