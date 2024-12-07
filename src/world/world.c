/**
 * @file world/world.c
 * @brief World generation and rendering.
 * @author frankischilling, VladimirJanus
 * @version 0.1
 * @date 2024-11-19
 *
 */
#include <GL/glew.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "world.h"
#include "../graphics/camera.h"
#include "../graphics/frustum.h"
#include "../graphics/shader.h"
#include "../graphics/texture.h"
#include "../math/math.h"
#include "cube.h"
#include "chunk.h"
static GLuint stoneTexture, dirtTexture, grassTopTexture, grassSideTexture;

// 2d array of chunk pointers
Chunk*** chunks = NULL;
//                    index I      J coord X      Z
// FIRST  QUADRANT chunks[8 ,15][8 ,15] [0 , 7][0 , 7]
// SECOND QUADRANT chunks[8 ,15][0 , 7] [0 , 7][-8,-1]
// THIRD  QUADRANT chunks[0 , 7][0 , 7] [-8,-1][-8,-1]
// FOURTH QUADRANT chunks[0 , 7][8 ,15] [-8,-1][0 , 7]

static GLuint VBO, VAO;

// Chunk functions
void initChunks() {
  // Calculate how many chunks fit into the world

  // Allocate memory for chunks
  chunks = (Chunk***)malloc(CHUNKS_PER_AXIS * sizeof(Chunk**)); // Allocate memory for the array of Chunk* pointers
  for (int chunkI = 0; chunkI < CHUNKS_PER_AXIS; chunkI++) {
    chunks[chunkI] = (Chunk**)malloc(CHUNKS_PER_AXIS * sizeof(Chunk*)); // Allocate memory for each row of Chunk* pointers
    for (int chunkJ = 0; chunkJ < CHUNKS_PER_AXIS; chunkJ++) {
      Chunk* chunk = (Chunk*)malloc(sizeof(Chunk)); // allocate memory for single chunk

      chunk->position.a = (chunkI - CHUNKS_PER_AXIS / 2);
      chunk->position.b = (chunkJ - CHUNKS_PER_AXIS / 2);

      for (int i = 0; i < CHUNK_SIZE; i++) {
        for (int k = 0; k < CHUNK_SIZE; k++) {
          float worldX = chunk->position.a * 16 + i * CUBE_SIZE;
          float worldZ = chunk->position.b * 16 + k * CUBE_SIZE;
          int height = (int)floor(getTerrainHeight(worldX, worldZ));
          for (int j = 0; j < CHUNK_HEIGHT; j++) {
            if (j < height - DIRT_LAYERS) {
              chunk->blocks[i][j][k] = (Block){BLOCK_STONE, false, {0, 0, 0, 0, 0, 0}};
            } else if (j < height) {
              chunk->blocks[i][j][k] = (Block){BLOCK_DIRT, false, {0, 0, 0, 0, 0, 0}};
            } else if (j == (int)height) {
              chunk->blocks[i][j][k] = (Block){BLOCK_GRASS, false, {0, 0, 0, 0, 0, 0}};
            } else {
              chunk->blocks[i][j][k] = (Block){BLOCK_AIR, false, {0, 0, 0, 0, 0, 0}};
            }
          }
        }
      }

      chunks[chunkI][chunkJ] = chunk;
    }
  }
}
void renderChunkGrid(GLuint shaderProgram, const Camera* camera) {
  static GLuint gridVAO = 0;
  static GLuint gridVBO = 0;

  // Initialize grid buffers if not already done
  if (gridVAO == 0) {
    // Create vertices for grid lines
    float* vertices = malloc(sizeof(float) * 6 * (WORLD_SIZE + WORLD_SIZE));
    int vertexCount = 0;

    // Calculate offset to center the grid
    float offsetX = -WORLD_SIZE / 2.0f;
    float offsetZ = -WORLD_SIZE / 2.0f;

    // Vertical lines
    for (int x = 0; x <= WORLD_SIZE; x += CHUNK_SIZE) {
      float worldX = x + offsetX;
      vertices[vertexCount++] = worldX;
      vertices[vertexCount++] = 0.0f;
      vertices[vertexCount++] = offsetZ;

      vertices[vertexCount++] = worldX;
      vertices[vertexCount++] = 0.0f;
      vertices[vertexCount++] = WORLD_SIZE + offsetZ;
    }

    // Horizontal lines
    for (int z = 0; z <= WORLD_SIZE; z += CHUNK_SIZE) {
      float worldZ = z + offsetZ;
      vertices[vertexCount++] = offsetX;
      vertices[vertexCount++] = 0.0f;
      vertices[vertexCount++] = worldZ;

      vertices[vertexCount++] = WORLD_SIZE + offsetX;
      vertices[vertexCount++] = 0.0f;
      vertices[vertexCount++] = worldZ;
    }

    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);

    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertexCount, vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    free(vertices);
  }

  glUseProgram(shaderProgram);

  // Set grid color (white)
  glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), 0.3f, 0.3f, 0.3f);

  // Calculate view and projection matrices
  Mat4 view, projection;
  Vec3 target;
  vec3_add(&target, &camera->position, &camera->front);
  mat4_lookAt(view, &camera->position, &target, &camera->up);
  mat4_perspective(projection, 70.0f, 1920.0f / 1080.0f, 0.1f, 1000.0f);

  // Set matrices in shader
  glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, view);
  glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, projection);

  Mat4 model;
  mat4_identity(model);
  glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, model);

  // Draw grid
  glBindVertexArray(gridVAO);
  glDrawArrays(GL_LINES, 0, (WORLD_SIZE / CHUNK_SIZE * 2 + 2) * 2);
  glBindVertexArray(0);
}
void cleanupChunks() {
  for (int x = 0; x < CHUNKS_PER_AXIS; x++) {
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      free(chunks[x][z]);
    }
    free(chunks[x]);
  }
  free(chunks);
}

static const GLfloat cubeVerticesWithNormals[] = {
    // Front face
    // Positions          // Normals           // Texture Coords
    // Good look reading this LMAO
    -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f,
    1.0f, 1.0f, 0.0f, -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,

    // Back face
    0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, -0.5f, 0.5f, -0.5f,
    0.0f, 0.0f, -1.0f, 1.0f, 0.0f, -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,

    // Top face
    -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 0.0f, 1.0f,
    0.0f, 1.0f, 1.0f, -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,

    // Bottom face
    -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.5f, -0.5f, 0.5f,
    0.0f, -1.0f, 0.0f, 1.0f, 0.0f, -0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f,

    // Right face
    0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,

    // Left face
    -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, -0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.5f, 0.5f, 0.5f,
    -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f};

static BiomeParameters biomeParameters[] = { // Plains biome - flatter, lower amplitude
    {0.03f, 0.5f, 0.3f, 4.0f},               // Lower frequency and amplitude for flatter terrain
                                             // Hills biome - more varied, higher amplitude
    {0.1f, 1.2f, 0.5f, 12.0f}};
static float getBiomeBlendFactor(float x, float z) {
  // Use a different noise frequency for biome transitions
  float biomeNoise = perlin(x * 0.02f, 0, z * 0.02f);
  return smoothstep(0.4f, 0.6f, biomeNoise);
}
BiomeParameters getInterpolatedBiomeParameters(float x, float z) {
  float blendFactor = getBiomeBlendFactor(x, z);
  BiomeParameters result;

  result.frequency = lerp(biomeParameters[BIOME_PLAINS].frequency, biomeParameters[BIOME_HILLS].frequency, blendFactor);
  result.amplitude = lerp(biomeParameters[BIOME_PLAINS].amplitude, biomeParameters[BIOME_HILLS].amplitude, blendFactor);
  result.persistence = lerp(biomeParameters[BIOME_PLAINS].persistence, biomeParameters[BIOME_HILLS].persistence, blendFactor);
  result.heightScale = lerp(biomeParameters[BIOME_PLAINS].heightScale, biomeParameters[BIOME_HILLS].heightScale, blendFactor);

  return result;
}
// Add this function to get height based on biome
float getTerrainHeight(float x, float z) {
  BiomeParameters params = getInterpolatedBiomeParameters(x, z);
  float height = 0.0f;
  float amplitude = params.amplitude;
  float frequency = params.frequency;

  // Use more octaves for more detailed terrain
  for (int i = 0; i < 4; i++) {
    height += perlin(x * frequency, 0, z * frequency) * amplitude;
    amplitude *= params.persistence;
    frequency *= 2.0f;
  }

  return height * params.heightScale;
}
const char* getCurrentBiomeText(float x, float z) {
  float blendFactor = getBiomeBlendFactor(x, z);
  if (blendFactor < 0.4f) {
    return "Plains";
  } else if (blendFactor > 0.6f) {
    return "Hills";
  } else {
    return "Transition"; // Optional: show when we're between biomes
  }
}

void initWorld() {
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindVertexArray(VAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerticesWithNormals), cubeVerticesWithNormals, GL_STATIC_DRAW);

  // Position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)0);
  glEnableVertexAttribArray(0);

  // Normal attribute
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
  glEnableVertexAttribArray(1);

  // Texture coordinate attribute
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
  glEnableVertexAttribArray(2);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  // Load textures
  stoneTexture = loadTexture("assets/textures/stone.png");
  dirtTexture = loadTexture("assets/textures/dirt.png");
  grassTopTexture = loadTexture("assets/textures/grass-top.png");
  grassSideTexture = loadTexture("assets/textures/grass-side.png");
}

RenderResult renderWorld(GLuint shaderProgram, const Camera* camera) {
  int visibleCubes = 0; // Reset counter

  // Create and update frustum
  Frustum frustum;
  Mat4 projection, view;
  mat4_perspective(projection, 70.0f, 1920.0f / 1080.0f, 0.1f, 1000.0f);

  Vec3 target;
  vec3_add(&target, &camera->position, &camera->front);
  mat4_lookAt(view, &camera->position, &target, &camera->up);

  frustum_update(&frustum, projection, view);

  // Set light properties
  Vec3 lightPos = {5.0f, 50.0f, 5.0f};
  Vec3 lightColor = {1.0f, 1.0f, 1.0f};

  glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, (float*)&lightPos);
  glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, (float*)&lightColor);
  glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, (float*)&camera->position);

  glBindVertexArray(VAO);

  const float RENDER_DISTANCE = 4.0f;

  for (int x = 0; x < CHUNKS_PER_AXIS; x++) {
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Chunk* chunk = chunks[x][z];

      // check if chunk out of render distance
      Vec3 chunkWorldCoords = chunkToWorld(&chunk->position);
      Vec3 chunkCenter = getChunkCenter(&chunk->position);
      Vec2i cameraXZ = {camera->position.x, camera->position.z};
      Vec2i chunkXZ = {chunkCenter.x, chunkCenter.z};

      if (vec2i_distance(&cameraXZ, &chunkXZ) > CHUNK_SIZE * RENDER_DISTANCE / 2) {
        continue;
      }
      Vec3 chunkDimensions = CHUNK_DIMENSIONS;

      // Check if the chunk is in the view frustum
      bool chunkVisible = frustum_block_visible(&frustum, &chunkCenter, &chunkDimensions, camera);
      if (!chunkVisible) {
        continue;
      }
      // Add alternating color pattern for chunks
      Vec3 chunkColor;
      if ((x + z) % 2 == 0) {
        chunkColor.x = 1.0f; // More reddish
        chunkColor.y = 0.8f;
        chunkColor.z = 0.8f;
      } else {
        chunkColor.x = 0.8f; // More bluish
        chunkColor.y = 0.8f;
        chunkColor.z = 1.0f;
      }

      // Render each block in the chunk
      for (int i = 0; i < CHUNK_SIZE; i++) {
        for (int j = 0; j < CHUNK_HEIGHT; j++) {
          for (int k = 0; k < CHUNK_SIZE; k++) {
            Block* block = &chunk->blocks[i][j][k];
            if (block->id == BLOCK_AIR) {
              continue;
            }
            Vec3i pos = {chunkWorldCoords.x + i * CUBE_SIZE, j * CUBE_SIZE, chunkWorldCoords.z + k * CUBE_SIZE};
            // printf("p: %d, %d, %d chunk: %d,%d world: %d,%d,%d type:%d\n", i, j, k, x, z, pos.x, pos.y, pos.z, block->id);

            // bool occluded = is_block_occluded(&pos, CUBE_SIZE, camera);

            // if (occluded) {
            //  continue;
            //}
            if (!block->checkedNeighbors) {
              block->checkedNeighbors = true;
              for (int face = 0; face < 6; face++) {
                Vec3i nPos;
                vec3i_add(&nPos, &pos, &vec3iFaceMap[face]);
                Block* n = getBlock(&nPos);
                if (!n || n->id == BLOCK_AIR) {
                  block->neighbor[face] = false;
                } else {
                  block->neighbor[face] = true;
                }
              }
            }

            visibleCubes++;
            // Blend block color with chunk color
            Vec3 finalColor;
            finalColor.x = blockColors[block->id].x * chunkColor.x;
            finalColor.y = blockColors[block->id].y * chunkColor.y;
            finalColor.z = blockColors[block->id].z * chunkColor.z;

            Mat4 model;
            mat4_identity(model);
            model[12] = (float)pos.x + 0.5f;
            model[13] = (float)pos.y + 0.5f;
            model[14] = (float)pos.z + 0.5f;

            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, model);

            glUseProgram(shaderProgram);

            // Set the texture sampler uniform to use texture unit 0
            glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);

            // Bind the appropriate texture for each face
            for (int face = 0; face < 6; face++) {
              GLuint texture = 0;
              if (block->neighbor[face]) {
                continue;
              }

              switch (block->id) {
              case BLOCK_STONE:
                texture = stoneTexture;
                break;
              case BLOCK_DIRT:
                texture = dirtTexture;
                break;
              case BLOCK_GRASS:
                if (face == TOP) {
                  texture = grassTopTexture;
                } else if (face == BOTTOM) {
                  texture = dirtTexture;
                } else { // Side faces
                  texture = grassSideTexture;
                }
                break;
              }

              renderCubeFace(face, texture);
            }
          }
        }
      }
    }
  }

  glBindVertexArray(0);
  RenderResult result = {visibleCubes};
  return result;
}
void cleanupWorld() {
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteTextures(1, &stoneTexture);
  glDeleteTextures(1, &dirtTexture);
  glDeleteTextures(1, &grassTopTexture);
  glDeleteTextures(1, &grassSideTexture);
}

// Get a pointer to the chunk at the given position.
Chunk* getChunk(Vec2i* chunkPos) {
  if (chunkPos->a < 0 || chunkPos->a >= CHUNKS_PER_AXIS || chunkPos->b < 0 || chunkPos->b >= CHUNKS_PER_AXIS) {
    // printf("INVALID CHUNK PASSED pos: %d,%d\n", chunkPos->a, chunkPos->b);
    return NULL;
  }
  return chunks[chunkPos->a][chunkPos->b];
}

// Get the block at the specified world position.
Block* getBlock(Vec3i* pos) {
  if (pos->y < 0 || pos->y > CHUNK_HEIGHT) {
    return NULL;
  }
  Vec2i chunkPos = blockToChunk(pos);
  Vec3i localPos = getLocal(pos);

  // Get the chunk
  Chunk* chunk = getChunk(&chunkPos);
  if (!chunk) {
    // printf("NULL CHUNK WHEN p: %d, %d, %d chunk: %d,%d\n", pos->x, pos->y, pos->z, chunkPos.a, chunkPos.b);
    return NULL;
  }

  return &chunk->blocks[localPos.x][pos->y][localPos.z];
}