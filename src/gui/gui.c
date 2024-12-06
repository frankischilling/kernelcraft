#include "gui.h"
#include <GL/gl.h>
#include <string.h>
#include <GL/freeglut.h>

void guiInit() {
    // Nothing special yet
}

void guiRenderBackground(GLuint textureID, int screenWidth, int screenHeight) {
    // Render a fullscreen quad with the dirt background texture
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, screenWidth, screenHeight, 0, -1, 1);  // top-left origin
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glBegin(GL_QUADS);
      glTexCoord2f(0.0f, 0.0f); glVertex2f(0, 0);
      glTexCoord2f(1.0f, 0.0f); glVertex2f((float)screenWidth, 0);
      glTexCoord2f(1.0f, 1.0f); glVertex2f((float)screenWidth, (float)screenHeight);
      glTexCoord2f(0.0f, 1.0f); glVertex2f(0, (float)screenHeight);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
}

UIButton guiCreateButton(const char* text, float centerX, float centerY, float width, float height) {
    UIButton btn;
    btn.x = centerX - width * 0.5f;
    btn.y = centerY - height * 0.5f;
    btn.width = width;
    btn.height = height;
    btn.text = text;
    btn.hovered = false;
    btn.clicked = false;
    return btn;
}

void guiRenderButton(UIButton* btn) {
    // Simple button rendering: draw a colored rectangle and text
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float r = btn->hovered ? 0.8f : 0.6f;
    float g = btn->hovered ? 0.8f : 0.6f;
    float b = btn->hovered ? 0.8f : 0.6f;

    glColor3f(r, g, b);
    glBegin(GL_QUADS);
      glVertex2f(btn->x, btn->y);
      glVertex2f(btn->x + btn->width, btn->y);
      glVertex2f(btn->x + btn->width, btn->y + btn->height);
      glVertex2f(btn->x, btn->y + btn->height);
    glEnd();

    // Render text in the center of the button
    float textWidth = 0;
    for (const char* c = btn->text; *c; c++)
        textWidth += glutBitmapWidth(GLUT_BITMAP_HELVETICA_18, *c);

    float textX = btn->x + (btn->width - textWidth) * 0.5f;
    float textY = btn->y + (btn->height * 0.5f) + 6; // offset for centering text vertically

    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(textX, textY);
    for (const char* c = btn->text; *c; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    glEnable(GL_DEPTH_TEST);
}

bool guiButtonContains(UIButton* btn, float mouseX, float mouseY) {
    return (mouseX >= btn->x && mouseX <= btn->x + btn->width &&
            mouseY >= btn->y && mouseY <= btn->y + btn->height);
}

void guiSetButtonHover(UIButton* btn, bool hover) {
    btn->hovered = hover;
}

void guiSetButtonClick(UIButton* btn, bool click) {
    btn->clicked = click;
}
