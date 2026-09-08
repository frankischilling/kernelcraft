#include "../world/mesh.h"
#include "../world/world.h"
#include "texture.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  GLuint vao, vbo, ebo;
  MeshBatch batches[MATERIAL_COUNT];
  Vec3 center, dimensions;
  int surfaceBlocks;
} RenderChunk;

static RenderChunk renderChunks[CHUNKS_PER_AXIS][CHUNKS_PER_AXIS];
static GLuint textures[MATERIAL_COUNT];
static GLuint program, gridVAO, gridVBO;
static GLint viewProjectionLocation, viewPositionLocation, gridLocation;
enum { GRID_VERTICES = (CHUNKS_PER_AXIS + 1) * 4 };

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

bool initWorld(GLuint shaderProgram) {
  cleanupWorld();
  program = shaderProgram;
  const char* paths[MATERIAL_COUNT] = {"assets/textures/stone.png", "assets/textures/dirt.png", "assets/textures/grass-top.png", "assets/textures/grass-side.png"};
  glActiveTexture(GL_TEXTURE0);
  for (int i = 0; i < MATERIAL_COUNT; i++) {
    textures[i] = loadTexture(paths[i]);
    if (!textures[i])
      goto failure;
  }
  glUseProgram(program);
  viewProjectionLocation = glGetUniformLocation(program, "viewProjection");
  viewPositionLocation = glGetUniformLocation(program, "viewPos");
  gridLocation = glGetUniformLocation(program, "drawGrid");
  glUniform1i(glGetUniformLocation(program, "texture1"), 0);
  glUniform3f(glGetUniformLocation(program, "lightPos"), 5.0f, 50.0f, 5.0f);
  glUniform3f(glGetUniformLocation(program, "lightColor"), 1.0f, 1.0f, 1.0f);

  for (int x = 0; x < CHUNKS_PER_AXIS; x++) {
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Vec2i index = {x, z};
      Chunk* chunk = getChunk(&index);
      ChunkMesh mesh;
      if (!chunk || !buildChunkMesh(chunk, &mesh))
        goto failure;
      RenderChunk* render = &renderChunks[x][z];
      render->surfaceBlocks = mesh.surfaceBlocks;
      vec3_add(&render->center, &mesh.min, &mesh.max);
      vec3_scale(&render->center, &render->center, 0.5f);
      vec3_subtract(&render->dimensions, &mesh.max, &mesh.min);
      memcpy(render->batches, mesh.batches, sizeof(render->batches));
      if (mesh.indexCount) {
        glGenVertexArrays(1, &render->vao);
        glGenBuffers(1, &render->vbo);
        glGenBuffers(1, &render->ebo);
        glBindVertexArray(render->vao);
        glBindBuffer(GL_ARRAY_BUFFER, render->vbo);
        glBufferData(GL_ARRAY_BUFFER, mesh.vertexCount * sizeof(MeshVertex), mesh.vertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, render->ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indexCount * sizeof(uint32_t), mesh.indices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, position));
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, normal));
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, uv));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
      }
      freeChunkMesh(&mesh);
      if (glGetError() != GL_NO_ERROR)
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

RenderResult renderWorld(const Camera* camera, const Mat4 view, const Mat4 projection) {
  RenderResult result = {0};
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

  // Free flight can place the camera inside terrain, so retain both sides.
  glActiveTexture(GL_TEXTURE0);
  const float radius = CHUNK_SIZE * CUBE_SIZE * 4.0f / 2.0f;
  RenderChunk* visible[CHUNKS_PER_AXIS * CHUNKS_PER_AXIS];
  int visibleCount = 0;
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
      visible[visibleCount++] = chunk;
      result.visisbleCubes += chunk->surfaceBlocks;
    }
  }
  for (int material = 0; material < MATERIAL_COUNT; material++) {
    glBindTexture(GL_TEXTURE_2D, textures[material]);
    for (int i = 0; i < visibleCount; i++) {
      RenderChunk* chunk = visible[i];
      MeshBatch batch = chunk->batches[material];
      if (!batch.indexCount)
        continue;
      glBindVertexArray(chunk->vao);
      glDrawElements(GL_TRIANGLES, (GLsizei)batch.indexCount, GL_UNSIGNED_INT, (void*)(uintptr_t)(batch.firstIndex * sizeof(uint32_t)));
    }
  }
  glBindVertexArray(0);
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
  glDeleteTextures(MATERIAL_COUNT, textures);
  memset(textures, 0, sizeof(textures));
  program = 0;
}
