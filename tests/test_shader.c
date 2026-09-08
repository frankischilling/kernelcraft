#include "graphics/shader.h"
#include "graphics/texture.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>

static PFNGLCREATESHADERPROC realCreateShader;
static PFNGLCREATEPROGRAMPROC realCreateProgram;
static GLenum failShaderType;
static int failProgram;
static GLuint createdShaders[2];
static GLuint createdProgram;
static int createdCount;
static int shaderAttempts, programAttempts;

static GLuint GLAPIENTRY createShaderForTest(GLenum type) {
  shaderAttempts++;
  if (type == failShaderType)
    return 0;
  GLuint shader = realCreateShader(type);
  if (createdCount < 2)
    createdShaders[createdCount++] = shader;
  return shader;
}

static GLuint GLAPIENTRY createProgramForTest(void) {
  programAttempts++;
  createdProgram = failProgram ? 0 : realCreateProgram();
  return createdProgram;
}

static int testShaderFailure(const char* label, const char* vertex, const char* fragment, GLenum shaderType, int programFailure, int expectedShaders, int expectedPrograms) {
  failShaderType = shaderType;
  failProgram = programFailure;
  createdCount = 0;
  createdProgram = 0;
  shaderAttempts = programAttempts = 0;
  realCreateShader = __glewCreateShader;
  realCreateProgram = __glewCreateProgram;
  __glewCreateShader = createShaderForTest;
  __glewCreateProgram = createProgramForTest;
  while (glGetError() != GL_NO_ERROR) {
  }
  GLuint program = loadShaders(vertex, fragment);
  __glewCreateShader = realCreateShader;
  __glewCreateProgram = realCreateProgram;
  int failed = 0;
  if (shaderAttempts != expectedShaders || programAttempts != expectedPrograms || (expectedPrograms && !programFailure && !createdProgram)) {
    fprintf(stderr, "%s did not reach the expected loading stage\n", label);
    failed = 1;
  }
  if (program || glGetError() != GL_NO_ERROR) {
    fprintf(stderr, "%s must return zero without using an invalid GL handle\n", label);
    failed = 1;
  }
  if (program)
    glDeleteProgram(program);
  if (createdProgram && glIsProgram(createdProgram)) {
    fprintf(stderr, "%s must release its program\n", label);
    failed = 1;
    glDeleteProgram(createdProgram);
  }
  for (int i = 0; i < createdCount; i++) {
    if (!createdShaders[i] || glIsShader(createdShaders[i])) {
      fprintf(stderr, "%s must release previously created shaders\n", label);
      failed = 1;
      if (createdShaders[i])
        glDeleteShader(createdShaders[i]);
    }
  }
  return failed;
}

static int writeShader(const char* path, const char* source) {
  FILE* file = fopen(path, "wb");
  if (!file)
    return 0;
  int written = fputs(source, file) >= 0;
  return fclose(file) == 0 && written;
}

int main(void) {
  if (!glfwInit())
    return 1;
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  GLFWwindow* window = glfwCreateWindow(32, 32, "shader test", NULL, NULL);
  if (!window)
    return 1;
  glfwMakeContextCurrent(window);
  if (glewInit() != GLEW_OK)
    return 1;
  const char* vertex = "test-no-newline.vert";
  const char* fragment = "test-no-newline.frag";
  const char* validVertex = "#version 330 core\nvoid main(){gl_Position=vec4(0.0,0.0,0.0,1.0);}";
  const char* validFragment = "#version 330 core\nout vec4 color;\nvoid main(){color=vec4(1.0);}";
  if (!writeShader(vertex, validVertex) || !writeShader(fragment, validFragment))
    return 1;
  GLuint shader = loadShaders(vertex, fragment);
  int failed = shader == 0;
  if (failed)
    fprintf(stderr, "Valid shaders without trailing newlines must compile\n");
  glDeleteProgram(shader);
  // Only creation is substituted; compilation, linking, and lifetime queries
  // use the driver so invalid handle use and leaked objects remain observable.
  failed |= testShaderFailure("Vertex creation failure", vertex, fragment, GL_VERTEX_SHADER, 0, 1, 0);
  failed |= testShaderFailure("Fragment creation failure", vertex, fragment, GL_FRAGMENT_SHADER, 0, 2, 0);
  failed |= testShaderFailure("Program creation failure", vertex, fragment, 0, 1, 2, 1);
  if (!writeShader(vertex, "#version 330 core\ninvalid shader"))
    return 1;
  failed |= testShaderFailure("Vertex compilation failure", vertex, fragment, 0, 0, 1, 0);
  if (!writeShader(vertex, validVertex) || !writeShader(fragment, "#version 330 core\ninvalid shader"))
    return 1;
  failed |= testShaderFailure("Fragment compilation failure", vertex, fragment, 0, 0, 2, 0);
  if (!writeShader(vertex, "#version 330 core\nout vec3 mismatch;\nvoid main(){mismatch=vec3(1.0);gl_Position=vec4(0.0);}") ||
      !writeShader(fragment, "#version 330 core\nin vec4 mismatch;\nout vec4 color;\nvoid main(){color=mismatch;}"))
    return 1;
  failed |= testShaderFailure("Program link failure", vertex, fragment, 0, 0, 2, 1);
  if (!writeShader(vertex, validVertex) || !writeShader(fragment, validFragment))
    return 1;
  shader = loadShaders(vertex, fragment);
  if (!shader || glGetError() != GL_NO_ERROR) {
    fprintf(stderr, "Valid shaders must load after failed attempts\n");
    failed = 1;
  }
  glDeleteProgram(shader);
  if (!writeShader(vertex, ""))
    return 1;
  shader = loadShaders(vertex, fragment);
  if (shader) {
    failed = 1;
    glDeleteProgram(shader);
  }
  const char* texturePath = "test-gray-alpha.tga";
  const unsigned char tga[] = {0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 16, 8, 128, 255};
  FILE* file = fopen(texturePath, "wb");
  if (!file)
    return 1;
  fwrite(tga, 1, sizeof(tga), file);
  fclose(file);
  while (glGetError() != GL_NO_ERROR) {
  }
  GLuint texture = loadTexture(texturePath);
  unsigned char pixel[4] = {0};
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
  if (!texture || glGetError() != GL_NO_ERROR || pixel[0] != 128 || pixel[1] != 128 || pixel[2] != 128 || pixel[3] != 255) {
    fprintf(stderr, "Grayscale-alpha texture must upload as RGBA\n");
    failed = 1;
  }
  glDeleteTextures(1, &texture);
  remove(texturePath);
  texture = loadTexture("nonexistent-texture.png");
  if (texture) {
    fprintf(stderr, "Missing texture must report failure\n");
    failed = 1;
    glDeleteTextures(1, &texture);
  }
  remove(vertex);
  remove(fragment);
  glfwDestroyWindow(window);
  glfwTerminate();
  if (!failed)
    puts("Shader regression tests passed");
  return failed;
}
