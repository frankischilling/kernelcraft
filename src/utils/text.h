/**
 * @file utils/text.h
 * @brief Text rendering utility functions.
 * @author frankischilling
 * @version 0.1
 * @date 2024-11-20
 *
 */
#ifndef TEXT_H
#define TEXT_H

#include <GL/freeglut.h>

void renderText2D(float x, float y, const char* text);
void renderFPS(float fps);
void renderBuildInfo(GLuint shaderProgram);
#endif 