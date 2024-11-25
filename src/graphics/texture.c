#define STB_IMAGE_IMPLEMENTATION
#include "/home/frank/game/kernelcraft/libs/stb_image.h"
#include "texture.h"
#include <stdio.h>

GLuint loadTexture(const char* filePath) {
       GLuint textureID;
       glGenTextures(1, &textureID);
       glBindTexture(GL_TEXTURE_2D, textureID);

       // Set texture parameters
       glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
       glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
       glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
       glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

       // Load image
       int width, height, nrChannels;
       unsigned char* data = stbi_load(filePath, &width, &height, &nrChannels, 0);
       if (data) {
           GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
           glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
           glGenerateMipmap(GL_TEXTURE_2D);
           printf("Loaded texture: %s (width: %d, height: %d, channels: %d)\n", filePath, width, height, nrChannels);
       } else {
           fprintf(stderr, "Failed to load texture: %s\n", filePath);
       }
       stbi_image_free(data);

       return textureID;
   }
