#ifndef EDIT_H
#define EDIT_H

#include "cube.h"

#define EDIT_REACH 6.0f
#define BREAK_MAX_FRAME_SECONDS 0.1

typedef struct {
  Vec3i target;
  int block;
  double elapsed;
  bool active;
} BlockBreaking;

// All current hotbar slots use hand rates. Unsupported blocks cannot be broken.
double blockHandBreakSeconds(int block);
void resetBlockBreaking(BlockBreaking* breaking);
float blockBreakingProgress(const BlockBreaking* breaking);
// Acquire a new target with zero progress. Only a subsequent update of the same
// cell/material earns time. Completion removes at most one block, with no carry.
// The caller resets on release/pause. Zero time can acquire; invalid time resets.
bool advanceBlockBreaking(BlockBreaking* breaking, Vec3 eye, Vec3 direction, double frameSeconds);

// Pass the player's actual feet to share exact collision bounds with placement.
bool editTarget(Vec3 eye, Vec3 direction, Vec3 bodyFeet, bool crouched, int selectedBlock, bool place);

#endif
