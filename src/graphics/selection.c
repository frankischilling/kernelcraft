#include <GL/glew.h>
#include "selection.h"
#include "../world/chunk.h"

void drawSelection(const Ray* selection, const Mat4 view, const Mat4 projection) {
  if (!selection->hit)
    return;
  // The application already requires compatibility GL for FreeGLUT text.
  // The face tint remains visible when close-up edges lie outside the viewport.
  // These overlays need no per-frame buffer allocation or upload.
  GLint program, matrixMode;
  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
  glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LINE_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_POLYGON_BIT);
  glUseProgram(0);
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDepthFunc(GL_LEQUAL);
  glLineWidth(2);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadMatrixf(projection);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadMatrixf(view);
  const Vec3i p = selection->blockCoords;
  for (int face = 0; face < 6; face++) {
    Vec3i normal = vec3iFaceMap[face];
    if (normal.x != selection->normal.x || normal.y != selection->normal.y || normal.z != selection->normal.z)
      continue;
    const float* vertices = getCubeFaceVertices(face);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    // Bias the coplanar face toward the camera without expanding into neighbors.
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1, -1);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glColor4f(1.0f, 0.85f, 0.2f, 0.16f);
    glBegin(GL_TRIANGLES);
    for (int vertex = 0; vertex < 6; vertex++)
      glVertex3f((p.x + 0.5f + vertices[vertex * 8]) * CUBE_SIZE, (p.y + 0.5f + vertices[vertex * 8 + 1]) * CUBE_SIZE, (p.z + 0.5f + vertices[vertex * 8 + 2]) * CUBE_SIZE);
    glEnd();
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_BLEND);
    break;
  }
  glColor3f(1.0f, 0.85f, 0.2f);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  // Expanding the box buries its bottom/side edges inside adjoining blocks.
  // Keep exact face bounds and bias the depth of their polygon borders instead.
  // Polygon offset does not affect standalone GL_LINES; quads also avoid the
  // diagonal that would appear when outlining each face's two triangles.
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glEnable(GL_POLYGON_OFFSET_LINE);
  glPolygonOffset(-1, -1);
  const int corners[] = {0, 1, 2, 4};
  glBegin(GL_QUADS);
  for (int face = 0; face < 6; face++) {
    const float* vertices = getCubeFaceVertices(face);
    for (int corner = 0; corner < 4; corner++) {
      const float* vertex = vertices + corners[corner] * 8;
      glVertex3f((p.x + 0.5f + vertex[0]) * CUBE_SIZE, (p.y + 0.5f + vertex[1]) * CUBE_SIZE, (p.z + 0.5f + vertex[2]) * CUBE_SIZE);
    }
  }
  glEnd();
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(matrixMode);
  glPopAttrib();
  glUseProgram((GLuint)program);
}
