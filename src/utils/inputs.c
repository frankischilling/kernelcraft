#include <GLFW/glfw3.h>
#include <stdbool.h>
#include "../graphics/camera.h"
#include "inputs.h"

// Declare the variables
static float lastX = 400.0f;  // Initial value, adjust as needed
static float lastY = 300.0f;  // Initial value, adjust as needed
static bool firstMouse = true;

void processInput(GLFWwindow* window, Camera* camera, float deltaTime) {
    float velocity = camera->speed * deltaTime;
    Vec3 temp;
    
    // Forward/Backward
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        vec3_scale(temp, camera->front, velocity);
        vec3_add(camera->position, camera->position, temp);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        vec3_scale(temp, camera->front, velocity);
        vec3_subtract(camera->position, camera->position, temp);
    }
    
    // Calculate right vector
    Vec3 right;
    vec3_cross(right, camera->front, camera->up);
    vec3_normalize(right, right);
    
    // Left/Right
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        vec3_scale(temp, right, velocity);
        vec3_subtract(camera->position, camera->position, temp);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        vec3_scale(temp, right, velocity);
        vec3_add(camera->position, camera->position, temp);
    }
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    Camera* camera = (Camera*)glfwGetWindowUserPointer(window);
    
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
    if (camera->pitch > 89.0f) camera->pitch = 89.0f;
    if (camera->pitch < -89.0f) camera->pitch = -89.0f;
    
    // Update camera vectors
    updateCameraVectors(camera);
}