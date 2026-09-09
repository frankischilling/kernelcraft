/**
 * @file graphics/camera.h
 * @brief Camera module for handling camera movement and orientation.
 * @author frankischilling
 * @date 2024-11-19
 */
#ifndef CAMERA_H
#define CAMERA_H

#include "../math/math.h"
#include <stdbool.h>

#define CAMERA_BASE_FOV 70.0f
#define CAMERA_RUN_FOV 80.0f
#define CAMERA_FOV_RESPONSE 12.0

typedef struct {
  Vec3 position;
  Vec3 front;
  Vec3 up;
  float yaw;
  float pitch;
  float speed;
  float sensitivity;
  float fov;
} Camera;

// Function declarations
void initCamera(Camera* camera);
void updateCameraVectors(Camera* camera);
// Smooth the running cue by elapsed time, with at most 0.1 seconds per frame.
void updateCameraFov(Camera* camera, bool running, double seconds);

#endif
