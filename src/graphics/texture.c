#define STB_IMAGE_IMPLEMENTATION
#include "texture.h"
#include "../../libs/stb_image.h"
#include <stdio.h>

// Function to load a texture from a file
GLuint loadTexture(const char* filePath) {
  GLuint textureID;
  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_2D, textureID);

  int width, height, nrChannels;
  unsigned char* data = stbi_load(filePath, &width, &height, &nrChannels, 0);
  if (data) {
    GLenum format;
    if (nrChannels == 1)
      format = GL_RED;
    else if (nrChannels == 3)
      format = GL_RGB;
    else if (nrChannels == 4)
      format = GL_RGBA;

    // Use nearest neighbor filtering for pixel art/low-res textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Disable mipmapping for pixel textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
  } else {
    fprintf(stderr, "Failed to load texture: %s\n", filePath);
  }
  stbi_image_free(data);

  return textureID;
}
