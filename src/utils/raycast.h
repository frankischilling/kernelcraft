
#ifndef RAYCAST_H
#define RAYCAST_H

#include <GL/glew.h>
#include "../graphics/camera.h"
#include "../world/world.h"
#include "../math/math.h"
typedef struct {
  bool hit;
  Vec3 hitCoords;
  Vec3i blockCoords;
} Ray;

Ray rayCast(const Camera* camera);

#endif // RAYCAST_H