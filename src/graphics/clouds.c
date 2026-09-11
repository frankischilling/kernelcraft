#include "clouds.h"
#include "shader.h"
#include <stdint.h>
#include <string.h>

static float cloudNode(unsigned x, unsigned z) {
  uint32_t h = (x & 15u) * UINT32_C(374761393) + (z & 15u) * UINT32_C(668265263) + 1447u;
  h = (h ^ (h >> 13u)) * UINT32_C(1274126177);
  h ^= h >> 16u;
  return (float)(h & 65535u) / 65535.0f;
}

static void cloudPattern(GLuint rows[64][2]) {
  memset(rows, 0, 64 * 2 * sizeof(GLuint));
  for (unsigned z = 0; z < 64; z++)
    for (unsigned x = 0; x < 64; x++) {
      float fx = ((x & 3u) + 0.5f) / 4, fz = ((z & 3u) + 0.5f) / 4;
      fx = fx * fx * (3 - 2 * fx);
      fz = fz * fz * (3 - 2 * fz);
      unsigned bx = x / 4, bz = z / 4;
      float a = cloudNode(bx, bz) * (1 - fx) + cloudNode(bx + 1, bz) * fx;
      float b = cloudNode(bx, bz + 1) * (1 - fx) + cloudNode(bx + 1, bz + 1) * fx;
      if (a * (1 - fz) + b * fz > 0.50f)
        rows[z][x / 32] |= (GLuint)1u << (x & 31u);
    }
}

bool initClouds(CloudRenderer* clouds) {
  clouds->program = loadShaders("assets/shaders/sky_vertex.glsl", "assets/shaders/cloud_fragment.glsl");
  if (!clouds->program)
    return false;
  glGenVertexArrays(1, &clouds->vao);
  clouds->front = glGetUniformLocation(clouds->program, "cameraFront");
  clouds->right = glGetUniformLocation(clouds->program, "cameraRight");
  clouds->up = glGetUniformLocation(clouds->program, "cameraUp");
  clouds->scale = glGetUniformLocation(clouds->program, "viewScale");
  clouds->origin = glGetUniformLocation(clouds->program, "cloudOrigin");
  clouds->projection = glGetUniformLocation(clouds->program, "depthProjection");
  clouds->weights = glGetUniformLocation(clouds->program, "weights");
  // The shape never changes: upload a 512-byte occupancy bitset once instead of
  // hashing and interpolating four noise nodes at every ray-march step.
  GLuint rows[64][2];
  cloudPattern(rows);
  GLint previousProgram;
  glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
  glUseProgram(clouds->program);
  glUniform2uiv(glGetUniformLocation(clouds->program, "cloudRows[0]"), 64, &rows[0][0]);
  glUseProgram(previousProgram);
  if (!clouds->vao || glGetError() != GL_NO_ERROR) {
    cleanupClouds(clouds);
    return false;
  }
  return true;
}

void cleanupClouds(CloudRenderer* clouds) {
  glDeleteVertexArrays(1, &clouds->vao);
  if (clouds->program)
    glDeleteProgram(clouds->program);
  memset(clouds, 0, sizeof(*clouds));
}

void advanceClouds(CloudRenderer* clouds, double seconds, bool active) {
  bool resumed = !clouds->active;
  clouds->active = active;
  if (!active || resumed || !isfinite(seconds) || seconds <= 0)
    return;
  // Independent of /time set; wrap at the shader's 64 twelve-block cells.
  clouds->offset = fmod(clouds->offset + fmin(seconds, 0.1) * 0.6, 768.0);
}

void renderClouds(const CloudRenderer* clouds, const Camera* camera, float aspect, const Mat4 projection, const DayNightState* state) {
  GLint program, vao, polygon[2], depthFunc, sourceRGB, destRGB, sourceAlpha, destAlpha, equationRGB, equationAlpha;
  GLboolean depth = glIsEnabled(GL_DEPTH_TEST), blend = glIsEnabled(GL_BLEND), cull = glIsEnabled(GL_CULL_FACE), depthMask;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
  glGetIntegerv(GL_POLYGON_MODE, polygon);
  glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
  glGetIntegerv(GL_BLEND_SRC_RGB, &sourceRGB);
  glGetIntegerv(GL_BLEND_DST_RGB, &destRGB);
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &sourceAlpha);
  glGetIntegerv(GL_BLEND_DST_ALPHA, &destAlpha);
  glGetIntegerv(GL_BLEND_EQUATION_RGB, &equationRGB);
  glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equationAlpha);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
  glDisable(GL_CULL_FACE);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glUseProgram(clouds->program);
  Vec3 right, up;
  vec3_cross(&right, &camera->front, &camera->up);
  vec3_normalize(&right, &right);
  vec3_cross(&up, &right, &camera->front);
  glUniform3f(clouds->front, camera->front.x, camera->front.y, camera->front.z);
  glUniform3f(clouds->right, right.x, right.y, right.z);
  glUniform3f(clouds->up, up.x, up.y, up.z);
  float scale = tanf(toRadians(camera->fov) * 0.5f);
  glUniform2f(clouds->scale, scale * aspect, scale);
  // Positive modulo keeps the repeating field stable across negative coordinates.
  double x = fmod((double)camera->position.x - clouds->offset, 768.0);
  double z = fmod((double)camera->position.z, 768.0);
  glUniform3f(clouds->origin, (float)(x < 0 ? x + 768 : x), camera->position.y, (float)(z < 0 ? z + 768 : z));
  glUniform2f(clouds->projection, projection[10], projection[14]);
  glUniform3f(clouds->weights, state->day, state->twilight, state->night);
  glBindVertexArray(clouds->vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(vao);
  glUseProgram(program);
  glDepthMask(depthMask);
  glDepthFunc(depthFunc);
  if (!depth)
    glDisable(GL_DEPTH_TEST);
  if (!blend)
    glDisable(GL_BLEND);
  if (cull)
    glEnable(GL_CULL_FACE);
  glBlendFuncSeparate(sourceRGB, destRGB, sourceAlpha, destAlpha);
  glBlendEquationSeparate(equationRGB, equationAlpha);
  glPolygonMode(GL_FRONT, polygon[0]);
  glPolygonMode(GL_BACK, polygon[1]);
}
