#ifndef INPUTS_H
#define INPUTS_H

#include <GLFW/glfw3.h>
#include "../graphics/camera.h"

void processInput(GLFWwindow* window, Camera* camera, float deltaTime);
void mouseCallback(GLFWwindow* window, double xpos, double ypos);

#endif