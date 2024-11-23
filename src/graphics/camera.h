/**
 * @file graphics/camera.h
 * @brief Camera module for handling camera movement and orientation.
 * @author frankischilling
 * @date 2024-11-19
 */
#ifndef CAMERA_H
#define CAMERA_H

#include "../math/math.h"
#include <GLFW/glfw3.h>

typedef struct {
    Vec3 position;
    Vec3 front;
    Vec3 up;
    float yaw;
    float pitch;
    float speed;
    float sensitivity;
} Camera;

// Function declarations
void initCamera(Camera* camera);
void processInput(GLFWwindow* window, Camera* camera, float deltaTime);
void mouseCallback(GLFWwindow* window, double xpos, double ypos);
void updateCameraVectors(Camera* camera);

#endif