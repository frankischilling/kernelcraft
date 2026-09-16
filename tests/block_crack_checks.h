#ifndef BLOCK_CRACK_CHECKS_H
#define BLOCK_CRACK_CHECKS_H

static size_t crackPixelDifferences(const unsigned char* before, const unsigned char* after, size_t bytes) {
  size_t changed = 0;
  for (size_t pixel = 0; pixel < bytes; pixel += 3)
    changed += abs(after[pixel] - before[pixel]) > 3 || abs(after[pixel + 1] - before[pixel + 1]) > 3 || abs(after[pixel + 2] - before[pixel + 2]) > 3;
  return changed;
}

static void crackFaceCamera(int face, Vec3i block, Camera* camera, Mat4 view, Mat4 projection) {
  Vec3 normal = vec3FaceMap[face];
  Vec3 center = {block.x + 0.5f + normal.x * 0.5f, block.y + 0.5f + normal.y * 0.5f, block.z + 0.5f + normal.z * 0.5f};
  *camera = (Camera){.position = {center.x + normal.x * 4, center.y + normal.y * 4, center.z + normal.z * 4},
                     .front = {-normal.x, -normal.y, -normal.z},
                     .up = {0, normal.y ? 0 : 1, normal.y ? 1 : 0}};
  mat4_lookAt(view, &camera->position, &center, &camera->up);
  mat4_perspective(projection, 70, 960.0f / 540.0f, 0.1f, 1000);
}

static bool crackRenderBase(const Camera* camera, const Mat4 view, const Mat4 projection, unsigned char* pixels, int x, int y, int width, int height) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (!renderWorld(camera, view, projection, false).success)
    return false;
  glReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
  return glGetError() == GL_NO_ERROR;
}

static bool testBlockCrackGrowth(GLuint shader) {
  const Vec3i selected = {-1, 20, 0};
  const float stages[] = {0.15f, 0.50f, 0.90f};

  enum { PROBE = 192 };

  unsigned char* before = malloc(PROBE * PROBE * 3);
  unsigned char* after = malloc(PROBE * PROBE * 3);
  if (!before || !after) {
    free(before);
    free(after);
    return false;
  }

  bool success = true;
  for (int face = 0; face < 6 && success; face++) {
    clearTerrainFixture();
    if (!setBlock(&selected, BLOCK_STONE) || !initWorld(shader)) {
      success = false;
      break;
    }

    Camera camera;
    Mat4 view, projection;
    crackFaceCamera(face, selected, &camera, view, projection);
    Ray selection = rayCast(camera.position, camera.front, EDIT_REACH);
    Vec3i normal = vec3iFaceMap[face];
    if (!selection.hit || selection.blockCoords.x != selected.x || selection.blockCoords.y != selected.y || selection.blockCoords.z != selected.z ||
        selection.normal.x != normal.x || selection.normal.y != normal.y || selection.normal.z != normal.z) {
      success = false;
      break;
    }

    size_t changed[3] = {0};
    for (size_t stage = 0; stage < sizeof(stages) / sizeof(stages[0]); stage++) {
      if (!crackRenderBase(&camera, view, projection, before, 480 - PROBE / 2, 270 - PROBE / 2, PROBE, PROBE)) {
        success = false;
        break;
      }
      BlockBreaking breaking = {.target = selected, .block = BLOCK_STONE, .elapsed = blockHandBreakSeconds(BLOCK_STONE) * stages[stage], .active = true};
      drawBlockBreaking(&breaking, &selection, view, projection);
      glReadPixels(480 - PROBE / 2, 270 - PROBE / 2, PROBE, PROBE, GL_RGB, GL_UNSIGNED_BYTE, after);
      changed[stage] = crackPixelDifferences(before, after, PROBE * PROBE * 3);
      success &= glGetError() == GL_NO_ERROR;
    }

    printf("Block crack face %d: early=%zu mid=%zu late=%zu pixels\n", face, changed[0], changed[1], changed[2]);
    success &= changed[0] > 8 && changed[1] > changed[0] + 8 && changed[2] > changed[1] + 8;
  }

  free(before);
  free(after);
  return success;
}

static bool testBlockCrackNoOutput(GLuint shader) {
  const Vec3i selected = {-1, 20, 0};
  clearTerrainFixture();
  if (!setBlock(&selected, BLOCK_STONE) || !initWorld(shader))
    return false;
  Camera camera;
  Mat4 view, projection;
  crackFaceCamera(FRONT, selected, &camera, view, projection);
  Ray selection = rayCast(camera.position, camera.front, EDIT_REACH);

  enum { PROBE = 192 };

  unsigned char before[PROBE * PROBE * 3], after[sizeof(before)];
  BlockBreaking cases[] = {
      {.target = selected, .block = BLOCK_STONE, .elapsed = 0, .active = true},
      {.target = selected, .block = BLOCK_STONE, .elapsed = 0.5, .active = false},
      {.target = selected, .block = BLOCK_DIRT, .elapsed = 0.25, .active = true},
  };
  bool success = selection.hit;
  for (size_t test = 0; test < sizeof(cases) / sizeof(cases[0]) && success; test++) {
    success = crackRenderBase(&camera, view, projection, before, 480 - PROBE / 2, 270 - PROBE / 2, PROBE, PROBE);
    drawBlockBreaking(&cases[test], &selection, view, projection);
    glReadPixels(480 - PROBE / 2, 270 - PROBE / 2, PROBE, PROBE, GL_RGB, GL_UNSIGNED_BYTE, after);
    success &= memcmp(before, after, sizeof(before)) == 0;
  }

  BlockBreaking active = {.target = selected, .block = BLOCK_STONE, .elapsed = 0.75, .active = true};
  Ray mismatch = selection;
  mismatch.blockCoords.x++;
  if (success) {
    success = crackRenderBase(&camera, view, projection, before, 480 - PROBE / 2, 270 - PROBE / 2, PROBE, PROBE);
    drawBlockBreaking(&active, &mismatch, view, projection);
    glReadPixels(480 - PROBE / 2, 270 - PROBE / 2, PROBE, PROBE, GL_RGB, GL_UNSIGNED_BYTE, after);
    success &= memcmp(before, after, sizeof(before)) == 0;
  }
  Ray miss = {0};
  if (success) {
    drawBlockBreaking(&active, &miss, view, projection);
    glReadPixels(480 - PROBE / 2, 270 - PROBE / 2, PROBE, PROBE, GL_RGB, GL_UNSIGNED_BYTE, after);
    success &= memcmp(before, after, sizeof(before)) == 0;
  }
  if (success) {
    setBlock(&selected, BLOCK_AIR);
    success = crackRenderBase(&camera, view, projection, before, 480 - PROBE / 2, 270 - PROBE / 2, PROBE, PROBE);
    drawBlockBreaking(&active, &selection, view, projection);
    glReadPixels(480 - PROBE / 2, 270 - PROBE / 2, PROBE, PROBE, GL_RGB, GL_UNSIGNED_BYTE, after);
    success &= memcmp(before, after, sizeof(before)) == 0;
  }
  return success && glGetError() == GL_NO_ERROR;
}

static bool testBlockCrackOcclusionAndNeighbor(GLuint shader) {
  const Vec3i selected = {-1, 20, 0}, neighbor = {0, 20, 0};
  clearTerrainFixture();
  if (!setBlock(&selected, BLOCK_STONE) || !setBlock(&neighbor, BLOCK_STONE) || !initWorld(shader))
    return false;
  Camera camera;
  Mat4 view, projection;
  crackFaceCamera(FRONT, selected, &camera, view, projection);
  Ray selection = rayCast(camera.position, camera.front, EDIT_REACH);
  BlockBreaking breaking = {.target = selected, .block = BLOCK_STONE, .elapsed = blockHandBreakSeconds(BLOCK_STONE) * 0.9, .active = true};
  unsigned char before[24 * 24 * 3], after[sizeof(before)];
  Mat4 combined;
  mat4_multiply(combined, projection, view);
  Vec3 neighborCenter = {neighbor.x + 0.5f, neighbor.y + 0.5f, neighbor.z + 1.0f};
  float w = combined[3] * neighborCenter.x + combined[7] * neighborCenter.y + combined[11] * neighborCenter.z + combined[15];
  float clipX = combined[0] * neighborCenter.x + combined[4] * neighborCenter.y + combined[8] * neighborCenter.z + combined[12];
  float clipY = combined[1] * neighborCenter.x + combined[5] * neighborCenter.y + combined[9] * neighborCenter.z + combined[13];
  int screenX = (int)((clipX / w + 1) * 480), screenY = (int)((clipY / w + 1) * 270);
  if (!selection.hit || screenX < 12 || screenX >= 948 || screenY < 12 || screenY >= 528)
    return false;
  if (!crackRenderBase(&camera, view, projection, before, screenX - 12, screenY - 12, 24, 24))
    return false;
  drawBlockBreaking(&breaking, &selection, view, projection);
  glReadPixels(screenX - 12, screenY - 12, 24, 24, GL_RGB, GL_UNSIGNED_BYTE, after);
  if (memcmp(before, after, sizeof(before)) != 0)
    return false;

  Vec3i foreground = {-1, 20, 2};
  if (!setBlock(&foreground, BLOCK_STONE) || !initWorld(shader))
    return false;
  size_t bytes = 960 * 540 * 3;
  unsigned char* fullBefore = malloc(bytes);
  unsigned char* fullAfter = malloc(bytes);
  bool success = fullBefore && fullAfter;
  if (success) {
    success = crackRenderBase(&camera, view, projection, fullBefore, 0, 0, 960, 540);
    drawBlockBreaking(&breaking, &selection, view, projection);
    glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, fullAfter);
    success &= memcmp(fullBefore, fullAfter, bytes) == 0;
  }
  free(fullBefore);
  free(fullAfter);
  return success && glGetError() == GL_NO_ERROR;
}

static bool crackProjectLeafTexel(const Mat4 combined, Vec3i block, int texelX, int texelY, int* screenX, int* screenY) {
  float s = (texelX + 0.5f) / 16.0f, t = (texelY + 0.5f) / 16.0f;
  Vec3 point = {(block.x + s) * CUBE_SIZE, (block.y + 1.0f - t) * CUBE_SIZE, (block.z + 1.0f) * CUBE_SIZE};
  float w = combined[3] * point.x + combined[7] * point.y + combined[11] * point.z + combined[15];
  if (w <= 0)
    return false;
  float x = combined[0] * point.x + combined[4] * point.y + combined[8] * point.z + combined[12];
  float y = combined[1] * point.x + combined[5] * point.y + combined[9] * point.z + combined[13];
  *screenX = (int)((x / w + 1) * 480);
  *screenY = (int)((y / w + 1) * 270);
  return *screenX >= 2 && *screenX < 958 && *screenY >= 2 && *screenY < 538;
}

static bool testLeafBlockCrackMask(GLuint shader) {
  const Vec3i selected = {-1, 20, 0};
  // These source-PNG texels independently exercise late crack branches: the
  // first pair is fully opaque, while the second pair has alpha zero.
  const int probes[][3] = {{12, 1, 1}, {5, 2, 1}, {4, 1, 0}, {13, 7, 0}};
  clearTerrainFixture();
  if (!setBlock(&selected, BLOCK_OAK_LEAVES) || !initWorld(shader) || !worldLeafTexture())
    return false;
  Camera camera;
  Mat4 view, projection, combined;
  crackFaceCamera(FRONT, selected, &camera, view, projection);
  mat4_multiply(combined, projection, view);
  Ray selection = rayCast(camera.position, camera.front, EDIT_REACH);
  if (!selection.hit || selection.blockCoords.x != selected.x || selection.blockCoords.y != selected.y || selection.blockCoords.z != selected.z)
    return false;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (!renderWorld(&camera, view, projection, false).success)
    return false;

  unsigned char before[4][3 * 3 * 3], after[4][3 * 3 * 3];
  float depthBefore[4][3 * 3], depthAfter[4][3 * 3];
  int screen[4][2];
  for (size_t probe = 0; probe < sizeof(probes) / sizeof(probes[0]); probe++) {
    if (!crackProjectLeafTexel(combined, selected, probes[probe][0], probes[probe][1], &screen[probe][0], &screen[probe][1]))
      return false;
    itemTestRead(screen[probe][0] - 1, screen[probe][1] - 1, 3, 3, GL_RGB, GL_UNSIGNED_BYTE, before[probe]);
    itemTestRead(screen[probe][0] - 1, screen[probe][1] - 1, 3, 3, GL_DEPTH_COMPONENT, GL_FLOAT, depthBefore[probe]);
  }

  GLuint sampler = 0;
  glGenSamplers(1, &sampler);
  if (!sampler)
    return false;
  glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glActiveTexture(GL_TEXTURE0);
  glBindSampler(0, sampler);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  glMatrixMode(GL_TEXTURE);
  glPushMatrix();
  glLoadIdentity();
  glTranslatef(0.125f, 0.25f, 0);
  GLfloat expectedTextureMatrix[16];
  glGetFloatv(GL_TEXTURE_MATRIX, expectedTextureMatrix);
  glActiveTexture(GL_TEXTURE1);
  glEnable(GL_TEXTURE_CUBE_MAP);
  glActiveTexture(GL_TEXTURE2);
  glEnable(GL_TEXTURE_GEN_S);
  glEnable(GL_TEXTURE_GEN_T);
  glActiveTexture(GL_TEXTURE3);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, worldLeafTexture());
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  glEnable(GL_ALPHA_TEST);
  glAlphaFunc(GL_LESS, 0.25f);
  BlockBreaking breaking = {.target = selected, .block = BLOCK_OAK_LEAVES, .elapsed = blockHandBreakSeconds(BLOCK_OAK_LEAVES) * 0.9, .active = true};
  drawBlockBreaking(&breaking, &selection, view, projection);

  GLint activeTexture, binding, environment, alphaFunction, matrixMode;
  GLfloat alphaReference;
  glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
  glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
  glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &environment);
  glGetIntegerv(GL_ALPHA_TEST_FUNC, &alphaFunction);
  glGetFloatv(GL_ALPHA_TEST_REF, &alphaReference);
  bool success = activeTexture == GL_TEXTURE3 && matrixMode == GL_TEXTURE && binding == (GLint)worldLeafTexture() && environment == GL_REPLACE && glIsEnabled(GL_TEXTURE_2D) &&
                 glIsEnabled(GL_ALPHA_TEST) && alphaFunction == GL_LESS && alphaReference == 0.25f;
  glActiveTexture(GL_TEXTURE0);
  GLint restoredSampler, unit0Environment;
  GLfloat restoredTextureMatrix[16];
  glGetIntegerv(GL_SAMPLER_BINDING, &restoredSampler);
  glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &unit0Environment);
  glGetFloatv(GL_TEXTURE_MATRIX, restoredTextureMatrix);
  success &= restoredSampler == (GLint)sampler && unit0Environment == GL_REPLACE && memcmp(expectedTextureMatrix, restoredTextureMatrix, sizeof(expectedTextureMatrix)) == 0;
  glActiveTexture(GL_TEXTURE1);
  success &= glIsEnabled(GL_TEXTURE_CUBE_MAP);
  glActiveTexture(GL_TEXTURE2);
  success &= glIsEnabled(GL_TEXTURE_GEN_S) && glIsEnabled(GL_TEXTURE_GEN_T);
  glActiveTexture(GL_TEXTURE3);
  for (size_t probe = 0; probe < sizeof(probes) / sizeof(probes[0]); probe++) {
    itemTestRead(screen[probe][0] - 1, screen[probe][1] - 1, 3, 3, GL_RGB, GL_UNSIGNED_BYTE, after[probe]);
    itemTestRead(screen[probe][0] - 1, screen[probe][1] - 1, 3, 3, GL_DEPTH_COMPONENT, GL_FLOAT, depthAfter[probe]);
    // Each source texel spans about six framebuffer pixels in this view. This
    // interior 3x3 window stays inside one texel while allowing a branch endpoint
    // to miss its exact center. Every pixel of a transparent probe must stay clear.
    bool changed = crackPixelDifferences(before[probe], after[probe], sizeof(before[probe])) != 0;
    success &= changed == (bool)probes[probe][2];
    success &= memcmp(depthBefore[probe], depthAfter[probe], sizeof(depthBefore[probe])) == 0;
    if (changed != (bool)probes[probe][2])
      fprintf(stderr, "Leaf crack texel (%d,%d): changed=%d expected=%d\n", probes[probe][0], probes[probe][1], changed, probes[probe][2]);
  }

  glDisable(GL_ALPHA_TEST);
  glDisable(GL_TEXTURE_2D);
  glActiveTexture(GL_TEXTURE2);
  glDisable(GL_TEXTURE_GEN_S);
  glDisable(GL_TEXTURE_GEN_T);
  glActiveTexture(GL_TEXTURE1);
  glDisable(GL_TEXTURE_CUBE_MAP);
  glActiveTexture(GL_TEXTURE0);
  glBindSampler(0, 0);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glMatrixMode(GL_TEXTURE);
  glPopMatrix();
  glDeleteSamplers(1, &sampler);
  glMatrixMode(GL_MODELVIEW);
  return success && glGetError() == GL_NO_ERROR;
}

static bool testBlockCrackDepthAndState(GLuint shader) {
  const Vec3i selected = {-1, 20, 0};
  clearTerrainFixture();
  if (!setBlock(&selected, BLOCK_STONE) || !initWorld(shader))
    return false;
  Camera camera;
  Mat4 view, projection;
  crackFaceCamera(FRONT, selected, &camera, view, projection);
  Ray selection = rayCast(camera.position, camera.front, EDIT_REACH);
  if (!selection.hit)
    return false;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (!renderWorld(&camera, view, projection, false).success)
    return false;
  unsigned char before[192 * 192 * 3], after[sizeof(before)];
  float depthBefore[96 * 96], depthAfter[96 * 96];
  glReadPixels(384, 174, 192, 192, GL_RGB, GL_UNSIGNED_BYTE, before);
  glReadPixels(432, 222, 96, 96, GL_DEPTH_COMPONENT, GL_FLOAT, depthBefore);

  glUseProgram(shader);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA);
  glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_SUBTRACT);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(3, 7);
  glDisable(GL_DEPTH_TEST);
  glDepthFunc(GL_GREATER);
  glColor4f(0.2f, 0.4f, 0.6f, 0.8f);
  glLineWidth(1);
  glEnable(GL_TEXTURE_2D);
  glMatrixMode(GL_TEXTURE);
  BlockBreaking breaking = {.target = selected, .block = BLOCK_STONE, .elapsed = blockHandBreakSeconds(BLOCK_STONE) * 0.9, .active = true};
  drawBlockBreaking(&breaking, &selection, view, projection);

  const GLenum names[] = {GL_CURRENT_PROGRAM, GL_MATRIX_MODE,     GL_DEPTH_FUNC,         GL_DEPTH_WRITEMASK,      GL_BLEND_SRC_RGB, GL_BLEND_DST_RGB,
                          GL_BLEND_SRC_ALPHA, GL_BLEND_DST_ALPHA, GL_BLEND_EQUATION_RGB, GL_BLEND_EQUATION_ALPHA, GL_CULL_FACE_MODE};
  const GLint expected[] = {(GLint)shader,    GL_TEXTURE, GL_GREATER, GL_TRUE, GL_ONE, GL_ZERO, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_FUNC_REVERSE_SUBTRACT,
                            GL_FUNC_SUBTRACT, GL_FRONT};
  bool restored = !glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_BLEND) && glIsEnabled(GL_CULL_FACE) && glIsEnabled(GL_POLYGON_OFFSET_FILL) && glIsEnabled(GL_TEXTURE_2D) &&
                  !glIsEnabled(GL_POLYGON_OFFSET_LINE);
  for (size_t state = 0; state < sizeof(names) / sizeof(names[0]); state++) {
    GLint value;
    glGetIntegerv(names[state], &value);
    restored &= value == expected[state];
  }
  GLint polygonMode[2];
  GLfloat factor, units, color[4], width;
  glGetIntegerv(GL_POLYGON_MODE, polygonMode);
  glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &factor);
  glGetFloatv(GL_POLYGON_OFFSET_UNITS, &units);
  glGetFloatv(GL_CURRENT_COLOR, color);
  glGetFloatv(GL_LINE_WIDTH, &width);
  restored &= polygonMode[0] == GL_LINE && polygonMode[1] == GL_LINE && factor == 3 && units == 7 && width == 1 && color[0] == 0.2f && color[1] == 0.4f && color[2] == 0.6f &&
              color[3] == 0.8f;

  glReadPixels(384, 174, 192, 192, GL_RGB, GL_UNSIGNED_BYTE, after);
  glReadPixels(432, 222, 96, 96, GL_DEPTH_COMPONENT, GL_FLOAT, depthAfter);
  size_t changed = crackPixelDifferences(before, after, sizeof(before));
  bool depthPreserved = memcmp(depthBefore, depthAfter, sizeof(depthBefore)) == 0;

  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glDisable(GL_TEXTURE_2D);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glMatrixMode(GL_MODELVIEW);
  return restored && depthPreserved && changed > 20 && glGetError() == GL_NO_ERROR;
}

static bool testBlockCracks(GLuint shader) {
  bool success = testBlockCrackGrowth(shader);
  if (success && !(success = testBlockCrackNoOutput(shader)))
    fprintf(stderr, "Block crack no-output checks failed\n");
  if (success && !(success = testBlockCrackOcclusionAndNeighbor(shader)))
    fprintf(stderr, "Block crack occlusion/neighbor checks failed\n");
  if (success && !(success = testLeafBlockCrackMask(shader)))
    fprintf(stderr, "Block crack leaf masking checks failed\n");
  if (success && !(success = testBlockCrackDepthAndState(shader)))
    fprintf(stderr, "Block crack depth/state checks failed\n");
  if (success)
    puts("Block crack growth, face coverage, cutout masking, stale-target, occlusion, depth, wireframe, and GL state checks passed");
  return success;
}

#endif
