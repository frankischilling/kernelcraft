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
  static const int pivotPixels[PLAYER_MODEL_PART_COUNT][3] = {
      [PLAYER_MODEL_HEAD] = {0, 24, 0},      [PLAYER_MODEL_TORSO] = {0, 12, 0},     [PLAYER_MODEL_RIGHT_ARM] = {6, 24, 0},
      [PLAYER_MODEL_LEFT_ARM] = {-6, 24, 0}, [PLAYER_MODEL_RIGHT_LEG] = {2, 12, 0}, [PLAYER_MODEL_LEFT_LEG] = {-2, 12, 0},
  };
  static const int centerPixels[PLAYER_MODEL_PART_COUNT][3] = {
      [PLAYER_MODEL_HEAD] = {0, 4, 0},      [PLAYER_MODEL_TORSO] = {0, 6, 0},      [PLAYER_MODEL_RIGHT_ARM] = {0, -6, 0},
      [PLAYER_MODEL_LEFT_ARM] = {0, -6, 0}, [PLAYER_MODEL_RIGHT_LEG] = {0, -6, 0}, [PLAYER_MODEL_LEFT_LEG] = {0, -6, 0},
  };

  float lowest = PLAYER_HEIGHT, highest = 0, leftmost = 0, rightmost = 0;
  for (int part = 0; part < PLAYER_MODEL_PART_COUNT; part++) {
    const PlayerPartSpec* spec = playerModelPartSpec((PlayerModelPart)part);
    CHECK(spec != NULL);
    Vec3 expectedSize = {sizePixels[part][0] * unit, sizePixels[part][1] * unit, sizePixels[part][2] * unit};
    Vec3 expectedPivot = {pivotPixels[part][0] * unit, pivotPixels[part][1] * unit, pivotPixels[part][2] * unit};
    Vec3 expectedCenter = {centerPixels[part][0] * unit, centerPixels[part][1] * unit, centerPixels[part][2] * unit};
    checkModelVec3(spec->size, expectedSize);
    checkModelVec3(spec->pivot, expectedPivot);
    checkModelVec3(spec->centerOffset, expectedCenter);

    float centerX = spec->pivot.x + spec->centerOffset.x;
    float centerY = spec->pivot.y + spec->centerOffset.y;
    lowest = fminf(lowest, centerY - spec->size.y * 0.5f);
    highest = fmaxf(highest, centerY + spec->size.y * 0.5f);
    leftmost = fminf(leftmost, centerX - spec->size.x * 0.5f);
    rightmost = fmaxf(rightmost, centerX + spec->size.x * 0.5f);
  }

  checkModelFloat(lowest, 0);
  checkModelFloat(highest, PLAYER_HEIGHT);
  checkModelFloat(leftmost, -8 * unit);
  checkModelFloat(rightmost, 8 * unit);
}

static void testPlayerModelPoses(void) {
  PlayerModelPose neutral;
  PlayerPoseInput input = {.yaw = -90, .grounded = true};
  playerModelPose(&neutral, &input);
  CHECK(fabsf(sinf(neutral.rootYaw)) < 0.00001f && fabsf(cosf(neutral.rootYaw) - 1) < 0.00001f);
  checkModelFloat(neutral.rootScale, 1);
  for (int part = 0; part < PLAYER_MODEL_PART_COUNT; part++) {
    checkModelVec3(neutral.parts[part].translation, (Vec3){0});
    checkModelVec3(neutral.parts[part].rotation, (Vec3){0});
    checkModelVec3(neutral.parts[part].scale, (Vec3){1, 1, 1});
  }

  input.yaw = 0;
  input.pitch = 30;
  PlayerModelPose looking;
  playerModelPose(&looking, &input);
  CHECK(fabsf(cosf(looking.rootYaw)) < 0.00001f && fabsf(sinf(looking.rootYaw) + 1) < 0.00001f);
  checkModelFloat(looking.parts[PLAYER_MODEL_HEAD].rotation.x, 0.5235987755982988f);

  input = (PlayerPoseInput){.yaw = -90, .gaitPhase = 0.25, .gaitWeight = 1, .grounded = true};
  PlayerModelPose walking;
  playerModelPose(&walking, &input);
  float rightArm = walking.parts[PLAYER_MODEL_RIGHT_ARM].rotation.x;
  float leftArm = walking.parts[PLAYER_MODEL_LEFT_ARM].rotation.x;
  float rightLeg = walking.parts[PLAYER_MODEL_RIGHT_LEG].rotation.x;
  float leftLeg = walking.parts[PLAYER_MODEL_LEFT_LEG].rotation.x;
  CHECK(fabsf(rightArm) > 0.1f);
  CHECK(fabsf(rightArm + leftArm) < 0.00001f);
  CHECK(fabsf(rightLeg + leftLeg) < 0.00001f);
  CHECK(fabsf(rightArm - leftLeg) < 0.00001f);
  CHECK(fabsf(leftArm - rightLeg) < 0.00001f);

  input.gaitWeight = 0;
  PlayerModelPose stopped;
  playerModelPose(&stopped, &input);
  checkModelFloat(stopped.parts[PLAYER_MODEL_RIGHT_ARM].rotation.x, 0);
  checkModelFloat(stopped.parts[PLAYER_MODEL_LEFT_ARM].rotation.x, 0);
  checkModelFloat(stopped.parts[PLAYER_MODEL_RIGHT_LEG].rotation.x, 0);
  checkModelFloat(stopped.parts[PLAYER_MODEL_LEFT_LEG].rotation.x, 0);

  input.punch = 1;
  PlayerModelPose punching;
  playerModelPose(&punching, &input);
  CHECK(punching.parts[PLAYER_MODEL_RIGHT_ARM].rotation.x > 0.3f);
  checkModelFloat(punching.parts[PLAYER_MODEL_LEFT_ARM].rotation.x, 0);

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
  const float pitches[] = {-89, -25, 0, 30, 89};
  for (size_t pitch = 0; pitch < sizeof(pitches) / sizeof(*pitches); pitch++)
    for (int gait = 0; gait < 3; gait++)
      for (int punch = 0; punch < 3; punch++) {
        PlayerPoseInput input = {
            .yaw = 45, .pitch = pitches[pitch], .gaitPhase = gait * 1.5707963267948966, .gaitWeight = 1, .punch = punch * 0.5f, .grounded = true, .crouched = true};
        PlayerModelPose pose;
        playerModelPose(&pose, &input);
        // Transform actual corners independently of the production analytic
        // interval bound. Include the outer layer, which grows along X and Z.
        for (int part = 0; part < PLAYER_MODEL_PART_COUNT; part++) {
          const PlayerPartSpec* spec = playerModelPartSpec((PlayerModelPart)part);
          const PlayerPartPose* joint = &pose.parts[part];
          for (int corner = 0; corner < 8; corner++) {
            Vec3 p = {(spec->centerOffset.x + ((corner & 1) ? 0.5f : -0.5f) * spec->size.x * PLAYER_MODEL_OUTER_SCALE) * joint->scale.x,
                      (spec->centerOffset.y + ((corner & 2) ? 0.5f : -0.5f) * spec->size.y) * joint->scale.y,
                      (spec->centerOffset.z + ((corner & 4) ? 0.5f : -0.5f) * spec->size.z * PLAYER_MODEL_OUTER_SCALE) * joint->scale.z};
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
  advancePlayerModelAnimation(&animation, &player, false, true, 0.1);
  double movingPhase = animation.gaitPhase;
  float movingWeight = animation.gaitWeight;
  CHECK(isfinite(movingPhase) && movingPhase > 0);
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
}

static void testPlayerModelPunch(void) {
  BlockBreaking breaking = {0};
  checkModelFloat(playerModelPunch(NULL), 0);
  checkModelFloat(playerModelPunch(&breaking), 0);
  breaking.active = true;
  breaking.block = BLOCK_DIRT;
  breaking.elapsed = 0;
  checkModelFloat(playerModelPunch(&breaking), 0);
  breaking.elapsed = 0.1;
  float dirt = playerModelPunch(&breaking);
  CHECK(dirt > 0 && dirt <= 1);
  breaking.block = BLOCK_STONE;
  checkModelFloat(playerModelPunch(&breaking), dirt);
  breaking.target = (Vec3i){17, 3, -9};
  checkModelFloat(playerModelPunch(&breaking), dirt);
  breaking.elapsed += 0.35;
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
  testPlayerModelPunch();
}

#endif
