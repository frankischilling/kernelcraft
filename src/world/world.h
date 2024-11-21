#ifndef WORLD_H
#define WORLD_H

#include <GL/glew.h>
#include "../math/math.h"
#include "../graphics/shader.h"
#include "../graphics/camera.h"
#include "../graphics/frustum.h"
#include "block_types.h"

#define WORLD_SIZE_X 256
#define WORLD_SIZE_Z 256
#define WORLD_HEIGHT 64
#define CUBE_SIZE 1.0f
#define DIRT_LAYERS 3  // Number of dirt layers below the surface

#define CHUNK_SIZE_X 16
#define CHUNK_SIZE_Y 64
#define CHUNK_SIZE_Z 16

typedef struct {
    BlockType blocks[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
    Vec3 position; // Position of the chunk in world coordinates
} Chunk;

typedef struct {
    float frequency;
    float amplitude;
    float persistence;
    float heightScale;
} BiomeParameters;

typedef enum {
    BIOME_PLAINS,
    BIOME_HILLS,
} BiomeType;

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
void addCubeFace(GLfloat* vertices, int* vertexCount, float x, float y, float z, int face);
void renderChunkGrid(GLuint shaderProgram, const Camera* camera);
#endif // WORLD_H
