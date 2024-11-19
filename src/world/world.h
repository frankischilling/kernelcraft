#ifndef WORLD_H
#define WORLD_H

#include <GL/glew.h>
#include "../graphics/shader.h"
#include "../graphics/camera.h"

#define WORLD_SIZE_X 16
#define WORLD_SIZE_Z 16
#define CUBE_SIZE 1.0f

void initWorld();
void renderWorld(GLuint shaderProgram, const Camera* camera);
void cleanupWorld();

#endif