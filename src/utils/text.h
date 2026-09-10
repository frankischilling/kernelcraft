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
#include <stdbool.h>

typedef struct {
  GLint program, matrixMode;
  GLint viewport[4];
  GLboolean depthTest;
  void* font;
  int fontHeight;
} TextState;

// Cache the HUD's three FreeGLUT bitmap fonts while a GL context is current.
// Call initialization before drawing and cleanup before destroying that context.
bool initText(void);
void cleanupText(void);
void beginText(TextState* state);
void renderText(const TextState* state, const char* text, float x, float y);
int textWidth(const TextState* state, const char* text);
void endText(const TextState* state);

#endif
