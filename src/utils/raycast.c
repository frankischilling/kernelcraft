#include "raycast.h"

Ray rayCast(const Camera* camera) {
  Vec3 rayOrigin = camera->position;
  Vec3 rayDirection = camera->front;

  vec3_normalize(&rayDirection, &rayDirection);
  for (float t = 0.1f; t < 10; t += 0.1f) {
    Vec3 rayPoint = {rayOrigin.x + rayDirection.x * t, rayOrigin.y + rayDirection.y * t, rayOrigin.z + rayDirection.z * t};
    Vec3i onGrid = getPositionOnGrid(&rayPoint);
    if (!getBlock(&onGrid)) {
      Ray hitRay = {1, rayPoint};
      return hitRay;
    }
  }
  return (Ray){0, VEC3_ZERO};
}