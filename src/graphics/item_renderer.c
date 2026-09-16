#include "item_renderer.h"
#include <math.h>

static void itemQuad(float left, float bottom, float right, float top, float z) {
  glBegin(GL_QUADS);
  glTexCoord2f(0, 1);
  glVertex3f(left, bottom, z);
  glTexCoord2f(1, 1);
  glVertex3f(right, bottom, z);
  glTexCoord2f(1, 0);
  glVertex3f(right, top, z);
  glTexCoord2f(0, 0);
  glVertex3f(left, top, z);
  glEnd();
}

static void armorLetter(int slot) {
  // H, C, L, B mark equipment until dedicated item artwork is available.
  static const unsigned char letters[4][5] = {{17, 17, 31, 17, 17}, {15, 16, 16, 16, 15}, {16, 16, 16, 16, 31}, {30, 17, 30, 17, 30}};
  if (slot < 0 || slot >= 4)
    return;
  glColor3f(1, 0.9f, 0.7f);
  for (int row = 0; row < 5; row++)
    for (int column = 0; column < 5; column++) {
      if (!(letters[slot][row] & (1 << (4 - column))))
        continue;
      float x = -0.075f + column * 0.03f, y = 0.075f - row * 0.03f;
      itemQuad(x, y - 0.03f, x + 0.03f, y, 0.001f);
      itemQuad(x, y - 0.03f, x + 0.03f, y, -0.001f);
    }
}

void renderDroppedItems(const DroppedItems* drops, const Mat4 view, const Mat4 projection, const GLuint blockTextures[6]) {
  if (!drops || !blockTextures)
    return;
  bool any = false;
  for (size_t i = 0; i < DROPPED_ITEM_CAPACITY; i++)
    any |= drops->items[i].active;
  if (!any)
    return;

  GLint program, matrixMode, activeTexture;
  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glUseProgram(0);
  glActiveTexture(GL_TEXTURE0);
  glDisable(GL_LIGHTING);
  glDisable(GL_FOG);
  glDisable(GL_ALPHA_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_COLOR_LOGIC_OP);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_TRUE);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadMatrixf(projection);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadMatrixf(view);
  for (size_t i = 0; i < DROPPED_ITEM_CAPACITY; i++) {
    const DroppedItem* item = &drops->items[i];
    if (!item->active)
      continue;
    float phase = (float)drops->animationSeconds + (float)i * 0.4f;
    glPushMatrix();
    glTranslatef(item->position.x, item->position.y + 0.05f + 0.04f * sinf(phase * 3), item->position.z);
    glRotatef(phase * 90, 0, 1, 0);
    int block = inventoryItemBlock(item->stack.item);
    if (block > 0 && block <= 6) {
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, blockTextures[block - 1]);
      glColor3f(1, 1, 1);
    } else {
      glDisable(GL_TEXTURE_2D);
      uint32_t color = inventoryItemColor(item->stack.item);
      glColor3ub((GLubyte)(color >> 16), (GLubyte)(color >> 8), (GLubyte)color);
    }
    itemQuad(-0.125f, -0.125f, 0.125f, 0.125f, 0);
    if (!block)
      armorLetter(inventoryItemArmorSlot(item->stack.item));
    glPopMatrix();
  }
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode((GLenum)matrixMode);
  glPopAttrib();
  glActiveTexture((GLenum)activeTexture);
  glUseProgram((GLuint)program);
}
