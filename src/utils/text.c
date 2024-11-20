#include <GL/freeglut.h>
#include <stdio.h>
#include "text.h"

#define BUILD_VERSION "v0.1-alpha"
#define BUILD_NAME "KernelCraft"

void renderText2D(float x, float y, const char* text) {
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, 800, 0.0, 600, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glRasterPos2f(x, y);
    
    while (*text) {
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *text);
        text++;
    }
    
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
}

void renderFPS(float fps) {
    char fpsText[32];
    snprintf(fpsText, sizeof(fpsText), "FPS: %.1f", fps);
    renderText2D(700, 580, fpsText);
}

void renderBuildInfo(void) {
    char buildText[64];
    snprintf(buildText, sizeof(buildText), "%s %s", BUILD_NAME, BUILD_VERSION);
    renderText2D(650, 560, buildText);
} 