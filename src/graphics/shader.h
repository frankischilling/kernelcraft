// shader.h
#ifndef SHADER_H
#define SHADER_H

#include <GL/glew.h>

GLuint loadShaders(const char* vertexPath, const char* fragmentPath);
void renderText(GLuint shaderProgram, const char* text, float x, float y);

#endif