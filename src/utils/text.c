/**
 * @file utils/text.c
 * @brief Text rendering utility functions.
 * @author frankischilling
 * @version 0.2
 * @date 2025-11-17
 *
 */
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <stdio.h>
#include <stdlib.h>
#include "../graphics/shader.h"
#include "text.h"

void renderText(GLuint shaderProgram, const char* text, float x, float y) {
  // Save current OpenGL state
  GLint currentProgram;
  GLint currentMatrixMode;
  GLboolean depthTestEnabled;
  glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
  glGetIntegerv(GL_MATRIX_MODE, &currentMatrixMode);
  depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
  
  // Disable shader program for fixed-function pipeline text rendering
  glUseProgram(0);
  
  // Disable depth test for text rendering
  glDisable(GL_DEPTH_TEST);
  
  // Switch to projection matrix mode and save current matrix
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  
  // Set up orthographic projection for screen-space coordinates
  // Assuming window size is 1920x1080 (adjust if needed)
  glOrtho(0, 1920, 0, 1080, -1, 1);
  
  // Switch to modelview matrix mode and save current matrix
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  
  // For right-aligned text, calculate the width of the text
  float textWidth = 0;
  for (const char* c = text; *c != '\0'; c++) {
    textWidth += glutBitmapWidth(GLUT_BITMAP_HELVETICA_18, *c);
  }

  // Adjust x position for right alignment if x is negative
  if (x < 0) {
    x = 1920.0f + x - textWidth; // 1920 is window width
  }

  // Set raster position (y coordinate is flipped for OpenGL)
  glRasterPos2f(x, 1080.0f - y);
  
  // Draw text
  for (const char* c = text; *c != '\0'; c++) {
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
  }
  
  // Restore OpenGL state
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(currentMatrixMode);
  
  if (depthTestEnabled) {
    glEnable(GL_DEPTH_TEST);
  }
  
  // Restore shader program
  glUseProgram(currentProgram);
}
