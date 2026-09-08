/**
 * @file utils/inputs.c
 * @brief Input processing module for handling user input.
 * @author frankischilling
 * @version 0.1
 * @date 2024-11-23
 *
 */
#include "inputs.h"
#include "../graphics/camera.h"
#include "../world/edit.h"
#include <GLFW/glfw3.h>
#include <stdbool.h>

static double lastX, lastY;
static bool firstMouse = true;
static int selected = BLOCK_GRASS;

bool initInputs(InputState* input, Camera* camera) {
  *input = (InputState){.camera = camera};
  if (!playerFindSpawn(&input->player, camera->position))
    return false;
  camera->position = playerEyePosition(&input->player);
  return true;
}

bool initSavedInputs(InputState* input, Camera* camera, const SavedPlayer* saved) {
  *input = (InputState){.camera = camera};
  if (!saved || !playerSetPosition(&input->player, saved->feet))
    return false;
  camera->position = playerEyePosition(&input->player);
  camera->yaw = saved->yaw;
  camera->pitch = saved->pitch;
  updateCameraVectors(camera);
  selected = saved->selectedBlock;
  return true;
}

void resetInputTiming(InputState* input) {
  if (!input)
    return;
  // A pause may deliver no cursor events. The next position starts a new delta.
  firstMouse = true;
  playerResetTiming(&input->player);
  input->jumpRequested = false;
  input->simulationSteps = 0;
}

void setCursorCaptured(GLFWwindow* window, bool captured) {
  // GLFW may move the cursor while changing mode. Discard the next delta.
  firstMouse = true;
  resetInputTiming(glfwGetWindowUserPointer(window));
  glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void windowFocusCallback(GLFWwindow* window, int focused) {
  firstMouse = true;
  if (!focused)
    setCursorCaptured(window, false);
}

static bool acceptsWindowInput(GLFWwindow* window) {
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  return width > 0 && height > 0 && glfwGetWindowAttrib(window, GLFW_FOCUSED) && !glfwGetWindowAttrib(window, GLFW_ICONIFIED);
}

static bool acceptsEditing(GLFWwindow* window) {
  return acceptsWindowInput(window) && glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
}

int selectedBlock(void) {
  return selected;
}

static Vec3 inputBodyFeet(const InputState* input) {
  if (!input->flying)
    return input->player.position;
  Vec3 feet = input->camera->position;
  Vec3 initialEye = playerEyePosition(&input->player);
  // Preserve the exact standing height when flight starts. Subtracting the eye
  // offset directly can round below the floor even without any camera motion.
  feet.y = (float)(input->player.position.y + ((double)feet.y - initialEye.y));
  return feet;
}

bool snapshotPlayer(const InputState* input, SavedPlayer* saved) {
  if (!input || !input->camera || !saved)
    return false;
  Player standing = {0};
  Vec3 feet = inputBodyFeet(input);
  if (!playerSetPosition(&standing, feet) && (!input->flying || !playerFindSpawn(&standing, feet)))
    return false;
  float yaw = fmodf(input->camera->yaw, 360.0f);
  if (yaw < 0)
    yaw += 360.0f;
  // Adding 360 can round a tiny negative remainder to exactly 360.
  if (yaw >= 360)
    yaw = 0;
  *saved = (SavedPlayer){.feet = standing.position, .yaw = yaw, .pitch = input->camera->pitch, .selectedBlock = selected};
  return true;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  (void)scancode;
  (void)mods;
  if (action != GLFW_PRESS)
    return;
  if (key == GLFW_KEY_ESCAPE && acceptsWindowInput(window)) {
    setCursorCaptured(window, glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED);
    return;
  }
  InputState* input = glfwGetWindowUserPointer(window);
  // Diagnostics remain accessible while the cursor is released. They never
  // resume movement or alter the world, and repeats are rejected above.
  if (input && key == GLFW_KEY_F3 && acceptsWindowInput(window)) {
    input->showDebug = !input->showDebug;
    return;
  }
  if (acceptsEditing(window) && key >= GLFW_KEY_1 && key <= GLFW_KEY_3) {
    const int materials[] = {BLOCK_GRASS, BLOCK_DIRT, BLOCK_STONE};
    selected = materials[key - GLFW_KEY_1];
  }
  if (!input || !acceptsEditing(window))
    return;
  if (key == GLFW_KEY_F5)
    input->saveRequested = true;
  if (key == GLFW_KEY_SPACE && !input->flying)
    input->jumpRequested = true;
  if (key == GLFW_KEY_F) {
    resetInputTiming(input);
    input->modeBlocked = false;
    if (!input->flying) {
      input->flying = true;
    } else {
      Vec3 feet = inputBodyFeet(input);
      if (playerSetPosition(&input->player, feet) || playerFindSpawn(&input->player, feet)) {
        input->flying = false;
        input->camera->position = playerEyePosition(&input->player);
      } else {
        input->modeBlocked = true;
      }
    }
  }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
  (void)mods;
  InputState* input = glfwGetWindowUserPointer(window);
  if (!input || action != GLFW_PRESS || !acceptsEditing(window))
    return;
  Camera* camera = input->camera;
  Vec3 feet = inputBodyFeet(input);
  if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT)
    editTarget(camera->position, camera->front, feet, selected, button == GLFW_MOUSE_BUTTON_RIGHT);
}

void processInput(GLFWwindow* window, InputState* input, double deltaTime) {
  if (!input)
    return;
  input->simulationSteps = 0;
  if (!acceptsEditing(window)) {
    resetInputTiming(input);
    return;
  }
  if (!isfinite(deltaTime) || deltaTime <= 0)
    return;
  Camera* camera = input->camera;
  if (!input->flying) {
    float yaw = toRadians(camera->yaw);
    Vec3 forward = {cosf(yaw), 0, sinf(yaw)}, wish = {0};
    int longitudinal = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) - (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS);
    int lateral = (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) - (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS);
    wish.x = forward.x * longitudinal - forward.z * lateral;
    wish.z = forward.z * longitudinal + forward.x * lateral;
    input->simulationSteps = playerAdvance(&input->player, wish, input->jumpRequested, deltaTime);
    input->jumpRequested = false;
    camera->position = playerEyePosition(&input->player);
    return;
  }
  float velocity = camera->speed * (float)fmin(deltaTime, 0.1);
  Vec3 temp;

  // Forward/Backward
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
    vec3_scale(&temp, &camera->front, velocity);
    vec3_add(&camera->position, &camera->position, &temp);
  }
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
    vec3_scale(&temp, &camera->front, velocity);
    vec3_subtract(&camera->position, &camera->position, &temp);
  }
  // Up/Down
  if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
    vec3_scale(&temp, &camera->up, velocity);
    vec3_add(&camera->position, &camera->position, &temp);
  }
  if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
    vec3_scale(&temp, &camera->up, velocity);
    vec3_subtract(&camera->position, &camera->position, &temp);
  }

  Vec3 right;
  vec3_cross(&right, &camera->front, &camera->up);
  vec3_normalize(&right, &right);

  // Left/Right
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
    vec3_scale(&temp, &right, velocity);
    vec3_add(&camera->position, &camera->position, &temp);
  }
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
    vec3_scale(&temp, &right, velocity);
    vec3_subtract(&camera->position, &camera->position, &temp);
  }
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
  InputState* input = glfwGetWindowUserPointer(window);
  if (!input || !acceptsEditing(window)) {
    firstMouse = true;
    return;
  }
  Camera* camera = input->camera;

  if (firstMouse) {
    lastX = xpos;
    lastY = ypos;
    firstMouse = false;
    return;
  }

  float xoffset = xpos - lastX;
  float yoffset = lastY - ypos; // Reversed since y-coordinates range from bottom to top
  lastX = xpos;
  lastY = ypos;

  xoffset *= camera->sensitivity;
  yoffset *= camera->sensitivity;

  camera->yaw += xoffset;
  camera->pitch += yoffset;

  // Constrain pitch
  if (camera->pitch > 89.0f)
    camera->pitch = 89.0f;
  if (camera->pitch < -89.0f)
    camera->pitch = -89.0f;

  // Update camera vectors
  updateCameraVectors(camera);
}
