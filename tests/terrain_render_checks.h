// Framebuffer fixtures included by render_benchmark.c after its GL setup helpers.
// This CPU oracle is separate from the GLSL implementation and chooses a whole
// unit face's texture before the reference sampler2D draw.
static int referenceTerrainLayer(int material, Vec3i block, uint32_t seed) {
  uint32_t h = seed ^ (uint32_t)block.x * UINT32_C(0x8da6b343) ^ (uint32_t)block.y * UINT32_C(0xd8163841) ^ (uint32_t)block.z * UINT32_C(0xcb1ab31f) ^
               (uint32_t)material * UINT32_C(0x9e3779b9);
  h = (h ^ (h >> 16)) * UINT32_C(0x7feb352d);
  h = (h ^ (h >> 15)) * UINT32_C(0x846ca68b);
  h ^= h >> 16;
  unsigned roll = h % 100;
  return material == 1 && roll < 25 ? 4 : material == 2 && roll < 10 ? 5 : material == 3 && roll < 2 ? 6 : material;
}

static int referenceTerrainMaterial(int id, int face) {
  return id == BLOCK_COBBLESTONE ? 7 : id == BLOCK_STONE ? 0 : id == BLOCK_DIRT || face == BOTTOM ? 1 : face == TOP ? 2 : 3;
}

static void clearTerrainFixture(void) {
  for (int x = 0; x < CHUNKS_PER_AXIS; x++)
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Chunk* chunk = getChunk(&(Vec2i){x, z});
      memset(chunk->blocks, 0, sizeof(chunk->blocks));
    }
}

static bool testTerrainVariants(GLuint shader) {
  const uint32_t seeds[] = {0, UINT32_MAX, 0};
  uint64_t fingerprints[3] = {0};
  size_t totals[8] = {0}, variants[8] = {0};
  unsigned char* pixels = malloc(960 * 540 * 3);
  unsigned char* rebuilt = malloc(960 * 540 * 3);
  bool success = pixels && rebuilt;
  for (int seed = 0; success && seed < 3; seed++) {
    if (!initChunksSeeded(seeds[seed])) {
      success = false;
      break;
    }
    uint64_t fingerprint = UINT64_C(14695981039346656037);
    for (int id = BLOCK_GRASS; success && id <= BLOCK_COBBLESTONE; id++)
      for (int face = 0; success && face < 6; face++) {
        clearTerrainFixture();
        Vec3 n = vec3FaceMap[face], u = n.x ? (Vec3){0, 0, 1} : (Vec3){1, 0, 0}, v;
        vec3_cross(&v, &n, &u);
        for (int a = -16; a < 16; a++)
          for (int b = -16; b < 16; b++)
            setBlock(&(Vec3i){-1 + (int)(a * u.x + b * v.x), 24 + (int)(a * u.y + b * v.y), (int)(a * u.z + b * v.z)}, id);
        if (!initWorld(shader)) {
          success = false;
          break;
        }
        GLint layers = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_DEPTH, &layers);
        if (layers != 8) {
          fprintf(stderr, "Expected eight terrain texture layers, got %d\n", layers);
          success = false;
          break;
        }
        // Replace only the in-memory fixture tiles with distinct RGB bit masks.
        // The real shader, array binding, greedy meshes, and culling still run.
        unsigned char colors[8 * 4];
        for (int layer = 0; layer < 7; layer++) {
          for (int channel = 0; channel < 3; channel++)
            colors[layer * 4 + channel] = ((layer + 1) & (1 << channel)) ? 255 : 0;
          colors[layer * 4 + 3] = 255;
        }
        // An eighth binary RGB mask would be black and could hide missing faces.
        // Orange stays distinct from the seven masks under the Phong lighting.
        colors[28] = 255;
        colors[29] = 64;
        colors[30] = 0;
        colors[31] = 255;
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 1, 1, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors);
        Vec3 center = {-0.5f + 0.5f * (n.x - u.x - v.x), 24.5f + 0.5f * (n.y - u.y - v.y), 0.5f + 0.5f * (n.z - u.z - v.z)};
        Camera camera = {.position = {center.x + 26 * n.x, center.y + 26 * n.y, center.z + 26 * n.z}, .front = {-n.x, -n.y, -n.z}, .up = v};
        Mat4 view, projection, combined;
        mat4_lookAt(view, &camera.position, &center, &camera.up);
        mat4_perspective(projection, 70, 960.0f / 540, 0.1f, 1000);
        mat4_multiply(combined, projection, view);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!renderWorld(&camera, view, projection, false).success) {
          success = false;
          break;
        }
        glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, pixels);
        for (int a = -16; success && a < 16; a++)
          for (int b = -16; b < 16; b++) {
            Vec3i block = {-1 + (int)(a * u.x + b * v.x), 24 + (int)(a * u.y + b * v.y), (int)(a * u.z + b * v.z)};
            Vec3 p = {block.x + 0.5f + n.x * 0.5f, block.y + 0.5f + n.y * 0.5f, block.z + 0.5f + n.z * 0.5f};
            float w = combined[3] * p.x + combined[7] * p.y + combined[11] * p.z + combined[15];
            int x = (int)(((combined[0] * p.x + combined[4] * p.y + combined[8] * p.z + combined[12]) / w + 1) * 480);
            int y = (int)(((combined[1] * p.x + combined[5] * p.y + combined[9] * p.z + combined[13]) / w + 1) * 270);
            if (x < 0 || x >= 960 || y < 0 || y >= 540) {
              success = false;
              break;
            }
            const unsigned char* rgb = pixels + (y * 960 + x) * 3;
            int actual = (rgb[0] > 20 ? 1 : 0) + (rgb[1] > 20 ? 2 : 0) + (rgb[2] > 20 ? 4 : 0) - 1;
            if (rgb[1] > 5 && rgb[0] > 2 * rgb[1] && rgb[2] == 0)
              actual = 7;
            int material = referenceTerrainMaterial(id, face);
            int expected = referenceTerrainLayer(material, block, seeds[seed]);
            if (actual != expected) {
              fprintf(stderr, "Terrain variant seed %u face %d block (%d,%d,%d): layer %d, expected %d\n", seeds[seed], face, block.x, block.y, block.z, actual, expected);
              success = false;
              break;
            }
            totals[material]++;
            variants[material] += actual != material;
            fingerprint = (fingerprint ^ (unsigned)actual) * UINT64_C(1099511628211);
          }
        // Rebuilding after an edit must leave the remaining tile choices stable.
        if (success && face == FRONT) {
          setBlock(&(Vec3i){-1, 24, 0}, BLOCK_AIR);
          setBlock(&(Vec3i){-1, 24, 0}, id);
          glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
          success = renderWorld(&camera, view, projection, false).success;
          glReadPixels(0, 0, 960, 540, GL_RGB, GL_UNSIGNED_BYTE, rebuilt);
          success &= memcmp(pixels, rebuilt, 960 * 540 * 3) == 0;
        }
        success &= glGetError() == GL_NO_ERROR;
      }
    fingerprints[seed] = fingerprint;
  }
  const int low[] = {0, 22, 7, 1}, high[] = {0, 28, 13, 3};
  for (int material = 0; success && material < 4; material++) {
    printf("Terrain material %d: %zu/%zu variant tiles\n", material, variants[material], totals[material]);
    success &= totals[material] > 1000 && variants[material] * 100 >= totals[material] * low[material] && variants[material] * 100 <= totals[material] * high[material];
  }
  printf("Terrain cobblestone: %zu/%zu variant tiles\n", variants[7], totals[7]);
  success &= totals[7] == 3 * 6 * 32 * 32 && variants[7] == 0;
  success &= fingerprints[0] == fingerprints[2] && fingerprints[0] != fingerprints[1];
  free(pixels);
  free(rebuilt);
  return success;
}

static bool testFarTerrain(GLuint shader) {
  clearTerrainFixture();
  if (!initWorld(shader))
    return false;
  Camera camera = {.position = {0.5f, 20.5f, 0.5f}, .up = {0, 1, 0}};
  const int distances[] = {64, -64, 80, -80, 112, -112};
  for (size_t i = 0; i < sizeof(distances) / sizeof(distances[0]); i++) {
    Vec3i block = {distances[i], 20, 0};
    setBlock(&block, BLOCK_GRASS);
    Vec3 target = {block.x + 0.5f, 20.5f, 0.5f};
    vec3_subtract(&camera.front, &target, &camera.position);
    Mat4 view, projection;
    mat4_lookAt(view, &camera.position, &target, &camera.up);
    mat4_perspective(projection, 70, 960.0f / 540, 0.1f, 1000);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    RenderResult result = renderWorld(&camera, view, projection, false);
    unsigned char rgb[3];
    glReadPixels(480, 270, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    bool expected = abs(distances[i]) < 96;
    bool visible = rgb[0] || rgb[1] || rgb[2];
    if (!result.success || visible != expected || result.chunksRendered != (int)expected) {
      fprintf(stderr, "Terrain at %d blocks: visible=%d, chunks=%d, expected=%d\n", distances[i], visible, result.chunksRendered, expected);
      return false;
    }
    RenderResult wireframe = renderWorld(&camera, view, projection, true);
    if (!wireframe.success || wireframe.chunksRendered != result.chunksRendered || wireframe.submittedTriangles != result.submittedTriangles || wireframe.chunksRebuilt)
      return false;
    // Turning away must cull the same distant block in wireframe as in solid.
    Vec3 away = {camera.position.x - camera.front.x, camera.position.y, camera.position.z};
    mat4_lookAt(view, &camera.position, &away, &camera.up);
    wireframe = renderWorld(&camera, view, projection, true);
    if (!wireframe.success || wireframe.chunksRendered)
      return false;
    setBlock(&block, BLOCK_AIR);
  }
  puts("Terrain render radius: 64/80 blocks visible, 112 blocks culled in both directions");
  return glGetError() == GL_NO_ERROR;
}
