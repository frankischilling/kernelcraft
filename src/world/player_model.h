#ifndef PLAYER_MODEL_H
#define PLAYER_MODEL_H

#include "edit.h"
#include "player.h"
#include <stdbool.h>
#include <stdint.h>

#define PLAYER_SKIN_SIZE 64

typedef enum {
  PLAYER_MODEL_HEAD = 0,
  PLAYER_MODEL_TORSO,
  PLAYER_MODEL_RIGHT_ARM,
  PLAYER_MODEL_LEFT_ARM,
  PLAYER_MODEL_RIGHT_LEG,
  PLAYER_MODEL_LEFT_LEG,
  PLAYER_MODEL_PART_COUNT,
} PlayerModelPart;

typedef enum {
  PLAYER_MODEL_FACE_RIGHT = 0,
  PLAYER_MODEL_FACE_LEFT,
  PLAYER_MODEL_FACE_TOP,
  PLAYER_MODEL_FACE_BOTTOM,
  PLAYER_MODEL_FACE_FRONT,
  PLAYER_MODEL_FACE_BACK,
  PLAYER_MODEL_FACE_COUNT,
} PlayerModelFace;

typedef enum {
  PLAYER_SKIN_BASE = 0,
  PLAYER_SKIN_OUTER,
  PLAYER_SKIN_LAYER_COUNT,
} PlayerSkinLayer;

// Pixel-space skin rectangle. x/y use the PNG's top-left origin.
typedef struct {
  uint8_t x, y, width, height;
} PlayerSkinRect;

// Cuboid dimensions and joint placement in model-local world units.
// Local +X is the player's right, +Y is up, and the player faces local -Z.
typedef struct {
  Vec3 size;
  Vec3 pivot;
  Vec3 centerOffset;
} PlayerPartSpec;

// Euler rotations are radians around local X/Y/Z, applied about PartSpec.pivot.
typedef struct {
  Vec3 translation;
  Vec3 rotation;
  Vec3 scale;
} PlayerPartPose;

typedef struct {
  PlayerPartPose parts[PLAYER_MODEL_PART_COUNT];
  // Right-handed +Y rotation. After rootYaw, local -Z follows camera yaw.
  float rootYaw;
  // Uniform render-only scale about the feet. Normal player poses keep this at 1.
  float rootScale;
  // Presentation inputs retained separately from the posed third-person joints.
  double gaitPhase;
  float gaitWeight;
  float punch;
} PlayerModelPose;

typedef struct {
  float yaw;
  float pitch;
  double gaitPhase;
  float gaitWeight;
  float punch; // Normalized swing progress: 0 starts at rest, 1 returns to rest.
  bool crouched;
  bool running;
  bool grounded;
  bool flying;
} PlayerPoseInput;

typedef struct {
  Vec3 previousFeet;
  double gaitPhase;
  float gaitWeight;
  bool initialized;
} PlayerModelAnimation;

const PlayerSkinRect* playerModelSkinRect(PlayerModelPart part, PlayerSkinLayer layer, PlayerModelFace face);
const PlayerPartSpec* playerModelPartSpec(PlayerModelPart part);
// Per-face shell dilation in model units, shared by rendering and pose bounds.
float playerModelOuterInflation(PlayerModelPart part);
void playerModelPose(PlayerModelPose* pose, const PlayerPoseInput* input);
// Maps the unposed right-arm cuboid (including centerOffset) into camera space.
void playerModelHandTransform(Mat4 transform, const PlayerModelPose* pose, float aspect);

void resetPlayerModelAnimation(PlayerModelAnimation* state, Vec3 feet);
void advancePlayerModelAnimation(PlayerModelAnimation* state, const Player* player, bool flying, bool active, double seconds);
float playerModelPunch(const BlockBreaking* breaking);

#endif
