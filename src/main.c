#if defined(__linux__)
#define _POSIX_C_SOURCE 200809L
#endif
#include <GL/glew.h>
#include "graphics/camera.h"
#include "graphics/hud.h"
#include "graphics/shader.h"
#include "graphics/world_renderer.h"
#include "graphics/selection.h"
#include "graphics/sky.h"
#include "graphics/clouds.h"
#include "graphics/player_renderer.h"
#include "graphics/inventory_ui.h"
#include "graphics/item_renderer.h"
#include "math/math.h"
#include "utils/inputs.h"
#include "utils/options.h"
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

static bool saveSession(const AppOptions* options) {
  SavedPlayer saved;
  char error[256];
  if (!snapshotPlayer(&input, &saved)) {
    fprintf(stderr, "Cannot save world: no clear player position\n");
    return false;
  }

  if (saveWorld(options->worldPath, &saved, error, sizeof(error)) != SAVE_OK) {
    fprintf(stderr, "Cannot save world '%s': %s\n", options->worldPath, error);
    return false;
  }

  printf("Saved world: %s\n", options->worldPath);
  return true;
}

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
  (void)window;
  glViewport(0, 0, width, height);
}

static void error_callback(int error, const char* description) {
  fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

int main(int argc, char** argv) {
  AppOptions options;
  char error[256];
  if (!parseOptions(argc, argv, &options, error, sizeof(error))) {
    fprintf(stderr, "%s\n", error);
    return EXIT_FAILURE;
  }

  if (options.help) {
    puts("Usage: minecraft_clone [--world PATH] [--seed N] [--no-save] [--render-distance N]\n"
         "--world PATH  Load or create this file (default: kernelcraft.kcw in the launch directory)\n"
         "--seed N      New-world seed, decimal 0..4294967295 (default: 0)\n"
         "--no-save     Temporary session; cannot be combined with --world\n"
         "--render-distance N  Horizontal chunk-center radius, 1..16 chunks (default: 12)\n"
         "F5 saves while the mouse is captured; clean exit also saves. Restart reloads the file.");
    return EXIT_SUCCESS;
  }
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

  int glutArgc = 1;
  char* glutArgv[] = {argv[0], NULL};
  glutInit(&glutArgc, glutArgv);
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

  SavedPlayer saved;
  SaveResult loaded = options.noSave ? SAVE_NOT_FOUND : loadWorld(options.worldPath, &saved, error, sizeof(error));
  bool worldReady = false;
  const char* saveStatus = options.noSave ? "Temporary session" : "New world";
  if (loaded == SAVE_OK && options.seedGiven) {
    fprintf(stderr, "--seed cannot be used with an existing save; omit --seed or choose a new --world path\n");
  } else if (loaded == SAVE_OK) {
    worldReady = true;
    saveStatus = "Loaded";
  } else if (loaded == SAVE_NOT_FOUND) {
    worldReady = initChunksSeeded(options.seed);
    if (!worldReady)
      fprintf(stderr, "Cannot allocate new world\n");
  } else {
    fprintf(stderr, "Cannot load world '%s': %s\n", options.worldPath, error);
  }

  if (!worldReady || !setWorldRenderDistance(options.renderDistance) || !initWorld(shaderProgram)) {
    cleanupChunks();
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }

  SkyRenderer sky = {0};
  CloudRenderer clouds = {0};
  PlayerRenderer playerRenderer = {0};
  InventoryUI inventoryUI = {0};
  if (!initSky(&sky) || !initClouds(&clouds) || !initPlayerRenderer(&playerRenderer, "assets/player/skin.png") || !HUDInit(BUILD_NAME, BUILD_VERSION) ||
      !inventoryUIInit(&inventoryUI)) {
    inventoryUICleanup(&inventoryUI);
    HUDCleanup();
    cleanupPlayerRenderer(&playerRenderer);
    cleanupClouds(&clouds);
    cleanupSky(&sky);
    cleanupWorld();
    cleanupChunks();
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }
  ItemRenderer* itemRenderer = HUDItems();

  initCamera(&camera);
  if (!(loaded == SAVE_OK ? initSavedInputs(&input, &camera, &saved) : initInputs(&input, &camera))) {
    fprintf(stderr, "Failed to find a clear player spawn\n");
    inventoryUICleanup(&inventoryUI);
    cleanupPlayerRenderer(&playerRenderer);
    cleanupClouds(&clouds);
    cleanupSky(&sky);
    HUDCleanup();
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
  glfwSetCharCallback(window, characterCallback);
  glfwSetMouseButtonCallback(window, mouseButtonCallback);
  glfwSetScrollCallback(window, scrollCallback);
  setCursorCaptured(window, !input.inventoryOpen);
  printf("World seed: %u\n", (unsigned)worldSeed());
  if (!options.noSave)
    printf("World file: %s\n", options.worldPath);

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
    // Iconification is independent of framebuffer size on some window systems.
    if (width <= 0 || height <= 0 || glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
      advanceDayNight(&input.clock, 0, false);
      advanceClouds(&clouds, 0, false);
      pauseInput(&input);
      glfwWaitEvents();
      lastFrame = glfwGetTime();
      continue;
    }

    processInput(window, &input, deltaTime);
    bool active = inputSimulationActive(window, &input);
    advanceDayNight(&input.clock, deltaTime, active);
    advanceClouds(&clouds, deltaTime, active);
    DayNightState daylight = sampleDayNight(dayNightPhase(&input.clock));
    processBlockBreaking(window, &input, deltaTime);
    droppedItemsAdvance(&input.drops, &input.inventory, inputBodyFeet(&input), deltaTime, active);
    if (input.saveRequested) {
      input.saveRequested = false;
      if (!options.noSave)
        saveStatus = saveSession(&options) ? "Saved (F5)" : "Save failed; see console";
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, width, height);
    float aspect = (float)width / height;
    Camera displayCamera = camera;
    bool showBody = false;
    if (input.view != CAMERA_FIRST_PERSON) {
      Camera thirdPersonEye = camera;
      if (!input.flying && input.player.crouched)
        thirdPersonEye.position.y = input.player.position.y + PLAYER_EYE_HEIGHT;
      showBody = makeThirdPersonCamera(&displayCamera, &thirdPersonEye, input.view == CAMERA_THIRD_PERSON_FRONT, aspect);
      if (!showBody)
        displayCamera = camera;
    }
    PlayerModelPose playerPose;
    inputPlayerPose(&input, &playerPose);
    Mat4 view, projection;
    Vec3 target;
    vec3_add(&target, &displayCamera.position, &displayCamera.front);
    mat4_lookAt(view, &displayCamera.position, &target, &displayCamera.up);
    mat4_perspective(projection, displayCamera.fov, aspect, 0.1f, 1000.0f);
    setWorldDayNight(&daylight);
    RenderResult result = renderWorld(&displayCamera, view, projection, input.wireframe);

    if (!result.success) {
      exitStatus = EXIT_FAILURE;
      break;
    }

    renderSky(&sky, &displayCamera, aspect, &daylight);
    ItemStack mainHand, offhand;
    inputHeldItems(&input, &mainHand, &offhand);
    if (showBody) {
      renderPlayerModel(&playerRenderer, inputBodyFeet(&input), &playerPose, view, projection, &daylight);
      PlayerEquipmentVisuals equipment = {.helmet = input.inventory.armor[INVENTORY_ARMOR_HEAD].count != 0,
                                          .chestplate = input.inventory.armor[INVENTORY_ARMOR_CHEST].count != 0,
                                          .leggings = input.inventory.armor[INVENTORY_ARMOR_LEGS].count != 0,
                                          .boots = input.inventory.armor[INVENTORY_ARMOR_FEET].count != 0};
      renderPlayerEquipment(&playerRenderer, inputBodyFeet(&input), &playerPose, &equipment, view, projection, &daylight);
      renderPlayerHeldItems(itemRenderer, inputBodyFeet(&input), &playerPose, mainHand, offhand, view, projection, &daylight);
    }
    Ray selection = rayCast(camera.position, camera.front, EDIT_REACH);
    if (!input.inventoryOpen)
      drawSelection(&selection, view, projection);
    renderDroppedItems(itemRenderer, &input.drops, view, projection, &daylight);
    renderClouds(&clouds, &displayCamera, aspect, projection, &daylight);
    if (!showBody && !input.inventoryOpen) {
      if (!mainHand.count)
        renderPlayerHand(&playerRenderer, &playerPose, aspect, &daylight);
      if (!renderHeldItems(itemRenderer, mainHand, offhand, &playerPose, aspect, &daylight)) {
        fprintf(stderr, "Cannot render held items: framebuffer allocation or drawing failed\n");
        exitStatus = EXIT_FAILURE;
        break;
      }
    }
    DebugData data = {.camera = &camera,
                      .fps = fps,
                      .visibleBlocks = result.surfaceBlocks,
                      .selection = selection,
                      .selectedSlot = selectedHotbarSlot(&input),
                      .breakingProgress = blockBreakingProgress(&input.breaking),
                      .captured = glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED,
                      .flying = input.flying,
                      .grounded = input.player.grounded,
                      .crouched = input.player.crouched,
                      .running = input.player.running,
                      .modeBlocked = input.modeBlocked,
                      .simulationSteps = input.simulationSteps,
                      .saveStatus = saveStatus,
                      .showDebug = input.showDebug,
                      .wireframe = input.wireframe,
                      .stats = &result};
    data.chat = &input.chat;
    data.inventory = &input.inventory;
    data.inventoryOpen = input.inventoryOpen;
    data.inventoryNotice = input.inventoryNotice;
    HUDDraw(shaderProgram, &data);
    if (input.inventoryOpen) {
      int mouseX = -1, mouseY = -1;
      inventoryPointer(window, &input, &mouseX, &mouseY);
      inventoryUIDraw(&inventoryUI, &input.inventory, &playerRenderer, &playerPose, &daylight, itemRenderer, input.selectedSlot, width, height, mouseX, mouseY);
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  if (exitStatus == EXIT_SUCCESS && !options.noSave && !saveSession(&options))
    exitStatus = EXIT_FAILURE;
  inventoryUICleanup(&inventoryUI);
  HUDCleanup();
  cleanupPlayerRenderer(&playerRenderer);
  cleanupClouds(&clouds);
  cleanupSky(&sky);
  cleanupWorld();
  cleanupChunks();
  glDeleteProgram(shaderProgram);
  glfwDestroyWindow(window);
  glfwTerminate();
  return exitStatus;
}
