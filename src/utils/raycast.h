
#ifndef RAYCAST_H
#define RAYCAST_H

#include "../math/math.h"
#include <stdbool.h>

typedef struct {
  bool hit;
  Vec3 hitCoords;
  Vec3i blockCoords;
  Vec3i normal; // Outward axis normal; zero when no face was entered at the start.
  Vec3i adjacent;
  float distance; // World units, independent of direction length.
  bool hasPlacementFace;
} Ray;

// Half-open cells; a boundary start selects the cell entered by the ray.
// Simultaneous crossings advance all tied axes, choosing X, then Y, then Z
// for the reported face. Cells touched only at an edge/corner are skipped.
// Reach is inclusive. Zero/non-finite direction, non-finite origin/reach, and
// negative reach return a miss. Missing world cells cannot be selected.
Ray rayCast(Vec3 origin, Vec3 direction, float maxDistance);

#endif // RAYCAST_H
