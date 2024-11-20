/**
 * @file world.c
 * @brief World generation and rendering.
 * @author frankischilling
 * @version 0.1
 * @date 2024-11-19
 *
 */
#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../math/math.h"
#include "../graphics/shader.h"
#include "../graphics/camera.h"  
#include "../graphics/frustum.h"
#include "world.h"
static GLuint VBO, VAO;
static int visibleCubes = 0;

// Block colors (RGB)
static const Vec3 blockColors[] = {
    {0.0f, 0.0f, 0.0f},     // AIR (not used)
    {0.4f, 0.6f, 0.3f},     // GRASS
    {0.6f, 0.4f, 0.2f},     // DIRT
    {0.5f, 0.5f, 0.5f}      // STONE
};

static const GLfloat cubeVerticesWithNormals[] = {
    // Front face         // Normal
    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
     0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

    // Back face
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
     0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

    // Top face
    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,

    // Bottom face
    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
     0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

    // Right face
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
     0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,

    // Left face
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f
};

static BiomeParameters biomeParameters[] = {
    // Plains biome - flatter, lower amplitude
    {0.03f, 0.5f, 0.3f, 4.0f},  // Lower frequency and amplitude for flatter terrain
    // Hills biome - more varied, higher amplitude
    {0.1f, 1.2f, 0.5f, 12.0f}
};

static float getBiomeBlendFactor(float x, float z) {
    // Use a different noise frequency for biome transitions
    float biomeNoise = perlin(x * 0.02f, 0, z * 0.02f);
    return smoothstep(0.4f, 0.6f, biomeNoise);
}

BiomeParameters getInterpolatedBiomeParameters(float x, float z) {
    float blendFactor = getBiomeBlendFactor(x, z);
    BiomeParameters result;
    
    result.frequency = lerp(biomeParameters[BIOME_PLAINS].frequency, 
                          biomeParameters[BIOME_HILLS].frequency, 
                          blendFactor);
    result.amplitude = lerp(biomeParameters[BIOME_PLAINS].amplitude, 
                          biomeParameters[BIOME_HILLS].amplitude, 
                          blendFactor);
    result.persistence = lerp(biomeParameters[BIOME_PLAINS].persistence, 
                            biomeParameters[BIOME_HILLS].persistence, 
                            blendFactor);
    result.heightScale = lerp(biomeParameters[BIOME_PLAINS].heightScale, 
                            biomeParameters[BIOME_HILLS].heightScale, 
                            blendFactor);
    
    return result;
}

// Add this function to get height based on biome
static float getTerrainHeight(float x, float z) {
    BiomeParameters params = getInterpolatedBiomeParameters(x, z);
    float height = 0.0f;
    float amplitude = params.amplitude;
    float frequency = params.frequency;
    
    // Use more octaves for more detailed terrain
    for(int i = 0; i < 4; i++) {
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void renderWorld(GLuint shaderProgram, const Camera* camera) {
    visibleCubes = 0;  // Reset counter
    
    // Create and update frustum
    Frustum frustum;
    Mat4 projection, view;
    // Use a wider FOV and appropriate near/far planes
    mat4_perspective(projection, 70.0f, 800.0f / 600.0f, 0.1f, 1000.0f);
       
    Vec3 target;
    vec3_add(target, camera->position, camera->front);
    mat4_lookAt(view, camera->position, target, camera->up);
    
    frustum_update(&frustum, projection, view);

    // Set light properties
    Vec3 lightPos = {5.0f, 30.0f, 5.0f};
    Vec3 lightColor = {1.0f, 1.0f, 1.0f};

    glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, lightPos);
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, lightColor);
    glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, camera->position);

    glBindVertexArray(VAO);
    
    // Define render distance
    const float RENDER_DISTANCE = 64.0f;
    const float RENDER_DISTANCE_SQ = RENDER_DISTANCE * RENDER_DISTANCE;

    for(int x = 0; x < WORLD_SIZE_X; x++) {
        for(int z = 0; z < WORLD_SIZE_Z; z++) {
            float xPos = (x - WORLD_SIZE_X/2) * CUBE_SIZE;
            float zPos = (z - WORLD_SIZE_Z/2) * CUBE_SIZE;
            
            // Distance check from camera
            float dx = xPos - camera->position[0];
            float dz = zPos - camera->position[2];
            float distSq = dx * dx + dz * dz;
            
            if(distSq > RENDER_DISTANCE_SQ) {
                continue;
            }

            float surfaceHeight = floor(getTerrainHeight(xPos, zPos));

            for(float y = -2.0f; y <= surfaceHeight; y += 1.0f) {
                // Frustum culling check
                BlockVisibility visibility = frustum_check_cube(&frustum, xPos, y, zPos, CUBE_SIZE, camera);
                if (visibility == BLOCK_HIDDEN) {
                    continue;
                }
                visibleCubes++;  // Only increment for visible blocks

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
                
                // Set block color based on type
                glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, blockColors[blockType]);
                glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, model);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
        }
    }
    
    glBindVertexArray(0);
}

void cleanupWorld() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

const char* getCurrentBiomeText(float x, float z) {
    float blendFactor = getBiomeBlendFactor(x, z);
    if (blendFactor < 0.4f) {
        return "Plains";
    } else if (blendFactor > 0.6f) {
        return "Hills";
    } else {
        return "Transition";  // Optional: show when we're between biomes
    }
}

int getVisibleCubesCount() {
    return visibleCubes;
}