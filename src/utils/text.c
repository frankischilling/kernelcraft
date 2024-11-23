/**
 * @file utils/text.c
 * @brief Text rendering utility functions.
 * @author frankischilling
 * @version 0.1
 * @date 2024-11-20
 *
 */
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <stdio.h>
#include <stdlib.h>
#include "../graphics/shader.h"
#include "text.h"

#define BUILD_VERSION "v0.0.3-alpha"
#define BUILD_NAME "kernelcraft"

void renderText(GLuint shaderProgram, const char* text, float x, float y) {
    glUseProgram(shaderProgram);
    
    // Set text color to white
    glUniform3f(glGetUniformLocation(shaderProgram, "textColor"), 1.0f, 1.0f, 1.0f);
    
    // For right-aligned text, calculate the width of the text
    float textWidth = 0;
    for (const char* c = text; *c != '\0'; c++) {
        textWidth += glutBitmapWidth(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    // Adjust x position for right alignment if x is negative
    if (x < 0) {
        x = 800.0f + x - textWidth; // 800 is window width
    }
    
    // Draw text at specified position
    glWindowPos2f(x, y);
    for (const char* c = text; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
}

void renderBuildInfo(GLuint shaderProgram) {
    char buildText[64];
    snprintf(buildText, sizeof(buildText), "%s %s", BUILD_NAME, BUILD_VERSION);
    renderText(shaderProgram, buildText, 10.0f, 600.0f);
}