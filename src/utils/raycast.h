#ifndef RAYCAST_H
#define RAYCAST_H

#include <GL/glew.h>
#include "../graphics/camera.h"
#include "../math/math.h"
#include "../world/world.h"
typedef struct {
  int hit;
  Vec3 coords;
} Ray;

Ray rayCast(const Camera* camera);

#endif // RAYCAST_H