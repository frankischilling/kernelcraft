#include <GL/glew.h>
#include "graphics/camera.h"
#include "graphics/hud.h"
#include "graphics/shader.h"
#include "math/math.h"
#include "utils/inputs.h"
#include "utils/text.h"
#include "world/cube.h"
#include "world/world.h"
#include <GL/freeglut.h>
#include <GLFW/glfw3.h>
#include <stdbool.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#endif

#define BUILD_VERSION "v0.0.3-alpha"
#define BUILD_NAME "kernelcraft"

static double lastTime = 0.0;
static int frameCount = 0;
static float fps = 0.0f;
static Camera camera;
static bool cursorEnabled = false;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    cursorEnabled = !cursorEnabled;
    if (cursorEnabled) {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    } else {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
  }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
}

int main(int argc, char** argv) {
#ifdef _WIN32
  // Explorer, shortcuts, and terminals can start the game in any directory.
  wchar_t executablePath[32768];
  DWORD length = GetModuleFileNameW(NULL, executablePath, sizeof(executablePath) / sizeof(executablePath[0]));
  if (!length || length >= sizeof(executablePath) / sizeof(executablePath[0])) {
    fprintf(stderr, "Failed to locate the executable directory\n");
    return -1;
  }
  wchar_t* separator = wcsrchr(executablePath, L'\\');
  if (!separator) {
    fprintf(stderr, "Invalid executable path\n");
    return -1;
  }
  *separator = L'\0';
  if (!SetCurrentDirectoryW(executablePath)) {
    fprintf(stderr, "Failed to open the executable directory\n");
    return -1;
  }
#endif
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n");
    return -1;
  }

  glutInit(&argc, argv);
  GLFWwindow* window = glfwCreateWindow(1920, 1080, "kernelcraft", NULL, NULL);
  if (!window) {
    fprintf(stderr, "Failed to open GLFW window\n");
    glfwTerminate();
    return -1;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(0);
  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) {
    fprintf(stderr, "Failed to initialize GLEW\n");
    glfwDestroyWindow(window);
    glfwTerminate();
    return -1;
  }
  // Some compatibility drivers leave an error while GLEW probes extensions.
  while (glGetError() != GL_NO_ERROR) {
  }

  glEnable(GL_DEPTH_TEST);

  GLuint shaderProgram = loadShaders("assets/shaders/vertex_shader.glsl", "assets/shaders/fragment_shader.glsl");
  if (!shaderProgram) {
    fprintf(stderr, "Failed to load shaders\n");
    glfwDestroyWindow(window);
    glfwTerminate();
    return -1;
  }

  if (!initChunks() || !initWorld(shaderProgram)) {
    cleanupChunks();
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return -1;
  }
  HUDInit(BUILD_NAME, BUILD_VERSION);

  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetCursorPosCallback(window, mouseCallback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetWindowUserPointer(window, &camera);
  glfwSetKeyCallback(window, key_callback);

  initCamera(&camera);

  double lastFrame = glfwGetTime();
  lastTime = lastFrame;

  while (!glfwWindowShouldClose(window)) {
    double currentFrame = glfwGetTime();
    float deltaTime = (float)(currentFrame - lastFrame);
    lastFrame = currentFrame;

    frameCount++;
    if (currentFrame - lastTime >= 1.0) {
      fps = (float)(frameCount / (currentFrame - lastTime));
      frameCount = 0;
      lastTime = currentFrame;
    }

    processInput(window, &camera, deltaTime);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    if (width == 0 || height == 0) {
      glfwWaitEvents();
      lastFrame = glfwGetTime();
      continue;
    }
    glViewport(0, 0, width, height);
    Mat4 view, projection;
    Vec3 target;
    vec3_add(&target, &camera.position, &camera.front);
    mat4_lookAt(view, &camera.position, &target, &camera.up);
    mat4_perspective(projection, 70.0f, (float)width / height, 0.1f, 1000.0f);
    RenderResult result = renderWorld(&camera, view, projection);

    DebugData data = (DebugData){&camera, fps, result.visisbleCubes};
    HUDDraw(shaderProgram, &data);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  cleanupWorld();
  cleanupChunks();
  glDeleteProgram(shaderProgram);
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
