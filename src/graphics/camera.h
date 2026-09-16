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

typedef enum { CAMERA_FIRST_PERSON, CAMERA_THIRD_PERSON_BACK, CAMERA_THIRD_PERSON_FRONT, CAMERA_VIEW_COUNT } CameraView;

// Function declarations
void initCamera(Camera* camera);
void updateCameraVectors(Camera* camera);
// Smooth the running cue by elapsed time, with at most 0.1 seconds per frame.
void updateCameraFov(Camera* camera, bool running, double seconds);
// Derive a display camera without moving the authoritative eye/aim. Returns
// false (and the original camera) when nearby terrain leaves no room for a body.
bool makeThirdPersonCamera(Camera* result, const Camera* eye, bool frontView, float aspect);

#endif
