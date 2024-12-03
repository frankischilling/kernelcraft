/**
 * @file world/world.h
 * @brief World generation and rendering.
 * @author frankischilling
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
#include "block.h"

#define WORLD_SIZE 256
#define WORLD_HEIGHT 64
#define CUBE_SIZE 1.0f
#define CHUNK_SIZE 16   // block count
#define CHUNK_HEIGHT 64 // block count
#define CHUNKS_PER_AXIS WORLD_SIZE / CHUNK_SIZE

#define DIRT_LAYERS 3 // Number of dirt layers below the surface

typedef enum {
  BIOME_PLAINS,
  BIOME_HILLS,
} BiomeID;

typedef struct {
  Block blocks[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE];
  Vec2i position; // Chunk coordinates
  BiomeID id;
} Chunk;

typedef struct {
  float frequency;
  float amplitude;
  float persistence;
  float heightScale;
} BiomeParameters;

BiomeParameters getInterpolatedBiomeParameters(float x, float z);
// World Functions
void initWorld();
void renderWorld(GLuint shaderProgram, const Camera* camera);
void cleanupWorld();
const char* getCurrentBiomeText(float x, float z);
int getVisibleCubesCount();
float getTerrainHeight(float x, float z);

// Chunk functions
void initChunks();
void renderChunks(GLuint shaderProgram, const Camera* camera);
void cleanupChunks();
void renderChunkGrid(GLuint shaderProgram, const Camera* camera);

Block* getBlock(Vec3* coords);
BlockID getBlockType(Vec3* coords);

#endif // WORLD_H
