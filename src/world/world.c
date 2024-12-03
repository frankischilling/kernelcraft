/**
 * @file world/world.c
 * @brief World generation and rendering.
 * @author frankischilling
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
#include "block_types.h"
static GLuint stoneTexture, dirtTexture, grassTopTexture, grassSideTexture;

static Chunk** chunks = NULL;

static int chunkCountX, chunkCountZ;

static GLuint VBO, VAO;

static int visibleCubes = 0;

// Chunk functions
void initChunks() {
  // Calculate how many chunks fit into the world
  chunkCountX = WORLD_SIZE_X / CHUNK_SIZE_X;
  chunkCountZ = WORLD_SIZE_Z / CHUNK_SIZE_Z;

  // Allocate memory for chunks
  chunks = (Chunk**)malloc(chunkCountX * chunkCountZ * sizeof(Chunk*));
  for (int x = 0; x < chunkCountX; x++) {
    for (int z = 0; z < chunkCountZ; z++) {
      Chunk* chunk = (Chunk*)malloc(sizeof(Chunk));

      // Calculate chunk position in world coordinates
      chunk->position.x = (x - chunkCountX / 2) * CHUNK_SIZE_X * CUBE_SIZE;
      chunk->position.y = 0;
      chunk->position.z = (z - chunkCountZ / 2) * CHUNK_SIZE_Z * CUBE_SIZE;

      // Populate chunk with block data
      for (int i = 0; i < CHUNK_SIZE_X; i++) {
        for (int j = 0; j < CHUNK_SIZE_Y; j++) {
          for (int k = 0; k < CHUNK_SIZE_Z; k++) {
            // Calculate actual world coordinates for height generation
            float worldX = chunk->position.x + i * CUBE_SIZE;
            float worldZ = chunk->position.z + k * CUBE_SIZE;
            float height = floor(getTerrainHeight(worldX, worldZ));

            // Ensure height is within chunk bounds
            if (j < height - DIRT_LAYERS) {
              chunk->blocks[i][j][k] = BLOCK_STONE;
            } else if (j < height) {
              chunk->blocks[i][j][k] = BLOCK_DIRT;
            } else if (j == (int)height) {
              chunk->blocks[i][j][k] = BLOCK_GRASS;
            } else {
              chunk->blocks[i][j][k] = BLOCK_AIR;
            }
          }
        }
      }

      chunks[x * chunkCountZ + z] = chunk;
    }
  }
}

void renderChunkGrid(GLuint shaderProgram, const Camera* camera) {
  static GLuint gridVAO = 0;
  static GLuint gridVBO = 0;

  // Initialize grid buffers if not already done
  if (gridVAO == 0) {
    // Create vertices for grid lines
    float* vertices = malloc(sizeof(float) * 6 * (WORLD_SIZE_X + WORLD_SIZE_Z));
    int vertexCount = 0;

    // Calculate offset to center the grid
    float offsetX = -WORLD_SIZE_X / 2.0f;
    float offsetZ = -WORLD_SIZE_Z / 2.0f;

    // Vertical lines
    for (int x = 0; x <= WORLD_SIZE_X; x += CHUNK_SIZE_X) {
      float worldX = x + offsetX;
      vertices[vertexCount++] = worldX;
      vertices[vertexCount++] = 0.0f;
      vertices[vertexCount++] = offsetZ;

      vertices[vertexCount++] = worldX;
      vertices[vertexCount++] = 0.0f;
      vertices[vertexCount++] = WORLD_SIZE_Z + offsetZ;
    }

    // Horizontal lines
    for (int z = 0; z <= WORLD_SIZE_Z; z += CHUNK_SIZE_Z) {
      float worldZ = z + offsetZ;
      vertices[vertexCount++] = offsetX;
      vertices[vertexCount++] = 0.0f;
      vertices[vertexCount++] = worldZ;

      vertices[vertexCount++] = WORLD_SIZE_X + offsetX;
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
  glDrawArrays(GL_LINES, 0, (WORLD_SIZE_X / CHUNK_SIZE_X + WORLD_SIZE_Z / CHUNK_SIZE_Z + 2) * 2);
  glBindVertexArray(0);
}

void renderChunks(GLuint shaderProgram, const Camera* camera) {
  // Create and update frustum
  Frustum frustum;
  Mat4 projection, view;

  // Set up projection matrix
  mat4_perspective(projection, 70.0f, 1920.0f / 1080.0f, 0.1f, 1000.0f);

  // Set up view matrix
  Vec3 target;
  vec3_add(&target, &camera->position, &camera->front);
  mat4_lookAt(view, &camera->position, &target, &camera->up);

  // Update frustum
  frustum_update(&frustum, projection, view);

  // Update projection and view uniforms
  glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, projection);
  glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, view);

  // Calculate current chunk position of camera
  int camChunkX = (int)floor(camera->position.x / (CHUNK_SIZE_X * CUBE_SIZE));
  int camChunkZ = (int)floor(camera->position.z / (CHUNK_SIZE_Z * CUBE_SIZE));

  // Define render distance in chunks
  const int CHUNK_RENDER_DISTANCE = 8;

  // Second pass: Render the actual chunks with frustum culling
  for (int x = 0; x < chunkCountX; x++) {
    for (int z = 0; z < chunkCountZ; z++) {
      Chunk* chunk = chunks[x * chunkCountZ + z];

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
      BlockVisibility visibility =
          frustum_check_cube(&frustum, chunk->position.x + CHUNK_SIZE_X * 0.5f, CHUNK_SIZE_Y * 0.5f, chunk->position.z + CHUNK_SIZE_Z * 0.5f, CHUNK_SIZE_X * CUBE_SIZE, camera);

      if (visibility == BLOCK_HIDDEN) {
        continue;
      }

      // Render each block in the chunk
      for (int i = 0; i < CHUNK_SIZE_X; i++) {
        for (int j = 0; j < CHUNK_SIZE_Y; j++) {
          for (int k = 0; k < CHUNK_SIZE_Z; k++) {
            BlockType block = chunk->blocks[i][j][k];
            if (block == BLOCK_AIR)
              continue;

            // Blend block color with chunk color
            Vec3 finalColor;
            finalColor.x = blockColors[block].x * chunkColor.x;
            finalColor.y = blockColors[block].y * chunkColor.y;
            finalColor.z = blockColors[block].z * chunkColor.z;

            Mat4 model;
            mat4_identity(model);
            model[12] = chunk->position.x + i * CUBE_SIZE;
            model[13] = j * CUBE_SIZE;
            model[14] = chunk->position.z + k * CUBE_SIZE;

            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, model);
            glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, (float*)&finalColor); 

            renderCube();
          }
        }
      }
    }
  }
}

void cleanupChunks() {
  for (int x = 0; x < chunkCountX; x++) {
    for (int z = 0; z < chunkCountZ; z++) {
      free(chunks[x * chunkCountZ + z]);
    }
  }
  free(chunks);
}

Chunk* getChunk(int x, int z) {
  if (x < 0 || x >= chunkCountX || z < 0 || z >= chunkCountZ) {
    return NULL;
  }
  return chunks[x * chunkCountZ + z];
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

  const float RENDER_DISTANCE = 64.0f;
  const float RENDER_DISTANCE_SQ = RENDER_DISTANCE * RENDER_DISTANCE;

  for (int x = 0; x < WORLD_SIZE_X; x++) {
    for (int z = 0; z < WORLD_SIZE_Z; z++) {
      float xPos = (x - WORLD_SIZE_X / 2) * CUBE_SIZE;
      float zPos = (z - WORLD_SIZE_Z / 2) * CUBE_SIZE;

      float dx = xPos - camera->position.x;
      float dz = zPos - camera->position.z;
      float distSq = dx * dx + dz * dz;

      if (distSq > RENDER_DISTANCE_SQ) {
        continue;
      }

      float surfaceHeight = floor(getTerrainHeight(xPos, zPos));

      for (float y = -2.0f; y <= surfaceHeight; y += 1.0f) {
        BlockVisibility visibility = frustum_check_cube(&frustum, xPos, y, zPos, CUBE_SIZE, camera);
        if (visibility == BLOCK_HIDDEN) {
          continue;
        }
        visibleCubes++; // Only increment for visible blocks

        BlockType blockType;

        if (y < surfaceHeight - DIRT_LAYERS) {
          blockType = BLOCK_STONE;
        } else if (y == surfaceHeight) {
          blockType = BLOCK_GRASS;
        } else {
          blockType = BLOCK_DIRT;
        }

        Mat4 model;
        mat4_identity(model);

        model[12] = xPos;
        model[13] = y;
        model[14] = zPos;

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, model);

        glUseProgram(shaderProgram);

        // Set the texture sampler uniform to use texture unit 0
        glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);

        // Bind the appropriate texture for each face
        for (int face = 0; face < 6; face++) {
          if (blockType == BLOCK_STONE) {
            glBindTexture(GL_TEXTURE_2D, stoneTexture);
          } else if (blockType == BLOCK_DIRT) {
            glBindTexture(GL_TEXTURE_2D, dirtTexture);
          } else if (blockType == BLOCK_GRASS) {
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

// Add this function implementation
BlockType getBlockType(float x, float y, float z) {
  // Convert world coordinates to chunk coordinates
  int chunkX = (int)floor((x + WORLD_SIZE_X * CUBE_SIZE / 2) / (CHUNK_SIZE_X * CUBE_SIZE));
  int chunkZ = (int)floor((z + WORLD_SIZE_Z * CUBE_SIZE / 2) / (CHUNK_SIZE_Z * CUBE_SIZE));

  // Get local coordinates within the chunk
  float localX = fmod(x + WORLD_SIZE_X * CUBE_SIZE / 2, CHUNK_SIZE_X * CUBE_SIZE) / CUBE_SIZE;
  if (localX < 0)
    localX += CHUNK_SIZE_X;
  float localZ = fmod(z + WORLD_SIZE_Z * CUBE_SIZE / 2, CHUNK_SIZE_Z * CUBE_SIZE) / CUBE_SIZE;
  if (localZ < 0)
    localZ += CHUNK_SIZE_Z;
  int localY = (int)floor(y / CUBE_SIZE);

  // Check bounds
  if (chunkX < 0 || chunkX >= chunkCountX || chunkZ < 0 || chunkZ >= chunkCountZ || localY < 0 || localY >= CHUNK_SIZE_Y) {
    return BLOCK_AIR;
  }

  // Get the chunk
  Chunk* chunk = chunks[chunkX * chunkCountZ + chunkZ];
  if (!chunk) {
    return BLOCK_AIR;
  }

  return chunk->blocks[(int)localX][localY][(int)localZ];
}
