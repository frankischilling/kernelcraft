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

#include <GL/glew.h>

typedef struct {
  GLint program, matrixMode;
  GLint viewport[4];
  GLboolean depthTest;
  void* font;
  int fontHeight;
} TextState;

void beginText(TextState* state);
void renderText(const TextState* state, const char* text, float x, float y);
int textWidth(const TextState* state, const char* text);
void endText(const TextState* state);

#endif
