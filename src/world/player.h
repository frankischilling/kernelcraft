#ifndef PLAYER_H
#define PLAYER_H

#include "../math/math.h"
#include <stdbool.h>

#define PLAYER_RADIUS 0.3f
#define PLAYER_HEIGHT 1.8f
#define PLAYER_EYE_HEIGHT 1.62f
#define PLAYER_CROUCH_HEIGHT 1.0f
#define PLAYER_CROUCH_EYE_HEIGHT 0.9f
#define PLAYER_WALK_SPEED 4.5f
#define PLAYER_CROUCH_SPEED 1.5f
#define PLAYER_RUN_SPEED 7.0f
#define PLAYER_RUN_TAP_SECONDS 0.25
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
  bool crouched, running;
} Player;

typedef struct {
  Vec3 wish;
  bool jump, crouch, run;
} PlayerMotion;

// W event state is CPU-only so timing boundaries do not depend on GLFW.
typedef struct {
  double firstPress;
  bool forwardDown, tapPending, running;
} PlayerRunInput;

void playerForwardEvent(PlayerRunInput* input, bool pressed, double seconds);
void playerResetRunInput(PlayerRunInput* input);

// Inclusive cell range overlapped by a body inside finite world bounds.
// Saves and spawn positions always require the standing body.
bool playerCellRange(Vec3 feet, Vec3i* first, Vec3i* last);
bool playerCanOccupy(Vec3 feet);
bool playerCanOccupyPosture(Vec3 feet, bool crouched);
bool playerOverlapsBlock(Vec3 feet, Vec3i cell, bool crouched);
// Failure leaves the previous player state unchanged.
bool playerSetPosition(Player* player, Vec3 feet);
bool playerFindSpawn(Player* player, Vec3 preferred);
Vec3 playerEyePosition(const Player* player);
// Horizontal wish is clamped to unit length. A jump is an edge, not a held key.
// Returns completed steps; invalid input does nothing. Excess frame time is dropped.
// Posture changes at each simulation tick; standing waits for full clearance.
int playerAdvance(Player* player, PlayerMotion motion, double frameSeconds);
void playerResetTiming(Player* player);

#endif
