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

void renderText(GLuint shaderProgram, const char* text, float x, float y);

#endif