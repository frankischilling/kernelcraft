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
#include "../graphics/cube.h"
#include "../graphics/frustum.h"
#include "../graphics/shader.h"
#include "../graphics/texture.h"
#include "../math/math.h"
#include "block.h"
static GLuint stoneTexture, dirtTexture, grassTopTexture, grassSideTexture;

// 2d array of chunk pointers
static Chunk*** chunks = NULL;
//                    index I      J coord X      Z
// FIRST  QUADRANT chunks[8 ,15][8 ,15] [0 , 7][0 , 7]
// SECOND QUADRANT chunks[8 ,15][0 , 7] [0 , 7][-8,-1]
// THIRD  QUADRANT chunks[0 , 7][0 , 7] [-8,-1][-8,-1]
// FOURTH QUADRANT chunks[0 , 7][8 ,15] [-8,-1][0 , 7]

static GLuint VBO, VAO;

static int visibleCubes = 0;

Vec3 convertChunkToWorld(Vec2i* chunkPos) {
  Vec3 worldCoords = (Vec3){chunkPos->a * CHUNK_SIZE, 0, chunkPos->b * CHUNK_SIZE};
  return worldCoords;
}
Vec3 getChunkCenter(Vec2i* chunkPos) {
  Vec3 worldCoords = convertChunkToWorld(chunkPos);
  return (Vec3){worldCoords.x + CHUNK_SIZE / 2, CHUNK_HEIGHT / 2, worldCoords.z + CHUNK_SIZE / 2};
}

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
              chunk->blocks[i][j][k] = (Block){BLOCK_STONE};
            } else if (j < height) {
              chunk->blocks[i][j][k] = (Block){BLOCK_DIRT};
            } else if (j == (int)height) {
              chunk->blocks[i][j][k] = (Block){BLOCK_GRASS};
            } else {
              chunk->blocks[i][j][k] = (Block){BLOCK_AIR};
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

Chunk* getChunk(int x, int z) {
  if (x < 0 || x >= CHUNKS_PER_AXIS || z < 0 || z >= CHUNKS_PER_AXIS) {
    return NULL;
  }
  return chunks[x][z];
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

static BiomeParameters biomeParameters[] = {
    // Plains biome - flatter, lower amplitude
    {0.03f, 0.5f, 0.3f, 4.0f}, // Lower frequency and amplitude for flatter terrain
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

void renderWorld(GLuint shaderProgram, const Camera* camera) {
  visibleCubes = 0; // Reset counter

  // Create and update frustum
  Frustum frustum;
  Mat4 projection, view;
  mat4_perspective(projection, 70.0f, 1920.0f / 1080.0f, 0.1f, 1000.0f);

  Vec3 target;
  vec3_add(&target, &camera->position, &camera->front);
  mat4_lookAt(view, &camera->position, &target, &camera->up);

  frustum_update(&frustum, projection, view);

  // Set light properties
  Vec3 lightPos = {5.0f, 30.0f, 5.0f};
  Vec3 lightColor = {1.0f, 1.0f, 1.0f};

  glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, (float*)&lightPos);
  glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, (float*)&lightColor);
  glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, (float*)&camera->position);

  glBindVertexArray(VAO);

  const float RENDER_DISTANCE = 6.0f;

  for (int x = 0; x < CHUNKS_PER_AXIS; x++) {
    for (int z = 0; z < CHUNKS_PER_AXIS; z++) {
      Chunk* chunk = chunks[x][z];

      // check if chunk out of render distance
      Vec3 chunkWorldCoords = convertChunkToWorld(&chunk->position);
      Vec3 chunkCenter = getChunkCenter(&chunk->position);
      Vec2i cameraXZ = {camera->position.x, camera->position.z};
      Vec2i chunkXZ = {chunkCenter.x, chunkCenter.z};

      if (vec2i_distance(&cameraXZ, &chunkXZ) > CHUNK_SIZE * RENDER_DISTANCE / 2) {
        continue;
      }
      Vec3 chunkDimensions = CHUNK_DIMENSIONS;
      BlockVisibility chunkVisible = frustum_check_block(&frustum, &chunkCenter, &chunkDimensions, camera);
      if (chunkVisible == BLOCK_HIDDEN) {
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

      // Check if the chunk is in the view frustum

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
            bool occluded = is_block_occluded(&pos, CUBE_SIZE, camera);

            if (occluded == true) {
              continue;
            }
            visibleCubes++;
            // Blend block color with chunk color
            Vec3 finalColor;
            finalColor.x = blockColors[block->id].x * chunkColor.x;
            finalColor.y = blockColors[block->id].y * chunkColor.y;
            finalColor.z = blockColors[block->id].z * chunkColor.z;

            Mat4 model;
            mat4_identity(model);
            model[12] = (float)pos.x;
            model[13] = (float)pos.y;
            model[14] = (float)pos.z;

            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, model);

            glUseProgram(shaderProgram);

            // Set the texture sampler uniform to use texture unit 0
            glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);

            // Bind the appropriate texture for each face
            for (int face = 0; face < 6; face++) {
              if (block->id == BLOCK_STONE) {
                glBindTexture(GL_TEXTURE_2D, stoneTexture);
              } else if (block->id == BLOCK_DIRT) {
                glBindTexture(GL_TEXTURE_2D, dirtTexture);
              } else if (block->id == BLOCK_GRASS) {
                if (face == 2) { // Top face
                  glBindTexture(GL_TEXTURE_2D, grassTopTexture);
                } else if (face == 3) { // Bottom face
                  glBindTexture(GL_TEXTURE_2D, dirtTexture);
                } else { // Side faces
                  glBindTexture(GL_TEXTURE_2D, grassSideTexture);
                }
              }
              // Draw the face
              glDrawArrays(GL_TRIANGLES, face * 6, 6);
            }
          }
        }
      }
    }
  }

  glBindVertexArray(0);
}

void cleanupWorld() {
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteTextures(1, &stoneTexture);
  glDeleteTextures(1, &dirtTexture);
  glDeleteTextures(1, &grassTopTexture);
  glDeleteTextures(1, &grassSideTexture);
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

int getVisibleCubesCount() {
  return visibleCubes;
}

Vec3i getPositionOnGrid(const Vec3* pos) {
  return (Vec3i){(int)pos->x, (int)pos->y, (int)pos->z};
}

Block* getBlock(Vec3i* pos) {
  // Convert world coordinates to chunk coordinates
  int chunkI = floorf(pos->x / (CHUNK_SIZE * CUBE_SIZE)) + CHUNKS_PER_AXIS / 2;
  int chunkJ = floorf(pos->z / (CHUNK_SIZE * CUBE_SIZE)) + CHUNKS_PER_AXIS / 2;
  // Get local coordinates within the chunk
  int localX = (int)((pos->x % CHUNK_SIZE) / CUBE_SIZE);
  int localZ = (int)((pos->z % CHUNK_SIZE) / CUBE_SIZE);
  if (localX < 0)
    localX += CHUNK_SIZE;
  if (localZ < 0)
    localZ += CHUNK_SIZE;

  if (localX < 0 || localX > 15 || pos->y < 0 || localZ < 0 || localZ > 15 || chunkI < 0 || chunkJ < 0 || chunkI > CHUNKS_PER_AXIS - 1 || chunkJ > CHUNKS_PER_AXIS - 1) {
    // printf(" OUT OF BOUNDS p: %d, %d, %d chunk: %d,%d\n", localX, pos->y, localZ, chunkI, chunkJ);
    return NULL;
  }
  // Get the chunk
  Chunk* chunk = getChunk(chunkI, chunkJ);
  if (!chunk) {
    printf("NULL CHUNK WHEN p: %d, %d, %d chunk: %d,%d\n", pos->x, pos->y, pos->z, chunkI, chunkJ);
    return NULL;
  }
  // printf("p: %d, %d, %d chunk: %d,%d world: %d,%d,%d \n", localX, pos->y, localZ, chunkI, chunkJ, pos->x, pos->y, pos->z);

  return &chunk->blocks[localX][pos->y][localZ];
}
