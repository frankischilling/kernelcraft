#include "renderer.h"
#include "log.h"
#include <GL/glx.h>   /* glXGetProcAddress */

KC_GL g_gl; /* used by texture.c too */

/* ---- Optional VAO support (GL 3.0 / ARB_vertex_array_object) ---- */
static PFNGLGENVERTEXARRAYSPROC    p_glGenVertexArrays    = NULL;
static PFNGLBINDVERTEXARRAYPROC    p_glBindVertexArray    = NULL;
static PFNGLDELETEVERTEXARRAYSPROC p_glDeleteVertexArrays = NULL;

static void vao_try_load(KC_Renderer* r) {
    /* core names */
    p_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)glXGetProcAddress((const GLubyte*)"glGenVertexArrays");
    p_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)glXGetProcAddress((const GLubyte*)"glBindVertexArray");
    p_glDeleteVertexArrays =
        (PFNGLDELETEVERTEXARRAYSPROC)glXGetProcAddress((const GLubyte*)"glDeleteVertexArrays");

    if (!p_glGenVertexArrays || !p_glBindVertexArray || !p_glDeleteVertexArrays) {
        /* ARB extension names */
        p_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)glXGetProcAddress((const GLubyte*)"glGenVertexArraysARB");
        p_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)glXGetProcAddress((const GLubyte*)"glBindVertexArrayARB");
        p_glDeleteVertexArrays =
            (PFNGLDELETEVERTEXARRAYSPROC)glXGetProcAddress((const GLubyte*)"glDeleteVertexArraysARB");
    }

    r->has_vao = (p_glGenVertexArrays && p_glBindVertexArray && p_glDeleteVertexArrays);
    if (r->has_vao) KC_INFO("Renderer: VAO path enabled");
    else KC_INFO("Renderer: VAO path unavailable; using non-VAO fallback");
}

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint sh = g_gl.CreateShader(type);
    g_gl.ShaderSource(sh, 1, &src, NULL);
    g_gl.CompileShader(sh);

    GLint ok = 0;
    g_gl.GetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        GLsizei n = 0;
        g_gl.GetShaderInfoLog(sh, (GLsizei)sizeof(log), &n, log);
        KC_ERR("Shader compile failed: %.*s", (int)n, log);
        g_gl.DeleteShader(sh);
        return 0;
    }
    return sh;
}

static GLuint link_program(GLuint vs, GLuint fs) {
    GLuint p = g_gl.CreateProgram();
    g_gl.AttachShader(p, vs);
    g_gl.AttachShader(p, fs);
    g_gl.LinkProgram(p);

    GLint ok = 0;
    g_gl.GetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        GLsizei n = 0;
        g_gl.GetProgramInfoLog(p, (GLsizei)sizeof(log), &n, log);
        KC_ERR("Program link failed: %.*s", (int)n, log);
        g_gl.DeleteProgram(p);
        return 0;
    }
    return p;
}

bool kc_renderer_init(KC_Renderer* r) {
    memset(r, 0, sizeof(*r));
    if (!kc_gl_load(&g_gl)) return false;

    vao_try_load(r);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    const char* vs_src =
        "#version 120\n"
        "attribute vec3 a_pos;\n"
        "attribute vec2 a_uv;\n"
        "uniform mat4 u_mvp;\n"
        "varying vec2 v_uv;\n"
        "void main(){\n"
        "  v_uv = a_uv;\n"
        "  gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
        "}\n";

    const char* fs_src =
        "#version 120\n"
        "uniform sampler2D u_tex;\n"
        "varying vec2 v_uv;\n"
        "void main(){\n"
        "  gl_FragColor = texture2D(u_tex, v_uv);\n"
        "}\n";

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    if (!vs || !fs) return false;

    r->program = link_program(vs, fs);
    g_gl.DeleteShader(vs);
    g_gl.DeleteShader(fs);
    if (!r->program) return false;

    /* Cache attrib locations ONCE (no per-draw glGetAttribLocation) */
    r->a_pos = glGetAttribLocation(r->program, "a_pos");
    r->a_uv  = glGetAttribLocation(r->program, "a_uv");
    if (r->a_pos < 0 || r->a_uv < 0) {
        KC_ERR("Missing shader attribs (a_pos=%d, a_uv=%d)", r->a_pos, r->a_uv);
        return false;
    }

    g_gl.UseProgram(r->program);
    r->u_mvp = g_gl.GetUniformLocation(r->program, "u_mvp");
    r->u_tex = g_gl.GetUniformLocation(r->program, "u_tex");
    g_gl.Uniform1i(r->u_tex, 0);
    g_gl.UseProgram(0);

    if (!kc_texture2d_load_png(&r->atlas, "textures/texture_atlas.png", true)) return false;

    KC_INFO("Renderer init OK");
    return true;
}

void kc_renderer_shutdown(KC_Renderer* r) {
    if (r->program) g_gl.DeleteProgram(r->program);
    kc_texture2d_destroy(&r->atlas);
    memset(r, 0, sizeof(*r));
}

/* Batched frame: bind program + atlas once */
void kc_renderer_begin(KC_Renderer* r) {
    KC_ASSERT(r);
    KC_ASSERT(!r->in_frame);
    r->in_frame = true;

    g_gl.UseProgram(r->program);
    g_gl.ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->atlas.id);

    if (!r->has_vao) {
        /* Non-VAO path: enable attrib arrays once per frame */
        g_gl.EnableVertexAttribArray((GLuint)r->a_pos);
        g_gl.EnableVertexAttribArray((GLuint)r->a_uv);
    }
}

void kc_renderer_end(KC_Renderer* r) {
    KC_ASSERT(r);
    KC_ASSERT(r->in_frame);

    if (r->has_vao && p_glBindVertexArray) {
        p_glBindVertexArray(0);
    } else {
        glDisableVertexAttribArray((GLuint)r->a_pos);
        glDisableVertexAttribArray((GLuint)r->a_uv);
        g_gl.BindBuffer(GL_ARRAY_BUFFER, 0);
        g_gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    g_gl.UseProgram(0);
    r->in_frame = false;
}

/* Upload mesh + (optionally) bake VAO so draw becomes tiny */
bool kc_renderer_upload_mesh(KC_Renderer* r, KC_MeshGPU* gpu, const KC_MeshData* cpu) {
    KC_ASSERT(r && gpu && cpu);

    if (!gpu->vbo) g_gl.GenBuffers(1, &gpu->vbo);
    if (!gpu->ibo) g_gl.GenBuffers(1, &gpu->ibo);

    g_gl.BindBuffer(GL_ARRAY_BUFFER, gpu->vbo);
    g_gl.BufferData(GL_ARRAY_BUFFER,
                    (GLsizeiptr)(cpu->vert_count * sizeof(KC_Vertex)),
                    cpu->verts,
                    GL_STATIC_DRAW);

    g_gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu->ibo);
    g_gl.BufferData(GL_ELEMENT_ARRAY_BUFFER,
                    (GLsizeiptr)(cpu->idx_count * sizeof(uint32_t)),
                    cpu->indices,
                    GL_STATIC_DRAW);

    gpu->idx_count = (GLsizei)cpu->idx_count;

    if (r->has_vao) {
        if (!gpu->vao) p_glGenVertexArrays(1, &gpu->vao);
        p_glBindVertexArray(gpu->vao);

        g_gl.BindBuffer(GL_ARRAY_BUFFER, gpu->vbo);
        g_gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu->ibo);

        g_gl.EnableVertexAttribArray((GLuint)r->a_pos);
        g_gl.EnableVertexAttribArray((GLuint)r->a_uv);

        g_gl.VertexAttribPointer((GLuint)r->a_pos, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei)sizeof(KC_Vertex), (void*)0);
        g_gl.VertexAttribPointer((GLuint)r->a_uv,  2, GL_FLOAT, GL_FALSE,
                                 (GLsizei)sizeof(KC_Vertex), (void*)(3 * sizeof(float)));

        p_glBindVertexArray(0);
    }

    g_gl.BindBuffer(GL_ARRAY_BUFFER, 0);
    g_gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    return true;
}

void kc_renderer_draw_mesh(const KC_Renderer* r, const KC_MeshGPU* gpu, m4 mvp) {
    KC_ASSERT(r && gpu);
    if (gpu->idx_count <= 0) return;

    /* If caller forgot begin/end, do a safe one-off path */
    bool oneoff = !r->in_frame;
    if (oneoff) kc_renderer_begin((KC_Renderer*)r);

    g_gl.UniformMatrix4fv(r->u_mvp, 1, GL_FALSE, mvp.m);

    if (r->has_vao && gpu->vao) {
        p_glBindVertexArray(gpu->vao);
        glDrawElements(GL_TRIANGLES, gpu->idx_count, GL_UNSIGNED_INT, (void*)0);
    } else {
        g_gl.BindBuffer(GL_ARRAY_BUFFER, gpu->vbo);
        g_gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu->ibo);

        g_gl.VertexAttribPointer((GLuint)r->a_pos, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei)sizeof(KC_Vertex), (void*)0);
        g_gl.VertexAttribPointer((GLuint)r->a_uv,  2, GL_FLOAT, GL_FALSE,
                                 (GLsizei)sizeof(KC_Vertex), (void*)(3 * sizeof(float)));

        glDrawElements(GL_TRIANGLES, gpu->idx_count, GL_UNSIGNED_INT, (void*)0);
    }

    if (oneoff) kc_renderer_end((KC_Renderer*)r);
}

void kc_meshgpu_destroy(KC_MeshGPU* gpu) {
    if (!gpu) return;
    if (gpu->vao && p_glDeleteVertexArrays) p_glDeleteVertexArrays(1, &gpu->vao);
    if (gpu->vbo) g_gl.DeleteBuffers(1, &gpu->vbo);
    if (gpu->ibo) g_gl.DeleteBuffers(1, &gpu->ibo);
    memset(gpu, 0, sizeof(*gpu));
}
