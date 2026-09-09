#include "player.h"
#include "world.h"

static bool finitePosition(Vec3 position) {
  return isfinite(position.x) && isfinite(position.y) && isfinite(position.z);
}

static void bodyBounds(Vec3 feet, bool crouched, double min[3], double max[3]) {
  min[0] = (double)feet.x - PLAYER_RADIUS;
  min[1] = feet.y;
  min[2] = (double)feet.z - PLAYER_RADIUS;
  max[0] = (double)feet.x + PLAYER_RADIUS;
  max[1] = (double)feet.y + (crouched ? PLAYER_CROUCH_HEIGHT : PLAYER_HEIGHT);
  max[2] = (double)feet.z + PLAYER_RADIUS;
}

bool playerOverlapsBlock(Vec3 feet, Vec3i cell, bool crouched) {
  double min[3], max[3];
  bodyBounds(feet, crouched, min, max);
  const int coordinates[3] = {cell.x, cell.y, cell.z};
  for (int axis = 0; axis < 3; axis++)
    if (coordinates[axis] * (double)CUBE_SIZE >= max[axis] || (coordinates[axis] + 1.0) * CUBE_SIZE <= min[axis])
      return false;
  return finitePosition(feet);
}

static bool bodyCellRange(Vec3 feet, bool crouched, Vec3i* firstCell, Vec3i* lastCell) {
  if (!firstCell || !lastCell || !finitePosition(feet))
    return false;
  double min[3], max[3];
  bodyBounds(feet, crouched, min, max);
  const double lo[3] = {-WORLD_SIZE * CUBE_SIZE / 2, 0, -WORLD_SIZE * CUBE_SIZE / 2};
  const double hi[3] = {WORLD_SIZE * CUBE_SIZE / 2, CHUNK_HEIGHT * CUBE_SIZE, WORLD_SIZE * CUBE_SIZE / 2};
  int first[3], last[3];
  for (int axis = 0; axis < 3; axis++) {
    if (min[axis] < lo[axis] || max[axis] > hi[axis])
      return false;
    first[axis] = (int)floor(min[axis] / CUBE_SIZE);
    last[axis] = (int)ceil(max[axis] / CUBE_SIZE) - 1;
  }
  *firstCell = (Vec3i){first[0], first[1], first[2]};
  *lastCell = (Vec3i){last[0], last[1], last[2]};
  return true;
}

bool playerCellRange(Vec3 feet, Vec3i* firstCell, Vec3i* lastCell) {
  return bodyCellRange(feet, false, firstCell, lastCell);
}

bool playerCanOccupy(Vec3 feet) {
  return playerCanOccupyPosture(feet, false);
}

bool playerCanOccupyPosture(Vec3 feet, bool crouched) {
  Vec3i firstCell, lastCell;
  if (!bodyCellRange(feet, crouched, &firstCell, &lastCell))
    return false;
  int first[3] = {firstCell.x, firstCell.y, firstCell.z};
  int last[3] = {lastCell.x, lastCell.y, lastCell.z};
  for (int x = first[0]; x <= last[0]; x++)
    for (int y = first[1]; y <= last[1]; y++)
      for (int z = first[2]; z <= last[2]; z++) {
        const Block* block = getBlock(&(Vec3i){x, y, z});
        if (!block || blockIsSolid(block->id))
          return false;
      }
  return true;
}

static double clipAxis(Vec3 feet, bool crouched, int axis, double displacement) {
  if (displacement == 0)
    return 0;
  double min[3], max[3];
  bodyBounds(feet, crouched, min, max);
  const double lo[3] = {-WORLD_SIZE * CUBE_SIZE / 2, 0, -WORLD_SIZE * CUBE_SIZE / 2};
  const double hi[3] = {WORLD_SIZE * CUBE_SIZE / 2, CHUNK_HEIGHT * CUBE_SIZE, WORLD_SIZE * CUBE_SIZE / 2};
  double allowed = displacement > 0 ? fmin(displacement, hi[axis] - max[axis]) : fmax(displacement, lo[axis] - min[axis]);
  int first[3], last[3];
  for (int a = 0; a < 3; a++) {
    double start = min[a] + (a == axis ? fmin(0, allowed) : 0);
    double end = max[a] + (a == axis ? fmax(0, allowed) : 0);
    first[a] = (int)floor(start / CUBE_SIZE);
    last[a] = (int)ceil(end / CUBE_SIZE) - 1;
  }
  // Scan the swept volume, not just the endpoint, so thin walls cannot be skipped.
  for (int x = first[0]; x <= last[0]; x++)
    for (int y = first[1]; y <= last[1]; y++)
      for (int z = first[2]; z <= last[2]; z++) {
        const Block* block = getBlock(&(Vec3i){x, y, z});
        if (!block || !blockIsSolid(block->id))
          continue;
        const int cell[3] = {x, y, z};
        double near = cell[axis] * (double)CUBE_SIZE, far = (cell[axis] + 1.0) * CUBE_SIZE;
        if (displacement > 0 && near >= max[axis])
          allowed = fmin(allowed, near - max[axis]);
        if (displacement < 0 && far <= min[axis])
          allowed = fmax(allowed, far - min[axis]);
      }
  return allowed;
}

static bool supported(Vec3 feet, bool crouched) {
  return clipAxis(feet, crouched, 1, -0.0001) > -0.0001;
}

bool playerSetPosition(Player* player, Vec3 feet) {
  if (!player || !playerCanOccupy(feet))
    return false;
  *player = (Player){.position = feet, .grounded = supported(feet, false)};
  return true;
}

static bool spawnColumn(Player* player, int x, int z) {
  if (x < -WORLD_SIZE / 2 || x >= WORLD_SIZE / 2 || z < -WORLD_SIZE / 2 || z >= WORLD_SIZE / 2)
    return false;
  int top = -1;
  for (int y = CHUNK_HEIGHT - 1; y >= 0; y--) {
    const Block* block = getBlock(&(Vec3i){x, y, z});
    if (!block)
      return false;
    if (blockIsSolid(block->id)) {
      top = y;
      break;
    }
  }
  return playerSetPosition(player, (Vec3){(x + 0.5f) * CUBE_SIZE, (top + 1) * CUBE_SIZE, (z + 0.5f) * CUBE_SIZE});
}

bool playerFindSpawn(Player* player, Vec3 preferred) {
  if (!player || !finitePosition(preferred) || !getBlock(&(Vec3i){0, 0, 0}))
    return false;
  int cx = (int)floor(fmax(-WORLD_SIZE / 2, fmin(WORLD_SIZE / 2 - 1, (double)preferred.x / CUBE_SIZE)));
  int cz = (int)floor(fmax(-WORLD_SIZE / 2, fmin(WORLD_SIZE / 2 - 1, (double)preferred.z / CUBE_SIZE)));
  // Visit square rings in a stable order; every column is tried at most once.
  for (int radius = 0; radius < WORLD_SIZE; radius++) {
    for (int dx = -radius; dx <= radius; dx++) {
      if (spawnColumn(player, cx + dx, cz - radius))
        return true;
      if (radius && spawnColumn(player, cx + dx, cz + radius))
        return true;
    }
    for (int dz = -radius + 1; dz < radius; dz++) {
      if (spawnColumn(player, cx - radius, cz + dz) || spawnColumn(player, cx + radius, cz + dz))
        return true;
    }
  }
  return false;
}

Vec3 playerEyePosition(const Player* player) {
  return (Vec3){player->position.x, player->position.y + (player->crouched ? PLAYER_CROUCH_EYE_HEIGHT : PLAYER_EYE_HEIGHT), player->position.z};
}

static bool moveAxis(Player* player, int axis, double displacement) {
  double allowed = clipAxis(player->position, player->crouched, axis, displacement);
  float* coordinate = axis == 0 ? &player->position.x : axis == 1 ? &player->position.y : &player->position.z;
  double exact = *coordinate + allowed;
  float rounded = (float)exact;
  bool blocked = allowed != displacement;
  *coordinate = rounded;
  // Even an unclipped endpoint just before a face can round into the obstacle.
  // Round back only at contact, avoiding a bias on ordinary free movement.
  if (((displacement > 0 && rounded > exact) || (displacement < 0 && rounded < exact)) && (blocked || !playerCanOccupyPosture(player->position, player->crouched))) {
    *coordinate = nextafterf(rounded, displacement > 0 ? -INFINITY : INFINITY);
    blocked = true;
  }
  return blocked;
}

static void playerStep(Player* player, PlayerMotion motion) {
  if (motion.crouch)
    player->crouched = true;
  else if (player->crouched && playerCanOccupy(player->position))
    player->crouched = false;
  player->running = motion.run && !player->crouched && (motion.wish.x != 0 || motion.wish.z != 0);
  float speed = player->crouched ? PLAYER_CROUCH_SPEED : player->running ? PLAYER_RUN_SPEED : PLAYER_WALK_SPEED;
  player->grounded = supported(player->position, player->crouched);
  if (motion.jump && player->grounded) {
    player->velocity.y = PLAYER_JUMP_SPEED;
    player->grounded = false;
  }
  player->velocity.x = motion.wish.x * speed;
  player->velocity.z = motion.wish.z * speed;
  player->velocity.y = fmaxf(-PLAYER_TERMINAL_SPEED, player->velocity.y - PLAYER_GRAVITY * PLAYER_STEP_SECONDS);
  if (moveAxis(player, 0, player->velocity.x * PLAYER_STEP_SECONDS))
    player->velocity.x = 0;
  if (moveAxis(player, 2, player->velocity.z * PLAYER_STEP_SECONDS))
    player->velocity.z = 0;
  if (moveAxis(player, 1, player->velocity.y * PLAYER_STEP_SECONDS)) {
    player->grounded = player->velocity.y < 0;
    player->velocity.y = 0;
  } else {
    player->grounded = false;
  }
}

int playerAdvance(Player* player, PlayerMotion motion, double frameSeconds) {
  if (!player || !isfinite(frameSeconds) || frameSeconds <= 0 || !finitePosition(motion.wish) || !finitePosition(player->velocity) || !isfinite(player->accumulator) ||
      player->accumulator < 0 || player->accumulator >= PLAYER_STEP_SECONDS || !playerCanOccupyPosture(player->position, player->crouched))
    return 0;
  double length = hypot((double)motion.wish.x, (double)motion.wish.z);
  if (length > 1) {
    motion.wish.x = (float)(motion.wish.x / length);
    motion.wish.z = (float)(motion.wish.z / length);
  }
  player->jumpPending |= motion.jump;
  player->accumulator += fmin(frameSeconds, PLAYER_MAX_STEPS * PLAYER_STEP_SECONDS);
  // The small tolerance prevents double rounding from losing a whole step.
  int steps = (int)floor((player->accumulator + 1e-12) / PLAYER_STEP_SECONDS);
  if (steps > PLAYER_MAX_STEPS)
    steps = PLAYER_MAX_STEPS;
  player->accumulator = fmax(0, player->accumulator - steps * PLAYER_STEP_SECONDS);
  for (int step = 0; step < steps; step++) {
    motion.jump = player->jumpPending;
    playerStep(player, motion);
    player->jumpPending = false;
  }
  return steps;
}

void playerResetTiming(Player* player) {
  player->accumulator = 0;
  player->jumpPending = false;
  player->running = false;
}

void playerResetRunInput(PlayerRunInput* input) {
  *input = (PlayerRunInput){0};
}

void playerForwardEvent(PlayerRunInput* input, bool pressed, double seconds) {
  if (!isfinite(seconds) || seconds < 0) {
    playerResetRunInput(input);
    return;
  }
  if (!pressed) {
    input->forwardDown = false;
    input->running = false;
    return; // Retain the first tap across its release.
  }
  if (input->forwardDown)
    return;
  input->forwardDown = true;
  double elapsed = seconds - input->firstPress;
  input->running = input->tapPending && elapsed >= 0 && elapsed <= PLAYER_RUN_TAP_SECONDS;
  input->tapPending = !input->running;
  input->firstPress = seconds;
}
