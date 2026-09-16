#include "player_model.h"
#include <math.h>
#include <stddef.h>

#define MODEL_PIXEL (PLAYER_HEIGHT / 32.0f)
#define MODEL_TWO_PI 6.28318530717958647692
#define MODEL_GAIT_RADIANS_PER_BLOCK (4.0 * 0.6662)
#define MODEL_GAIT_RESPONSE 12.0
#define MODEL_MAX_FRAME_SECONDS 0.1
#define MODEL_PUNCH_SECONDS 0.3

static const PlayerSkinRect skinRects[PLAYER_MODEL_PART_COUNT][PLAYER_SKIN_LAYER_COUNT]
                                     [PLAYER_MODEL_FACE_COUNT] =
                                         {
                                             [PLAYER_MODEL_HEAD] =
                                                 {
                                                     [PLAYER_SKIN_BASE] =
                                                         {
                                                             [PLAYER_MODEL_FACE_RIGHT] = {0, 8, 8, 8},
                                                             [PLAYER_MODEL_FACE_LEFT] = {16, 8, 8, 8},
                                                             [PLAYER_MODEL_FACE_TOP] = {8, 0, 8, 8},
                                                             [PLAYER_MODEL_FACE_BOTTOM] = {16, 0, 8, 8},
                                                             [PLAYER_MODEL_FACE_FRONT] = {8, 8, 8, 8},
                                                             [PLAYER_MODEL_FACE_BACK] = {24, 8, 8, 8},
                                                         },
                                                     [PLAYER_SKIN_OUTER] =
                                                         {
                                                             [PLAYER_MODEL_FACE_RIGHT] = {32, 8, 8, 8},
                                                             [PLAYER_MODEL_FACE_LEFT] = {48, 8, 8, 8},
                                                             [PLAYER_MODEL_FACE_TOP] = {40, 0, 8, 8},
                                                             [PLAYER_MODEL_FACE_BOTTOM] = {48, 0, 8, 8},
                                                             [PLAYER_MODEL_FACE_FRONT] = {40, 8, 8, 8},
                                                             [PLAYER_MODEL_FACE_BACK] = {56, 8, 8, 8},
                                                         },
                                                 },
                                             [PLAYER_MODEL_TORSO] =
                                                 {
                                                     [PLAYER_SKIN_BASE] =
                                                         {
                                                             [PLAYER_MODEL_FACE_RIGHT] = {16, 20, 4, 12},
                                                             [PLAYER_MODEL_FACE_LEFT] = {28, 20, 4, 12},
                                                             [PLAYER_MODEL_FACE_TOP] = {20, 16, 8, 4},
                                                             [PLAYER_MODEL_FACE_BOTTOM] = {28, 16, 8, 4},
                                                             [PLAYER_MODEL_FACE_FRONT] = {20, 20, 8, 12},
                                                             [PLAYER_MODEL_FACE_BACK] = {32, 20, 8, 12},
                                                         },
                                                     [PLAYER_SKIN_OUTER] =
                                                         {
                                                             [PLAYER_MODEL_FACE_RIGHT] = {16, 36, 4, 12},
                                                             [PLAYER_MODEL_FACE_LEFT] = {28, 36, 4, 12},
                                                             [PLAYER_MODEL_FACE_TOP] = {20, 32, 8, 4},
                                                             [PLAYER_MODEL_FACE_BOTTOM] = {28, 32, 8, 4},
                                                             [PLAYER_MODEL_FACE_FRONT] = {20, 36, 8, 12},
                                                             [PLAYER_MODEL_FACE_BACK] = {32, 36, 8, 12},
                                                         },
                                                 },
                                             [PLAYER_MODEL_RIGHT_ARM] =
                                                 {
                                                     [PLAYER_SKIN_BASE] =
                                                         {
                                                             [PLAYER_MODEL_FACE_RIGHT] = {40, 20, 4, 12},
                                                             [PLAYER_MODEL_FACE_LEFT] = {48, 20, 4, 12},
                                                             [PLAYER_MODEL_FACE_TOP] = {44, 16, 4, 4},
                                                             [PLAYER_MODEL_FACE_BOTTOM] = {48, 16, 4, 4},
                                                             [PLAYER_MODEL_FACE_FRONT] = {44, 20, 4, 12},
                                                             [PLAYER_MODEL_FACE_BACK] = {52, 20, 4, 12},
                                                         },
                                                     [PLAYER_SKIN_OUTER] =
                                                         {
                                                             [PLAYER_MODEL_FACE_RIGHT] = {40, 36, 4, 12},
                                                             [PLAYER_MODEL_FACE_LEFT] = {48, 36, 4, 12},
                                                             [PLAYER_MODEL_FACE_TOP] = {44, 32, 4, 4},
                                                             [PLAYER_MODEL_FACE_BOTTOM] = {48, 32, 4, 4},
                                                             [PLAYER_MODEL_FACE_FRONT] = {44, 36, 4, 12},
                                                             [PLAYER_MODEL_FACE_BACK] = {52, 36, 4, 12},
                                                         },
                                                 },
                                             [PLAYER_MODEL_LEFT_ARM] =
                                                 {
                                                     [PLAYER_SKIN_BASE] =
                                                         {
                                                             [PLAYER_MODEL_FACE_RIGHT] = {32, 52, 4, 12},
                                                             [PLAYER_MODEL_FACE_LEFT] = {40, 52, 4, 12},
                                                             [PLAYER_MODEL_FACE_TOP] = {36, 48, 4, 4},
                                                             [PLAYER_MODEL_FACE_BOTTOM] = {40, 48, 4, 4},
                                                             [PLAYER_MODEL_FACE_FRONT] = {36, 52, 4, 12},
                                                             [PLAYER_MODEL_FACE_BACK] = {44, 52, 4, 12},
                                                         },
                                                     [PLAYER_SKIN_OUTER] =
                                                         {
                                                             [PLAYER_MODEL_FACE_RIGHT] = {48, 52, 4, 12},
                                                             [PLAYER_MODEL_FACE_LEFT] = {56, 52, 4, 12},
                                                             [PLAYER_MODEL_FACE_TOP] = {52, 48, 4, 4},
                                                             [PLAYER_MODEL_FACE_BOTTOM] = {56, 48, 4, 4},
                                                             [PLAYER_MODEL_FACE_FRONT] = {52, 52, 4, 12},
                                                             [PLAYER_MODEL_FACE_BACK] = {60, 52, 4, 12},
                                                         },
                                                 },
                                             [PLAYER_MODEL_RIGHT_LEG] =
                                                 {
                                                     [PLAYER_SKIN_BASE] =
                                                         {
                                                             [PLAYER_MODEL_FACE_RIGHT] = {0, 20, 4, 12},
                                                             [PLAYER_MODEL_FACE_LEFT] = {8, 20, 4, 12},
                                                             [PLAYER_MODEL_FACE_TOP] = {4, 16, 4, 4},
                                                             [PLAYER_MODEL_FACE_BOTTOM] = {8, 16, 4, 4},
                                                             [PLAYER_MODEL_FACE_FRONT] = {4, 20, 4, 12},
                                                             [PLAYER_MODEL_FACE_BACK] = {12, 20, 4, 12},
                                                         },
                                                     [PLAYER_SKIN_OUTER] =
                                                         {
                                                             [PLAYER_MODEL_FACE_RIGHT] = {0, 36, 4, 12},
                                                             [PLAYER_MODEL_FACE_LEFT] = {8, 36, 4, 12},
                                                             [PLAYER_MODEL_FACE_TOP] = {4, 32, 4, 4},
                                                             [PLAYER_MODEL_FACE_BOTTOM] = {8, 32, 4, 4},
                                                             [PLAYER_MODEL_FACE_FRONT] = {4, 36, 4, 12},
                                                             [PLAYER_MODEL_FACE_BACK] = {12, 36, 4, 12},
                                                         },
                                                 },
                                             [PLAYER_MODEL_LEFT_LEG] =
                                                 {
                                                     [PLAYER_SKIN_BASE] =
                                                         {
                                                             [PLAYER_MODEL_FACE_RIGHT] = {16, 52, 4, 12},
                                                             [PLAYER_MODEL_FACE_LEFT] = {24, 52, 4, 12},
                                                             [PLAYER_MODEL_FACE_TOP] = {20, 48, 4, 4},
                                                             [PLAYER_MODEL_FACE_BOTTOM] = {24, 48, 4, 4},
                                                             [PLAYER_MODEL_FACE_FRONT] = {20, 52, 4, 12},
                                                             [PLAYER_MODEL_FACE_BACK] = {28, 52, 4, 12},
                                                         },
                                                     [PLAYER_SKIN_OUTER] =
                                                         {
                                                             [PLAYER_MODEL_FACE_RIGHT] = {0, 52, 4, 12},
                                                             [PLAYER_MODEL_FACE_LEFT] = {8, 52, 4, 12},
                                                             [PLAYER_MODEL_FACE_TOP] = {4, 48, 4, 4},
                                                             [PLAYER_MODEL_FACE_BOTTOM] = {8, 48, 4, 4},
                                                             [PLAYER_MODEL_FACE_FRONT] = {4, 52, 4, 12},
                                                             [PLAYER_MODEL_FACE_BACK] = {12, 52, 4, 12},
                                                         },
                                                 },
};

static const PlayerPartSpec partSpecs[PLAYER_MODEL_PART_COUNT] = {
    [PLAYER_MODEL_HEAD] =
        {
            .size = {8 * MODEL_PIXEL, 8 * MODEL_PIXEL, 8 * MODEL_PIXEL},
            .pivot = {0, 24 * MODEL_PIXEL, 0},
            .centerOffset = {0, 4 * MODEL_PIXEL, 0},
        },
    [PLAYER_MODEL_TORSO] =
        {
            .size = {8 * MODEL_PIXEL, 12 * MODEL_PIXEL, 4 * MODEL_PIXEL},
            .pivot = {0, 24 * MODEL_PIXEL, 0},
            .centerOffset = {0, -6 * MODEL_PIXEL, 0},
        },
    [PLAYER_MODEL_RIGHT_ARM] =
        {
            .size = {4 * MODEL_PIXEL, 12 * MODEL_PIXEL, 4 * MODEL_PIXEL},
            .pivot = {5 * MODEL_PIXEL, 22 * MODEL_PIXEL, 0},
            .centerOffset = {MODEL_PIXEL, -4 * MODEL_PIXEL, 0},
        },
    [PLAYER_MODEL_LEFT_ARM] =
        {
            .size = {4 * MODEL_PIXEL, 12 * MODEL_PIXEL, 4 * MODEL_PIXEL},
            .pivot = {-5 * MODEL_PIXEL, 22 * MODEL_PIXEL, 0},
            .centerOffset = {-MODEL_PIXEL, -4 * MODEL_PIXEL, 0},
        },
    [PLAYER_MODEL_RIGHT_LEG] =
        {
            .size = {4 * MODEL_PIXEL, 12 * MODEL_PIXEL, 4 * MODEL_PIXEL},
            .pivot = {1.9f * MODEL_PIXEL, 12 * MODEL_PIXEL, 0},
            .centerOffset = {0, -6 * MODEL_PIXEL, 0},
        },
    [PLAYER_MODEL_LEFT_LEG] =
        {
            .size = {4 * MODEL_PIXEL, 12 * MODEL_PIXEL, 4 * MODEL_PIXEL},
            .pivot = {-1.9f * MODEL_PIXEL, 12 * MODEL_PIXEL, 0},
            .centerOffset = {0, -6 * MODEL_PIXEL, 0},
        },
};

static bool finiteVec3(Vec3 value) {
  return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static float clamp01(float value) {
  return fmaxf(0.0f, fminf(1.0f, value));
}

static PlayerPartPose neutralPartPose(void) {
  return (PlayerPartPose){.scale = {1, 1, 1}};
}

const PlayerSkinRect* playerModelSkinRect(PlayerModelPart part, PlayerSkinLayer layer, PlayerModelFace face) {
  if (part < 0 || part >= PLAYER_MODEL_PART_COUNT || layer < 0 || layer >= PLAYER_SKIN_LAYER_COUNT || face < 0 || face >= PLAYER_MODEL_FACE_COUNT)
    return NULL;
  return &skinRects[part][layer][face];
}

const PlayerPartSpec* playerModelPartSpec(PlayerModelPart part) {
  return part >= 0 && part < PLAYER_MODEL_PART_COUNT ? &partSpecs[part] : NULL;
}

float playerModelOuterInflation(PlayerModelPart part) {
  if (part < 0 || part >= PLAYER_MODEL_PART_COUNT)
    return 0;
  return (part == PLAYER_MODEL_HEAD ? 0.5f : 0.25f) * MODEL_PIXEL;
}

static void fitCrouchedPose(PlayerModelPose* pose) {
  float bottom = 0, top = 0;
  for (int part = 0; part < PLAYER_MODEL_PART_COUNT; part++) {
    const PlayerPartSpec* spec = &partSpecs[part];
    const PlayerPartPose* joint = &pose->parts[part];
    float sx = sinf(joint->rotation.x), cx = cosf(joint->rotation.x);
    float sy = sinf(joint->rotation.y), cy = cosf(joint->rotation.y);
    float sz = sinf(joint->rotation.z), cz = cosf(joint->rotation.z);
    // Y row of Rz * Ry * Rx, including the local scale. The absolute row
    // projects the cuboid's half extents, including dilation on all six faces.
    Vec3 row = {sz * cy * joint->scale.x, (sz * sy * sx + cz * cx) * joint->scale.y, (sz * sy * cx - cz * sx) * joint->scale.z};
    float center = spec->pivot.y + joint->translation.y + vec3_dot(&row, &spec->centerOffset);
    float inflation = playerModelOuterInflation((PlayerModelPart)part);
    float extent = fabsf(row.x) * (spec->size.x * 0.5f + inflation) + fabsf(row.y) * (spec->size.y * 0.5f + inflation) + fabsf(row.z) * (spec->size.z * 0.5f + inflation);
    bottom = fminf(bottom, center - extent);
    top = fmaxf(top, center + extent);
  }
  // Rotating the head can increase height even after the nominal crouch scale.
  // Fit the whole posed shell between feet and ceiling without moving physics.
  pose->rootScale = fminf(pose->rootScale, PLAYER_CROUCH_HEIGHT / (top - bottom));
  for (int part = 0; part < PLAYER_MODEL_PART_COUNT; part++)
    pose->parts[part].translation.y -= bottom;
}

void playerModelPose(PlayerModelPose* pose, const PlayerPoseInput* input) {
  if (!pose)
    return;
  *pose = (PlayerModelPose){.rootScale = 1.0f};
  for (int part = 0; part < PLAYER_MODEL_PART_COUNT; part++)
    pose->parts[part] = neutralPartPose();
  if (!input)
    return;

  float yaw = isfinite(input->yaw) ? fmodf(input->yaw, 360.0f) : 90.0f;
  pose->rootYaw = toRadians(270.0f - yaw);
  float pitch = isfinite(input->pitch) ? fmaxf(-89.0f, fminf(89.0f, input->pitch)) : 0.0f;
  pose->parts[PLAYER_MODEL_HEAD].rotation.x = toRadians(pitch);

  double phase = isfinite(input->gaitPhase) ? fmod(input->gaitPhase, MODEL_TWO_PI) : 0.0;
  float weight = isfinite(input->gaitWeight) ? clamp01(input->gaitWeight) : 0.0f;
  if (!input->grounded || input->flying)
    weight = 0.0f;
  pose->gaitPhase = phase;
  pose->gaitWeight = weight;
  // Negate the vanilla X/Y joint angles when converting its downward-Y model
  // to our upward-Y model. Positive arm X reaches toward the local -Z front.
  float swing = cosf((float)phase) * weight;
  pose->parts[PLAYER_MODEL_RIGHT_ARM].rotation.x += swing;
  pose->parts[PLAYER_MODEL_LEFT_ARM].rotation.x -= swing;
  pose->parts[PLAYER_MODEL_RIGHT_LEG].rotation.x -= swing * 1.4f;
  pose->parts[PLAYER_MODEL_LEFT_LEG].rotation.x += swing * 1.4f;
  pose->parts[PLAYER_MODEL_RIGHT_ARM].rotation.z = 0.1f;
  pose->parts[PLAYER_MODEL_LEFT_ARM].rotation.z = -0.1f;

  if (!input->grounded && !input->flying) {
    // Keep jumping/falling visually distinct from a frozen walk cycle.
    pose->parts[PLAYER_MODEL_RIGHT_ARM].rotation.x -= 0.16f;
    pose->parts[PLAYER_MODEL_LEFT_ARM].rotation.x -= 0.16f;
    pose->parts[PLAYER_MODEL_RIGHT_LEG].rotation.x += 0.20f;
    pose->parts[PLAYER_MODEL_LEFT_LEG].rotation.x -= 0.10f;
  }

  if (input->crouched) {
    // Scale about the feet so the rendered body obeys the existing one-unit
    // crouched height, then lean the upper body and legs so the posture reads as
    // a squat instead of only a uniformly smaller standing model.
    pose->rootScale = PLAYER_CROUCH_HEIGHT / PLAYER_HEIGHT;
    const float lean = -0.5f;
    const float shoulderHeight = 12 * MODEL_PIXEL;
    Vec3 shoulderShift = {0, shoulderHeight * (cosf(lean) - 1.0f), shoulderHeight * sinf(lean)};
    pose->parts[PLAYER_MODEL_TORSO].rotation.x += lean;
    pose->parts[PLAYER_MODEL_TORSO].translation = shoulderShift;
    pose->parts[PLAYER_MODEL_HEAD].translation = shoulderShift;
    pose->parts[PLAYER_MODEL_RIGHT_ARM].translation = shoulderShift;
    pose->parts[PLAYER_MODEL_LEFT_ARM].translation = shoulderShift;
    pose->parts[PLAYER_MODEL_RIGHT_ARM].rotation.x -= 0.4f;
    pose->parts[PLAYER_MODEL_LEFT_ARM].rotation.x -= 0.4f;
    pose->parts[PLAYER_MODEL_RIGHT_LEG].rotation.x += 0.24f;
    pose->parts[PLAYER_MODEL_LEFT_LEG].rotation.x += 0.24f;
  }

  float punch = isfinite(input->punch) ? clamp01(input->punch) : 0.0f;
  pose->punch = punch;
  if (punch > 0 && punch < 1) {
    float bodyYaw = -sinf(sqrtf(punch) * (float)MODEL_TWO_PI) * 0.2f;
    pose->parts[PLAYER_MODEL_TORSO].rotation.y = bodyYaw;
    for (int arm = PLAYER_MODEL_RIGHT_ARM; arm <= PLAYER_MODEL_LEFT_ARM; arm++) {
      float shoulderX = partSpecs[arm].pivot.x;
      pose->parts[arm].translation.x += shoulderX * (cosf(bodyYaw) - 1);
      pose->parts[arm].translation.z -= shoulderX * sinf(bodyYaw);
      pose->parts[arm].rotation.y += bodyYaw;
    }
    float recovery = 1 - punch;
    float strike = sinf((1 - recovery * recovery * recovery * recovery) * (float)(MODEL_TWO_PI * 0.5));
    float arc = sinf(punch * (float)(MODEL_TWO_PI * 0.5));
    pose->parts[PLAYER_MODEL_RIGHT_ARM].rotation.x += strike * 1.2f + arc * (0.7f + toRadians(pitch)) * 0.75f;
    pose->parts[PLAYER_MODEL_RIGHT_ARM].rotation.y += bodyYaw * 2;
    pose->parts[PLAYER_MODEL_RIGHT_ARM].rotation.z -= arc * 0.4f;
    pose->parts[PLAYER_MODEL_LEFT_ARM].rotation.x += bodyYaw;
  }
  if (input->crouched)
    fitCrouchedPose(pose);
}

static void translateHand(Mat4 transform, float x, float y, float z) {
  for (int row = 0; row < 4; row++)
    transform[12 + row] += transform[row] * x + transform[4 + row] * y + transform[8 + row] * z;
}

static void rotateHand(Mat4 transform, int axis, float radians) {
  Mat4 rotation;
  mat4_identity(rotation);
  int first = (axis + 1) % 3, second = (axis + 2) % 3;
  float sine = sinf(radians), cosine = cosf(radians);
  rotation[first * 4 + first] = rotation[second * 4 + second] = cosine;
  rotation[first * 4 + second] = sine;
  rotation[second * 4 + first] = -sine;
  mat4_multiply(transform, transform, rotation);
}

void playerModelHandTransform(Mat4 transform, const PlayerModelPose* pose, float aspect) {
  if (!transform)
    return;
  mat4_identity(transform);
  if (!pose || !isfinite(aspect) || aspect <= 0)
    return;
  float punch = isfinite(pose->punch) ? clamp01(pose->punch) : 0;
  if (punch == 1)
    punch = 0;
  float phase = isfinite(pose->gaitPhase) ? (float)fmod(pose->gaitPhase, MODEL_TWO_PI) : 0;
  float weight = isfinite(pose->gaitWeight) ? clamp01(pose->gaitWeight) : 0;

  // Keep the classic 16:9 composition. Fit narrower windows uniformly about
  // the foreground arm, preserving the four-pixel wrist's proportions.
  translateHand(transform, 0.75f * (aspect / (16.0f / 9.0f) - 1), 0, 0);
  float fit = fminf(1, aspect / (4.0f / 3.0f));
  if (fit < 1) {
    // Fit about the resting fist, so reducing the foreground mesh does not
    // pull its visible end down below the hotbar in a portrait framebuffer.
    translateHand(transform, 0.75f, -0.44f, -1.1f);
    for (int column = 0; column < 3; column++)
      for (int row = 0; row < 4; row++)
        transform[column * 4 + row] *= fit;
    translateHand(transform, -0.75f, 0.44f, 1.1f);
  }

  // Foreground bob is a small camera-space motion, independent of the world
  // shoulder rotation. Pauses and lack of real displacement ease it to rest.
  float bob = weight * 0.1f;
  translateHand(transform, -sinf(phase) * bob * 0.5f, -fabsf(cosf(phase)) * bob, 0);
  rotateHand(transform, 2, toRadians(-sinf(phase) * bob * 3));
  rotateHand(transform, 0, toRadians(fabsf(cosf(phase + 0.2f)) * bob * 5));

  // Classic bare-hand swing: lift/reach, inward sweep, then lowered recovery.
  // Progress must remain monotonic; a symmetric peak loses the strike/recovery
  // distinction and makes the arm run back through the same poses in reverse.
  float root = sqrtf(punch);
  float reach = sinf(root * (float)(MODEL_TWO_PI * 0.5));
  translateHand(transform, 0.64f - 0.3f * reach, -0.6f + 0.4f * sinf(root * (float)MODEL_TWO_PI), -0.72f - 0.4f * sinf(punch * (float)(MODEL_TWO_PI * 0.5)));
  rotateHand(transform, 1, toRadians(45 + 70 * reach));
  rotateHand(transform, 2, toRadians(-20 * sinf(punch * punch * (float)(MODEL_TWO_PI * 0.5))));
  translateHand(transform, -1, 3.6f, 3.5f);
  rotateHand(transform, 2, toRadians(120));
  rotateHand(transform, 0, toRadians(200));
  rotateHand(transform, 1, toRadians(-135));
  translateHand(transform, 5.6f, 0, 0);

  // The shared mesh uses upward Y and wearer-right X. The classic arm matrix
  // consumes downward Y/wearer-left X in sixteenths, with the shoulder at -5,2.
  translateHand(transform, -5.0f / 16, 2.0f / 16, 0);
  rotateHand(transform, 2, 0.1f);
  const float modelToView = 1.0f / (16 * MODEL_PIXEL);
  for (int column = 0; column < 3; column++)
    for (int row = 0; row < 4; row++)
      transform[column * 4 + row] *= column == 2 ? modelToView : -modelToView;
}

static void clearAnimation(PlayerModelAnimation* state) {
  if (state)
    *state = (PlayerModelAnimation){0};
}

void resetPlayerModelAnimation(PlayerModelAnimation* state, Vec3 feet) {
  if (!state)
    return;
  clearAnimation(state);
  if (!finiteVec3(feet))
    return;
  state->previousFeet = feet;
  state->initialized = true;
}

void advancePlayerModelAnimation(PlayerModelAnimation* state, const Player* player, bool flying, bool active, double seconds) {
  if (!state)
    return;
  if (!player || !finiteVec3(player->position) || !isfinite(seconds) || seconds < 0) {
    clearAnimation(state);
    return;
  }
  if (!state->initialized || !active || flying) {
    resetPlayerModelAnimation(state, player->position);
    return;
  }

  double dt = fmin(seconds, MODEL_MAX_FRAME_SECONDS);
  double dx = (double)player->position.x - state->previousFeet.x;
  double dz = (double)player->position.z - state->previousFeet.z;
  double distance = hypot(dx, dz);
  double teleportLimit = PLAYER_RUN_SPEED * dt + PLAYER_RADIUS;
  if (!isfinite(distance) || distance > teleportLimit) {
    resetPlayerModelAnimation(state, player->position);
    return;
  }

  bool walking = player->grounded && distance > 1e-6;
  if (walking) {
    state->gaitPhase = fmod(state->gaitPhase + distance * MODEL_GAIT_RADIANS_PER_BLOCK, MODEL_TWO_PI);
    if (state->gaitPhase < 0)
      state->gaitPhase += MODEL_TWO_PI;
  }

  float targetWeight = walking && dt > 0 ? clamp01((float)(distance * 0.2 / dt)) : 0;
  float blend = (float)(-expm1(-MODEL_GAIT_RESPONSE * dt));
  state->gaitWeight += (targetWeight - state->gaitWeight) * blend;
  state->gaitWeight = clamp01(state->gaitWeight);
  state->previousFeet = player->position;
}

float playerModelPunch(const BlockBreaking* breaking) {
  if (!breaking || !breaking->active || !isfinite(breaking->elapsed) || breaking->elapsed <= 0)
    return 0.0f;
  double phase = fmod(breaking->elapsed, MODEL_PUNCH_SECONDS) / MODEL_PUNCH_SECONDS;
  if (phase < 0)
    phase += 1.0;
  return (float)phase;
}
