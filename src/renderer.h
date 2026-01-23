#pragma once
#include "gl_loader.h"
#include "texture.h"
#include "mesh_builder.h"
#include "math.h"
#include <stdbool.h>
#include <GL/gl.h>
#include <stddef.h>

typedef struct KC_MeshGPU {
    GLuint vbo;
    GLuint ibo;
    GLuint vao;      /* 0 if VAOs unsupported */
    GLsizei idx_count;
    size_t vbo_cap_bytes;
    size_t ibo_cap_bytes;
} KC_MeshGPU;

typedef struct KC_Renderer {
    KC_Texture2D atlas;
    GLuint program;

    GLint u_mvp;
    GLint u_tex;

    /* Cached attribute locations */
    GLint a_pos;
    GLint a_uv;

    bool in_frame;
    bool has_vao;
} KC_Renderer;

bool kc_renderer_init(KC_Renderer* r);
void kc_renderer_shutdown(KC_Renderer* r);

/* Batched frame */
void kc_renderer_begin(KC_Renderer* r);
void kc_renderer_end(KC_Renderer* r);

/* NOTE: signature changed (needs renderer for attrib locations + VAO setup) */
bool kc_renderer_upload_mesh(KC_Renderer* r, KC_MeshGPU* gpu, const KC_MeshData* cpu);

void kc_renderer_draw_mesh(const KC_Renderer* r, const KC_MeshGPU* gpu, m4 mvp);

void kc_meshgpu_destroy(KC_MeshGPU* gpu);
