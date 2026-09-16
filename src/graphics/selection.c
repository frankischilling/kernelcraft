#include <GL/glew.h>
#include "selection.h"
#include "world_renderer.h"
#include "../world/chunk.h"
#include "../world/world.h"

typedef struct {
  float ax;
  float ay;
  float bx;
  float by;
  float reveal;
} CrackSegment;

static const CrackSegment crackSegments[] = {
    {0.00f, 0.00f, -0.11f, 0.10f, 0.00f},   {0.00f, 0.00f, 0.13f, 0.08f, 0.05f},   {0.00f, 0.00f, 0.03f, -0.14f, 0.11f},    {-0.11f, 0.10f, -0.24f, 0.25f, 0.18f},
    {-0.11f, 0.10f, -0.29f, 0.06f, 0.24f},  {0.13f, 0.08f, 0.29f, 0.22f, 0.31f},   {0.13f, 0.08f, 0.32f, 0.01f, 0.37f},     {0.03f, -0.14f, 0.17f, -0.29f, 0.44f},
    {0.03f, -0.14f, -0.13f, -0.32f, 0.50f}, {-0.24f, 0.25f, -0.34f, 0.37f, 0.57f}, {-0.24f, 0.25f, -0.39f, 0.22f, 0.63f},   {0.29f, 0.22f, 0.40f, 0.34f, 0.69f},
    {0.32f, 0.01f, 0.42f, -0.09f, 0.75f},   {0.17f, -0.29f, 0.29f, -0.40f, 0.81f}, {-0.13f, -0.32f, -0.25f, -0.42f, 0.87f}, {-0.29f, 0.06f, -0.41f, -0.02f, 0.93f}};

static bool blockMatchesBreaking(const BlockBreaking* breaking, const Ray* selection, float* progress) {
  if (!breaking || !selection || !breaking->active || !selection->hit || selection->blockCoords.x != breaking->target.x || selection->blockCoords.y != breaking->target.y ||
      selection->blockCoords.z != breaking->target.z)
    return false;
  float value = blockBreakingProgress(breaking);
  if (!(value > 0))
    return false;
  const Block* block = getBlock(&breaking->target);
  if (!block || block->id != breaking->block)
    return false;
  *progress = value;
  return true;
}

static Vec3 crackFacePoint(int face, Vec3i block, float s, float t) {
  Vec3 point = {block.x + 0.5f, block.y + 0.5f, block.z + 0.5f};
  switch (face) {
  case RIGHT:
    point.x += 0.5f;
    point.y += 0.5f - t;
    point.z += s - 0.5f;
    break;
  case LEFT:
    point.x -= 0.5f;
    point.y += 0.5f - t;
    point.z += s - 0.5f;
    break;
  case TOP:
    point.x += s - 0.5f;
    point.y += 0.5f;
    point.z += t - 0.5f;
    break;
  case BOTTOM:
    point.x += s - 0.5f;
    point.y -= 0.5f;
    point.z += 0.5f - t;
    break;
  case FRONT:
    point.x += s - 0.5f;
    point.y += 0.5f - t;
    point.z += 0.5f;
    break;
  default:
    point.x += s - 0.5f;
    point.y += 0.5f - t;
    point.z -= 0.5f;
    break;
  }
  point.x *= CUBE_SIZE;
  point.y *= CUBE_SIZE;
  point.z *= CUBE_SIZE;
  return point;
}

static void emitCrackSegment(int face, Vec3i block, const CrackSegment* segment, float halfWidth) {
  float dx = segment->bx - segment->ax, dy = segment->by - segment->ay;
  float length = sqrtf(dx * dx + dy * dy);
  if (length <= 0)
    return;
  float px = -dy / length * halfWidth, py = dx / length * halfWidth;
  const float points[][2] = {
      {segment->ax + px, segment->ay + py}, {segment->ax - px, segment->ay - py}, {segment->bx - px, segment->by - py}, {segment->bx + px, segment->by + py}};
  for (int corner = 0; corner < 4; corner++) {
    float s = points[corner][0] + 0.5f, t = points[corner][1] + 0.5f;
    Vec3 point = crackFacePoint(face, block, s, t);
    glTexCoord2f(s, t);
    glVertex3f(point.x, point.y, point.z);
  }
}

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

void drawBlockBreaking(const BlockBreaking* breaking, const Ray* selection, const Mat4 view, const Mat4 projection) {
  float progress;
  if (!blockMatchesBreaking(breaking, selection, &progress))
    return;
  bool leafMask = breaking->block == BLOCK_OAK_LEAVES;
  GLuint leafTexture = leafMask ? worldLeafTexture() : 0;
  if (leafMask && !leafTexture)
    return;

  GLint program, matrixMode, activeTexture, textureUnits, sampler0;
  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
  glGetIntegerv(GL_MAX_TEXTURE_UNITS, &textureUnits);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_SAMPLER_BINDING, &sampler0);
  glActiveTexture((GLenum)activeTexture);
  glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LINE_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_POLYGON_BIT | GL_TEXTURE_BIT);
  glUseProgram(0);
  for (int unit = 0; unit < textureUnits; unit++) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glDisable(GL_TEXTURE_1D);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_TEXTURE_3D);
    glDisable(GL_TEXTURE_CUBE_MAP);
    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);
  }
  glActiveTexture(GL_TEXTURE0);
  glBindSampler(0, 0);
  glDisable(GL_ALPHA_TEST);
  if (leafMask) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, leafTexture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.0f);
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
  }
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDepthFunc(GL_LEQUAL);
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_CULL_FACE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(-1, -1);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glColor4f(0.015f, 0.012f, 0.01f, 0.38f + 0.52f * progress);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadMatrixf(projection);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadMatrixf(view);

  Vec3i block = breaking->target;
  float halfWidth = 0.007f + 0.009f * progress;
  for (int face = 0; face < 6; face++) {
    Vec3i normal = vec3iFaceMap[face];
    Vec3i neighborPos = {block.x + normal.x, block.y + normal.y, block.z + normal.z};
    const Block* neighbor = getBlock(&neighborPos);
    int neighborID = neighbor ? neighbor->id : BLOCK_AIR;
    if (!blockFaceVisible(breaking->block, neighborID))
      continue;
    glBegin(GL_QUADS);
    for (size_t segment = 0; segment < sizeof(crackSegments) / sizeof(crackSegments[0]); segment++)
      if (progress >= crackSegments[segment].reveal)
        emitCrackSegment(face, block, &crackSegments[segment], halfWidth);
    glEnd();
  }

  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  if (leafMask) {
    glActiveTexture(GL_TEXTURE0);
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
  }
  glMatrixMode(matrixMode);
  glPopAttrib();
  glBindSampler(0, (GLuint)sampler0);
  glActiveTexture((GLenum)activeTexture);
  glUseProgram((GLuint)program);
}
