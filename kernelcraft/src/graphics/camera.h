#ifndef CAMERA_H
#define CAMERA_H

#include <GLFW/glfw3.h>
#include "../math/math.h"

typedef struct {
    Vec3 position;
    Vec3 front;
    Vec3 up;
    float yaw;
    float pitch;
    float speed;
    float sensitivity;
} Camera;

void initCamera(Camera* camera);
void processInput(GLFWwindow* window, Camera* camera, float deltaTime);
void mouseCallback(GLFWwindow* window, double xpos, double ypos);
void updateCameraVectors(Camera* camera);
#endif