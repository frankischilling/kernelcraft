// The supplied leaf export contains 42 transparent texels. Check their actual
// color/depth effect, including bark immediately behind foliage and shadow maps.
static bool testLeafCutout(GLuint shader) {
  const Vec3i leafCell = {0, 20, 0}, logCell = {0, 20, -1};
  // Reinitialization destroys renderer-owned GL names. Save caller bindings
  // only after it completes, so restoration cannot bind deleted objects.
  clearTerrainFixture();
  if (!setBlock(&leafCell, BLOCK_OAK_LEAVES) || !initWorld(shader))
    return false;
  InventoryCheckGLState original;
  inventoryCheckCaptureState(&original);
  GLint savedArray, savedBuffer;
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &savedArray);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &savedBuffer);
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  ItemTestTarget target = {0};
  ShadowMap shadow = {0};
  GLuint vao = 0, vbo = 0, ebo = 0, materials = 0;
  ChunkMesh mesh = {0};
  int width, height, channels;
  unsigned char* leaf = stbi_load("assets/textures/oak-leaves.png", &width, &height, &channels, 4);
  bool ok = leaf && width == 16 && height == 16 && channels == 4;
  unsigned char* bark = stbi_load("assets/textures/oak-log-side.png", &width, &height, &channels, 4);
  ok &= bark && width == 16 && height == 16;
  if (!ok || !itemTestTarget(&target, 256, 256))
    goto finish;
  Camera camera = {.position = {0.5f, 20.5f, 4}, .front = {0, 0, -1}, .up = {0, 1, 0}};
  Vec3 center = {0.5f, 20.5f, 0.5f};
  Mat4 view, projection;
  mat4_lookAt(view, &camera.position, &center, &camera.up);
  mat4_identity(projection);
  projection[0] = projection[5] = 2;
  projection[10] = -0.2f;
  DayNightState light = itemTestNeutralLight();
  unsigned transparent = 0, opaque = 0;
  for (int pass = 0; ok && pass < 2; pass++) {
    if (pass)
      ok &= setBlock(&logCell, BLOCK_OAK_LOG);
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glViewport(0, 0, 256, 256);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    itemTestClear(1);
    glClearColor(17.0f / 255, 31.0f / 255, 47.0f / 255, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    setWorldDayNight(&light);
    RenderResult result = renderWorld(&camera, view, projection, false);
    ok &= result.success;
    for (int y = 0; ok && y < 16; y++)
      for (int x = 0; ok && x < 16; x++) {
        unsigned char actual[4];
        float depth;
        itemTestRead(x * 16 + 8, y * 16 + 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, actual);
        itemTestRead(x * 16 + 8, y * 16 + 8, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
        size_t source = (size_t)((15 - y) * 16 + x) * 4;
        bool hole = leaf[source + 3] == 0;
        const unsigned char background[] = {17, 31, 47, 255};
        const unsigned char* expected = hole ? (pass ? bark + source : background) : leaf + source;
        for (int c = 0; c < 4; c++)
          ok &= abs((int)actual[c] - expected[c]) <= 1;
        ok &= hole && !pass ? depth == 1 : depth < 1;
        if (!pass) {
          transparent += hole;
          opaque += !hole;
        }
      }
  }
  ok &= transparent == 42 && opaque == 214;
  if (!ok) {
    fprintf(stderr, "Leaf source alpha, background depth, or adjacent bark failed (%u transparent/%u opaque)\n", transparent, opaque);
    goto finish;
  }

  // Build a real leaf cube for a close shadow-map probe. The opaque control
  // proves geometry is present; enabling the terrain array must punch holes.
  ok &= setBlock(&logCell, BLOCK_AIR);
  ok &= buildChunkMesh(getChunk(&(Vec2i){8, 8}), &mesh);
  if (!ok)
    goto finish;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ebo);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, mesh.vertexCount * sizeof(MeshVertex), mesh.vertices, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indexCount * sizeof(uint32_t), mesh.indices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, position));
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, uv));
  glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, material));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(2);
  glEnableVertexAttribArray(3);
  glActiveTexture(GL_TEXTURE0);
  const char* masks[13];
  for (int layer = 0; layer < 13; layer++)
    masks[layer] = "assets/textures/oak-leaves.png";
  materials = loadTextureArray(masks, 13);
  ok &= materials != 0 && initShadowMap(&shadow, center, 1);
  for (int pass = 0; ok && pass < 2; pass++) {
    ShadowGeometry geometry = {vao, (GLsizei)mesh.indexCount, GL_UNSIGNED_INT, pass ? (GLuint)materials : 0};
    int calls = 0;
    glActiveTexture(GL_TEXTURE3);
    ok &= updateShadowMap(&shadow, (Vec3){0, 0, 1}, true, &geometry, 1, &calls) && calls == 1;
    GLint active;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    ok &= active == GL_TEXTURE3;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, shadow.framebuffer);
    for (int y = 0; ok && y < 16; y++)
      for (int x = 0; ok && x < 16; x++) {
        int px = (int)((0.25 + (x + 0.5) / 32) * shadow.size);
        int py = (int)((0.25 + (y + 0.5) / 32) * shadow.size);
        float depth;
        itemTestRead(px, py, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
        bool hole = pass && leaf[((15 - y) * 16 + x) * 4 + 3] == 0;
        ok &= hole ? depth == 1 : depth < 0.5f;
      }
  }
  if (!ok)
    fprintf(stderr, "Leaf shadow cutouts or texture state failed\n");
finish:
  freeChunkMesh(&mesh);
  glBindVertexArray(0);
  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(1, &vbo);
  glDeleteBuffers(1, &ebo);
  glDeleteTextures(1, &materials);
  cleanupShadowMap(&shadow);
  stbi_image_free(leaf);
  stbi_image_free(bark);
  itemTestDestroyTarget(&target);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)original.drawFramebuffer);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)original.readFramebuffer);
  glPopAttrib();
  inventoryCheckRestoreState(&original);
  glBindBuffer(GL_ARRAY_BUFFER, (GLuint)savedBuffer);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, (GLuint)savedArray);
  glActiveTexture((GLenum)original.activeTexture);
  if (ok)
    puts("Leaf artwork: 42 cutout/214 opaque texels, adjacent bark, world depth, and shadow cutouts passed");
  return ok && glGetError() == GL_NO_ERROR;
}
