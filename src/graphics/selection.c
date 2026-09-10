#include <GL/glew.h>
#include "selection.h"
#include "../world/chunk.h"

static float outlineOffsetFactor(const float* vertices, Vec3i block, const Mat4 matrix, const GLint viewport[4]) {
  // Estimate the polygon's depth slope in window coordinates, just as GL does.
  // Limit the slope term to 0.0005 of the depth range: enough for shared-edge
  // rasterization, without pulling nearly edge-on faces through nearer blocks.
  double screen[3][3];
  for (int corner = 0; corner < 3; corner++) {
    double point[] = {(block.x + 0.5 + vertices[corner * 8]) * CUBE_SIZE, (block.y + 0.5 + vertices[corner * 8 + 1]) * CUBE_SIZE,
                      (block.z + 0.5 + vertices[corner * 8 + 2]) * CUBE_SIZE};
    double w = matrix[3] * point[0] + matrix[7] * point[1] + matrix[11] * point[2] + matrix[15];
    if (fabs(w) < 1e-8)
      return 0;
    for (int axis = 0; axis < 3; axis++) {
      double clip = matrix[axis] * point[0] + matrix[axis + 4] * point[1] + matrix[axis + 8] * point[2] + matrix[axis + 12];
      screen[corner][axis] = clip / w * 0.5 * (axis < 2 ? viewport[axis + 2] : 1);
    }
  }

  double dx1 = screen[1][0] - screen[0][0], dy1 = screen[1][1] - screen[0][1], dz1 = screen[1][2] - screen[0][2];
  double dx2 = screen[2][0] - screen[0][0], dy2 = screen[2][1] - screen[0][1], dz2 = screen[2][2] - screen[0][2];
  double area = dx1 * dy2 - dx2 * dy1;
  if (fabs(area) < 1e-12)
    return 0;
  double slope = fmax(fabs((dz1 * dy2 - dz2 * dy1) / area), fabs((dx1 * dz2 - dx2 * dz1) / area));
  return slope > 0.0005 ? (float)(-0.0005 / slope) : -1;
}

void drawSelection(const Ray* selection, const Mat4 view, const Mat4 projection) {
  if (!selection->hit)
    return;
  // The application already requires compatibility GL for FreeGLUT text.
  // The face tint remains visible when close-up edges lie outside the viewport.
  // These overlays need no per-frame buffer allocation or upload.
  GLint program, matrixMode, viewport[4];
  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
  glGetIntegerv(GL_VIEWPORT, viewport);
  Mat4 combined;
  mat4_multiply(combined, projection, view);
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
  const int corners[] = {0, 1, 2, 4};
  for (int face = 0; face < 6; face++) {
    const float* vertices = getCubeFaceVertices(face);
    glPolygonOffset(outlineOffsetFactor(vertices, p, combined, viewport), -1);
    glBegin(GL_QUADS);
    for (int corner = 0; corner < 4; corner++) {
      const float* vertex = vertices + corners[corner] * 8;
      glVertex3f((p.x + 0.5f + vertex[0]) * CUBE_SIZE, (p.y + 0.5f + vertex[1]) * CUBE_SIZE, (p.z + 0.5f + vertex[2]) * CUBE_SIZE);
    }

    glEnd();
  }

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(matrixMode);
  glPopAttrib();
  glUseProgram((GLuint)program);
}
