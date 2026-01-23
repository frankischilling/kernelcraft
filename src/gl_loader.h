#pragma once
#include <stdbool.h>
#include <GL/gl.h>
#include <GL/glext.h>

/* Load only what we actually use. */
typedef struct KC_GL {
    PFNGLCREATESHADERPROC            CreateShader;
    PFNGLSHADERSOURCEPROC            ShaderSource;
    PFNGLCOMPILESHADERPROC           CompileShader;
    PFNGLGETSHADERIVPROC             GetShaderiv;
    PFNGLGETSHADERINFOLOGPROC        GetShaderInfoLog;
    PFNGLDELETESHADERPROC            DeleteShader;

    PFNGLCREATEPROGRAMPROC           CreateProgram;
    PFNGLATTACHSHADERPROC            AttachShader;
    PFNGLLINKPROGRAMPROC             LinkProgram;
    PFNGLGETPROGRAMIVPROC            GetProgramiv;
    PFNGLGETPROGRAMINFOLOGPROC       GetProgramInfoLog;
    PFNGLUSEPROGRAMPROC              UseProgram;
    PFNGLDELETEPROGRAMPROC           DeleteProgram;

    PFNGLGETUNIFORMLOCATIONPROC      GetUniformLocation;
    PFNGLUNIFORMMATRIX4FVPROC        UniformMatrix4fv;
    PFNGLUNIFORM1IPROC               Uniform1i;

    PFNGLGENBUFFERSPROC              GenBuffers;
    PFNGLBINDBUFFERPROC              BindBuffer;
    PFNGLBUFFERDATAPROC              BufferData;
    PFNGLDELETEBUFFERSPROC           DeleteBuffers;

    PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;
    PFNGLVERTEXATTRIBPOINTERPROC     VertexAttribPointer;

    PFNGLACTIVETEXTUREPROC           ActiveTexture;

    PFNGLGENERATEMIPMAPPROC          GenerateMipmap;
} KC_GL;

bool kc_gl_load(KC_GL* gl);
