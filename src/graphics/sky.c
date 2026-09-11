#include "sky.h"
#include "shader.h"
#include "texture.h"
#include <stdio.h>
#include <string.h>

bool initSky(SkyRenderer* sky) {
  GLint previousProgram, previousActive, previousTexture;
  glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActive);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
  bool success = false;
  const char* paths[] = {"assets/sky/day.png", "assets/sky/dawn-dusk.png", "assets/sky/night.png", "assets/sky/sun.png", "assets/sky/full-moon.png"};
  sky->program = loadShaders("assets/shaders/sky_vertex.glsl", "assets/shaders/sky_fragment.glsl");
  if (!sky->program)
    goto failure;
  glActiveTexture(GL_TEXTURE0);
  for (int i = 0; i < 5; i++) {
    sky->textures[i] = loadTexture(paths[i]);
    if (!sky->textures[i])
      goto failure;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // The shader reads swatch centers; celestial images retain pixel-art edges.
  }
  glGenVertexArrays(1, &sky->vao);
  glUseProgram(sky->program);
  const char* samplers[] = {"dayPalette", "twilightPalette", "nightPalette", "sunImage", "moonImage"};
  for (int i = 0; i < 5; i++)
    glUniform1i(glGetUniformLocation(sky->program, samplers[i]), i);
  sky->front = glGetUniformLocation(sky->program, "cameraFront");
  sky->right = glGetUniformLocation(sky->program, "cameraRight");
  sky->up = glGetUniformLocation(sky->program, "cameraUp");
  sky->scale = glGetUniformLocation(sky->program, "viewScale");
  sky->weights = glGetUniformLocation(sky->program, "weights");
  sky->sun = glGetUniformLocation(sky->program, "sunDirection");
  sky->moon = glGetUniformLocation(sky->program, "moonDirection");
  sky->stars = glGetUniformLocation(sky->program, "starBrightness");
  if (!sky->vao || glGetError() != GL_NO_ERROR)
    goto failure;
  success = true;
  goto restore;
failure:
  fprintf(stderr, "Failed to initialize sky rendering\n");
restore:
  glUseProgram(previousProgram);
  glBindTexture(GL_TEXTURE_2D, previousTexture);
  glActiveTexture(previousActive);
  if (!success)
    cleanupSky(sky);
  return success;
}

void cleanupSky(SkyRenderer* sky) {
  glDeleteTextures(5, sky->textures);
  glDeleteVertexArrays(1, &sky->vao);
  if (sky->program)
    glDeleteProgram(sky->program);
  memset(sky, 0, sizeof(*sky));
}

static void uniformVec(GLint location, Vec3 v) {
  glUniform3f(location, v.x, v.y, v.z);
}

void renderSky(const SkyRenderer* sky, const Camera* camera, float aspect, const DayNightState* state) {
  GLint program, vao, activeTexture, bindings[5], polygon[2], depthFunc;
  GLdouble depthRange[2];
  GLboolean depth = glIsEnabled(GL_DEPTH_TEST), blend = glIsEnabled(GL_BLEND), cull = glIsEnabled(GL_CULL_FACE), depthMask;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
  glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
  glGetDoublev(GL_DEPTH_RANGE, depthRange);
  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
  glGetIntegerv(GL_POLYGON_MODE, polygon);
  for (int i = 0; i < 5; i++) {
    glActiveTexture(GL_TEXTURE0 + i);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bindings[i]);
    glBindTexture(GL_TEXTURE_2D, sky->textures[i]);
  }
  // Only shade background pixels left by opaque terrain. Map the shared
  // fullscreen triangle to the far depth without changing cloud projection.
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glDepthRange(1, 1);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glDepthMask(GL_FALSE);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glUseProgram(sky->program);
  Vec3 right, up;
  vec3_cross(&right, &camera->front, &camera->up);
  vec3_normalize(&right, &right);
  vec3_cross(&up, &right, &camera->front);
  uniformVec(sky->front, camera->front);
  uniformVec(sky->right, right);
  uniformVec(sky->up, up);
  float scale = tanf(toRadians(camera->fov) * 0.5f);
  glUniform2f(sky->scale, scale * aspect, scale);
  glUniform3f(sky->weights, state->day, state->twilight, state->night);
  uniformVec(sky->sun, state->sunDirection);
  uniformVec(sky->moon, state->moonDirection);
  glUniform1f(sky->stars, state->stars);
  glBindVertexArray(sky->vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(vao);
  glUseProgram(program);
  glDepthMask(depthMask);
  glDepthFunc(depthFunc);
  glDepthRange(depthRange[0], depthRange[1]);
  if (!depth)
    glDisable(GL_DEPTH_TEST);
  if (blend)
    glEnable(GL_BLEND);
  if (cull)
    glEnable(GL_CULL_FACE);
  glPolygonMode(GL_FRONT, polygon[0]);
  glPolygonMode(GL_BACK, polygon[1]);
  for (int i = 0; i < 5; i++) {
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, bindings[i]);
  }
  glActiveTexture(activeTexture);
}
