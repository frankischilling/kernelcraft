/**
 * @file graphics/cube.c
 * @brief Cube module for rendering a cube.
 * @author frankischilling
 * @date 2024-11-19
 */
#include "cube.h"
#include <stdio.h>
typedef struct {
  GLfloat vertices[48]; // 6 vertices * 8 floats per vertex
} CubeFace;

// Predefined faces with positions, normals, and texture coordinates
static const CubeFace cubeFaces[6] = {
    // Right face (X-positive)
    {{0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,   //
      0.5f, -0.5f, 0.5f,  1.0f, 0.0f, 0.0f, 1.0f, 1.0f,   //
      0.5f, 0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   //
      0.5f, 0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   //
      0.5f, 0.5f,  -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,   //
      0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}}, //

    // Left face (X-negative)
    {{-0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,   //
      -0.5f, -0.5f, 0.5f,  -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,   //
      -0.5f, 0.5f,  0.5f,  -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   //
      -0.5f, 0.5f,  0.5f,  -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   //
      -0.5f, 0.5f,  -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,   //
      -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f}}, //

    // Top face (Y-positive)
    {{-0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,   //
      0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   //
      0.5f,  0.5f, 0.5f,  0.0f, 1.0f, 0.0f, 1.0f, 1.0f,   //
      0.5f,  0.5f, 0.5f,  0.0f, 1.0f, 0.0f, 1.0f, 1.0f,   //
      -0.5f, 0.5f, 0.5f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f,   //
      -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f}}, //

    // Bottom face (Y-negative)
    {{-0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f,   //
      0.5f,  -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f,   //
      0.5f,  -0.5f, 0.5f,  0.0f, -1.0f, 0.0f, 1.0f, 0.0f,   //
      0.5f,  -0.5f, 0.5f,  0.0f, -1.0f, 0.0f, 1.0f, 0.0f,   //
      -0.5f, -0.5f, 0.5f,  0.0f, -1.0f, 0.0f, 0.0f, 0.0f,   //
      -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f}}, //

    // Front face (Z-negative)
    {{-0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,   //
      0.5f,  -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,   //
      0.5f,  0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,   //
      0.5f,  0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,   //
      -0.5f, 0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,   //
      -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}}, //

    // Back face (Z-positive)
    {{-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,   //
      0.5f,  -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,   //
      0.5f,  0.5f,  -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f,   //
      0.5f,  0.5f,  -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f,   //
      -0.5f, 0.5f,  -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,   //
      -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f}}, //
};

// Global VAO and VBO for cube rendering
static GLuint cubeVAO, cubeVBO;

void initCube() {
  // Generate and bind VAO and VBO
  glGenVertexArrays(1, &cubeVAO);
  glGenBuffers(1, &cubeVBO);

  glBindVertexArray(cubeVAO);
  glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);

  // Allocate buffer large enough for all faces
  glBufferData(GL_ARRAY_BUFFER, sizeof(cubeFaces), NULL, GL_DYNAMIC_DRAW);

  // Set up vertex attributes
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
  glEnableVertexAttribArray(1);

  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(6 * sizeof(GLfloat)));
  glEnableVertexAttribArray(2);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

void renderCubeFace(int face, GLuint texture) {
  // Validate face index
  if (face < 0 || face >= 6) {
    printf("Invalid face index: %d\n", face);
    return;
  }

  // Bind the correct texture
  glBindTexture(GL_TEXTURE_2D, texture);

  // Bind the VAO and buffer the specific face
  glBindVertexArray(cubeVAO);
  glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);

  // Update the buffer with the specific face's data
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(cubeFaces[face]), &cubeFaces[face]);

  // Draw the face
  glDrawArrays(GL_TRIANGLES, 0, 6);

  // Unbind
  glBindVertexArray(0);
}
