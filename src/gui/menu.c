#include "menu.h"
#include "gui.h"
#include "../graphics/texture.h"
#include "../world/world.h"
#include "../graphics/shader.h"
#include <GLFW/glfw3.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <dirent.h> // For listing saved worlds
#include <ctype.h>  // For character checking
// Global buttons
static UIButton playButton;
static UIButton quitButton;

static UIButton loadWorldButton;
static UIButton createWorldButton;
static UIButton deleteWorldButton;
static UIButton backButton;

static GLuint dirtBgTex = 0;
static bool quitGame = false;
static bool startGame = false; // set this when a world is chosen
static bool mousePreviouslyPressed = false; // Track previous state

#define MAX_WORLD_NAME 32
#define MAX_SAVED_WORLDS 50

static UIButton worldButtons[MAX_SAVED_WORLDS];
static UIButton loadBackButton;

char savedWorlds[MAX_SAVED_WORLDS][MAX_WORLD_NAME];
int worldCount = 0;
static int currentWorldCount = 0;

char worldNameInput[MAX_WORLD_NAME] = {0};
int nameInputCursor = 0;

void menuInit() {
    guiInit();
    dirtBgTex = loadTexture("assets/textures/dirt.png");

    // Assume your window is 1920x1080 or get dynamic values
    float centerX = 1920 / 2.0f;
    float centerY = 1080 / 2.0f;

    playButton = guiCreateButton("Play", centerX, centerY - 30, 200, 50);
    quitButton = guiCreateButton("Quit", centerX, centerY + 30, 200, 50);

    loadWorldButton = guiCreateButton("Load World", centerX, centerY - 90, 200, 50);
    createWorldButton = guiCreateButton("Create World", centerX, centerY - 30, 200, 50);
    deleteWorldButton = guiCreateButton("Delete World", centerX, centerY + 30, 200, 50);
    backButton = guiCreateButton("Back", centerX, centerY + 90, 200, 50);
}

void menuRender(MenuState state, int screenWidth, int screenHeight) {
    guiRenderBackground(dirtBgTex, screenWidth, screenHeight);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, screenWidth, screenHeight, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (state == MENU_MAIN) {
        guiRenderButton(&playButton);
        guiRenderButton(&quitButton);
    } else if (state == MENU_WORLD) {
        guiRenderButton(&loadWorldButton);
        guiRenderButton(&createWorldButton);
        guiRenderButton(&deleteWorldButton);
        guiRenderButton(&backButton);
    } else if (state == MENU_CREATE) {
        char displayText[MAX_WORLD_NAME + 16];
        snprintf(displayText, sizeof(displayText), "World Name: %s", worldNameInput);

        renderText(shaderProgram, displayText, screenWidth / 2 - 200, screenHeight / 2 - 50);
        renderText(shaderProgram, "Press ENTER to create the world", screenWidth / 2 - 200, screenHeight / 2);
    } else if (state == MENU_LOAD) {
        for (int i = 0; i < worldCount; i++) {
            UIButton worldButton = guiCreateButton(savedWorlds[i], screenWidth / 2 - 150, 300 + i * 60, 300, 50);
            guiRenderButton(&worldButton);
        }
        guiRenderButton(&backButton);
    }
}

// Function to list saved worlds
void listSavedWorlds(const char* directory, char worlds[][MAX_WORLD_NAME], int* count) {
    DIR* dir = opendir(directory);
    if (!dir) {
        fprintf(stderr, "Failed to open directory: %s\n", directory);
        return;
    }

    struct dirent* entry;
    *count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && strlen(entry->d_name) > 0 && *count < MAX_SAVED_WORLDS) {
            strncpy(worlds[*count], entry->d_name, MAX_WORLD_NAME - 1);
            worlds[*count][MAX_WORLD_NAME - 1] = '\0'; // Null-terminate
            printf("Found world: %s\n", worlds[*count]); // Debug log
            (*count)++;
        }
    }
    closedir(dir);
}

void menuProcessInput(MenuState *state, double mouseX, double mouseY, bool mousePressed, GLFWwindow* window) {
    bool mouseClicked = mousePressed && !mousePreviouslyPressed;
    mousePreviouslyPressed = mousePressed;

    if (*state == MENU_MAIN) {
        if (mouseClicked && guiButtonContains(&playButton, mouseX, mouseY)) {
            *state = MENU_WORLD;
            listSavedWorlds("saves", savedWorlds, &worldCount);
        } else if (mouseClicked && guiButtonContains(&quitButton, mouseX, mouseY)) {
            quitGame = true;
        }
    } else if (*state == MENU_WORLD) {
        if (mouseClicked && guiButtonContains(&createWorldButton, mouseX, mouseY)) {
            *state = MENU_CREATE;
            memset(worldNameInput, 0, MAX_WORLD_NAME);
            nameInputCursor = 0;
        } else if (mouseClicked && guiButtonContains(&loadWorldButton, mouseX, mouseY)) {
            *state = MENU_LOAD; // Transition to MENU_LOAD state
            listSavedWorlds("saves", savedWorlds, &worldCount); // Populate savedWorlds array
            printf("Transitioned to MENU_LOAD. Found %d worlds.\n", worldCount); // Debug
        } else if (mouseClicked && guiButtonContains(&backButton, mouseX, mouseY)) {
            *state = MENU_MAIN;
        }
    } else if (*state == MENU_CREATE) {
        // Handle Backspace
        if (glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS && nameInputCursor > 0) {
            worldNameInput[--nameInputCursor] = '\0';
        }

        // Handle Enter
        if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS && nameInputCursor > 0) {
            createWorld(worldNameInput);
            *state = MENU_WORLD;
        }
    } else if (*state == MENU_LOAD) {
        // Handle clicks on listed worlds
        for (int i = 0; i < worldCount; i++) {
            UIButton worldButton = guiCreateButton(savedWorlds[i], 960.0f, 300.0f + i * 60, 300, 50);
            if (mouseClicked && guiButtonContains(&worldButton, mouseX, mouseY)) {
                loadWorld(savedWorlds[i]);
                *state = MENU_WORLD;
                startGame = true;
            }
        }
        if (mouseClicked && guiButtonContains(&backButton, mouseX, mouseY)) {
            *state = MENU_WORLD;
        }
    }
}

bool menuShouldQuit() {
    return quitGame;
}

bool menuShouldStartGame() {
    return startGame;
}
