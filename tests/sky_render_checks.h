#include "graphics/sky.h"

static bool captureSkyViews(const SkyRenderer* sky, GLuint shader) {
  const char* prefix = getenv("KERNELCRAFT_SKY_CAPTURE");
  if (!prefix)
    return true;
  if (!initChunksSeeded(42) || !initWorld(shader))
    return false;
  const double phases[] = {0.125, 0.0, 0.625};
  const char* names[] = {"day", "dawn", "night"};
  Camera camera = {.position = {-45, 24, 0}, .front = {0.985f, 0.174f, 0}, .up = {0, 1, 0}, .fov = 80};
  vec3_normalize(&camera.front, &camera.front);
  Mat4 view, projection;
  Vec3 target;
  vec3_add(&target, &camera.position, &camera.front);
  mat4_lookAt(view, &camera.position, &target, &camera.up);
  mat4_perspective(projection, camera.fov, 960.0f / 540, 0.1f, 1000);
  static unsigned char pixels[960 * 540 * 3];
  for (int i = 0; i < 3; i++) {
    DayNightState state = sampleDayNight(phases[i]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderSky(sky, &camera, 960.0f / 540, &state);
    setWorldDayNight(&state);
    if (!renderWorld(&camera, view, projection, false).success)
      return false;
    glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    char path[1024];
    int length = snprintf(path, sizeof(path), "%s-%s.ppm", prefix, names[i]);
    if (length < 0 || (size_t)length >= sizeof(path))
      return false;
    FILE* file = fopen(path, "wb");
    if (!file)
      return false;
    bool ok = fprintf(file, "P6\n960 540\n255\n") > 0;
    for (int row = 539; row >= 0 && ok; row--)
      ok = fwrite(pixels + row * 960 * 3, 1, 960 * 3, file) == 960 * 3;
    if (fclose(file) != 0 || !ok)
      return false;
  }
  return true;
}

#define SKY_CHECK(condition)                                                                                                                                                       \
  do {                                                                                                                                                                             \
    if (!(condition)) {                                                                                                                                                            \
      fprintf(stderr, "Sky rendering: %s (line %d)\n", #condition, __LINE__);                                                                                                      \
      cleanupSky(&sky);                                                                                                                                                            \
      return false;                                                                                                                                                                \
    }                                                                                                                                                                              \
  } while (0)

static bool testSkyRendering(GLuint shader) {
  SkyRenderer sky = {0};
  glUseProgram(shader);
  glActiveTexture(GL_TEXTURE3);
  SKY_CHECK(initSky(&sky));
  GLint initialProgram, initialActive;
  glGetIntegerv(GL_CURRENT_PROGRAM, &initialProgram);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &initialActive);
  SKY_CHECK(initialProgram == (GLint)shader && initialActive == GL_TEXTURE3);
  glActiveTexture(GL_TEXTURE0);
  glViewport(0, 0, 960, 540);
  Camera camera = {.position = {0, 40, 0}, .up = {0, 1, 0}, .fov = 70};
  // Independent values read from the five solid bands in each supplied PNG.
  const unsigned char colors[3][5][3] = {{{235, 253, 255}, {214, 251, 255}, {175, 246, 255}, {129, 238, 252}, {59, 145, 231}},
                                         {{255, 97, 44}, {255, 137, 153}, {255, 188, 197}, {255, 228, 232}, {255, 250, 250}},
                                         {{39, 12, 70}, {64, 25, 96}, {96, 35, 125}, {151, 94, 178}, {180, 129, 205}}};
  const double phases[] = {0.25, 0, 0.75};
  const float elevations[] = {0, 22.5f, 45, 67.5f, 90};
  for (int phase = 0; phase < 3; phase++) {
    DayNightState state = sampleDayNight(phases[phase]);
    state.stars = 0;
    state.sunDirection = state.moonDirection = (Vec3){0, -1, 0};
    for (int band = 0; band < 5; band++) {
      float angle = toRadians(elevations[phase == 2 ? 4 - band : band]);
      camera.front = (Vec3){0, sinf(angle), cosf(angle)};
      camera.up = fabsf(camera.front.y) > 0.99f ? (Vec3){0, 0, 1} : (Vec3){0, 1, 0};
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      renderSky(&sky, &camera, 960.0f / 540, &state);
      unsigned char pixel[3];
      float depth;
      glReadPixels(480, 270, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);
      glReadPixels(480, 270, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
      SKY_CHECK(depth == 1);
      for (int channel = 0; channel < 3; channel++)
        SKY_CHECK(abs(pixel[channel] - colors[phase][band][channel]) <= 1);
    }
  }

  static unsigned char plain[960 * 540 * 3], stars[sizeof(plain)], moved[sizeof(plain)];
  camera.front = (Vec3){0, 0.70710678f, 0.70710678f};
  camera.up = (Vec3){0, 1, 0};
  DayNightState night = sampleDayNight(0.75), noStars = night;
  noStars.stars = 0;
  renderSky(&sky, &camera, 960.0f / 540, &noStars);
  glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, plain);
  renderSky(&sky, &camera, 960.0f / 540, &night);
  glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, stars);
  int starPixels = 0;
  for (size_t pixel = 0; pixel < sizeof(plain); pixel += 3)
    starPixels += stars[pixel] > plain[pixel] + 3;
  printf("Night sky: %d star pixels\n", starPixels);
  SKY_CHECK(starPixels > 50 && starPixels < 10000);
  camera.position = (Vec3){-121, 71, 119};
  renderSky(&sky, &camera, 960.0f / 540, &night);
  glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, moved);
  SKY_CHECK(memcmp(stars, moved, sizeof(stars)) == 0);
  // A narrow view across the longitude seam has no star centers in its two
  // central columns. Wrapped angular derivatives must not smear stars here.
  camera.front = (Vec3){-sqrtf(1 - 0.345f * 0.345f), 0.345f, 0.00003f};
  vec3_normalize(&camera.front, &camera.front);
  camera.fov = 1;
  renderSky(&sky, &camera, 960.0f / 540, &noStars);
  glReadPixels(480, 0, 2, 540, GL_RGB, GL_UNSIGNED_BYTE, plain);
  renderSky(&sky, &camera, 960.0f / 540, &night);
  glReadPixels(480, 0, 2, 540, GL_RGB, GL_UNSIGNED_BYTE, moved);
  int seamStars = 0;
  // Two RGB pixels have an eight-byte row stride at default pack alignment.
  for (int row = 0; row < 540; row++)
    for (int column = 0; column < 6; column++)
      seamStars += abs(plain[row * 8 + column] - moved[row * 8 + column]) > 2;
  printf("Longitude seam: %d unexpected star channels\n", seamStars);
  SKY_CHECK(seamStars == 0);
  camera.front = (Vec3){0, 0.70710678f, 0.70710678f};
  camera.fov = 70;
  DayNightState day = sampleDayNight(0.25);
  SKY_CHECK(day.stars == 0);
  renderSky(&sky, &camera, 960.0f / 540, &day);
  glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, plain);
  // Forcing the field on produces different pixels; ordinary daylight must
  // therefore suppress rendered stars, not just report a zero CPU weight.
  day.stars = 1;
  renderSky(&sky, &camera, 960.0f / 540, &day);
  glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, moved);
  SKY_CHECK(memcmp(plain, moved, sizeof(plain)) != 0);

  // Each supplied body must be visible along its orbit and absent when the
  // camera turns toward the opposite side of the sky at the same elevation.
  for (int body = 0; body < 2; body++) {
    DayNightState state = sampleDayNight(body ? 0.625 : 0.125);
    camera.front = body ? state.moonDirection : state.sunDirection;
    renderSky(&sky, &camera, 960.0f / 540, &state);
    unsigned char visible[3], absent[3];
    glReadPixels(480, 270, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, visible);
    // Look to the other side of the sky at the same elevation.
    camera.front.x = -camera.front.x;
    renderSky(&sky, &camera, 960.0f / 540, &state);
    glReadPixels(480, 270, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, absent);
    SKY_CHECK(visible[0] > 180 && visible[1] > 170 && visible[2] > 140);
    SKY_CHECK(abs(visible[0] - absent[0]) + abs(visible[1] - absent[1]) + abs(visible[2] - absent[2]) > 30);
  }

  // Nondefault caller state must survive the sky pass, including wireframe.
  glUseProgram(shader);
  glEnable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glPolygonMode(GL_FRONT, GL_LINE);
  glPolygonMode(GL_BACK, GL_POINT);
  glActiveTexture(GL_TEXTURE3);
  GLuint sentinel;
  glGenTextures(1, &sentinel);
  glBindTexture(GL_TEXTURE_2D, sentinel);
  renderSky(&sky, &camera, 960.0f / 540, &night);
  GLint program, active, binding, polygon[2];
  GLboolean mask;
  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
  glGetIntegerv(GL_POLYGON_MODE, polygon);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);
  SKY_CHECK(program == (GLint)shader && active == GL_TEXTURE3 && binding == (GLint)sentinel);
  SKY_CHECK(glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_BLEND) && glIsEnabled(GL_CULL_FACE) && mask);
  SKY_CHECK(polygon[0] == GL_LINE && polygon[1] == GL_POINT);
  glDeleteTextures(1, &sentinel);
  glActiveTexture(GL_TEXTURE0);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);

  // Terrain in front of the sky must stay opaque and dim at night without
  // uploads or dirty meshes. A uniform tile gives a readable lighting probe.
  clearTerrainFixture();
  Vec3i block = {0, 20, 0};
  SKY_CHECK(setBlock(&block, BLOCK_STONE) && initWorld(shader));
  unsigned char noonPixel[3], nightPixel[3];
  DayNightState noon = sampleDayNight(0.25);
  setWorldDayNight(&noon);
  SKY_CHECK(lightingProbe(block, TOP, 0, 180, noonPixel));
  uploads = 0;
  setWorldDayNight(&night);
  SKY_CHECK(lightingProbe(block, TOP, 0, 180, nightPixel));
  SKY_CHECK(uploads == 0 && nightPixel[0] > 20 && noonPixel[0] > nightPixel[0] * 2);
  camera = (Camera){.position = {0.5f, 24, 0.5f}, .front = {0, -1, 0}, .up = {0, 0, 1}, .fov = 70};
  Mat4 view, projection;
  Vec3 target = {0.5f, 20.5f, 0.5f};
  mat4_lookAt(view, &camera.position, &target, &camera.up);
  mat4_perspective(projection, 70, 960.0f / 540, 0.1f, 1000);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  renderSky(&sky, &camera, 960.0f / 540, &night);
  RenderResult result = renderWorld(&camera, view, projection, false);
  unsigned char opaque[3];
  glReadPixels(480, 270, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, opaque);
  SKY_CHECK(result.success && result.chunksRebuilt == 0 && memcmp(opaque, nightPixel, 3) == 0);
  SKY_CHECK(captureSkyViews(&sky, shader));
  cleanupSky(&sky);
  cleanupSky(&sky);
  SKY_CHECK(glGetError() == GL_NO_ERROR);
  puts("Sky palette, stars, bodies, translation, depth, lighting, and state checks passed");
  return true;
}

#undef SKY_CHECK
