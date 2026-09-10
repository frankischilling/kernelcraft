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
#include "../world/edit.h"
#include "../world/save.h"
#include "../world/chat.h"
#include <GLFW/glfw3.h>
#include <stdbool.h>

typedef struct {
  Camera* camera;
  Chat chat;
  DayNightClock clock;
  Player player;
  PlayerRunInput runInput;
  BlockBreaking breaking;
  bool breakHeld;
  bool flying;
  bool jumpRequested;
  bool saveRequested;
  bool modeBlocked;
  bool showDebug;
  bool wireframe;
  int simulationSteps;
} InputState;

bool initInputs(InputState* input, Camera* camera);
// The caller supplies state already validated by loadWorld.
bool initSavedInputs(InputState* input, Camera* camera, const SavedPlayer* saved);
bool snapshotPlayer(const InputState* input, SavedPlayer* saved);
// Discard simulation backlog, queued jumps, run/tap state, and cached mouse position.
// Retain the current body until active simulation can check standing clearance.
void pauseInput(InputState* input);
void processInput(GLFWwindow* window, InputState* input, double deltaTime);
// Run after movement and before rendering/saving so selection uses the new eye.
void processBlockBreaking(GLFWwindow* window, InputState* input, double deltaTime);
void mouseCallback(GLFWwindow* window, double xpos, double ypos);
void setCursorCaptured(GLFWwindow* window, bool captured);
void windowFocusCallback(GLFWwindow* window, int focused);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void characterCallback(GLFWwindow* window, unsigned int codepoint);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
int selectedBlock(void);
int selectedHotbarSlot(void);

#endif
