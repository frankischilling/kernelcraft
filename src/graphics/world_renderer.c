#include "world_renderer.h"
#include "frustum.h"
#include "../world/mesh.h"
#include "../world/occlusion.h"
#include "../world/mesh_visibility.h"
#include "../world/world.h"
#include "texture.h"
#include "shadows.h"
#include <GLFW/glfw3.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  GLuint vao, vbo, ebo;
  size_t indexCount;
  size_t indexBytes;
  GLenum indexType;
  Vec3 center, dimensions;
  int surfaceBlocks;
  MeshOccluders occluders;
  MeshVisibility visibility;
} RenderChunk;

static RenderChunk renderChunks[CHUNKS_PER_AXIS][CHUNKS_PER_AXIS];
static GLuint textureArray;
static GLuint program, gridVAO, gridVBO;
static GLint viewProjectionLocation, gridLocation;
static ShadowCache shadows;
static Vec3 shadowLight = {0.45f, 0.8f, 0.35f};
static GLint shadowTransformLocation[2], shadowScaleLocation, shadowBlendLocation;

enum { GRID_VERTICES = (CHUNKS_PER_AXIS + 1) * 4 };

typedef struct {
  RenderChunk* chunk;
  float distanceSquared;
  bool hidden;
} ChunkCandidate;

static OcclusionBuffer occlusion;
static ChunkCandidate candidates[CHUNKS_PER_AXIS * CHUNKS_PER_AXIS];
static int candidateCount, hiddenCount;
static Mat4 cachedTransform;
static Vec3 cachedPosition;
static GLint cachedViewport[4];
static bool visibilityValid, cachedWireframe;
static int renderDistance = WORLD_RENDER_DISTANCE_DEFAULT;
static size_t retainedIndexBytes;
static ShadowGeometry shadowGeometry[CHUNKS_PER_AXIS * CHUNKS_PER_AXIS];
static size_t shadowGeometryCount;

static void refreshShadowGeometry(void) {
  shadowGeometryCount = 0;
  for (int x = 0; x < CHUNKS_PER_AXIS; x++)
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      RenderChunk* chunk = &renderChunks[x][z];
      if (chunk->indexCount)
        shadowGeometry[shadowGeometryCount++] = (ShadowGeometry){chunk->vao, (GLsizei)chunk->indexCount, chunk->indexType};
    }
}

static void initGrid(void) {
  float vertices[GRID_VERTICES * 3];
  int count = 0;
  float halfSize = WORLD_SIZE * CUBE_SIZE * 0.5f;
  for (int line = 0; line <= CHUNKS_PER_AXIS; line++) {
    float coord = line * CHUNK_SIZE * CUBE_SIZE - halfSize;
    const float endpoints[] = {coord, 0, -halfSize, coord, 0, halfSize, -halfSize, 0, coord, halfSize, 0, coord};
    memcpy(vertices + count, endpoints, sizeof(endpoints));
    count += 12;
  }

  glGenVertexArrays(1, &gridVAO);
  glGenBuffers(1, &gridVBO);
  glBindVertexArray(gridVAO);
  glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
}

static GLenum compactIndexType(ChunkMesh* mesh, size_t* bytes) {
  uint32_t maxIndex = 0;
  for (size_t i = 0; i < mesh->indexCount; i++)
    if (mesh->indices[i] > maxIndex)
      maxIndex = mesh->indices[i];

  if (maxIndex > UINT16_MAX) {
    *bytes = mesh->indexCount * sizeof(uint32_t);
    return GL_UNSIGNED_INT;
  }

  // Visibility and occluder metadata have already consumed the uint32_t mesh.
  // Compact forward into the same allocation with memcpy so C aliasing rules do
  // not depend on treating the uint32_t array as a uint16_t array.
  for (size_t i = 0; i < mesh->indexCount; i++) {
    uint16_t index = (uint16_t)mesh->indices[i];
    memcpy((unsigned char*)mesh->indices + i * sizeof(index), &index, sizeof(index));
  }

  *bytes = mesh->indexCount * sizeof(uint16_t);
  return GL_UNSIGNED_SHORT;
}

static bool uploadChunk(Chunk* chunk, RenderChunk* render, ChunkMesh* mesh, size_t* uploadedIndexBytes) {
  MeshVisibility visibility;
  if (!buildMeshVisibility(mesh, &visibility)) {
    freeChunkMesh(mesh);
    return false;
  }

  MeshOccluders occluders;
  buildMeshOccluders(mesh, &occluders);

  size_t indexBytes = 0;
  GLenum indexType = GL_UNSIGNED_INT;
  if (mesh->indexCount)
    indexType = compactIndexType(mesh, &indexBytes);

  if (mesh->indexCount) {
    if (!render->vao) {
      glGenVertexArrays(1, &render->vao);
      glGenBuffers(1, &render->vbo);
      glGenBuffers(1, &render->ebo);
    }

    glBindVertexArray(render->vao);
    glBindBuffer(GL_ARRAY_BUFFER, render->vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh->vertexCount * sizeof(MeshVertex), mesh->vertices, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBytes, mesh->indices, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, position));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, normal));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, uv));
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, material));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
  }

  if (glGetError() != GL_NO_ERROR) {
    freeMeshVisibility(&visibility);
    freeChunkMesh(mesh);
    return false;
  }

  render->surfaceBlocks = mesh->surfaceBlocks;
  vec3_add(&render->center, &mesh->min, &mesh->max);
  vec3_scale(&render->center, &render->center, 0.5f);
  vec3_subtract(&render->dimensions, &mesh->max, &mesh->min);
  render->indexCount = mesh->indexCount;
  render->occluders = occluders;
  if (mesh->indexCount) {
    retainedIndexBytes -= render->indexBytes;
    retainedIndexBytes += indexBytes;
    render->indexBytes = indexBytes;
    render->indexType = indexType;
    if (uploadedIndexBytes)
      *uploadedIndexBytes += indexBytes;
  }
  freeMeshVisibility(&render->visibility);
  render->visibility = visibility;
  freeChunkMesh(mesh);
  chunk->dirty = false;
  return true;
}

static bool updateDirtyChunks(RenderResult* result) {
  double start = glfwGetTime();
  for (int x = 0; x < CHUNKS_PER_AXIS; x++)
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Chunk* chunk = getChunk(&(Vec2i){x, z});
      if (!chunk)
        return false;
      if (!chunk->dirty)
        continue;
      ChunkMesh mesh;
      if (!buildChunkMesh(chunk, &mesh) || !uploadChunk(chunk, &renderChunks[x][z], &mesh, &result->indexBytesUploaded)) {
        fprintf(stderr, "Failed to rebuild chunk (%d, %d)\n", chunk->position.a, chunk->position.b);
        return false;
      }

      result->chunksRebuilt++;
    }

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  result->meshUpdateMilliseconds = (glfwGetTime() - start) * 1000.0;
  return true;
}

static GLint lightDirectionLocation, lightColorLocation, skyFillLocation, groundFillLocation;

bool setWorldRenderDistance(int chunks) {
  if (chunks < WORLD_RENDER_DISTANCE_MIN || chunks > WORLD_RENDER_DISTANCE_MAX)
    return false;
  if (chunks != renderDistance) {
    renderDistance = chunks;
    visibilityValid = false;
  }
  return true;
}

int getWorldRenderDistance(void) {
  return renderDistance;
}

void setWorldDayNight(const DayNightState* state) {
  shadowLight = state->lightDirection;
  GLint previous;
  glGetIntegerv(GL_CURRENT_PROGRAM, &previous);
  glUseProgram(program);
  glUniform3f(lightDirectionLocation, state->lightDirection.x, state->lightDirection.y, state->lightDirection.z);
  glUniform3f(lightColorLocation, state->lightColor.x, state->lightColor.y, state->lightColor.z);
  glUniform3f(skyFillLocation, state->skyFill.x, state->skyFill.y, state->skyFill.z);
  glUniform3f(groundFillLocation, state->groundFill.x, state->groundFill.y, state->groundFill.z);
  glUseProgram(previous);
}

bool initWorld(GLuint shaderProgram) {
  cleanupWorld();
  program = shaderProgram;
  // Base material layers 0..3 match MeshVertex.material; GLSL selects variants
  // 4..6 per voxel. Building materials occupy layers 7..9; oak logs/leaves use
  // the appended layers 10..12 without changing existing material IDs.
  const char* paths[] = {"assets/textures/stone.png",        "assets/textures/dirt.png",         "assets/textures/grass-top.png",
                         "assets/textures/grass-side.png",   "assets/textures/dirt-rocks.png",   "assets/textures/grass-top-leaves.png",
                         "assets/textures/grass-bug.png",    "assets/textures/cobblestone.png",  "assets/textures/oak-planks.png",
                         "assets/textures/stone-bricks.png", "assets/textures/oak-log-side.png", "assets/textures/oak-log-top.png",
                         "assets/textures/oak-leaves.png"};
  glActiveTexture(GL_TEXTURE0);
  textureArray = loadTextureArray(paths, (int)(sizeof(paths) / sizeof(paths[0])));
  if (!textureArray)
    goto failure;
  glUseProgram(program);
  viewProjectionLocation = glGetUniformLocation(program, "viewProjection");
  gridLocation = glGetUniformLocation(program, "drawGrid");
  lightDirectionLocation = glGetUniformLocation(program, "lightDirection");
  lightColorLocation = glGetUniformLocation(program, "lightColor");
  skyFillLocation = glGetUniformLocation(program, "skyColor");
  groundFillLocation = glGetUniformLocation(program, "groundColor");
  glUniform1i(glGetUniformLocation(program, "texture1"), 0);
  glUniform1ui(glGetUniformLocation(program, "worldSeed"), worldSeed());
  glUniform1f(glGetUniformLocation(program, "blockSize"), CUBE_SIZE);
  shadowLight = (Vec3){0.45f, 0.8f, 0.35f};
  shadowTransformLocation[0] = glGetUniformLocation(program, "shadowTransform");
  shadowTransformLocation[1] = glGetUniformLocation(program, "nextShadowTransform");
  shadowScaleLocation = glGetUniformLocation(program, "shadowScale");
  shadowBlendLocation = glGetUniformLocation(program, "shadowBlend");
  glUniform1i(glGetUniformLocation(program, "terrainShadow"), 1);
  glUniform1i(glGetUniformLocation(program, "nextTerrainShadow"), 2);
  float halfWidth = WORLD_SIZE * CUBE_SIZE * 0.5f;
  float halfHeight = CHUNK_HEIGHT * CUBE_SIZE * 0.5f;
  if (!initShadowCache(&shadows, (Vec3){0, halfHeight, 0}, sqrtf(2 * halfWidth * halfWidth + halfHeight * halfHeight) + CUBE_SIZE))
    goto failure;
  // Fixed lighting keeps the same face readable throughout the finite world.
  // The fill and diffuse intensities leave headroom for bright texture detail.
  glUniform3f(glGetUniformLocation(program, "lightDirection"), 0.45f, 0.8f, 0.35f);
  glUniform3f(glGetUniformLocation(program, "lightColor"), 0.62f, 0.60f, 0.56f);
  glUniform3f(glGetUniformLocation(program, "skyColor"), 0.36f, 0.39f, 0.44f);
  glUniform3f(glGetUniformLocation(program, "groundColor"), 0.18f, 0.16f, 0.14f);

  for (int x = 0; x < CHUNKS_PER_AXIS; x++) {
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Vec2i index = {x, z};
      Chunk* chunk = getChunk(&index);
      ChunkMesh mesh;
      if (!chunk || !buildChunkMesh(chunk, &mesh))
        goto failure;
      // initWorld may be called again after replacing the CPU world.
      if (!uploadChunk(chunk, &renderChunks[x][z], &mesh, NULL))
        goto failure;
    }
  }

  refreshShadowGeometry();

  initGrid();
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  if (glGetError() != GL_NO_ERROR)
    goto failure;
  return true;

failure:
  fprintf(stderr, "Failed to initialize world rendering\n");
  cleanupWorld();
  return false;
}

static bool candidateAxisBounds(float position, int* first, int* last) {
  const float chunkSpan = CHUNK_SIZE * CUBE_SIZE;
  const float halfWorld = WORLD_SIZE * CUBE_SIZE * 0.5f;
  const float radius = chunkSpan * renderDistance;
  if (isinf(position) || (isfinite(position) && (position < -halfWorld - radius || position > halfWorld + radius)))
    return false;
  if (isnan(position)) {
    *first = 0;
    *last = CHUNKS_PER_AXIS - 1;
    return true;
  }

  int center = (int)floorf(position / chunkSpan) + CHUNKS_PER_AXIS / 2;
  *first = center - renderDistance;
  *last = center + renderDistance;
  if (*first < 0)
    *first = 0;
  if (*last >= CHUNKS_PER_AXIS)
    *last = CHUNKS_PER_AXIS - 1;
  return *first <= *last;
}

static void sortCandidates(void) {
  for (int i = 1; i < candidateCount; i++) {
    ChunkCandidate candidate = candidates[i];
    int slot = i;
    while (slot > 0 && candidates[slot - 1].distanceSquared > candidate.distanceSquared) {
      candidates[slot] = candidates[slot - 1];
      slot--;
    }
    candidates[slot] = candidate;
  }
}

static void prepareVisibility(const Camera* camera, const Mat4 view, const Mat4 projection, const Mat4 viewProjection, bool wireframe, bool rebuilt, RenderResult* result) {
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  if (visibilityValid && !rebuilt && wireframe == cachedWireframe && !memcmp(cachedTransform, viewProjection, sizeof(Mat4)) &&
      !memcmp(cachedViewport, viewport, sizeof(viewport)) && camera->position.x == cachedPosition.x && camera->position.y == cachedPosition.y &&
      camera->position.z == cachedPosition.z) {
    result->visibilityCandidates = candidateCount;
    return;
  }
  memcpy(cachedTransform, viewProjection, sizeof(Mat4));
  memcpy(cachedViewport, viewport, sizeof(viewport));
  cachedPosition = camera->position;
  cachedWireframe = wireframe;
  visibilityValid = true;
  candidateCount = hiddenCount = 0;
  Frustum frustum;
  frustum_update(&frustum, projection, view);
  const float radius = CHUNK_SIZE * CUBE_SIZE * renderDistance;
  int firstX, lastX, firstZ, lastZ;
  if (!candidateAxisBounds(camera->position.x, &firstX, &lastX) || !candidateAxisBounds(camera->position.z, &firstZ, &lastZ))
    return;
  for (int x = firstX; x <= lastX; x++) {
    for (int z = firstZ; z <= lastZ; z++) {
      result->visibilityChunksScanned++;
      RenderChunk* chunk = &renderChunks[x][z];
      if (!chunk->surfaceBlocks)
        continue;
      // Use the chunk's horizontal center for the existing render-distance limit.
      float dx = (x - CHUNKS_PER_AXIS / 2 + 0.5f) * CHUNK_SIZE * CUBE_SIZE - camera->position.x;
      float dz = (z - CHUNKS_PER_AXIS / 2 + 0.5f) * CHUNK_SIZE * CUBE_SIZE - camera->position.z;
      if (dx * dx + dz * dz > radius * radius)
        continue;
      if (!frustum_block_visible(&frustum, &chunk->center, &chunk->dimensions, camera))
        continue;
      if (!meshVisibilityIntersects(&chunk->visibility, frustum.planes))
        continue;
      Vec3 offset;
      vec3_subtract(&offset, &chunk->center, &camera->position);
      dx = fmaxf(0, fabsf(offset.x) - chunk->dimensions.x * 0.5f);
      float dy = fmaxf(0, fabsf(offset.y) - chunk->dimensions.y * 0.5f);
      dz = fmaxf(0, fabsf(offset.z) - chunk->dimensions.z * 0.5f);
      float distance = dx * dx + dy * dy + dz * dz;
      candidates[candidateCount++] = (ChunkCandidate){chunk, distance, false};
    }
  }

  sortCandidates();
  result->visibilityCandidates = candidateCount;

  if (wireframe || candidateCount < 2)
    return;
  occlusionClear(&occlusion, viewProjection, viewport[2], viewport[3]);
  for (int i = 0; i < candidateCount; i++) {
    RenderChunk* chunk = candidates[i].chunk;
    Vec3 half, min, max;
    vec3_scale(&half, &chunk->dimensions, 0.5f);
    vec3_subtract(&min, &chunk->center, &half);
    vec3_add(&max, &chunk->center, &half);
    if (occlusionBoundsHidden(&occlusion, min, max)) {
      candidates[i].hidden = true;
      hiddenCount++;
      continue;
    }

    // Only retained chunks contribute occluders. All visibility work finishes
    // before terrain submission so CPU rasterization does not interrupt draws.
    if (i + 1 < candidateCount)
      for (int quad = 0; quad < chunk->occluders.count; quad++)
        occlusionRasterizeQuad(&occlusion, chunk->occluders.quads[quad].corners);
  }
}

RenderResult renderWorld(const Camera* camera, const Mat4 view, const Mat4 projection, bool wireframe) {
  RenderResult result = {0};
  if (!program || !updateDirtyChunks(&result))
    return result;
  if (result.chunksRebuilt)
    refreshShadowGeometry();
  if (!updateShadowCache(&shadows, shadowLight, result.chunksRebuilt != 0, shadowGeometry, shadowGeometryCount, &result.shadowDrawCalls))
    return result;
  result.success = true;
  Mat4 viewProjection;
  mat4_multiply(viewProjection, projection, view);
  prepareVisibility(camera, view, projection, viewProjection, wireframe, result.chunksRebuilt != 0, &result);
  result.chunksConsidered = CHUNKS_PER_AXIS * CHUNKS_PER_AXIS;
  result.chunksOccluded = hiddenCount;
  result.indexBytesRetained = retainedIndexBytes;

  glUseProgram(program);
  glUniformMatrix4fv(viewProjectionLocation, 1, GL_FALSE, viewProjection);
  GLint previousShadowTexture[2];
  for (int endpoint = 0; endpoint < 2; endpoint++) {
    ShadowMap* map = &shadows.maps[endpoint ? 1 - shadows.first : shadows.first];
    glUniformMatrix4fv(shadowTransformLocation[endpoint], 1, GL_FALSE, map->transform);
    glActiveTexture(GL_TEXTURE1 + endpoint);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousShadowTexture[endpoint]);
    glBindTexture(GL_TEXTURE_2D, map->depth);
  }
  glUniform2f(shadowScaleLocation, 1.0f / shadows.maps[0].size, 1.0f / (2 * shadows.maps[0].radius));
  glUniform1f(shadowBlendLocation, shadows.blend);
  glActiveTexture(GL_TEXTURE0);
  glUniform1i(gridLocation, 1);
  glBindVertexArray(gridVAO);
  glDrawArrays(GL_LINES, 0, GRID_VERTICES);
  glUniform1i(gridLocation, 0);

  GLint polygonMode[2];
  glGetIntegerv(GL_POLYGON_MODE, polygonMode);
  glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);

  // Free flight can place the camera inside terrain, so retain both sides.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);
  for (int i = 0; i < candidateCount; i++) {
    if (candidates[i].hidden)
      continue;
    RenderChunk* chunk = candidates[i].chunk;
    result.surfaceBlocks += chunk->surfaceBlocks;
    result.chunksRendered++;
    glBindVertexArray(chunk->vao);
    result.terrainDrawCalls++;
    result.submittedQuads += chunk->indexCount / 6;
    result.submittedTriangles += chunk->indexCount / 3;
    glDrawElements(GL_TRIANGLES, (GLsizei)chunk->indexCount, chunk->indexType, NULL);
  }

  glBindVertexArray(0);
  glPolygonMode(GL_FRONT, (GLenum)polygonMode[0]);
  glPolygonMode(GL_BACK, (GLenum)polygonMode[1]);
  for (int endpoint = 0; endpoint < 2; endpoint++) {
    glActiveTexture(GL_TEXTURE1 + endpoint);
    glBindTexture(GL_TEXTURE_2D, previousShadowTexture[endpoint]);
  }
  glActiveTexture(GL_TEXTURE0);
  return result;
}

void cleanupWorld(void) {
  cleanupShadowCache(&shadows);
  for (int x = 0; x < CHUNKS_PER_AXIS; x++) {
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      RenderChunk* chunk = &renderChunks[x][z];
      glDeleteVertexArrays(1, &chunk->vao);
      glDeleteBuffers(1, &chunk->vbo);
      glDeleteBuffers(1, &chunk->ebo);
      freeMeshVisibility(&chunk->visibility);
    }
  }

  memset(renderChunks, 0, sizeof(renderChunks));
  glDeleteVertexArrays(1, &gridVAO);
  glDeleteBuffers(1, &gridVBO);
  gridVAO = gridVBO = 0;
  glDeleteTextures(1, &textureArray);
  textureArray = 0;
  program = 0;
  visibilityValid = false;
  candidateCount = hiddenCount = 0;
  shadowGeometryCount = 0;
  retainedIndexBytes = 0;
}
