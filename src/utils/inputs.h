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
#include "../world/save.h"
#include <GLFW/glfw3.h>
#include <stdbool.h>

typedef struct {
  Camera* camera;
  Player player;
  bool flying;
  bool jumpRequested;
  bool saveRequested;
  bool modeBlocked;
  bool showDebug;
  int simulationSteps;
} InputState;

bool initInputs(InputState* input, Camera* camera);
// The caller supplies state already validated by loadWorld.
bool initSavedInputs(InputState* input, Camera* camera, const SavedPlayer* saved);
bool snapshotPlayer(const InputState* input, SavedPlayer* saved);
// Discard simulation backlog, queued jumps, and the cached mouse position.
void pauseInput(InputState* input);
void processInput(GLFWwindow* window, InputState* input, double deltaTime);
void mouseCallback(GLFWwindow* window, double xpos, double ypos);
void setCursorCaptured(GLFWwindow* window, bool captured);
void windowFocusCallback(GLFWwindow* window, int focused);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
int selectedBlock(void);
int selectedHotbarSlot(void);

#endif
