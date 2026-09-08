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
#include <GLFW/glfw3.h>
#include <stdbool.h>

static double lastX, lastY;
static bool firstMouse = true;

void setCursorCaptured(GLFWwindow* window, bool captured) {
  // GLFW may move the cursor while changing mode. Discard the next delta.
  firstMouse = true;
  glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void windowFocusCallback(GLFWwindow* window, int focused) {
  firstMouse = true;
  if (!focused)
    setCursorCaptured(window, false);
}

static bool acceptsMovement(GLFWwindow* window) {
  return glfwGetWindowAttrib(window, GLFW_FOCUSED) && glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
}

void processInput(GLFWwindow* window, Camera* camera, float deltaTime) {
  if (!camera || !acceptsMovement(window) || !isfinite(deltaTime) || deltaTime <= 0)
    return;
  float velocity = camera->speed * deltaTime;
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
  Camera* camera = (Camera*)glfwGetWindowUserPointer(window);
  if (!camera || !acceptsMovement(window)) {
    firstMouse = true;
    return;
  }

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
