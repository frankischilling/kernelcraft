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
#include "../world/hotbar.h"
#include <GLFW/glfw3.h>
#include <stdbool.h>

static double lastX, lastY;
static bool firstMouse = true;
static int selectedSlot;

static void cancelBreaking(InputState* input) {
  if (input) {
    input->breakHeld = false;
    resetBlockBreaking(&input->breaking);
  }
}

bool initInputs(InputState* input, Camera* camera) {
  *input = (InputState){.camera = camera};
  camera->fov = CAMERA_BASE_FOV;
  if (!playerFindSpawn(&input->player, camera->position))
    return false;
  camera->position = playerEyePosition(&input->player);
  selectedSlot = 0;
  return true;
}

bool initSavedInputs(InputState* input, Camera* camera, const SavedPlayer* saved) {
  *input = (InputState){.camera = camera};
  camera->fov = CAMERA_BASE_FOV;
  if (!saved || saved->selectedSlot < 0 || saved->selectedSlot >= HOTBAR_SLOT_COUNT || !playerSetPosition(&input->player, saved->feet))
    return false;
  camera->position = playerEyePosition(&input->player);
  camera->yaw = saved->yaw;
  camera->pitch = saved->pitch;
  updateCameraVectors(camera);
  selectedSlot = saved->selectedSlot;
  return true;
}

static void resetInputTiming(InputState* input) {
  if (!input)
    return;
  playerResetTiming(&input->player);
  playerResetRunInput(&input->runInput);
  input->camera->fov = CAMERA_BASE_FOV;
  input->jumpRequested = false;
  input->simulationSteps = 0;
  cancelBreaking(input);
}

void pauseInput(InputState* input) {
  // A pause may deliver no cursor events. The next position starts a new delta.
  firstMouse = true;
  resetInputTiming(input);
}

void setCursorCaptured(GLFWwindow* window, bool captured) {
  // GLFW may move the cursor while changing mode. Discard the next delta.
  pauseInput(glfwGetWindowUserPointer(window));
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
  return hotbarBlock(selectedSlot);
}

int selectedHotbarSlot(void) {
  return selectedSlot;
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
  if (!playerSetPosition(&standing, feet) && ((!input->flying && !input->player.crouched) || !playerFindSpawn(&standing, feet)))
    return false;
  float yaw = fmodf(input->camera->yaw, 360.0f);
  if (yaw < 0)
    yaw += 360.0f;
  // Adding 360 can round a tiny negative remainder to exactly 360.
  if (yaw >= 360)
    yaw = 0;
  *saved = (SavedPlayer){.feet = standing.position, .yaw = yaw, .pitch = input->camera->pitch, .selectedSlot = selectedSlot};
  return true;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  (void)scancode;
  (void)mods;
  InputState* input = glfwGetWindowUserPointer(window);
  if (input && !input->flying && acceptsEditing(window)) {
    bool crouchHeld = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    if ((action == GLFW_PRESS && (key == GLFW_KEY_S || key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)) || crouchHeld || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
        input->player.crouched) {
      playerResetRunInput(&input->runInput);
      input->player.running = false;
    } else if (key == GLFW_KEY_W && (action == GLFW_PRESS || action == GLFW_RELEASE)) {
      playerForwardEvent(&input->runInput, action == GLFW_PRESS, glfwGetTime());
      if (action == GLFW_RELEASE)
        input->player.running = false;
    }
  }
  if (action != GLFW_PRESS)
    return;
  if (key == GLFW_KEY_ESCAPE && acceptsWindowInput(window)) {
    setCursorCaptured(window, glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED);
    return;
  }
  // Diagnostics remain accessible while the cursor is released. They never
  // resume movement or alter the world, and repeats are rejected above.
  if (input && key == GLFW_KEY_F3 && acceptsWindowInput(window)) {
    input->showDebug = !input->showDebug;
    return;
  }
  if (input && key == GLFW_KEY_F4 && acceptsWindowInput(window)) {
    input->wireframe = !input->wireframe;
    return;
  }
  if (acceptsEditing(window) && key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
    if (selectedSlot != key - GLFW_KEY_1)
      cancelBreaking(input);
    selectedSlot = key - GLFW_KEY_1;
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
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
    cancelBreaking(input);
    return;
  }
  if (!input || action != GLFW_PRESS || !acceptsEditing(window))
    return;
  Camera* camera = input->camera;
  if (button == GLFW_MOUSE_BUTTON_LEFT && !input->breakHeld) {
    input->breakHeld = true;
    advanceBlockBreaking(&input->breaking, camera->position, camera->front, 0);
  }
  if (button == GLFW_MOUSE_BUTTON_RIGHT) {
    cancelBreaking(input);
    editTarget(camera->position, camera->front, inputBodyFeet(input), !input->flying && input->player.crouched, selectedBlock(), true);
  }
}

void processBlockBreaking(GLFWwindow* window, InputState* input, double deltaTime) {
  if (!input)
    return;
  if (!acceptsEditing(window)) {
    cancelBreaking(input);
    return;
  }
  if (input->breakHeld)
    advanceBlockBreaking(&input->breaking, input->camera->position, input->camera->front, deltaTime);
}

void processInput(GLFWwindow* window, InputState* input, double deltaTime) {
  if (!input)
    return;
  input->simulationSteps = 0;
  if (!acceptsEditing(window)) {
    pauseInput(input);
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
    bool crouch = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    if (crouch || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || input->player.crouched)
      playerResetRunInput(&input->runInput);
    else if (glfwGetKey(window, GLFW_KEY_W) != GLFW_PRESS)
      playerForwardEvent(&input->runInput, false, glfwGetTime());
    PlayerMotion motion = {.wish = wish, .jump = input->jumpRequested, .crouch = crouch, .run = input->runInput.running && longitudinal > 0};
    input->simulationSteps = playerAdvance(&input->player, motion, deltaTime);
    input->jumpRequested = false;
    camera->position = playerEyePosition(&input->player);
    updateCameraFov(camera, input->player.running, deltaTime);
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
