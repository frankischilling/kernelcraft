#ifndef PLAYER_H
#define PLAYER_H

#include "../math/math.h"
#include <stdbool.h>

#define PLAYER_RADIUS 0.3f
#define PLAYER_HEIGHT 1.8f
#define PLAYER_EYE_HEIGHT 1.62f
#define PLAYER_WALK_SPEED 4.5f
#define PLAYER_JUMP_SPEED 8.0f
#define PLAYER_GRAVITY 24.0f
#define PLAYER_TERMINAL_SPEED 50.0f
#define PLAYER_STEP_SECONDS (1.0 / 120.0)
#define PLAYER_MAX_STEPS 8

typedef struct {
  Vec3 position; // Center of the feet, in world units.
  Vec3 velocity;
  bool grounded;
  double accumulator;
  bool jumpPending;
} Player;

bool playerCanOccupy(Vec3 feet);
bool playerOverlapsBlock(Vec3 feet, Vec3i cell);
// Failure leaves the previous player state unchanged.
bool playerSetPosition(Player* player, Vec3 feet);
bool playerFindSpawn(Player* player, Vec3 preferred);
Vec3 playerEyePosition(const Player* player);
// Horizontal wish is clamped to unit length. A jump is an edge, not a held key.
// Returns completed steps; invalid input does nothing. Excess frame time is dropped.
int playerAdvance(Player* player, Vec3 wish, bool jump, double frameSeconds);
void playerResetTiming(Player* player);

#endif
