#ifndef RAYCAST_H
#define RAYCAST_H

#include "../graphics/camera.h"
#include "../math/math.h"
typedef struct {
  bool hit;
  Vec3 coords;
} Ray;

Ray rayCast(const Camera* camera);

#endif //RAYCAST_H