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
