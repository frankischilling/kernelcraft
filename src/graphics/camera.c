/**
 * @file graphics/camera.c
 * @brief Camera module for handling camera movement and orientation.
 * @author frankischilling
 * @date 2024-11-19
 */
#include <GLFW/glfw3.h>
#include <math.h>
#include <stdbool.h>

#include "../math/math.h"
#include "../utils/inputs.h"
#include "camera.h"

void initCamera(Camera* camera) {
  camera->position.x = 0.0f;
  camera->position.y = 10.0f; // Set the camera above the terrain
  camera->position.z = 3.0f;

  camera->front.x = 0.0f;
  camera->front.y = 0.0f;
  camera->front.z = -1.0f;

  camera->up.x = 0.0f;
  camera->up.y = 1.0f;
  camera->up.z = 0.0f;

  camera->yaw = -90.0f;
  camera->pitch = 0.0f;
  camera->speed = 10.0f;
  camera->sensitivity = 0.05f;

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

void updateViewMatrix(Camera* camera, float* viewMatrix) {
  // Calculate the new front vector
  float frontX = cosf(camera->yaw) * cosf(camera->pitch);
  float frontY = sinf(camera->pitch);
  float frontZ = sinf(camera->yaw) * cosf(camera->pitch);
  camera->front.x = frontX;
  camera->front.y = frontY;
  camera->front.z = frontZ;

  // Normalize the front vector
  vec3_normalize(&camera->front, &camera->front);

  // Calculate the right and up vectors
  Vec3 right = {camera->up.y * camera->front.z - camera->up.z * camera->front.y, camera->up.z * camera->front.x - camera->up.x * camera->front.z,
                camera->up.x * camera->front.y - camera->up.y * camera->front.x};

  // Normalize the right vector
  vec3_normalize(&right, &right);

  // Recalculate the up vector
  Vec3 newUp = {right.y * camera->front.z - right.z * camera->front.y, right.z * camera->front.x - right.x * camera->front.z,
                right.x * camera->front.y - right.y * camera->front.x};

  // Construct the view matrix
  // This is a simplified version; typically, you'd use a library for this
  viewMatrix[0] = right.x;
  viewMatrix[1] = newUp.x;
  viewMatrix[2] = -camera->front.x;
  viewMatrix[3] = 0.0f;

  viewMatrix[4] = right.y;
  viewMatrix[5] = newUp.y;
  viewMatrix[6] = -camera->front.y;
  viewMatrix[7] = 0.0f;

  viewMatrix[8] = right.z;
  viewMatrix[9] = newUp.z;
  viewMatrix[10] = -camera->front.z;
  viewMatrix[11] = 0.0f;

  viewMatrix[12] = -camera->position.x;
  viewMatrix[13] = -camera->position.y;
  viewMatrix[14] = -camera->position.z;
  viewMatrix[15] = 1.0f;
}
