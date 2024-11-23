/**
 * @file graphics/shader.h
 * @brief Shader module for loading and compiling shaders.
 * @author frankischilling
 * @date 2024-11-19
 */
#ifndef SHADER_H
#define SHADER_H

#include <GL/glew.h>

GLuint loadShaders(const char* vertexPath, const char* fragmentPath);
void renderText(GLuint shaderProgram, const char* text, float x, float y);

#endif