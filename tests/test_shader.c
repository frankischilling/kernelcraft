#include "graphics/shader.h"
#include "graphics/texture.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>

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
  const char* vertex = "bin/test-no-newline.vert";
  const char* fragment = "bin/test-no-newline.frag";
  FILE* file = fopen(vertex, "wb");
  if (!file)
    return 1;
  fputs("#version 330 core\nvoid main(){gl_Position=vec4(0.0,0.0,0.0,1.0);}", file);
  fclose(file);
  file = fopen(fragment, "wb");
  if (!file)
    return 1;
  fputs("#version 330 core\nout vec4 color;\nvoid main(){color=vec4(1.0);}", file);
  fclose(file);
  GLuint shader = loadShaders(vertex, fragment);
  int failed = shader == 0;
  if (failed)
    fprintf(stderr, "Valid shaders without trailing newlines must compile\n");
  glDeleteProgram(shader);
  file = fopen(vertex, "wb");
  if (!file)
    return 1;
  fclose(file);
  shader = loadShaders(vertex, fragment);
  if (shader) {
    failed = 1;
    glDeleteProgram(shader);
  }
  const char* texturePath = "bin/test-gray-alpha.tga";
  const unsigned char tga[] = {0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 16, 8, 128, 255};
  file = fopen(texturePath, "wb");
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
  texture = loadTexture("bin/nonexistent-texture.png");
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
