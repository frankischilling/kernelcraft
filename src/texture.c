#include "texture.h"
#include "log.h"
#include "gl_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <string.h>

bool kc_texture2d_load_png(KC_Texture2D* t, const char* path, bool generate_mips) {
    memset(t, 0, sizeof(*t));

    stbi_set_flip_vertically_on_load(1);

    int w=0,h=0,comp=0;
    unsigned char* data = stbi_load(path, &w, &h, &comp, 4);
    if (!data) {
        KC_ERR("stbi_load failed for '%s': %s", path, stbi_failure_reason());
        return false;
    }

    glGenTextures(1, &t->id);
    glBindTexture(GL_TEXTURE_2D, t->id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, generate_mips ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    if (generate_mips) {
        /* Use our loader for glGenerateMipmap */
        extern KC_GL g_gl;
        g_gl.GenerateMipmap(GL_TEXTURE_2D);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);

    t->w = w; t->h = h; t->comp = 4;

    KC_INFO("Loaded texture '%s' (%dx%d)", path, w, h);
    return true;
}

void kc_texture2d_destroy(KC_Texture2D* t) {
    if (t->id) glDeleteTextures(1, &t->id);
    memset(t, 0, sizeof(*t));
}
