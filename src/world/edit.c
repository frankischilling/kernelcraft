#include "edit.h"
#include "world.h"
#include "player.h"
#include "../utils/raycast.h"

bool editTarget(Vec3 eye, Vec3 direction, Vec3 bodyFeet, int selectedBlock, bool place) {
  Ray ray = rayCast(eye, direction, EDIT_REACH);
  if (!ray.hit)
    return false;
  if (!place)
    return setBlock(&ray.blockCoords, BLOCK_AIR);
  const Block* destination = getBlock(&ray.adjacent);
  if (!ray.hasPlacementFace || !blockIsSolid(selectedBlock) || !destination || destination->id != BLOCK_AIR || playerOverlapsBlock(bodyFeet, ray.adjacent))
    return false;
  return setBlock(&ray.adjacent, selectedBlock);
}
