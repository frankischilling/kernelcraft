#include "edit.h"
#include "world.h"
#include "../utils/raycast.h"

static bool overlapsBody(Vec3i cell, Vec3 eye) {
  return cell.x * CUBE_SIZE < eye.x + 0.3f && (cell.x + 1) * CUBE_SIZE > eye.x - 0.3f && cell.y * CUBE_SIZE < eye.y + 0.18f && (cell.y + 1) * CUBE_SIZE > eye.y - 1.62f &&
         cell.z * CUBE_SIZE < eye.z + 0.3f && (cell.z + 1) * CUBE_SIZE > eye.z - 0.3f;
}

bool editTarget(Vec3 eye, Vec3 direction, int selectedBlock, bool place) {
  Ray ray = rayCast(eye, direction, EDIT_REACH);
  if (!ray.hit)
    return false;
  if (!place)
    return setBlock(&ray.blockCoords, BLOCK_AIR);
  const Block* destination = getBlock(&ray.adjacent);
  if (!ray.hasPlacementFace || !blockIsSolid(selectedBlock) || !destination || destination->id != BLOCK_AIR || overlapsBody(ray.adjacent, eye))
    return false;
  return setBlock(&ray.adjacent, selectedBlock);
}
