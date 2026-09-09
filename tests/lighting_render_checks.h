// Real terrain draws with a uniform stone tile isolate lighting from texture
// variation. Moving the cube and orbiting its camera must not relight a face.
static bool lightingProbe(Vec3i block, int face, float orbit, unsigned char value, unsigned char rgb[3]) {
  Vec3 n = vec3FaceMap[face], tangent = n.x ? (Vec3){0, 0, 1} : (Vec3){1, 0, 0};
  Vec3 center = {block.x + 0.5f + n.x * 0.5f, block.y + 0.5f + n.y * 0.5f, block.z + 0.5f + n.z * 0.5f};
  Camera camera = {.position = {center.x + n.x * 3 + tangent.x * orbit, center.y + n.y * 3, center.z + n.z * 3 + tangent.z * orbit}, .up = {0, n.y ? 0 : 1, n.y ? 1 : 0}};
  vec3_subtract(&camera.front, &center, &camera.position);
  Mat4 view, projection;
  mat4_lookAt(view, &camera.position, &center, &camera.up);
  mat4_perspective(projection, 70, 960.0f / 540, 0.1f, 1000);
  unsigned char tile[10 * 4];
  memset(tile, value, sizeof(tile));
  for (int layer = 0; layer < 10; layer++)
    tile[layer * 4 + 3] = 255;
  // initWorld/renderWorld leave the terrain array bound on unit zero.
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 1, 1, 10, 0, GL_RGBA, GL_UNSIGNED_BYTE, tile);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  RenderResult result = renderWorld(&camera, view, projection, false);
  glReadPixels(480, 270, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, rgb);
  return result.success && result.submittedQuads == 6 && result.terrainDrawCalls == 1 && glGetError() == GL_NO_ERROR;
}

static double displayToLinear(unsigned char sample) {
  double value = sample / 255.0;
  return value <= 0.04045 ? value / 12.92 : pow((value + 0.055) / 1.055, 2.4);
}

static bool testTerrainLighting(GLuint shader) {
  const Vec3i positions[] = {{5, 20, 5}, {-97, 2, -81}, {95, 45, 79}, {-1, 31, -1}, {0, 31, 0}, {5, 49, 5}};
  const float orbits[] = {0, -2, 2};
  unsigned char reference[6][3] = {{0}};
  bool success = true;
  int unstable = 0;
  for (size_t position = 0; position < sizeof(positions) / sizeof(positions[0]); position++) {
    clearTerrainFixture();
    if (!setBlock(&positions[position], BLOCK_STONE) || !initWorld(shader))
      return false;
    for (int face = 0; face < 6; face++)
      for (size_t orbit = 0; orbit < sizeof(orbits) / sizeof(orbits[0]); orbit++) {
        unsigned char rgb[3];
        if (!lightingProbe(positions[position], face, orbits[orbit], 128, rgb))
          return false;
        if (position == 0 && orbit == 0)
          memcpy(reference[face], rgb, sizeof(rgb));
        for (int channel = 0; channel < 3; channel++)
          unstable += abs(rgb[channel] - reference[face][channel]) > 1;
      }
  }
  if (unstable) {
    fprintf(stderr, "Terrain lighting changed with world position or camera angle: %d channel samples\n", unstable);
    success = false;
  }
  // Mid-gray must remain visible even underneath a block. Tops should be
  // brighter than every side, and sides brighter than undersides.
  for (int face = 0; face < 6; face++) {
    printf("Terrain lighting face %d: gray = (%u,%u,%u)\n", face, reference[face][0], reference[face][1], reference[face][2]);
    for (int channel = 0; channel < 3; channel++) {
      success &= reference[face][channel] >= 45 && reference[face][channel] <= 128;
      if (face != TOP && face != BOTTOM)
        success &= reference[TOP][channel] > reference[face][channel] + 8 && reference[face][channel] > reference[BOTTOM][channel] + 8;
    }
  }
  // A linear-light multiply keeps the decoded gray/white reflectance ratio.
  // This catches gamma-space shading and applying output encoding twice.
  const unsigned char levels[] = {0, 8, 64, 128, 224, 240, 255};
  for (int face = 0; face < 6; face++) {
    unsigned char samples[7][3];
    for (int level = 0; level < 7; level++)
      if (!lightingProbe(positions[5], face, 0, levels[level], samples[level]))
        return false;
    for (int channel = 0; channel < 3; channel++) {
      success &= samples[0][channel] == 0 && samples[5][channel] < 250;
      success &= samples[5][channel] > samples[4][channel] + 5;
      for (int level = 1; level < 6; level++) {
        double reflectance = displayToLinear(samples[level][channel]) / displayToLinear(samples[6][channel]);
        if (!isfinite(reflectance) || fabs(reflectance - displayToLinear(levels[level])) > 0.012) {
          fprintf(stderr, "Terrain face %d level %u channel %d lost linear-light texture contrast\n", face, levels[level], channel);
          success = false;
        }
      }
    }
  }
  if (!success)
    fprintf(stderr, "Terrain lighting stability, face readability, or texture contrast failed\n");
  else
    puts("Terrain lighting: stable across six positions and three camera angles; all faces readable, linear-light contrast preserved");
  return success;
}
