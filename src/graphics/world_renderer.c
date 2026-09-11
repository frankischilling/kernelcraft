#include "world_renderer.h"
#include "frustum.h"
#include "../world/mesh.h"
#include "../world/occlusion.h"
#include "../world/mesh_visibility.h"
#include "../world/world.h"
#include "shader.h"
#include "texture.h"
#include <GLFW/glfw3.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  GLuint vao, vbo, ebo;
  size_t indexCount;
  Vec3 center, dimensions;
  int surfaceBlocks;
  MeshOccluders occluders;
  MeshVisibility visibility;
} RenderChunk;

static RenderChunk renderChunks[CHUNKS_PER_AXIS][CHUNKS_PER_AXIS];
static GLuint textureArray;
static GLuint program, gridVAO, gridVBO, shadowProgram, shadowFramebuffer, shadowDepthTexture;
static GLint viewProjectionLocation, gridLocation, lightSpaceMatrixLocation, shadowMapLocation, shadowTexelSizeLocation;
static GLint shadowViewProjectionLocation;

enum { GRID_VERTICES = (CHUNKS_PER_AXIS + 1) * 4, RENDER_RADIUS_CHUNKS = 6, SHADOW_MAP_SIZE = 2048 };
#define SHADOW_ORTHOGRAPHIC_HALF_SIZE 192.0f
#define SHADOW_LIGHT_DISTANCE 320.0f
#define SHADOW_NEAR_PLANE 0.1f
#define SHADOW_FAR_PLANE 700.0f
#define SHADOW_DIRECTION_EPSILON 0.005f

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

static bool uploadChunk(Chunk* chunk, RenderChunk* render, ChunkMesh* mesh) {
  MeshVisibility visibility;
  if (!buildMeshVisibility(mesh, &visibility)) {
    freeChunkMesh(mesh);
    return false;
  }

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
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indexCount * sizeof(uint32_t), mesh->indices, GL_DYNAMIC_DRAW);
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
  buildMeshOccluders(mesh, &render->occluders);
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
      if (!buildChunkMesh(chunk, &mesh) || !uploadChunk(chunk, &renderChunks[x][z], &mesh)) {
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
static const Vec3 defaultLightDirection = {0.45f, 0.8f, 0.35f};
static Vec3 currentLightDirection = {0.45f, 0.8f, 0.35f};
static Vec3 shadowMapDirection;
static Mat4 shadowTransform;
static bool shadowMapValid, shadowMapDirty = true, shadowMapDirectionValid;

static bool lightDirectionChanged(Vec3 a, Vec3 b) {
  float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
  return dx * dx + dy * dy + dz * dz > SHADOW_DIRECTION_EPSILON * SHADOW_DIRECTION_EPSILON;
}

static bool initShadowResources(void) {
  GLint previousDrawFramebuffer, previousReadFramebuffer;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFramebuffer);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
  shadowProgram = loadShaders("assets/shaders/shadow_vertex.glsl", "assets/shaders/shadow_fragment.glsl");
  if (!shadowProgram)
    return false;

  glGenTextures(1, &shadowDepthTexture);
  glBindTexture(GL_TEXTURE_2D, shadowDepthTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
  const GLfloat border[] = {1, 1, 1, 1};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

  glGenFramebuffers(1, &shadowFramebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepthTexture, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  bool complete = shadowDepthTexture && shadowFramebuffer && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, previousDrawFramebuffer);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, previousReadFramebuffer);
  if (!complete) {
    fprintf(stderr, "Failed to create terrain shadow framebuffer\n");
    return false;
  }

  shadowViewProjectionLocation = glGetUniformLocation(shadowProgram, "lightSpaceMatrix");
  return shadowViewProjectionLocation >= 0 && glGetError() == GL_NO_ERROR;
}

static void buildShadowTransform(Vec3 lightDirection) {
  Vec3 direction;
  vec3_normalize(&direction, &lightDirection);
  Vec3 focus = {0, CHUNK_HEIGHT * CUBE_SIZE * 0.5f, 0};
  Vec3 offset;
  vec3_scale(&offset, &direction, SHADOW_LIGHT_DISTANCE);
  Vec3 eye;
  vec3_add(&eye, &focus, &offset);
  Vec3 up = fabsf(direction.y) > 0.95f ? (Vec3){0, 0, 1} : (Vec3){0, 1, 0};
  Mat4 view, projection;
  mat4_lookAt(view, &eye, &focus, &up);
  mat4_orthographic(projection, -SHADOW_ORTHOGRAPHIC_HALF_SIZE, SHADOW_ORTHOGRAPHIC_HALF_SIZE, -SHADOW_ORTHOGRAPHIC_HALF_SIZE,
                    SHADOW_ORTHOGRAPHIC_HALF_SIZE, SHADOW_NEAR_PLANE, SHADOW_FAR_PLANE);
  mat4_multiply(shadowTransform, projection, view);
}

static bool refreshShadowMap(RenderResult* result, bool geometryChanged) {
  shadowMapDirty |= geometryChanged;
  if (shadowMapValid && shadowMapDirectionValid && !shadowMapDirty && !lightDirectionChanged(currentLightDirection, shadowMapDirection))
    return true;

  buildShadowTransform(currentLightDirection);
  GLint previousProgram, previousVAO, previousDrawFramebuffer, previousReadFramebuffer, previousDepthFunction, previousViewport[4], previousCullFace;
  GLint previousPolygonMode[2];
  GLfloat previousPolygonOffsetFactor, previousPolygonOffsetUnits, previousClearDepth;
  GLboolean previousDepthMask;
  GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST), blendEnabled = glIsEnabled(GL_BLEND), cullEnabled = glIsEnabled(GL_CULL_FACE);
  GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST), polygonOffsetEnabled = glIsEnabled(GL_POLYGON_OFFSET_FILL);
  glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFramebuffer);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
  glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunction);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
  glGetIntegerv(GL_VIEWPORT, previousViewport);
  glGetIntegerv(GL_CULL_FACE_MODE, &previousCullFace);
  glGetIntegerv(GL_POLYGON_MODE, previousPolygonMode);
  glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &previousPolygonOffsetFactor);
  glGetFloatv(GL_POLYGON_OFFSET_UNITS, &previousPolygonOffsetUnits);
  glGetFloatv(GL_DEPTH_CLEAR_VALUE, &previousClearDepth);

  glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer);
  glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(2.0f, 4.0f);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glClearDepth(1.0);
  glClear(GL_DEPTH_BUFFER_BIT);
  glUseProgram(shadowProgram);
  glUniformMatrix4fv(shadowViewProjectionLocation, 1, GL_FALSE, shadowTransform);
  for (int x = 0; x < CHUNKS_PER_AXIS; x++)
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      RenderChunk* chunk = &renderChunks[x][z];
      if (!chunk->indexCount)
        continue;
      glBindVertexArray(chunk->vao);
      glDrawElements(GL_TRIANGLES, (GLsizei)chunk->indexCount, GL_UNSIGNED_INT, NULL);
      result->shadowDrawCalls++;
    }

  glBindVertexArray(previousVAO);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, previousDrawFramebuffer);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, previousReadFramebuffer);
  glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
  glUseProgram(previousProgram);
  glDepthFunc(previousDepthFunction);
  glDepthMask(previousDepthMask);
  if (depthEnabled)
    glEnable(GL_DEPTH_TEST);
  else
    glDisable(GL_DEPTH_TEST);
  if (blendEnabled)
    glEnable(GL_BLEND);
  else
    glDisable(GL_BLEND);
  if (cullEnabled)
    glEnable(GL_CULL_FACE);
  else
    glDisable(GL_CULL_FACE);
  glCullFace((GLenum)previousCullFace);
  if (scissorEnabled)
    glEnable(GL_SCISSOR_TEST);
  else
    glDisable(GL_SCISSOR_TEST);
  if (polygonOffsetEnabled)
    glEnable(GL_POLYGON_OFFSET_FILL);
  else
    glDisable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(previousPolygonOffsetFactor, previousPolygonOffsetUnits);
  glClearDepth(previousClearDepth);
  glPolygonMode(GL_FRONT, (GLenum)previousPolygonMode[0]);
  glPolygonMode(GL_BACK, (GLenum)previousPolygonMode[1]);
  if (glGetError() != GL_NO_ERROR) {
    fprintf(stderr, "Failed to render terrain shadow map\n");
    shadowMapValid = false;
    return false;
  }

  shadowMapDirection = currentLightDirection;
  shadowMapDirectionValid = true;
  shadowMapDirty = false;
  shadowMapValid = true;
  return true;
}

void setWorldDayNight(const DayNightState* state) {
  if (!state)
    return;
  Vec3 lightDirection = state->lightDirection;
  float lengthSquared = lightDirection.x * lightDirection.x + lightDirection.y * lightDirection.y + lightDirection.z * lightDirection.z;
  if (!isfinite(lengthSquared) || lengthSquared < 0.000001f)
    lightDirection = defaultLightDirection;
  currentLightDirection = lightDirection;
  shadowMapDirty |= !shadowMapDirectionValid || lightDirectionChanged(currentLightDirection, shadowMapDirection);
  GLint previous;
  glGetIntegerv(GL_CURRENT_PROGRAM, &previous);
  glUseProgram(program);
  glUniform3f(lightDirectionLocation, lightDirection.x, lightDirection.y, lightDirection.z);
  glUniform3f(lightColorLocation, state->lightColor.x, state->lightColor.y, state->lightColor.z);
  glUniform3f(skyFillLocation, state->skyFill.x, state->skyFill.y, state->skyFill.z);
  glUniform3f(groundFillLocation, state->groundFill.x, state->groundFill.y, state->groundFill.z);
  glUseProgram(previous);
}

bool initWorld(GLuint shaderProgram) {
  cleanupWorld();
  program = shaderProgram;
  currentLightDirection = defaultLightDirection;
  // Base material layers 0..3 match MeshVertex.material; GLSL selects variants
  // 4..6 per voxel. Building materials occupy layers 7..9 on every face.
  const char* paths[] = {"assets/textures/stone.png",       "assets/textures/dirt.png",        "assets/textures/grass-top.png",
                         "assets/textures/grass-side.png",  "assets/textures/dirt-rocks.png",  "assets/textures/grass-top-leaves.png",
                         "assets/textures/grass-bug.png",   "assets/textures/cobblestone.png", "assets/textures/oak-planks.png",
                         "assets/textures/stone-bricks.png"};
  glActiveTexture(GL_TEXTURE0);
  textureArray = loadTextureArray(paths, (int)(sizeof(paths) / sizeof(paths[0])));
  if (!textureArray)
    goto failure;
  glUseProgram(program);
  viewProjectionLocation = glGetUniformLocation(program, "viewProjection");
  gridLocation = glGetUniformLocation(program, "drawGrid");
  lightSpaceMatrixLocation = glGetUniformLocation(program, "lightSpaceMatrix");
  shadowMapLocation = glGetUniformLocation(program, "shadowMap");
  shadowTexelSizeLocation = glGetUniformLocation(program, "shadowMapTexelSize");
  lightDirectionLocation = glGetUniformLocation(program, "lightDirection");
  lightColorLocation = glGetUniformLocation(program, "lightColor");
  skyFillLocation = glGetUniformLocation(program, "skyColor");
  groundFillLocation = glGetUniformLocation(program, "groundColor");
  glUniform1i(glGetUniformLocation(program, "texture1"), 0);
  glUniform1ui(glGetUniformLocation(program, "worldSeed"), worldSeed());
  glUniform1f(glGetUniformLocation(program, "blockSize"), CUBE_SIZE);
  glUniform1i(shadowMapLocation, 1);
  glUniform1f(shadowTexelSizeLocation, 1.0f / SHADOW_MAP_SIZE);
  // Fixed lighting keeps the same face readable throughout the finite world.
  // The fill and diffuse intensities leave headroom for bright texture detail.
  glUniform3f(glGetUniformLocation(program, "lightDirection"), 0.45f, 0.8f, 0.35f);
  glUniform3f(glGetUniformLocation(program, "lightColor"), 0.62f, 0.60f, 0.56f);
  glUniform3f(glGetUniformLocation(program, "skyColor"), 0.36f, 0.39f, 0.44f);
  glUniform3f(glGetUniformLocation(program, "groundColor"), 0.18f, 0.16f, 0.14f);
  if (!initShadowResources())
    goto failure;

  for (int x = 0; x < CHUNKS_PER_AXIS; x++) {
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Vec2i index = {x, z};
      Chunk* chunk = getChunk(&index);
      ChunkMesh mesh;
      if (!chunk || !buildChunkMesh(chunk, &mesh))
        goto failure;
      // initWorld may be called again after replacing the CPU world.
      if (!uploadChunk(chunk, &renderChunks[x][z], &mesh))
        goto failure;
    }
  }

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

static void prepareVisibility(const Camera* camera, const Mat4 view, const Mat4 projection, const Mat4 viewProjection, bool wireframe, bool rebuilt) {
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  if (visibilityValid && !rebuilt && wireframe == cachedWireframe && !memcmp(cachedTransform, viewProjection, sizeof(Mat4)) &&
      !memcmp(cachedViewport, viewport, sizeof(viewport)) && camera->position.x == cachedPosition.x && camera->position.y == cachedPosition.y &&
      camera->position.z == cachedPosition.z)
    return;
  memcpy(cachedTransform, viewProjection, sizeof(Mat4));
  memcpy(cachedViewport, viewport, sizeof(viewport));
  cachedPosition = camera->position;
  cachedWireframe = wireframe;
  visibilityValid = true;
  candidateCount = hiddenCount = 0;
  Frustum frustum;
  frustum_update(&frustum, projection, view);
  const float radius = CHUNK_SIZE * CUBE_SIZE * RENDER_RADIUS_CHUNKS;
  for (int x = 0; x < CHUNKS_PER_AXIS; x++) {
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
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
      int index = candidateCount++;
      while (index > 0 && candidates[index - 1].distanceSquared > distance) {
        candidates[index] = candidates[index - 1];
        index--;
      }

      candidates[index] = (ChunkCandidate){chunk, distance, false};
    }
  }

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
  if (!refreshShadowMap(&result, result.chunksRebuilt != 0))
    return result;
  result.success = true;
  Mat4 viewProjection;
  mat4_multiply(viewProjection, projection, view);
  prepareVisibility(camera, view, projection, viewProjection, wireframe, result.chunksRebuilt != 0);
  result.chunksConsidered = CHUNKS_PER_AXIS * CHUNKS_PER_AXIS;
  result.chunksOccluded = hiddenCount;

  glUseProgram(program);
  glUniformMatrix4fv(viewProjectionLocation, 1, GL_FALSE, viewProjection);
  glUniformMatrix4fv(lightSpaceMatrixLocation, 1, GL_FALSE, shadowTransform);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, shadowDepthTexture);
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
    glDrawElements(GL_TRIANGLES, (GLsizei)chunk->indexCount, GL_UNSIGNED_INT, NULL);
  }

  glBindVertexArray(0);
  glPolygonMode(GL_FRONT, (GLenum)polygonMode[0]);
  glPolygonMode(GL_BACK, (GLenum)polygonMode[1]);
  return result;
}

void cleanupWorld(void) {
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
  glDeleteFramebuffers(1, &shadowFramebuffer);
  glDeleteTextures(1, &shadowDepthTexture);
  if (shadowProgram)
    glDeleteProgram(shadowProgram);
  shadowFramebuffer = shadowDepthTexture = shadowProgram = 0;
  shadowMapValid = false;
  shadowMapDirty = true;
  shadowMapDirectionValid = false;
  currentLightDirection = defaultLightDirection;
  memset(shadowTransform, 0, sizeof(shadowTransform));
  program = 0;
  visibilityValid = false;
  candidateCount = hiddenCount = 0;
}
