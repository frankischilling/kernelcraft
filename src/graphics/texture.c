#define STB_IMAGE_IMPLEMENTATION
#include "texture.h"
#include "../../libs/stb_image.h"
#include <stdio.h>

GLuint loadTextureArray(const char* const paths[], int layers) {
  GLint maxLayers = 0, maxSize = 0;
  glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
  if (glGetError() != GL_NO_ERROR || !paths || layers <= 0 || layers > maxLayers) {
    fprintf(stderr, "Invalid texture array layer count or GL state\n");
    return 0;
  }

  int width, height, channels;
  if (!paths[0] || !stbi_info(paths[0], &width, &height, &channels)) {
    fprintf(stderr, "Failed to load texture: %s\n", paths[0] ? paths[0] : "(null)");
    return 0;
  }

  if (width <= 0 || height <= 0 || width > maxSize || height > maxSize) {
    fprintf(stderr, "Texture exceeds the supported dimensions: %s\n", paths[0]);
    return 0;
  }

  GLuint texture = 0;
  glGenTextures(1, &texture);
  if (!texture) {
    fprintf(stderr, "Failed to create texture array\n");
    return 0;
  }

  glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
  // Mutable storage is available in GL 3.3; glTexStorage3D requires GL 4.2.
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, width, height, layers, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  if (glGetError() != GL_NO_ERROR) {
    fprintf(stderr, "Failed to allocate texture array storage\n");
    goto failure;
  }

  for (int layer = 0; layer < layers; layer++) {
    int layerWidth, layerHeight;
    if (!paths[layer] || !stbi_info(paths[layer], &layerWidth, &layerHeight, &channels)) {
      fprintf(stderr, "Failed to load texture: %s\n", paths[layer] ? paths[layer] : "(null)");
      goto failure;
    }

    if (layerWidth != width || layerHeight != height) {
      fprintf(stderr, "Texture array layers must have matching dimensions: %s\n", paths[layer]);
      goto failure;
    }

    unsigned char* data = stbi_load(paths[layer], &layerWidth, &layerHeight, &channels, STBI_rgb_alpha);
    if (!data || layerWidth != width || layerHeight != height) {
      fprintf(stderr, "Failed to decode matching texture layer: %s\n", paths[layer]);
      stbi_image_free(data);
      goto failure;
    }

    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    if (glGetError() != GL_NO_ERROR) {
      fprintf(stderr, "Failed to upload texture layer: %s\n", paths[layer]);
      goto failure;
    }
  }

  return texture;

failure:
  glDeleteTextures(1, &texture);
  return 0;
}

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
