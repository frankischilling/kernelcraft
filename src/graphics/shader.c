/**
 * @file shader.c
 * @brief Shader module for loading and compiling shaders.
 * @author frankischilling
 * @date 2024-11-19
 */
#include "shader.h"
#include <GL/freeglut.h>
#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>

// Function to read shader code from a file
static char* readShaderFile(const char* filePath) {
  FILE* file = fopen(filePath, "r");
  if (!file) {
    fprintf(stderr, "Failed to open shader file: %s\n", filePath);
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  if (ftell(file) == -1) {
    fprintf(stderr, "Failed to seek to end of file: %s\n", filePath);
    fclose(file);
    return NULL;
  }
  long length = ftell(file);
  fseek(file, 0, SEEK_SET);

  char* buffer = (char*)malloc(length + 1);
  if (!buffer) {
    fprintf(stderr, "Failed to allocate memory for shader file: %s\n", filePath);
    fclose(file);
    return NULL;
  }

  fread(buffer, 1, length, file);
  buffer[length - 1] = '\0';
  fclose(file);

  return buffer;
}

// Function to compile a shader
static GLuint compileShader(const char* code, GLenum type) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &code, NULL);
  glCompileShader(shader);

  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char log[512];
    glGetShaderInfoLog(shader, sizeof(log), NULL, log);
    log[sizeof(log) - 1] = '\0';
    fprintf(stderr, "Shader compilation failed: %s\n", log);
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

// Function to load and link shaders
GLuint loadShaders(const char* vertexPath, const char* fragmentPath) {
  // Read vertex shader
  char* vertexSource = readShaderFile(vertexPath);
  if (!vertexSource) {
    return 0;
  }

  // Read fragment shader
  char* fragmentSource = readShaderFile(fragmentPath);
  if (!fragmentSource) {
    free(vertexSource);
    return 0;
  }

  // Create and compile vertex shader
  GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
  free(vertexSource);
  if (!vertexShader) {
    free(fragmentSource);
    return 0;
  }

  // Create and compile fragment shader
  GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);
  free(fragmentSource);
  if (!fragmentShader) {
    glDeleteShader(vertexShader);
    return 0;
  }

  // Create shader program
  GLuint shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);

  // Check for linking errors
  GLint success;
  glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
  if (!success) {
    char log[512];
    glGetProgramInfoLog(shaderProgram, sizeof(log), NULL, log);
    log[sizeof(log) - 1] = '\0';
    fprintf(stderr, "Shader program linking failed: %s\n", log);
    glDeleteProgram(shaderProgram);
    shaderProgram = 0;
  }

  // Clean up
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return shaderProgram;
}
