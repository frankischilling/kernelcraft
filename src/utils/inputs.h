/**
 * @file utils/inputs.h
 * @brief Input processing module for handling user input.
 * @author frankischilling
 * @version 0.1
 * @date 2024-11-23
 *
 */
#ifndef INPUTS_H
#define INPUTS_H

#include "../graphics/camera.h"
#include <GLFW/glfw3.h>
#include <stdbool.h>

void processInput(GLFWwindow* window, Camera* camera, float deltaTime);
void mouseCallback(GLFWwindow* window, double xpos, double ypos);
void setCursorCaptured(GLFWwindow* window, bool captured);
void windowFocusCallback(GLFWwindow* window, int focused);

#endif
