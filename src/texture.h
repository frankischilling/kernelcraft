#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <GL/gl.h>

typedef struct KC_Texture2D {
    GLuint id;
    int w, h, comp;
} KC_Texture2D;

bool kc_texture2d_load_png(KC_Texture2D* t, const char* path, bool generate_mips);
void kc_texture2d_destroy(KC_Texture2D* t);
