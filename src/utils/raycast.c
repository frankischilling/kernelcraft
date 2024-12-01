#include "../world/world.h"
#include "raycast.h"

Ray rayCast(const Camera* camera) {
  Vec3 rayOrigin = {camera->position.x, camera->position.y, camera->position.z};
  Vec3 rayDirection = {camera->front.x, camera->front.y, camera->front.z};
  
  vec3_normalize(&rayDirection, &rayDirection);
  for (float t = 0.1f; t < 10; t += 0.1f) {
    Vec3 rayPoint = {rayOrigin.x + rayDirection.x * t, rayOrigin.y + rayDirection.y * t, rayOrigin.z + rayDirection.z * t};
    if (getBlockType(rayPoint.x, rayPoint.y, rayPoint.z) != BLOCK_AIR) {
      Ray hitRay = {1, {rayPoint.x, rayPoint.y, rayPoint.z}};
      return hitRay;
    }
  }
  return (Ray){false, {0.0f, 0.0f, 0.0f}};
}