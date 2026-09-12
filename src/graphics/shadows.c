#include "shadows.h"
#include "shader.h"
#include <string.h>

bool initShadowMap(ShadowMap* map, Vec3 center, float radius) {
  *map = (ShadowMap){.center = center, .radius = radius};
  if (!isfinite(radius) || radius <= 0)
    return false;
  GLint draw, read, texture, limit;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &limit);
  // Two cached maps at 3072 use about 72 MiB with four-byte depth storage,
  // close to the former single 4096 map rather than doubling that budget.
  map->size = limit < 3072 ? limit : 3072;
  if (map->size < 1024)
    return false;
  map->program = loadShaders("assets/shaders/shadow_depth.vert", "assets/shaders/shadow_depth.frag");
  if (!map->program)
    return false;
  map->transformLocation = glGetUniformLocation(map->program, "shadowTransform");
  glGenTextures(1, &map->depth);
  glBindTexture(GL_TEXTURE_2D, map->depth);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, map->size, map->size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  const float border[] = {1, 1, 1, 1};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
  glGenFramebuffers(1, &map->framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, map->framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, map->depth, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  bool ok = map->depth && map->framebuffer && map->transformLocation >= 0 && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
  glBindTexture(GL_TEXTURE_2D, texture);
  ok = ok && glGetError() == GL_NO_ERROR;
  if (!ok)
    cleanupShadowMap(map);
  return ok;
}

void cleanupShadowMap(ShadowMap* map) {
  glDeleteFramebuffers(1, &map->framebuffer);
  glDeleteTextures(1, &map->depth);
  if (map->program)
    glDeleteProgram(map->program);
  memset(map, 0, sizeof(*map));
}

static void shadowTransform(ShadowMap* map, Vec3 light) {
  // Orthographic coordinates about the world's fixed center. Z points away
  // from the light, mapping the enclosing sphere to the clip cube [-1,1].
  Vec3 up = fabsf(light.z) < 0.99f ? (Vec3){0, 0, 1} : (Vec3){0, 1, 0};
  Vec3 right;
  vec3_cross(&right, &up, &light);
  vec3_normalize(&right, &right);
  vec3_cross(&up, &light, &right);
  Vec3 axes[] = {right, up, {-light.x, -light.y, -light.z}};
  mat4_identity(map->transform);
  for (int i = 0; i < 3; i++) {
    map->transform[i] = axes[i].x / map->radius;
    map->transform[4 + i] = axes[i].y / map->radius;
    map->transform[8 + i] = axes[i].z / map->radius;
    map->transform[12 + i] = -vec3_dot(&axes[i], &map->center) / map->radius;
  }
}

bool updateShadowMap(ShadowMap* map, Vec3 direction, bool edited, const ShadowGeometry* geometry, size_t count, int* drawCalls) {
  *drawCalls = 0;
  float length = vec3_dot(&direction, &direction);
  if (!map->program || !isfinite(length) || length < 1e-12f)
    return false;
  vec3_normalize(&direction, &direction);
  if (map->valid && !edited && direction.x == map->direction.x && direction.y == map->direction.y && direction.z == map->direction.z)
    return true;
  shadowTransform(map, direction);
  GLint draw, read, viewport[4], program, vao, depthFunc, polygon[2];
  GLboolean depthMask;
  GLdouble clearDepth;
  const GLenum capabilities[] = {GL_DEPTH_TEST, GL_BLEND, GL_CULL_FACE, GL_SCISSOR_TEST, GL_POLYGON_OFFSET_FILL, GL_RASTERIZER_DISCARD};
  GLboolean enabled[sizeof(capabilities) / sizeof(*capabilities)];
  for (size_t i = 0; i < sizeof(capabilities) / sizeof(*capabilities); i++) {
    enabled[i] = glIsEnabled(capabilities[i]);
    glDisable(capabilities[i]);
  }
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
  glGetIntegerv(GL_VIEWPORT, viewport);
  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
  glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
  glGetIntegerv(GL_POLYGON_MODE, polygon);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
  glGetDoublev(GL_DEPTH_CLEAR_VALUE, &clearDepth);
  glBindFramebuffer(GL_FRAMEBUFFER, map->framebuffer);
  glViewport(0, 0, map->size, map->size);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glClearDepth(1);
  glClear(GL_DEPTH_BUFFER_BIT);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glUseProgram(map->program);
  glUniformMatrix4fv(map->transformLocation, 1, GL_FALSE, map->transform);
  for (size_t i = 0; i < count; i++) {
    if (!geometry[i].indices)
      continue;
    glBindVertexArray(geometry[i].vao);
    glDrawElements(GL_TRIANGLES, geometry[i].indices, GL_UNSIGNED_INT, NULL);
    (*drawCalls)++;
  }
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
  glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
  glUseProgram(program);
  glBindVertexArray(vao);
  glDepthFunc(depthFunc);
  glDepthMask(depthMask);
  glClearDepth(clearDepth);
  glPolygonMode(GL_FRONT, polygon[0]);
  glPolygonMode(GL_BACK, polygon[1]);
  for (size_t i = 0; i < sizeof(capabilities) / sizeof(*capabilities); i++) {
    if (enabled[i])
      glEnable(capabilities[i]);
    else
      glDisable(capabilities[i]);
  }
  map->direction = direction;
  map->valid = glGetError() == GL_NO_ERROR;
  return map->valid;
}

bool initShadowCache(ShadowCache* cache, Vec3 center, float radius) {
  *cache = (ShadowCache){0};
  if (!initShadowMap(&cache->maps[0], center, radius) || !initShadowMap(&cache->maps[1], center, radius)) {
    cleanupShadowCache(cache);
    return false;
  }
  return true;
}

void cleanupShadowCache(ShadowCache* cache) {
  cleanupShadowMap(&cache->maps[0]);
  cleanupShadowMap(&cache->maps[1]);
  memset(cache, 0, sizeof(*cache));
}

static bool cachedAngle(const ShadowCache* cache, int slot, int angle, float planeZ) {
  return cache->maps[slot].valid && cache->angle[slot] == angle && cache->planeZ[slot] == planeZ;
}

bool updateShadowCache(ShadowCache* cache, Vec3 direction, bool edited, const ShadowGeometry* geometry, size_t count, int* drawCalls) {
  const double turn = 6.283185307179586;

  enum { ANGLES = 1024 };

  *drawCalls = 0;
  float length = vec3_dot(&direction, &direction);
  if (!isfinite(length) || length < 1e-12f)
    return false;
  vec3_normalize(&direction, &direction);
  double angle = atan2(direction.y, direction.x);
  if (angle < 0)
    angle += turn;
  double position = angle * ANGLES / turn;
  // Equivalent sun/moon cardinal directions can straddle an integer by a
  // double rounding error. Keep them in the same interval after time commands.
  if (fabs(position - round(position)) < 1e-10)
    position = round(position);
  int lower = (int)floor(position) % ANGLES, upper = (lower + 1) % ANGLES;
  cache->blend = (float)(position - floor(position));
  // The day's orbit is in XY. Retain Z for other fixed light directions used
  // by renderer clients; changing the orbit plane invalidates both endpoints.
  float planeZ = direction.z;
  int first = 0;
  if (cachedAngle(cache, 1, lower, planeZ))
    first = 1;
  else if (!cachedAngle(cache, 0, lower, planeZ) && cachedAngle(cache, 0, upper, planeZ))
    first = 1;
  cache->first = first;
  // Invalidate both before any rendering so a failed edit refresh cannot
  // leave the other endpoint eligible for reuse with stale geometry.
  if (edited)
    cache->maps[0].valid = cache->maps[1].valid = false;
  for (int endpoint = 0; endpoint < 2; endpoint++) {
    int slot = endpoint ? 1 - first : first;
    int key = endpoint ? upper : lower;
    if (cachedAngle(cache, slot, key, planeZ))
      continue;
    double radians = turn * key / ANGLES;
    float xy = sqrtf(fmaxf(0, 1 - planeZ * planeZ));
    Vec3 sample = {(float)cos(radians) * xy, (float)sin(radians) * xy, planeZ};
    int calls;
    bool updated = updateShadowMap(&cache->maps[slot], sample, true, geometry, count, &calls);
    *drawCalls += calls;
    if (!updated)
      return false;
    cache->angle[slot] = key;
    cache->planeZ[slot] = planeZ;
  }
  return true;
}
