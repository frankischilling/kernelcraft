#ifndef PLAYER_MODEL_CHECKS_H
#define PLAYER_MODEL_CHECKS_H

#include "world/player_model.h"

#include <math.h>

static void checkModelFloat(float actual, float expected) {
  CHECK(fabsf(actual - expected) < 0.00001f);
}

static void checkModelVec3(Vec3 actual, Vec3 expected) {
  checkModelFloat(actual.x, expected.x);
  checkModelFloat(actual.y, expected.y);
  checkModelFloat(actual.z, expected.z);
}

static void checkModelMat4(const Mat4 actual, const Mat4 expected) {
  for (int i = 0; i < 16; i++)
    checkModelFloat(actual[i], expected[i]);
}

static Vec3 transformModelPoint(const Mat4 transform, Vec3 point) {
  return (Vec3){
      transform[0] * point.x + transform[4] * point.y + transform[8] * point.z + transform[12],
      transform[1] * point.x + transform[5] * point.y + transform[9] * point.z + transform[13],
      transform[2] * point.x + transform[6] * point.y + transform[10] * point.z + transform[14],
  };
}

static Vec3 posedModelPoint(PlayerModelPart part, const PlayerModelPose* pose, Vec3 localPoint) {
  const PlayerPartSpec* spec = playerModelPartSpec(part);
  const PlayerPartPose* joint = &pose->parts[part];
  Vec3 p = {localPoint.x * joint->scale.x, localPoint.y * joint->scale.y, localPoint.z * joint->scale.z};
  float sx = sinf(joint->rotation.x), cx = cosf(joint->rotation.x);
  float sy = sinf(joint->rotation.y), cy = cosf(joint->rotation.y);
  float sz = sinf(joint->rotation.z), cz = cosf(joint->rotation.z);
  p = (Vec3){p.x, cx * p.y - sx * p.z, sx * p.y + cx * p.z};
  p = (Vec3){cy * p.x + sy * p.z, p.y, -sy * p.x + cy * p.z};
  p = (Vec3){cz * p.x - sz * p.y, sz * p.x + cz * p.y, p.z};
  return (Vec3){p.x + spec->pivot.x + joint->translation.x, p.y + spec->pivot.y + joint->translation.y, p.z + spec->pivot.z + joint->translation.z};
}

static Vec3 posedDistalCenter(PlayerModelPart part, const PlayerModelPose* pose) {
  const PlayerPartSpec* spec = playerModelPartSpec(part);
  return posedModelPoint(part, pose, (Vec3){spec->centerOffset.x, spec->centerOffset.y - spec->size.y * 0.5f, spec->centerOffset.z});
}

static void testPlayerSkinLayout(void) {
  // Independent standard 64x64 classic-arm atlas rectangles, in the public
  // RIGHT, LEFT, TOP, BOTTOM, FRONT, BACK face order. Pixel Y is top-down.
  static const PlayerSkinRect expected[PLAYER_MODEL_PART_COUNT][PLAYER_SKIN_LAYER_COUNT][PLAYER_MODEL_FACE_COUNT] =
      {
          [PLAYER_MODEL_HEAD] =
              {
                  [PLAYER_SKIN_BASE] = {{0, 8, 8, 8}, {16, 8, 8, 8}, {8, 0, 8, 8}, {16, 0, 8, 8}, {8, 8, 8, 8}, {24, 8, 8, 8}},
                  [PLAYER_SKIN_OUTER] = {{32, 8, 8, 8}, {48, 8, 8, 8}, {40, 0, 8, 8}, {48, 0, 8, 8}, {40, 8, 8, 8}, {56, 8, 8, 8}},
              },
          [PLAYER_MODEL_TORSO] =
              {
                  [PLAYER_SKIN_BASE] = {{16, 20, 4, 12}, {28, 20, 4, 12}, {20, 16, 8, 4}, {28, 16, 8, 4}, {20, 20, 8, 12}, {32, 20, 8, 12}},
                  [PLAYER_SKIN_OUTER] = {{16, 36, 4, 12}, {28, 36, 4, 12}, {20, 32, 8, 4}, {28, 32, 8, 4}, {20, 36, 8, 12}, {32, 36, 8, 12}},
              },
          [PLAYER_MODEL_RIGHT_ARM] =
              {
                  [PLAYER_SKIN_BASE] = {{40, 20, 4, 12}, {48, 20, 4, 12}, {44, 16, 4, 4}, {48, 16, 4, 4}, {44, 20, 4, 12}, {52, 20, 4, 12}},
                  [PLAYER_SKIN_OUTER] = {{40, 36, 4, 12}, {48, 36, 4, 12}, {44, 32, 4, 4}, {48, 32, 4, 4}, {44, 36, 4, 12}, {52, 36, 4, 12}},
              },
          [PLAYER_MODEL_LEFT_ARM] =
              {
                  [PLAYER_SKIN_BASE] = {{32, 52, 4, 12}, {40, 52, 4, 12}, {36, 48, 4, 4}, {40, 48, 4, 4}, {36, 52, 4, 12}, {44, 52, 4, 12}},
                  [PLAYER_SKIN_OUTER] = {{48, 52, 4, 12}, {56, 52, 4, 12}, {52, 48, 4, 4}, {56, 48, 4, 4}, {52, 52, 4, 12}, {60, 52, 4, 12}},
              },
          [PLAYER_MODEL_RIGHT_LEG] =
              {
                  [PLAYER_SKIN_BASE] = {{0, 20, 4, 12}, {8, 20, 4, 12}, {4, 16, 4, 4}, {8, 16, 4, 4}, {4, 20, 4, 12}, {12, 20, 4, 12}},
                  [PLAYER_SKIN_OUTER] = {{0, 36, 4, 12}, {8, 36, 4, 12}, {4, 32, 4, 4}, {8, 32, 4, 4}, {4, 36, 4, 12}, {12, 36, 4, 12}},
              },
          [PLAYER_MODEL_LEFT_LEG] =
              {
                  [PLAYER_SKIN_BASE] = {{16, 52, 4, 12}, {24, 52, 4, 12}, {20, 48, 4, 4}, {24, 48, 4, 4}, {20, 52, 4, 12}, {28, 52, 4, 12}},
                  [PLAYER_SKIN_OUTER] = {{0, 52, 4, 12}, {8, 52, 4, 12}, {4, 48, 4, 4}, {8, 48, 4, 4}, {4, 52, 4, 12}, {12, 52, 4, 12}},
              },
      };

  for (int part = 0; part < PLAYER_MODEL_PART_COUNT; part++)
    for (int layer = 0; layer < PLAYER_SKIN_LAYER_COUNT; layer++)
      for (int face = 0; face < PLAYER_MODEL_FACE_COUNT; face++) {
        const PlayerSkinRect* actual = playerModelSkinRect((PlayerModelPart)part, (PlayerSkinLayer)layer, (PlayerModelFace)face);
        const PlayerSkinRect* wanted = &expected[part][layer][face];
        CHECK(actual != NULL);
        CHECK(actual->x == wanted->x && actual->y == wanted->y && actual->width == wanted->width && actual->height == wanted->height);
        CHECK((int)actual->x + actual->width <= PLAYER_SKIN_SIZE && (int)actual->y + actual->height <= PLAYER_SKIN_SIZE);
      }
}

static void testPlayerPartGeometry(void) {
  const float unit = PLAYER_HEIGHT / 32.0f;
  static const int sizePixels[PLAYER_MODEL_PART_COUNT][3] = {
      [PLAYER_MODEL_HEAD] = {8, 8, 8},      [PLAYER_MODEL_TORSO] = {8, 12, 4},     [PLAYER_MODEL_RIGHT_ARM] = {4, 12, 4},
      [PLAYER_MODEL_LEFT_ARM] = {4, 12, 4}, [PLAYER_MODEL_RIGHT_LEG] = {4, 12, 4}, [PLAYER_MODEL_LEFT_LEG] = {4, 12, 4},
  };
  static const float pivotPixels[PLAYER_MODEL_PART_COUNT][3] = {
      [PLAYER_MODEL_HEAD] = {0, 24, 0},      [PLAYER_MODEL_TORSO] = {0, 24, 0},        [PLAYER_MODEL_RIGHT_ARM] = {5, 22, 0},
      [PLAYER_MODEL_LEFT_ARM] = {-5, 22, 0}, [PLAYER_MODEL_RIGHT_LEG] = {1.9f, 12, 0}, [PLAYER_MODEL_LEFT_LEG] = {-1.9f, 12, 0},
  };
  static const int centerPixels[PLAYER_MODEL_PART_COUNT][3] = {
      [PLAYER_MODEL_HEAD] = {0, 4, 0},       [PLAYER_MODEL_TORSO] = {0, -6, 0},     [PLAYER_MODEL_RIGHT_ARM] = {1, -4, 0},
      [PLAYER_MODEL_LEFT_ARM] = {-1, -4, 0}, [PLAYER_MODEL_RIGHT_LEG] = {0, -6, 0}, [PLAYER_MODEL_LEFT_LEG] = {0, -6, 0},
  };
  // Absolute base-cuboid bounds in classic-skin pixels. These check the
  // assembled silhouette independently from the joint representation above.
  static const float boundsPixels[PLAYER_MODEL_PART_COUNT][6] = {
      [PLAYER_MODEL_HEAD] = {-4, 4, 24, 32, -4, 4},      [PLAYER_MODEL_TORSO] = {-4, 4, 12, 24, -2, 2},          [PLAYER_MODEL_RIGHT_ARM] = {4, 8, 12, 24, -2, 2},
      [PLAYER_MODEL_LEFT_ARM] = {-8, -4, 12, 24, -2, 2}, [PLAYER_MODEL_RIGHT_LEG] = {-0.1f, 3.9f, 0, 12, -2, 2}, [PLAYER_MODEL_LEFT_LEG] = {-3.9f, 0.1f, 0, 12, -2, 2},
  };

  float lowest = PLAYER_HEIGHT, highest = 0, leftmost = 0, rightmost = 0;
  for (int part = 0; part < PLAYER_MODEL_PART_COUNT; part++) {
    const PlayerPartSpec* spec = playerModelPartSpec((PlayerModelPart)part);
    CHECK(spec != NULL);
    const float expectedOuterInflation = (part == PLAYER_MODEL_HEAD ? 0.5f : 0.25f) * unit;
    checkModelFloat(playerModelOuterInflation((PlayerModelPart)part), expectedOuterInflation);
    Vec3 expectedSize = {sizePixels[part][0] * unit, sizePixels[part][1] * unit, sizePixels[part][2] * unit};
    Vec3 expectedPivot = {pivotPixels[part][0] * unit, pivotPixels[part][1] * unit, pivotPixels[part][2] * unit};
    Vec3 expectedCenter = {centerPixels[part][0] * unit, centerPixels[part][1] * unit, centerPixels[part][2] * unit};
    checkModelVec3(spec->size, expectedSize);
    checkModelVec3(spec->pivot, expectedPivot);
    checkModelVec3(spec->centerOffset, expectedCenter);

    float centerX = spec->pivot.x + spec->centerOffset.x;
    float centerY = spec->pivot.y + spec->centerOffset.y;
    float centerZ = spec->pivot.z + spec->centerOffset.z;
    checkModelFloat(centerX - spec->size.x * 0.5f, boundsPixels[part][0] * unit);
    checkModelFloat(centerX + spec->size.x * 0.5f, boundsPixels[part][1] * unit);
    checkModelFloat(centerY - spec->size.y * 0.5f, boundsPixels[part][2] * unit);
    checkModelFloat(centerY + spec->size.y * 0.5f, boundsPixels[part][3] * unit);
    checkModelFloat(centerZ - spec->size.z * 0.5f, boundsPixels[part][4] * unit);
    checkModelFloat(centerZ + spec->size.z * 0.5f, boundsPixels[part][5] * unit);
    lowest = fminf(lowest, centerY - spec->size.y * 0.5f);
    highest = fmaxf(highest, centerY + spec->size.y * 0.5f);
    leftmost = fminf(leftmost, centerX - spec->size.x * 0.5f);
    rightmost = fmaxf(rightmost, centerX + spec->size.x * 0.5f);
  }

  checkModelFloat(lowest, 0);
  checkModelFloat(highest, PLAYER_HEIGHT);
  checkModelFloat(leftmost, -8 * unit);
  checkModelFloat(rightmost, 8 * unit);
  checkModelFloat(playerModelOuterInflation((PlayerModelPart)-1), 0);
  checkModelFloat(playerModelOuterInflation(PLAYER_MODEL_PART_COUNT), 0);
}

static void testPlayerModelPoses(void) {
  PlayerModelPose neutral;
  PlayerPoseInput input = {.yaw = -90, .grounded = true};
  playerModelPose(&neutral, &input);
  CHECK(fabsf(sinf(neutral.rootYaw)) < 0.00001f && fabsf(cosf(neutral.rootYaw) - 1) < 0.00001f);
  checkModelFloat(neutral.rootScale, 1);
  for (int part = 0; part < PLAYER_MODEL_PART_COUNT; part++) {
    checkModelVec3(neutral.parts[part].translation, (Vec3){0});
    Vec3 expectedRotation = {0};
    if (part == PLAYER_MODEL_RIGHT_ARM)
      expectedRotation.z = 0.1f;
    else if (part == PLAYER_MODEL_LEFT_ARM)
      expectedRotation.z = -0.1f;
    checkModelVec3(neutral.parts[part].rotation, expectedRotation);
    checkModelVec3(neutral.parts[part].scale, (Vec3){1, 1, 1});
  }

  input.yaw = 0;
  input.pitch = 30;
  PlayerModelPose looking;
  playerModelPose(&looking, &input);
  CHECK(fabsf(cosf(looking.rootYaw)) < 0.00001f && fabsf(sinf(looking.rootYaw) + 1) < 0.00001f);
  checkModelFloat(looking.parts[PLAYER_MODEL_HEAD].rotation.x, 0.5235987755982988f);

  input = (PlayerPoseInput){.yaw = -90, .gaitPhase = 0, .gaitWeight = 1, .grounded = true};
  PlayerModelPose walking;
  playerModelPose(&walking, &input);
  float rightArm = walking.parts[PLAYER_MODEL_RIGHT_ARM].rotation.x;
  float leftArm = walking.parts[PLAYER_MODEL_LEFT_ARM].rotation.x;
  float rightLeg = walking.parts[PLAYER_MODEL_RIGHT_LEG].rotation.x;
  float leftLeg = walking.parts[PLAYER_MODEL_LEFT_LEG].rotation.x;
  CHECK(fabsf(rightArm) > 0.1f);
  CHECK(fabsf(rightArm + leftArm) < 0.00001f);
  CHECK(fabsf(rightLeg + leftLeg) < 0.00001f);
  checkModelFloat(leftLeg, rightArm * 1.4f);
  checkModelFloat(rightLeg, leftArm * 1.4f);
  Vec3 rightHand = posedDistalCenter(PLAYER_MODEL_RIGHT_ARM, &walking);
  Vec3 leftHand = posedDistalCenter(PLAYER_MODEL_LEFT_ARM, &walking);
  Vec3 rightFoot = posedDistalCenter(PLAYER_MODEL_RIGHT_LEG, &walking);
  Vec3 leftFoot = posedDistalCenter(PLAYER_MODEL_LEFT_LEG, &walking);
  CHECK(rightHand.z < -0.01f && leftHand.z > 0.01f);
  CHECK(rightFoot.z > 0.01f && leftFoot.z < -0.01f);

  input.gaitWeight = 0;
  PlayerModelPose stopped;
  playerModelPose(&stopped, &input);
  checkModelFloat(stopped.parts[PLAYER_MODEL_RIGHT_ARM].rotation.x, 0);
  checkModelFloat(stopped.parts[PLAYER_MODEL_LEFT_ARM].rotation.x, 0);
  checkModelFloat(stopped.parts[PLAYER_MODEL_RIGHT_LEG].rotation.x, 0);
  checkModelFloat(stopped.parts[PLAYER_MODEL_LEFT_LEG].rotation.x, 0);

  PlayerPoseInput punchInput = {.yaw = -90, .grounded = true};
  PlayerModelPose punchRest, punching, punchFinished;
  playerModelPose(&punchRest, &punchInput);
  punchInput.punch = 0.1f;
  playerModelPose(&punching, &punchInput);
  punchInput.punch = 1.0f;
  playerModelPose(&punchFinished, &punchInput);
  Vec3 restHand = posedDistalCenter(PLAYER_MODEL_RIGHT_ARM, &punchRest);
  Vec3 punchingHand = posedDistalCenter(PLAYER_MODEL_RIGHT_ARM, &punching);
  Vec3 finishedHand = posedDistalCenter(PLAYER_MODEL_RIGHT_ARM, &punchFinished);
  CHECK(punchingHand.z < restHand.z - 0.01f);
  checkModelVec3(finishedHand, restHand);
  checkModelVec3(punchFinished.parts[PLAYER_MODEL_RIGHT_ARM].rotation, punchRest.parts[PLAYER_MODEL_RIGHT_ARM].rotation);
  checkModelVec3(punchFinished.parts[PLAYER_MODEL_RIGHT_ARM].translation, punchRest.parts[PLAYER_MODEL_RIGHT_ARM].translation);
  checkModelVec3(punchFinished.parts[PLAYER_MODEL_TORSO].rotation, punchRest.parts[PLAYER_MODEL_TORSO].rotation);

  input = (PlayerPoseInput){.yaw = -90, .grounded = false};
  PlayerModelPose airborne;
  playerModelPose(&airborne, &input);
  CHECK(fabsf(airborne.parts[PLAYER_MODEL_RIGHT_ARM].rotation.x) + fabsf(airborne.parts[PLAYER_MODEL_LEFT_ARM].rotation.x) +
            fabsf(airborne.parts[PLAYER_MODEL_RIGHT_LEG].rotation.x) + fabsf(airborne.parts[PLAYER_MODEL_LEFT_LEG].rotation.x) >
        0.1f);

  input = (PlayerPoseInput){.yaw = -90, .grounded = true, .crouched = true};
  PlayerModelPose crouched;
  playerModelPose(&crouched, &input);
  CHECK(crouched.rootScale > 0 && crouched.rootScale <= PLAYER_CROUCH_HEIGHT / PLAYER_HEIGHT);
  CHECK(fabsf(crouched.parts[PLAYER_MODEL_TORSO].rotation.x) > 0.05f);
  CHECK(crouched.rootScale * PLAYER_HEIGHT <= PLAYER_CROUCH_HEIGHT + 0.00001f);
}

static void testCrouchedSkinEnvelope(void) {
  const float unit = PLAYER_HEIGHT / 32.0f;
  const float pitches[] = {-89, -25, 0, 30, 89};
  for (size_t pitch = 0; pitch < sizeof(pitches) / sizeof(*pitches); pitch++)
    for (int gait = 0; gait < 3; gait++)
      for (int punch = 0; punch < 3; punch++) {
        PlayerPoseInput input = {
            .yaw = 45, .pitch = pitches[pitch], .gaitPhase = gait * 1.5707963267948966, .gaitWeight = 1, .punch = punch * 0.5f, .grounded = true, .crouched = true};
        PlayerModelPose pose;
        playerModelPose(&pose, &input);
        // Transform actual corners independently of the production analytic
        // interval bound. Minecraft's classic outer layer adds a fixed amount
        // on every axis rather than scaling the base cuboid proportionally.
        for (int part = 0; part < PLAYER_MODEL_PART_COUNT; part++) {
          const PlayerPartSpec* spec = playerModelPartSpec((PlayerModelPart)part);
          const PlayerPartPose* joint = &pose.parts[part];
          const float expectedOuterInflation = (part == PLAYER_MODEL_HEAD ? 0.5f : 0.25f) * unit;
          for (int layer = 0; layer < PLAYER_SKIN_LAYER_COUNT; layer++) {
            float inflation = layer == PLAYER_SKIN_OUTER ? expectedOuterInflation : 0.0f;
            for (int corner = 0; corner < 8; corner++) {
              Vec3 halfExtent = {spec->size.x * 0.5f + inflation, spec->size.y * 0.5f + inflation, spec->size.z * 0.5f + inflation};
              Vec3 p = {(spec->centerOffset.x + ((corner & 1) ? halfExtent.x : -halfExtent.x)) * joint->scale.x,
                        (spec->centerOffset.y + ((corner & 2) ? halfExtent.y : -halfExtent.y)) * joint->scale.y,
                        (spec->centerOffset.z + ((corner & 4) ? halfExtent.z : -halfExtent.z)) * joint->scale.z};
              float y = p.y * cosf(joint->rotation.x) - p.z * sinf(joint->rotation.x);
              float z = p.y * sinf(joint->rotation.x) + p.z * cosf(joint->rotation.x);
              float x = p.x * cosf(joint->rotation.y) + z * sinf(joint->rotation.y);
              y = x * sinf(joint->rotation.z) + y * cosf(joint->rotation.z);
              y = (y + joint->translation.y + spec->pivot.y) * pose.rootScale;
              CHECK(isfinite(y) && y >= -0.00001f && y <= PLAYER_CROUCH_HEIGHT + 0.00001f);
            }
          }
        }
      }
}

static void testPlayerModelAnimation(void) {
  PlayerModelAnimation animation;
  resetPlayerModelAnimation(&animation, (Vec3){2, 3, 4});
  CHECK(animation.initialized);
  checkModelVec3(animation.previousFeet, (Vec3){2, 3, 4});
  CHECK(animation.gaitPhase == 0 && animation.gaitWeight == 0);

  Player player = {.position = {2, 3, 4}, .grounded = true};
  advancePlayerModelAnimation(&animation, &player, false, true, 0.1);
  CHECK(animation.gaitPhase == 0 && animation.gaitWeight == 0);

  player.position.x += PLAYER_WALK_SPEED * 0.1f;
  double movedDistance = fabs((double)player.position.x - 2.0);
  advancePlayerModelAnimation(&animation, &player, false, true, 0.1);
  double movingPhase = animation.gaitPhase;
  float movingWeight = animation.gaitWeight;
  CHECK(isfinite(movingPhase) && movingPhase > 0);
  CHECK(fabs(movingPhase - movedDistance * 4.0 * 0.6662) < 0.000001);
  CHECK(movingWeight > 0 && movingWeight <= 1);

  advancePlayerModelAnimation(&animation, &player, false, true, 0.1);
  CHECK(animation.gaitPhase == movingPhase);
  CHECK(animation.gaitWeight < movingWeight);

  Vec3 beforeFlight = player.position;
  player.position.x += 1;
  advancePlayerModelAnimation(&animation, &player, true, true, 0.1);
  CHECK(animation.gaitPhase == 0 && animation.gaitWeight == 0 && animation.initialized);
  CHECK(animation.previousFeet.x == player.position.x && animation.previousFeet.y == player.position.y && animation.previousFeet.z == player.position.z);
  CHECK(player.position.x != beforeFlight.x);

  player.position.x += 100;
  advancePlayerModelAnimation(&animation, &player, false, true, 1.0 / 60.0);
  CHECK(animation.gaitPhase == 0 && animation.gaitWeight == 0);

  player.position.z += 0.25f;
  advancePlayerModelAnimation(&animation, &player, false, true, 1.0 / 60.0);
  CHECK(animation.gaitPhase > 0 && animation.gaitWeight > 0);
  advancePlayerModelAnimation(&animation, &player, false, false, 1.0 / 60.0);
  CHECK(animation.gaitPhase == 0 && animation.gaitWeight == 0);

  player.position.z += 0.25f;
  advancePlayerModelAnimation(&animation, &player, false, true, NAN);
  CHECK(animation.gaitPhase == 0 && animation.gaitWeight == 0);
  CHECK(!animation.initialized);
  CHECK(isfinite(animation.previousFeet.x) && isfinite(animation.previousFeet.y) && isfinite(animation.previousFeet.z));

  PlayerModelAnimation steadyWalk;
  Player steadyPlayer = {.position = {0, 0, 0}, .grounded = true};
  resetPlayerModelAnimation(&steadyWalk, steadyPlayer.position);
  for (int i = 0; i < 20; i++) {
    steadyPlayer.position.x += PLAYER_WALK_SPEED * 0.1f;
    advancePlayerModelAnimation(&steadyWalk, &steadyPlayer, false, true, 0.1);
  }
  // The existing walk speed drives 0.9 swing weight (4.5 blocks/s * 0.2), with
  // smoothing allowed to approach that target from below.
  CHECK(steadyWalk.gaitWeight > 0.89f && steadyWalk.gaitWeight <= 0.90001f);
}

static void testPlayerModelHandTransform(void) {
  const float unit = PLAYER_HEIGHT / 32.0f;
  const float aspect = 16.0f / 9.0f;
  const Vec3 distalCenter = {unit, -10 * unit, 0};
  static const float progress[] = {0, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 1};
  static const Vec3 expected[] = {
      {0.75517044f, -0.43795376f, -1.09521622f}, {0.12767027f, -0.07589995f, -1.13886127f}, {0.02248228f, -0.46192497f, -1.24866420f}, {0.15083696f, -0.91314818f, -1.45517019f},
      {0.44860715f, -0.86210127f, -1.42028903f}, {0.64555361f, -0.63541187f, -1.25480399f}, {0.75517044f, -0.43795376f, -1.09521622f},
  };

  PlayerModelPose pose = {.rootScale = 1};
  for (size_t i = 0; i < sizeof(progress) / sizeof(*progress); i++) {
    Mat4 transform;
    pose.punch = progress[i];
    playerModelHandTransform(transform, &pose, aspect);
    checkModelVec3(transformModelPoint(transform, distalCenter), expected[i]);
  }

  // The resting fist stays beside aim and above the hotbar in narrow windows.
  // Fitting about the offscreen shoulder used to push it below the visible HUD.
  const float aspects[] = {0.5f, 9.0f / 16, 4.0f / 3, 16.0f / 9};
  pose.punch = 0;
  for (size_t i = 0; i < sizeof(aspects) / sizeof(*aspects); i++) {
    Mat4 transform;
    playerModelHandTransform(transform, &pose, aspects[i]);
    Vec3 fist = transformModelPoint(transform, distalCenter);
    float halfHeight = -fist.z * 0.70020754f; // tan(35 degrees), the foreground half-FOV.
    CHECK(fist.z < -0.05f);
    CHECK(fist.x / (halfHeight * aspects[i]) > 0.45f && fist.x / (halfHeight * aspects[i]) < 0.65f);
    CHECK(fist.y / halfHeight > -0.7f && fist.y / halfHeight < -0.45f);
  }

  // First-person presentation must not inherit the posed world joint/root state.
  // The old shader path reused these values and made gait/body motion distort the hand.
  pose = (PlayerModelPose){.rootScale = 1, .gaitPhase = 0.4, .gaitWeight = 0.35f, .punch = 0.25f};
  Mat4 reference, perturbed;
  playerModelHandTransform(reference, &pose, aspect);
  pose.rootYaw = 2.3f;
  pose.rootScale = 0.37f;
  for (int part = 0; part < PLAYER_MODEL_PART_COUNT; part++) {
    pose.parts[part].translation = (Vec3){1.5f + part, -2.0f - part, 0.75f * part};
    pose.parts[part].rotation = (Vec3){0.4f + part, -0.7f - part, 1.1f + part};
    pose.parts[part].scale = (Vec3){0.2f + part, 1.3f + part, 2.4f + part};
  }
  playerModelHandTransform(perturbed, &pose, aspect);
  checkModelMat4(perturbed, reference);

  // Gait affects only a shallow camera-space bob. Quarter-cycle phase must move
  // the hand, while remaining much smaller than the block-breaking swing.
  PlayerModelPose bob = {.rootScale = 1, .gaitWeight = 1, .punch = 0};
  Mat4 phase0, phaseQuarter;
  bob.gaitPhase = 0;
  playerModelHandTransform(phase0, &bob, aspect);
  bob.gaitPhase = 1.5707963267948966;
  playerModelHandTransform(phaseQuarter, &bob, aspect);
  Vec3 bob0 = transformModelPoint(phase0, distalCenter);
  Vec3 bobQuarter = transformModelPoint(phaseQuarter, distalCenter);
  float bobDistance = vec3_distance(&bob0, &bobQuarter);
  CHECK(bobDistance > 0.01f && bobDistance < 0.2f);

  PlayerModelPose invalid = {.rootYaw = NAN, .rootScale = INFINITY, .gaitPhase = NAN, .gaitWeight = INFINITY, .punch = -INFINITY};
  for (int part = 0; part < PLAYER_MODEL_PART_COUNT; part++) {
    invalid.parts[part].translation = (Vec3){NAN, INFINITY, -INFINITY};
    invalid.parts[part].rotation = (Vec3){INFINITY, NAN, -INFINITY};
    invalid.parts[part].scale = (Vec3){NAN, INFINITY, -INFINITY};
  }
  Mat4 bounded;
  playerModelHandTransform(bounded, &invalid, aspect);
  for (int i = 0; i < 16; i++)
    CHECK(isfinite(bounded[i]));
  Vec3 boundedHand = transformModelPoint(bounded, distalCenter);
  CHECK(isfinite(boundedHand.x) && isfinite(boundedHand.y) && isfinite(boundedHand.z));
}

static void testPlayerModelPunch(void) {
  BlockBreaking breaking = {0};
  checkModelFloat(playerModelPunch(NULL), 0);
  checkModelFloat(playerModelPunch(&breaking), 0);
  breaking.active = true;
  breaking.block = BLOCK_DIRT;
  breaking.elapsed = 0;
  checkModelFloat(playerModelPunch(&breaking), 0);
  breaking.elapsed = 0.075;
  checkModelFloat(playerModelPunch(&breaking), 0.25f);
  breaking.elapsed = 0.15;
  checkModelFloat(playerModelPunch(&breaking), 0.5f);
  breaking.elapsed = 0.225;
  checkModelFloat(playerModelPunch(&breaking), 0.75f);
  breaking.elapsed = 0.3;
  checkModelFloat(playerModelPunch(&breaking), 0);
  breaking.elapsed = 0.375;
  float dirt = playerModelPunch(&breaking);
  checkModelFloat(dirt, 0.25f);
  breaking.block = BLOCK_STONE;
  checkModelFloat(playerModelPunch(&breaking), dirt);
  breaking.target = (Vec3i){17, 3, -9};
  checkModelFloat(playerModelPunch(&breaking), dirt);
  breaking.elapsed += 0.3;
  checkModelFloat(playerModelPunch(&breaking), dirt);
  breaking.elapsed = NAN;
  checkModelFloat(playerModelPunch(&breaking), 0);
}

static void testPlayerModel(void) {
  testPlayerSkinLayout();
  testPlayerPartGeometry();
  testPlayerModelPoses();
  testCrouchedSkinEnvelope();
  testPlayerModelAnimation();
  testPlayerModelHandTransform();
  testPlayerModelPunch();
}

#endif
