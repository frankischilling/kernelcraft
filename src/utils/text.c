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
#include <math.h>
#include <string.h>

enum { FONT_COUNT = 3, GLYPHS = 256, CELL_SIZE = 32, COLUMNS = 16, ATLAS_WIDTH = COLUMNS * CELL_SIZE, ATLAS_HEIGHT = FONT_COUNT * GLYPHS / COLUMNS * CELL_SIZE };

enum { GLYPH_X = 4, GLYPH_Y = 8 };

static void* const fonts[FONT_COUNT] = {GLUT_BITMAP_HELVETICA_10, GLUT_BITMAP_HELVETICA_12, GLUT_BITMAP_HELVETICA_18};
static GLuint fontTexture;
static int advances[FONT_COUNT][GLYPHS];

static int fontIndex(void* font) {
  for (int i = 0; i < FONT_COUNT; i++)
    if (font == fonts[i])
      return i;
  return -1;
}

void cleanupText(void) {
  glDeleteTextures(1, &fontTexture);
  fontTexture = 0;
  memset(advances, 0, sizeof(advances));
}

bool initText(void) {
  cleanupText();
  GLint program, matrixMode, drawFramebuffer, readFramebuffer, unpackBuffer;
  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
  glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glUseProgram(0);
  glActiveTexture(GL_TEXTURE0);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_ALPHA_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_COLOR_LOGIC_OP);
  glDisable(GL_LIGHTING);
  glDisable(GL_FOG);
  glDisable(GL_DITHER);
  glDisable(GL_FRAMEBUFFER_SRGB);
  glDisable(GL_RASTERIZER_DISCARD);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glGenTextures(1, &fontTexture);
  glBindTexture(GL_TEXTURE_2D, fontTexture);
  // RGBA preserves coverage alpha in the compatibility texture environment.
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ATLAS_WIDTH, ATLAS_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  GLuint framebuffer = 0;
  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fontTexture, 0);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  bool ready = fontTexture && framebuffer && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
  glViewport(0, 0, ATLAS_WIDTH, ATLAS_HEIGHT);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, ATLAS_WIDTH, 0, ATLAS_HEIGHT, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  if (ready) {
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor4f(1, 1, 1, 1);
    for (int font = 0; font < FONT_COUNT; font++) {
      for (int character = 1; character < GLYPHS; character++) {
        advances[font][character] = glutBitmapWidth(fonts[font], character);
        int cell = font * GLYPHS + character;
        glRasterPos2i(cell % COLUMNS * CELL_SIZE + GLYPH_X, cell / COLUMNS * CELL_SIZE + GLYPH_Y);
        unsigned char string[] = {(unsigned char)character, 0};
        glutBitmapString(fonts[font], string);
      }
    }
  }

  ready = glGetError() == GL_NO_ERROR && ready;
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode((GLenum)matrixMode);
  glUseProgram((GLuint)program);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)drawFramebuffer);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)readFramebuffer);
  glDeleteFramebuffers(1, &framebuffer);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)unpackBuffer);
  glPopAttrib();
  if (!ready) {
    fprintf(stderr, "Cannot create HUD font cache\n");
    cleanupText();
  }

  return ready;
}

void beginText(TextState* state) {
  glGetIntegerv(GL_CURRENT_PROGRAM, &state->program);
  glGetIntegerv(GL_MATRIX_MODE, &state->matrixMode);
  glGetIntegerv(GL_VIEWPORT, state->viewport);
  state->font = state->viewport[2] < 640 || state->viewport[3] < 480 ? GLUT_BITMAP_HELVETICA_12 : GLUT_BITMAP_HELVETICA_18;
  state->fontHeight = glutBitmapHeight(state->font);
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
  int font = fontIndex(state->font);
  if (!fontTexture || font < 0 || !text || !*text)
    return;
  if (x < 0) {
    x += state->viewport[2] - textWidth(state, text);
  }

  float baseline = state->viewport[3] - y;
  // Match the initial raster-position clipping of the bitmap path.
  if (x < 0 || x > state->viewport[2] || baseline < 0 || baseline > state->viewport[3])
    return;
  // Bitmap raster positions retain the driver's float transform rounding.
  // Read it once per label, then reproduce each glyph's raster advance.
  glRasterPos2f(x, baseline);
  GLfloat raster[4];
  glGetFloatv(GL_CURRENT_RASTER_POSITION, raster);
  float penX = raster[0], penY = raster[1], lineAdvance = 0;
  glPushAttrib(GL_TEXTURE_BIT | GL_ENABLE_BIT | GL_CURRENT_BIT);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, fontTexture);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glBegin(GL_QUADS);
  for (const unsigned char* character = (const unsigned char*)text; *character; character++) {
    if (*character == '\n') {
      penX -= lineAdvance;
      penY -= state->fontHeight;
      lineAdvance = 0;
      continue;
    }

    float left = floorf(penX) - GLYPH_X - state->viewport[0];
    float bottom = floorf(penY) - GLYPH_Y - state->viewport[1];
    int cell = font * GLYPHS + *character;
    float u = (float)(cell % COLUMNS * CELL_SIZE) / ATLAS_WIDTH;
    float v = (float)(cell / COLUMNS * CELL_SIZE) / ATLAS_HEIGHT;
    float du = (float)CELL_SIZE / ATLAS_WIDTH, dv = (float)CELL_SIZE / ATLAS_HEIGHT;
    glTexCoord2f(u, v);
    glVertex2f(left, bottom);
    glTexCoord2f(u + du, v);
    glVertex2f(left + CELL_SIZE, bottom);
    glTexCoord2f(u + du, v + dv);
    glVertex2f(left + CELL_SIZE, bottom + CELL_SIZE);
    glTexCoord2f(u, v + dv);
    glVertex2f(left, bottom + CELL_SIZE);
    penX += advances[font][*character];
    lineAdvance += advances[font][*character];
  }

  glEnd();
  glPopAttrib();
}

int textWidth(const TextState* state, const char* text) {
  int font = fontIndex(state->font);
  if (!fontTexture || font < 0)
    return glutBitmapLength(state->font, (const unsigned char*)text);
  int width = 0, line = 0;
  for (const unsigned char* character = (const unsigned char*)text; character && *character; character++) {
    if (*character == '\n') {
      if (line > width)
        width = line;
      line = 0;
    } else {
      line += advances[font][*character];
    }
  }

  return line > width ? line : width;
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
