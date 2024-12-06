#ifndef GUI_H
#define GUI_H

#include <GL/glew.h>
#include <stdbool.h>

typedef struct {
    float x, y;     // top-left coordinates
    float width, height;
    const char* text;
    bool hovered;
    bool clicked;
} UIButton;

void guiInit();
void guiRenderBackground(GLuint textureID, int screenWidth, int screenHeight);
UIButton guiCreateButton(const char* text, float centerX, float centerY, float width, float height);
void guiRenderButton(UIButton* btn);
bool guiButtonContains(UIButton* btn, float mouseX, float mouseY);
void guiSetButtonHover(UIButton* btn, bool hover);
void guiSetButtonClick(UIButton* btn, bool click);

#endif
