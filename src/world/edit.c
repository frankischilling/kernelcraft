#include "edit.h"
#include "world.h"
#include "player.h"
#include "../utils/raycast.h"

double blockHandBreakSeconds(int block) {
  switch (block) {
  case BLOCK_DIRT:
    return 0.5;
  case BLOCK_GRASS:
    return 0.75;
  case BLOCK_STONE:
    return 1.5;
  case BLOCK_COBBLESTONE:
    return 2.0;
  default:
    return 0;
  }
}

void resetBlockBreaking(BlockBreaking* breaking) {
  if (breaking)
    *breaking = (BlockBreaking){0};
}

float blockBreakingProgress(const BlockBreaking* breaking) {
  if (!breaking || !breaking->active)
    return 0;
  double duration = blockHandBreakSeconds(breaking->block);
  return duration > 0 ? (float)fmin(breaking->elapsed / duration, 1.0) : 0;
}

bool advanceBlockBreaking(BlockBreaking* breaking, Vec3 eye, Vec3 direction, double frameSeconds) {
  if (!breaking)
    return false;
  if (!isfinite(frameSeconds) || frameSeconds < 0) {
    resetBlockBreaking(breaking);
    return false;
  }
  Ray ray = rayCast(eye, direction, EDIT_REACH);
  const Block* block = ray.hit ? getBlock(&ray.blockCoords) : NULL;
  double duration = block ? blockHandBreakSeconds(block->id) : 0;
  if (duration <= 0) {
    resetBlockBreaking(breaking);
    return false;
  }
  if (!breaking->active || breaking->block != block->id || breaking->target.x != ray.blockCoords.x || breaking->target.y != ray.blockCoords.y ||
      breaking->target.z != ray.blockCoords.z) {
    *breaking = (BlockBreaking){.target = ray.blockCoords, .block = block->id, .active = true};
    return false;
  }
  breaking->elapsed += fmin(frameSeconds, BREAK_MAX_FRAME_SECONDS);
  // Allow sub-nanosecond summation error at rates such as 60 and 120 Hz.
  if (breaking->elapsed + 1e-9 < duration)
    return false;
  bool changed = setBlock(&breaking->target, BLOCK_AIR);
  resetBlockBreaking(breaking);
  return changed;
}

bool editTarget(Vec3 eye, Vec3 direction, Vec3 bodyFeet, bool crouched, int selectedBlock, bool place) {
  Ray ray = rayCast(eye, direction, EDIT_REACH);
  if (!ray.hit)
    return false;
  if (!place)
    return setBlock(&ray.blockCoords, BLOCK_AIR);
  const Block* destination = getBlock(&ray.adjacent);
  if (!ray.hasPlacementFace || !blockIsSolid(selectedBlock) || !destination || destination->id != BLOCK_AIR || playerOverlapsBlock(bodyFeet, ray.adjacent, crouched))
    return false;
  return setBlock(&ray.adjacent, selectedBlock);
}
