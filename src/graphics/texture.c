#define STB_IMAGE_IMPLEMENTATION
#include "texture.h"
#include "../../libs/stb_image.h"
#include <stdio.h>

GLuint loadTexture(const char* filePath) {
  int width, height, channels;
  unsigned char* data = stbi_load(filePath, &width, &height, &channels, STBI_rgb_alpha);
  if (!data) {
    fprintf(stderr, "Failed to load texture: %s\n", filePath);
    return 0;
  }

  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  // Greedy rectangle UVs span multiple blocks; each unit repeats one tile.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  // RGBA rows are always aligned to the default four-byte unpack boundary.
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
  stbi_image_free(data);
  return texture;
}
