#ifndef WORLD_H
#define WORLD_H

#include <GL/glew.h>
#include "../math/math.h"
#include "../graphics/shader.h"
#include "../graphics/camera.h"

#define WORLD_SIZE_X 256
#define WORLD_SIZE_Z 256
#define WORLD_HEIGHT 64
#define CUBE_SIZE 1.0f

// Block types
typedef enum {
    BLOCK_AIR,
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE
} BlockType;

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

void initWorld();
void renderWorld(GLuint shaderProgram, const Camera* camera);
void cleanupWorld();
const char* getCurrentBiomeText(float x, float z);

#endif