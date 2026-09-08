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
#include "../world/player.h"
#include <GLFW/glfw3.h>
#include <stdbool.h>

typedef struct {
  Camera* camera;
  Player player;
  bool flying;
  bool jumpRequested;
  bool modeBlocked;
  int simulationSteps;
} InputState;

bool initInputs(InputState* input, Camera* camera);
void resetInputTiming(InputState* input);
void processInput(GLFWwindow* window, InputState* input, double deltaTime);
void mouseCallback(GLFWwindow* window, double xpos, double ypos);
void setCursorCaptured(GLFWwindow* window, bool captured);
void windowFocusCallback(GLFWwindow* window, int focused);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
int selectedBlock(void);

#endif
