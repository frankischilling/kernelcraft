#include "../world/world.h"
#include "raycast.h"

Ray rayCast(const Camera* camera) {
  Vec3 rayOrigin = {camera->position[0], camera->position[1], camera->position[2]};
  Vec3 rayDirection;
  vec3_add(rayDirection, camera->front, camera->up);
  for (float t = 0.1f; t < 100000; t += 0.1f) {
    Vec3 rayPoint = {rayOrigin[0] + rayDirection[0] * t, rayOrigin[1] + rayDirection[1] * t, rayOrigin[2] + rayDirection[2] * t};
    if (getBlockType(rayPoint[0], rayPoint[1], rayPoint[2]) != BLOCK_AIR) {
      Ray hitRay = {1, {rayPoint[0], rayPoint[1], rayPoint[2]}};
      return hitRay;
    }
  }
  return (Ray){false, {0.0f, 0.0f, 0.0f}};
}