#ifndef TEXTURE_H
#define TEXTURE_H

#include <GL/glew.h>

GLuint loadTexture(const char* filePath);
// Equal-sized RGBA layers in path order, with nearest filtering and repeating
// UVs. Requires a current GL 3.3 context; caller deletes the returned texture.
// Returns zero and releases partial resources on invalid assets or GL failure.
GLuint loadTextureArray(const char* const paths[], int layers);

#endif // TEXTURE_H
