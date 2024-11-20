/**
 * @file main.c
 * @brief Main entry point for the kernelcraft, a Minecraft clone. Handles window creation,
 *        OpenGL context initialization, and the main game loop.
 * @author frankischilling
 * @version 0.1
 * @date 2024-11-19
 *
 */
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/freeglut.h>
#include <stdio.h>
#include <stdbool.h>

// Local includes
#include "graphics/cube.h"
#include "graphics/camera.h"
#include "graphics/shader.h"
#include "math/math.h"
#include "world/world.h"
#include "utils/inputs.h"
#include "utils/text.h"
// FPS variables
static double lastTime = 0.0;
static int frameCount = 0;
static float fps = 0.0f;

float lastX = 400.0f;  // Half of window width
float lastY = 300.0f;  // Half of window height
bool firstMouse = true;
static Camera camera;
// Cursor enabled/disabled
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

int main() {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    // Initialize GLUT for text rendering
    int argc = 1;
    char *argv[1] = {(char*)"Something"};
    glutInit(&argc, argv);

    GLFWwindow* window = glfwCreateWindow(800, 600, "kernelcraft", NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to open GLFW window\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);  // Disable VSync
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    // Load shaders
    GLuint shaderProgram = loadShaders("src/graphics/vertex_shader.glsl", "src/graphics/fragment_shader.glsl");
    if (!shaderProgram) {
        fprintf(stderr, "Failed to load shaders\n");
        return -1;
    }

    initWorld();

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowUserPointer(window, &camera);
    glfwSetKeyCallback(window, key_callback);
    
    initCamera(&camera);

    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Update FPS counter
        frameCount++;
        if (currentFrame - lastTime >= 1.0) {
            fps = (float)frameCount;
            frameCount = 0;
            lastTime = currentFrame;
        }

        processInput(window, &camera, deltaTime);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        Mat4 model, view, projection;
        mat4_identity(model);
        Vec3 target;
        vec3_add(target, camera.position, camera.front);
        mat4_lookAt(view, camera.position, target, camera.up);
        mat4_perspective(projection, 45.0f, 800.0f / 600.0f, 0.1f, 100.0f);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, model);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, view);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, projection);

        renderWorld(shaderProgram, &camera);

        const char* biomeText = getCurrentBiomeText(camera.position[0], camera.position[2]);
        renderText(shaderProgram, biomeText, 10.0f, 580.0f);
        
        char fpsText[32];
        snprintf(fpsText, sizeof(fpsText), "FPS: %.1f", fps);
        renderText(shaderProgram, fpsText, -10.0f, 580.0f);
        // Debug text for culling implementation
        char debugText[64];
        snprintf(debugText, sizeof(debugText), "Visible Cubes: %d", getVisibleCubesCount());
        renderText(shaderProgram, debugText, 10.0f, 560.0f);
        
        renderBuildInfo(shaderProgram);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    cleanupWorld();
    return 0;
}