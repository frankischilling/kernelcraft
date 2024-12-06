#include <GL/glew.h>
#include <GL/freeglut.h>
#include <GLFW/glfw3.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>   
#include "graphics/camera.h"
#include "graphics/cube.h"
#include "graphics/shader.h"
#include "math/math.h"
#include "utils/inputs.h"
#include "utils/text.h"
#include "world/world.h"
#include "graphics/hud.h"
#include "gui/menu.h"
#define BUILD_VERSION "v0.0.3-alpha"
#define BUILD_NAME "kernelcraft"

static double lastTime = 0.0;
static int frameCount = 0;
static float fps = 0.0f;
float lastX = 1920.0f / 2.0f;
float lastY = 1080.0f / 2.0f;
bool firstMouse = true;
static Camera camera;
static bool cursorEnabled = false;
GLuint shaderProgram;
static MenuState currentMenuState = MENU_MAIN;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        cursorEnabled = !cursorEnabled;
        if (cursorEnabled) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
    // Handle other global keys here if necessary
}

void char_callback(GLFWwindow* window, unsigned int codepoint) {
    if (nameInputCursor < MAX_WORLD_NAME - 1 && isprint(codepoint)) {
        worldNameInput[nameInputCursor++] = (char)codepoint;
        worldNameInput[nameInputCursor] = '\0';
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
}

int main() {
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n");
    return -1;
  }

  int argc = 1;
  char* argv[1] = {(char*)"Something"};
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
    return -1;
  }

  glEnable(GL_DEPTH_TEST);

  GLuint shaderProgram = loadShaders("assets/shaders/vertex_shader.glsl", "assets/shaders/fragment_shader.glsl");
  if (!shaderProgram) {
    fprintf(stderr, "Failed to load shaders\n");
    return -1;
  }

  // Initialize the menu
  menuInit();
  MenuState currentMenuState = MENU_MAIN;
  bool inGame = false;

  // Initialize the world and HUD
  initWorld();
  initCube();
  initChunks();
  HUDInit(BUILD_NAME, BUILD_VERSION);

  // Set GLFW callbacks
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetCursorPosCallback(window, mouseCallback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetWindowUserPointer(window, &camera);
  glfwSetKeyCallback(window, key_callback);
  glfwSetCharCallback(window, char_callback);
  glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);

  // Set input mode for the cursor
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  initCamera(&camera);

  float lastFrame = 0.0f;

  while (!glfwWindowShouldClose(window)) {
  float currentFrame = glfwGetTime();
  float deltaTime = currentFrame - lastFrame;
  lastFrame = currentFrame;

  frameCount++;
  if (currentFrame - lastTime >= 1.0) {
    fps = (float)frameCount;
    frameCount = 0;
    lastTime = currentFrame;
  }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  int width, height;
  glfwGetFramebufferSize(window, &width, &height);

  double mouseX, mouseY;
  glfwGetCursorPos(window, &mouseX, &mouseY);
  bool mousePressed = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

  if (!inGame) {
    // Process menu input
    menuProcessInput(&currentMenuState, mouseX, mouseY, mousePressed, window);

    // Render menu
    menuRender(currentMenuState, width, height);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    if (menuShouldQuit()) {
      // Quit the application
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    if (menuShouldStartGame()) { // For now just start game, will implement further world stuff 
      // Initialize the game world and transition to inGame
      initCamera(&camera);
      initWorld();
      initCube();
      initChunks();
      HUDInit(BUILD_NAME, BUILD_VERSION);

      // Hide the mouse and capture input for the game
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      inGame = true;
    }
  } else {
    // The game is running, process input and render the world
    processInput(window, &camera, deltaTime);

    glUseProgram(shaderProgram);

    Mat4 model, view, projection;
    mat4_identity(model);
    Vec3 target;
    vec3_add(&target, &camera.position, &camera.front);
    mat4_lookAt(view, &camera.position, &target, &camera.up);
    mat4_perspective(projection, 45.0f, (float)width/(float)height, 0.1f, 100.0f);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, model);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, projection);

    renderChunkGrid(shaderProgram, &camera);
    renderWorld(shaderProgram, &camera);
    HUDDraw(shaderProgram, &camera, fps);
  }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  cleanupChunks();
  cleanupWorld();
  return 0;
}