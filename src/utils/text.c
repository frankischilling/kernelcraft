/**
 * @file utils/text.c
 * @brief Text rendering utility functions.
 * @author frankischilling
 * @version 0.2
 * @date 2025-11-17
 *
 */
#include "text.h"
#include "../graphics/shader.h"
#include <GL/freeglut.h>
#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>

void beginText(TextState* state) {
  glGetIntegerv(GL_CURRENT_PROGRAM, &state->program);
  glGetIntegerv(GL_MATRIX_MODE, &state->matrixMode);
  glGetIntegerv(GL_VIEWPORT, state->viewport);
  state->depthTest = glIsEnabled(GL_DEPTH_TEST);
  glUseProgram(0);
  glDisable(GL_DEPTH_TEST);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, state->viewport[2], 0, state->viewport[3], -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
}

void renderText(const TextState* state, const char* text, float x, float y) {
  if (x < 0) {
    x += state->viewport[2] - glutBitmapLength(GLUT_BITMAP_HELVETICA_18, (const unsigned char*)text);
  }
  glRasterPos2f(x, state->viewport[3] - y);
  glutBitmapString(GLUT_BITMAP_HELVETICA_18, (const unsigned char*)text);
}

void endText(const TextState* state) {
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(state->matrixMode);
  if (state->depthTest)
    glEnable(GL_DEPTH_TEST);
  glUseProgram(state->program);
}
