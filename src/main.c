#if defined(__linux__)
#define _POSIX_C_SOURCE 200809L
#endif
#include <GL/glew.h>
#include "graphics/camera.h"
#include "graphics/hud.h"
#include "graphics/shader.h"
#include "graphics/world_renderer.h"
#include "graphics/selection.h"
#include "math/math.h"
#include "utils/inputs.h"
#include "utils/text.h"
#include "world/cube.h"
#include "world/world.h"
#include "world/edit.h"
#include <GL/freeglut.h>
#include <GLFW/glfw3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <errno.h>
#include <unistd.h>
#endif
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
static InputState input;

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
  (void)window;
  glViewport(0, 0, width, height);
}

static void error_callback(int error, const char* description) {
  fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

int main(int argc, char** argv) {
#ifdef _WIN32
  // Explorer, shortcuts, and terminals can start the game in any directory.
  wchar_t executablePath[32768];
  DWORD length = GetModuleFileNameW(NULL, executablePath, sizeof(executablePath) / sizeof(executablePath[0]));
  if (!length || length >= sizeof(executablePath) / sizeof(executablePath[0])) {
    fprintf(stderr, "Failed to locate the executable directory\n");
    return EXIT_FAILURE;
  }
  wchar_t* separator = wcsrchr(executablePath, L'\\');
  if (!separator) {
    fprintf(stderr, "Invalid executable path\n");
    return EXIT_FAILURE;
  }
  *separator = L'\0';
  if (!SetCurrentDirectoryW(executablePath)) {
    fprintf(stderr, "Failed to open the executable directory\n");
    return EXIT_FAILURE;
  }
#elif defined(__linux__)
  // Match the packaged Windows layout without depending on the launch directory.
  char executablePath[4096];
  ssize_t length = readlink("/proc/self/exe", executablePath, sizeof(executablePath) - 1);
  if (length < 0 || (size_t)length >= sizeof(executablePath) - 1) {
    fprintf(stderr, "Failed to locate the executable directory: %s\n", length < 0 ? strerror(errno) : "path too long");
    return EXIT_FAILURE;
  }
  executablePath[length] = '\0';
  char* separator = strrchr(executablePath, '/');
  if (!separator) {
    fprintf(stderr, "Invalid executable path\n");
    return EXIT_FAILURE;
  }
  separator[separator == executablePath ? 1 : 0] = '\0';
  if (chdir(executablePath) != 0) {
    fprintf(stderr, "Failed to open the executable directory: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }
#endif
  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n");
    return EXIT_FAILURE;
  }

  glutInit(&argc, argv);
  // GLSL 330 and chunk buffers need 3.3; FreeGLUT bitmap text uses legacy GL.
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
  GLFWwindow* window = glfwCreateWindow(1920, 1080, "kernelcraft", NULL, NULL);
  if (!window) {
    fprintf(stderr, "Failed to open an OpenGL 3.3 compatibility window\n");
    glfwTerminate();
    return EXIT_FAILURE;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(0);
  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) {
    fprintf(stderr, "Failed to initialize GLEW\n");
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
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
    return EXIT_FAILURE;
  }

  if (!initChunks() || !initWorld(shaderProgram)) {
    cleanupChunks();
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }
  HUDInit(BUILD_NAME, BUILD_VERSION);

  initCamera(&camera);
  if (!initInputs(&input, &camera)) {
    fprintf(stderr, "Failed to find a clear player spawn\n");
    cleanupWorld();
    cleanupChunks();
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }
  glfwSetWindowUserPointer(window, &input);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetCursorPosCallback(window, mouseCallback);
  glfwSetWindowFocusCallback(window, windowFocusCallback);
  glfwSetKeyCallback(window, keyCallback);
  glfwSetMouseButtonCallback(window, mouseButtonCallback);
  setCursorCaptured(window, true);

  double lastFrame = glfwGetTime();
  lastTime = lastFrame;

  int exitStatus = EXIT_SUCCESS;
  while (!glfwWindowShouldClose(window)) {
    double currentFrame = glfwGetTime();
    double deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    frameCount++;
    if (currentFrame - lastTime >= 1.0) {
      fps = (float)(frameCount / (currentFrame - lastTime));
      frameCount = 0;
      lastTime = currentFrame;
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    if (width == 0 || height == 0) {
      resetInputTiming(&input);
      glfwWaitEvents();
      lastFrame = glfwGetTime();
      continue;
    }
    processInput(window, &input, deltaTime);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, width, height);
    Mat4 view, projection;
    Vec3 target;
    vec3_add(&target, &camera.position, &camera.front);
    mat4_lookAt(view, &camera.position, &target, &camera.up);
    mat4_perspective(projection, 70.0f, (float)width / height, 0.1f, 1000.0f);
    RenderResult result = renderWorld(&camera, view, projection);

    if (!result.success) {
      exitStatus = EXIT_FAILURE;
      break;
    }
    Ray selection = rayCast(camera.position, camera.front, EDIT_REACH);
    drawSelection(&selection, view, projection);
    DebugData data = {.camera = &camera,
                      .fps = fps,
                      .visibleBlocks = result.surfaceBlocks,
                      .selection = selection,
                      .selectedBlock = selectedBlock(),
                      .captured = glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED,
                      .flying = input.flying,
                      .grounded = input.player.grounded,
                      .modeBlocked = input.modeBlocked,
                      .simulationSteps = input.simulationSteps,
                      .stats = &result};
    HUDDraw(shaderProgram, &data);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  cleanupWorld();
  cleanupChunks();
  glDeleteProgram(shaderProgram);
  glfwDestroyWindow(window);
  glfwTerminate();
  return exitStatus;
}
