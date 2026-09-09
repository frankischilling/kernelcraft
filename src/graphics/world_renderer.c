#include "world_renderer.h"
#include "frustum.h"
#include "../world/mesh.h"
#include "../world/world.h"
#include "texture.h"
#include <GLFW/glfw3.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  GLuint vao, vbo, ebo;
  size_t indexCount;
  Vec3 center, dimensions;
  int surfaceBlocks;
} RenderChunk;

static RenderChunk renderChunks[CHUNKS_PER_AXIS][CHUNKS_PER_AXIS];
static GLuint textureArray;
static GLuint program, gridVAO, gridVBO;
static GLint viewProjectionLocation, viewPositionLocation, gridLocation;
enum { GRID_VERTICES = (CHUNKS_PER_AXIS + 1) * 4, RENDER_RADIUS_CHUNKS = 6 };

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
    freeChunkMesh(mesh);
    return false;
  }
  render->surfaceBlocks = mesh->surfaceBlocks;
  vec3_add(&render->center, &mesh->min, &mesh->max);
  vec3_scale(&render->center, &render->center, 0.5f);
  vec3_subtract(&render->dimensions, &mesh->max, &mesh->min);
  render->indexCount = mesh->indexCount;
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

bool initWorld(GLuint shaderProgram) {
  cleanupWorld();
  program = shaderProgram;
  // Base material layers 0..3 match MeshVertex.material; GLSL selects variants
  // 4..6 per voxel. Cobblestone occupies layer 7 on every face.
  const char* paths[] = {"assets/textures/stone.png",      "assets/textures/dirt.png",       "assets/textures/grass-top.png",
                         "assets/textures/grass-side.png", "assets/textures/dirt-rocks.png", "assets/textures/grass-top-leaves.png",
                         "assets/textures/grass-bug.png",  "assets/textures/cobblestone.png"};
  glActiveTexture(GL_TEXTURE0);
  textureArray = loadTextureArray(paths, (int)(sizeof(paths) / sizeof(paths[0])));
  if (!textureArray)
    goto failure;
  glUseProgram(program);
  viewProjectionLocation = glGetUniformLocation(program, "viewProjection");
  viewPositionLocation = glGetUniformLocation(program, "viewPos");
  gridLocation = glGetUniformLocation(program, "drawGrid");
  glUniform1i(glGetUniformLocation(program, "texture1"), 0);
  glUniform1ui(glGetUniformLocation(program, "worldSeed"), worldSeed());
  glUniform1f(glGetUniformLocation(program, "blockSize"), CUBE_SIZE);
  glUniform3f(glGetUniformLocation(program, "lightPos"), 5.0f, 50.0f, 5.0f);
  glUniform3f(glGetUniformLocation(program, "lightColor"), 1.0f, 1.0f, 1.0f);

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

RenderResult renderWorld(const Camera* camera, const Mat4 view, const Mat4 projection, bool wireframe) {
  RenderResult result = {0};
  if (!program || !updateDirtyChunks(&result))
    return result;
  result.success = true;
  Mat4 viewProjection;
  mat4_multiply(viewProjection, projection, view);
  Frustum frustum;
  frustum_update(&frustum, projection, view);

  glUseProgram(program);
  glUniformMatrix4fv(viewProjectionLocation, 1, GL_FALSE, viewProjection);
  glUniform3f(viewPositionLocation, camera->position.x, camera->position.y, camera->position.z);
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
  const float radius = CHUNK_SIZE * CUBE_SIZE * RENDER_RADIUS_CHUNKS;
  for (int x = 0; x < CHUNKS_PER_AXIS; x++) {
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      RenderChunk* chunk = &renderChunks[x][z];
      result.chunksConsidered++;
      if (!chunk->surfaceBlocks)
        continue;
      // Use the chunk's horizontal center for the existing render-distance limit.
      float dx = (x - CHUNKS_PER_AXIS / 2 + 0.5f) * CHUNK_SIZE * CUBE_SIZE - camera->position.x;
      float dz = (z - CHUNKS_PER_AXIS / 2 + 0.5f) * CHUNK_SIZE * CUBE_SIZE - camera->position.z;
      if (dx * dx + dz * dz > radius * radius)
        continue;
      if (!frustum_block_visible(&frustum, &chunk->center, &chunk->dimensions, camera))
        continue;
      result.surfaceBlocks += chunk->surfaceBlocks;
      result.chunksRendered++;
      glBindVertexArray(chunk->vao);
      result.terrainDrawCalls++;
      result.submittedQuads += chunk->indexCount / 6;
      result.submittedTriangles += chunk->indexCount / 3;
      glDrawElements(GL_TRIANGLES, (GLsizei)chunk->indexCount, GL_UNSIGNED_INT, NULL);
    }
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
    }
  }
  memset(renderChunks, 0, sizeof(renderChunks));
  glDeleteVertexArrays(1, &gridVAO);
  glDeleteBuffers(1, &gridVBO);
  gridVAO = gridVBO = 0;
  glDeleteTextures(1, &textureArray);
  textureArray = 0;
  program = 0;
}
