/**
 * @file graphics/camera.c
 * @brief Camera module for handling camera movement and orientation.
 * @author frankischilling
 * @date 2024-11-19
 */
#include <math.h>
#include <stdbool.h>

#include "../math/math.h"
#include "camera.h"
#include "../utils/raycast.h"

void initCamera(Camera* camera) {
  camera->position.x = 0.0f;
  camera->position.y = 10.0f; // Set the camera above the terrain
  camera->position.z = 3.0f;

  camera->front = (Vec3)VEC3_FRONT;
  camera->up = (Vec3)VEC3_UP;

  camera->yaw = 90.0f;
  camera->pitch = 0.0f;
  camera->speed = 10.0f;
  camera->sensitivity = 0.05f;
  camera->fov = CAMERA_BASE_FOV;

  updateCameraVectors(camera);
}

void updateCameraVectors(Camera* camera) {
  // Calculate new front vector
  camera->front.x = cosf(toRadians(camera->yaw)) * cosf(toRadians(camera->pitch));
  camera->front.y = sinf(toRadians(camera->pitch));
  camera->front.z = sinf(toRadians(camera->yaw)) * cosf(toRadians(camera->pitch));

  // Normalize the front vector
  vec3_normalize(&camera->front, &camera->front);
}

void updateCameraFov(Camera* camera, bool running, double seconds) {
  if (!isfinite(seconds) || seconds <= 0)
    return;
  float target = running ? CAMERA_RUN_FOV : CAMERA_BASE_FOV;
  double blend = -expm1(-CAMERA_FOV_RESPONSE * fmin(seconds, 0.1));
  camera->fov += (float)((target - camera->fov) * blend);
}

bool makeThirdPersonCamera(Camera* result, const Camera* eye, bool frontView, float aspect) {
  if (!result || !eye)
    return false;
  *result = *eye;
  if (!isfinite(aspect) || aspect <= 0 || !isfinite(eye->fov) || eye->fov <= 0 || eye->fov >= 180)
    return false;
  Vec3 direction, right, up;
  vec3_normalize(&direction, &eye->front);
  if (!isfinite(direction.x) || !isfinite(direction.y) || !isfinite(direction.z) || vec3_dot(&direction, &direction) < 0.5f)
    return false;
  vec3_cross(&right, &direction, &eye->up);
  vec3_normalize(&right, &right);
  vec3_cross(&up, &right, &direction);
  if (!frontView)
    vec3_scale(&direction, &direction, -1);
  // Sweep the center and near-plane corners/edges so the wide end of a
  // landscape viewport cannot peek through a wall that the center ray misses.
  float halfHeight = 0.1f * tanf(toRadians(eye->fov) * 0.5f) + 0.03f;
  float halfWidth = halfHeight * aspect;
  float distance = 3.0f;
  for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++) {
      Vec3 origin = {eye->position.x + right.x * x * halfWidth + up.x * y * halfHeight, eye->position.y + right.y * x * halfWidth + up.y * y * halfHeight,
                     eye->position.z + right.z * x * halfWidth + up.z * y * halfHeight};
      Ray obstruction = rayCast(origin, direction, 3.2f);
      if (obstruction.hit)
        distance = fminf(distance, obstruction.distance - 0.2f);
    }
  if (distance < 0.8f)
    return false;
  result->position = (Vec3){eye->position.x + direction.x * distance, eye->position.y + direction.y * distance, eye->position.z + direction.z * distance};
  if (frontView) {
    vec3_scale(&result->front, &eye->front, -1);
    result->yaw = eye->yaw + 180;
    result->pitch = -eye->pitch;
  }
  return true;
}
