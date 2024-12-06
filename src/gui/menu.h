#ifndef MENU_H
#define MENU_H

#include <stdbool.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h> // Include GLFW for GLFWwindow*

#define MAX_WORLD_NAME 32 // Ensure this is defined here or remove if defined elsewhere

extern char worldNameInput[MAX_WORLD_NAME];
extern int nameInputCursor;

typedef enum {
    MENU_MAIN,
    MENU_WORLD,
    MENU_CREATE,
    MENU_LOAD
} MenuState;

// Function declarations
void menuInit();
void menuRender(MenuState state, int screenWidth, int screenHeight);
void menuProcessInput(MenuState *state, double mouseX, double mouseY, bool mousePressed, GLFWwindow* window); // Updated parameter
bool menuShouldQuit();
bool menuShouldStartGame();

#endif // MENU_H
