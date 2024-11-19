/**
 * @file world.c
 * @brief World generation and rendering.
 * @author frankischilling
 * @version 0.1
 * @date 2024-11-19
 *
 */
#include "world.h"
#include "../math/math.h"
#include <stdlib.h>

static GLuint VBO, VAO;

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
    // Set light properties
    Vec3 lightPos = {5.0f, 10.0f, 5.0f};  // Light position above the world
    Vec3 lightColor = {1.0f, 1.0f, 1.0f}; // White light
    Vec3 objectColor = {0.4f, 0.6f, 0.3f}; // Grass-like green color

    glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, lightPos);
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, lightColor);
    glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, objectColor);
    glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, camera->position);

    glBindVertexArray(VAO);
    
    for(int x = 0; x < WORLD_SIZE_X; x++) {
        for(int z = 0; z < WORLD_SIZE_Z; z++) {
            Mat4 model;
            mat4_identity(model);
            
            float xPos = (x - WORLD_SIZE_X/2) * CUBE_SIZE;
            float zPos = (z - WORLD_SIZE_Z/2) * CUBE_SIZE;
            float height = noise2d(xPos, zPos);
            
            model[12] = xPos;
            model[13] = height;
            model[14] = zPos;
            
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, model);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }
    
    glBindVertexArray(0);
}

void cleanupWorld() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}