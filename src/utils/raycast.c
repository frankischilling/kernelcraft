#include "raycast.h"
#include "../world/world.h"
#include <float.h>

Ray rayCast(Vec3 origin, Vec3 direction, float maxDistance) {
  Ray miss = {0};
  double o[3] = {origin.x / CUBE_SIZE, origin.y / CUBE_SIZE, origin.z / CUBE_SIZE};
  double d[3] = {direction.x, direction.y, direction.z};
  if (!isfinite(maxDistance) || maxDistance < 0)
    return miss;
  for (int axis = 0; axis < 3; axis++)
    if (!isfinite(o[axis]) || !isfinite(d[axis]))
      return miss;
  double length = sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  if (length == 0)
    return miss;
  for (int axis = 0; axis < 3; axis++)
    d[axis] /= length * CUBE_SIZE;

  const int lo[3] = {-WORLD_SIZE / 2, 0, -WORLD_SIZE / 2};
  const int hi[3] = {WORLD_SIZE / 2, CHUNK_HEIGHT, WORLD_SIZE / 2};
  double enter = 0, leave = maxDistance;
  int entryAxis = -1;
  // Clip before converting to integers. Debug flight may start outside the
  // finite world, and arbitrary float coordinates must not overflow an int.
  for (int axis = 0; axis < 3; axis++) {
    if (d[axis] == 0) {
      if (o[axis] < lo[axis] || o[axis] >= hi[axis])
        return miss;
      continue;
    }
    double a = (lo[axis] - o[axis]) / d[axis];
    double b = (hi[axis] - o[axis]) / d[axis];
    double near = fmin(a, b), far = fmax(a, b);
    if (near > enter || (near == enter && entryAxis < 0)) {
      enter = near;
      entryAxis = axis;
    }
    leave = fmin(leave, far);
    if (enter > leave)
      return miss;
  }

  int cell[3], step[3], normal[3] = {0};
  double next[3];
  for (int axis = 0; axis < 3; axis++) {
    step[axis] = (d[axis] > 0) - (d[axis] < 0);
    double p = o[axis] + enter * d[axis];
    // Snap the clipped entry face to avoid rounding into its outside neighbor.
    if (axis == entryAxis)
      p = step[axis] > 0 ? lo[axis] : hi[axis];
    p = fmax(lo[axis], fmin(hi[axis], p));
    double floorP = floor(p);
    cell[axis] = (int)floorP - (step[axis] < 0 && p == floorP);
    if (entryAxis < 0 && step[axis] && p == floorP && !normal[0] && !normal[1] && !normal[2])
      normal[axis] = -step[axis];
    next[axis] = step[axis] ? (cell[axis] + (step[axis] > 0) - o[axis]) / d[axis] : INFINITY;
  }
  if (entryAxis >= 0)
    normal[entryAxis] = -step[entryAxis];

  double distance = enter;
  // A ray crosses at most the sum of the world dimensions in cell planes.
  for (int visited = 0; visited <= WORLD_SIZE * 2 + CHUNK_HEIGHT; visited++) {
    if (cell[0] < lo[0] || cell[0] >= hi[0] || cell[1] < lo[1] || cell[1] >= hi[1] || cell[2] < lo[2] || cell[2] >= hi[2])
      return miss;
    Vec3i pos = {cell[0], cell[1], cell[2]};
    const Block* block = getBlock(&pos);
    if (block && blockIsSolid(block->id)) {
      return (Ray){.hit = true,
                   .hitCoords = {(float)((o[0] + distance * d[0]) * CUBE_SIZE), (float)((o[1] + distance * d[1]) * CUBE_SIZE), (float)((o[2] + distance * d[2]) * CUBE_SIZE)},
                   .blockCoords = pos,
                   .normal = {normal[0], normal[1], normal[2]},
                   .adjacent = {cell[0] + normal[0], cell[1] + normal[1], cell[2] + normal[2]},
                   .distance = (float)distance,
                   .hasPlacementFace = normal[0] || normal[1] || normal[2]};
    }
    double crossing = fmin(next[0], fmin(next[1], next[2]));
    if (crossing > leave)
      return miss;
    normal[0] = normal[1] = normal[2] = 0;
    bool foundFace = false;
    for (int axis = 0; axis < 3; axis++) {
      // Allow only double rounding noise when recognizing the same crossing.
      if (!step[axis] || fabs(next[axis] - crossing) > 8 * DBL_EPSILON * fmax(1, fabs(crossing)))
        continue;
      cell[axis] += step[axis];
      if (!foundFace) {
        normal[axis] = -step[axis];
        foundFace = true;
      }
      // Recompute from the origin instead of accumulating per-cell error.
      next[axis] = (cell[axis] + (step[axis] > 0) - o[axis]) / d[axis];
    }
    distance = crossing;
  }
  return miss;
}
