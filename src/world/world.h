/**
 * @file world/world.h
 * @brief World generation and rendering.
 * @author frankischilling, VladimirJanus
 * @version 0.1
 * @date 2024-11-19
 *
 */
#ifndef WORLD_H
#define WORLD_H

#include <GL/glew.h>
#include "../graphics/camera.h"
#include "../graphics/frustum.h"
#include "../graphics/shader.h"
#include "../math/math.h"
#include "cube.h"
#include "chunk.h"

#define WORLD_SIZE 256
#define WORLD_HEIGHT 64
#define CUBE_SIZE 1.0f

#define DIRT_LAYERS 3 // Number of dirt layers below the surface

typedef struct {
  int visisbleCubes;
} RenderResult;

typedef struct {
  float frequency;
  float amplitude;
  float persistence;
  float heightScale;
} BiomeParameters;
BiomeParameters getInterpolatedBiomeParameters(float x, float z);
float getTerrainHeight(float x, float z);
const char* getCurrentBiomeText(float x, float z);
// World Functions
void initWorld();
RenderResult renderWorld(GLuint shaderProgram, const Camera* camera);
void cleanupWorld();

// Chunk functions
void initChunks();
void cleanupChunks();
void renderChunkGrid(GLuint shaderProgram, const Camera* camera);

Chunk* getChunk(Vec2i* chunkPos);
Block* getBlock(Vec3i* pos);
#endif // WORLD_H
