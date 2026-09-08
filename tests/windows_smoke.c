// Run the application's real startup and shutdown without opening a visible window.
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

GLFWwindow* __real_glfwCreateWindow(int width, int height, const char* title, GLFWmonitor* monitor, GLFWwindow* share);
GLFWwindow* __wrap_glfwCreateWindow(int width, int height, const char* title, GLFWmonitor* monitor, GLFWwindow* share) {
  (void)width;
  (void)height;
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
  return __real_glfwCreateWindow(640, 360, title, monitor, share);
}

int __wrap_glfwWindowShouldClose(GLFWwindow* window) {
  (void)window;
  static int frames;
  return frames++ >= 3;
}

void __wrap_glfwSetInputMode(GLFWwindow* window, int mode, int value) {
  // Keep the user's pointer free during this hidden startup test.
  (void)window;
  (void)mode;
  (void)value;
}

void __real_glfwDestroyWindow(GLFWwindow* window);
void __wrap_glfwDestroyWindow(GLFWwindow* window) {
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    fprintf(stderr, "OpenGL startup/shutdown error: %u\n", error);
    exit(EXIT_FAILURE);
  }
  __real_glfwDestroyWindow(window);
}
