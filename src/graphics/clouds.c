#include "clouds.h"
#include "shader.h"
#include <string.h>

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
