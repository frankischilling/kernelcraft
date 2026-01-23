#include "gl_loader.h"
#include "log.h"
#include <GL/glx.h>
#include <string.h>

static void* get_proc(const char* name) {
    void* p = (void*)glXGetProcAddress((const GLubyte*)name);
    return p;
}

#define LOAD(fn) do { \
    gl->fn = (void*)get_proc("gl" #fn); \
    if (!gl->fn) { KC_ERR("Missing GL function: gl%s", #fn); return false; } \
} while (0)

bool kc_gl_load(KC_GL* gl) {
    memset(gl, 0, sizeof(*gl));

    LOAD(CreateShader);
    LOAD(ShaderSource);
    LOAD(CompileShader);
    LOAD(GetShaderiv);
    LOAD(GetShaderInfoLog);
    LOAD(DeleteShader);

    LOAD(CreateProgram);
    LOAD(AttachShader);
    LOAD(LinkProgram);
    LOAD(GetProgramiv);
    LOAD(GetProgramInfoLog);
    LOAD(UseProgram);
    LOAD(DeleteProgram);

    LOAD(GetUniformLocation);
    LOAD(UniformMatrix4fv);
    LOAD(Uniform1i);

    LOAD(GenBuffers);
    LOAD(BindBuffer);
    LOAD(BufferData);
    LOAD(BufferSubData);
    LOAD(DeleteBuffers);

    LOAD(GetAttribLocation);
    LOAD(EnableVertexAttribArray);
    LOAD(DisableVertexAttribArray);
    LOAD(VertexAttribPointer);

    LOAD(ActiveTexture);

    LOAD(GenerateMipmap);

    KC_INFO("GL loader OK");
    return true;
}
