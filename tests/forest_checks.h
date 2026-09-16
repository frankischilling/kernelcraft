#ifndef FOREST_CHECKS_H
#define FOREST_CHECKS_H

static bool forestColumnHasLog(const uint8_t* trunkColumns, int x, int z) {
  return trunkColumns[(size_t)(x + WORLD_SIZE / 2) * WORLD_SIZE + (size_t)(z + WORLD_SIZE / 2)] != 0;
}

static bool forestNearTrunk(const uint8_t* trunkColumns, int x, int z, int radius) {
  for (int dx = -radius; dx <= radius; dx++)
    for (int dz = -radius; dz <= radius; dz++) {
      if (dx * dx + dz * dz > radius * radius)
        continue;
      int tx = x + dx, tz = z + dz;
      if (tx < -WORLD_SIZE / 2 || tx >= WORLD_SIZE / 2 || tz < -WORLD_SIZE / 2 || tz >= WORLD_SIZE / 2)
        continue;
      if (forestColumnHasLog(trunkColumns, tx, tz))
        return true;
    }
  return false;
}

static bool forestTreeCrossesXSeam(int x, int z, int crownY, int seam) {
  if (x - 2 >= seam || x + 2 < seam)
    return false;
  bool negativeSide = false, positiveSide = false;
  for (int leafX = x - 2; leafX <= x + 2; leafX++) {
    const Block* leaf = getBlock(&(Vec3i){leafX, crownY - 1, z + 1});
    if (leaf->id != BLOCK_OAK_LEAVES)
      continue;
    negativeSide |= leafX < seam;
    positiveSide |= leafX >= seam;
  }
  return negativeSide && positiveSide;
}

static bool forestTreeCrossesZSeam(int x, int z, int crownY, int seam) {
  if (z - 2 >= seam || z + 2 < seam)
    return false;
  bool negativeSide = false, positiveSide = false;
  for (int leafZ = z - 2; leafZ <= z + 2; leafZ++) {
    const Block* leaf = getBlock(&(Vec3i){x + 1, crownY - 1, leafZ});
    if (leaf->id != BLOCK_OAK_LEAVES)
      continue;
    negativeSide |= leafZ < seam;
    positiveSide |= leafZ >= seam;
  }
  return negativeSide && positiveSide;
}

static void checkForestWorld(void) {
  uint8_t* trunkColumns = calloc((size_t)WORLD_SIZE * WORLD_SIZE, 1);
  CHECK(trunkColumns != NULL);
  size_t logs = 0, leaves = 0, leafy = 0;
  for (int x = -WORLD_SIZE / 2; x < WORLD_SIZE / 2; x++)
    for (int z = -WORLD_SIZE / 2; z < WORLD_SIZE / 2; z++)
      for (int y = 0; y < CHUNK_HEIGHT; y++) {
        int id = getBlock(&(Vec3i){x, y, z})->id;
        CHECK(blockIDValid(id));
        if (id == BLOCK_OAK_LOG) {
          logs++;
          trunkColumns[(size_t)(x + WORLD_SIZE / 2) * WORLD_SIZE + (size_t)(z + WORLD_SIZE / 2)] = 1;
        } else if (id == BLOCK_OAK_LEAVES) {
          leaves++;
        } else if (id == BLOCK_LEAFY_GRASS) {
          leafy++;
        }
      }

  CHECK(logs > 100 && leaves > 1000 && leafy > 100);
  size_t treeBases = 0, fullCanopies = 0;
  bool negativeXCrossing = false, positiveXCrossing = false;
  bool negativeZCrossing = false, positiveZCrossing = false;
  for (int x = -WORLD_SIZE / 2; x < WORLD_SIZE / 2; x++)
    for (int z = -WORLD_SIZE / 2; z < WORLD_SIZE / 2; z++)
      for (int y = 0; y < CHUNK_HEIGHT; y++) {
        const Block* block = getBlock(&(Vec3i){x, y, z});
        if (block->id == BLOCK_OAK_LEAVES) {
          bool supported = false;
          for (int dx = -2; dx <= 2 && !supported; dx++)
            for (int dz = -2; dz <= 2; dz++) {
              int tx = x + dx, tz = z + dz;
              if (tx < -WORLD_SIZE / 2 || tx >= WORLD_SIZE / 2 || tz < -WORLD_SIZE / 2 || tz >= WORLD_SIZE / 2)
                continue;
              if (forestColumnHasLog(trunkColumns, tx, tz)) {
                supported = true;
                break;
              }
            }
          CHECK(supported);
          continue;
        }
        if (block->id != BLOCK_OAK_LOG || (y > 0 && getBlock(&(Vec3i){x, y - 1, z})->id == BLOCK_OAK_LOG))
          continue;

        treeBases++;
        CHECK(x >= -WORLD_SIZE / 2 + 2 && x < WORLD_SIZE / 2 - 2);
        CHECK(z >= -WORLD_SIZE / 2 + 2 && z < WORLD_SIZE / 2 - 2);
        CHECK(y > 0);
        int ground = getBlock(&(Vec3i){x, y - 1, z})->id;
        CHECK(ground == BLOCK_GRASS || ground == BLOCK_LEAFY_GRASS);
        int height = 0;
        while (y + height < CHUNK_HEIGHT && getBlock(&(Vec3i){x, y + height, z})->id == BLOCK_OAK_LOG)
          height++;
        CHECK(height == 4 || height == 5);
        int crownY = y + height - 1;
        CHECK(crownY + 1 < CHUNK_HEIGHT);
        for (int dx = -1; dx <= 1; dx++)
          for (int dz = -1; dz <= 1; dz++)
            if (abs(dx) + abs(dz) <= 1 && getBlock(&(Vec3i){x + dx, crownY + 1, z + dz})->id != BLOCK_OAK_LEAVES)
              fprintf(stderr, "Oak cap obstructed: seed=%u root=(%d,%d,%d) height=%d cap=(%d,%d,%d) id=%u\n", worldSeed(), x, y, z, height, x + dx, crownY + 1, z + dz,
                      (unsigned)getBlock(&(Vec3i){x + dx, crownY + 1, z + dz})->id);
        CHECK(getBlock(&(Vec3i){x, crownY + 1, z})->id == BLOCK_OAK_LEAVES);
        CHECK(getBlock(&(Vec3i){x - 1, crownY + 1, z})->id == BLOCK_OAK_LEAVES);
        CHECK(getBlock(&(Vec3i){x + 1, crownY + 1, z})->id == BLOCK_OAK_LEAVES);
        CHECK(getBlock(&(Vec3i){x, crownY + 1, z - 1})->id == BLOCK_OAK_LEAVES);
        CHECK(getBlock(&(Vec3i){x, crownY + 1, z + 1})->id == BLOCK_OAK_LEAVES);
        size_t crownLeaves = 0;
        for (int dx = -2; dx <= 2; dx++)
          for (int dz = -2; dz <= 2; dz++)
            for (int dy = -2; dy <= 1; dy++)
              if (getBlock(&(Vec3i){x + dx, crownY + dy, z + dz})->id == BLOCK_OAK_LEAVES)
                crownLeaves++;
        if (crownLeaves >= 45)
          fullCanopies++;
        for (int seam = -WORLD_SIZE / 2 + CHUNK_SIZE; seam < WORLD_SIZE / 2; seam += CHUNK_SIZE) {
          if (seam < 0 && forestTreeCrossesXSeam(x, z, crownY, seam))
            negativeXCrossing = true;
          else if (seam > 0 && forestTreeCrossesXSeam(x, z, crownY, seam))
            positiveXCrossing = true;
          if (seam < 0 && forestTreeCrossesZSeam(x, z, crownY, seam))
            negativeZCrossing = true;
          else if (seam > 0 && forestTreeCrossesZSeam(x, z, crownY, seam))
            positiveZCrossing = true;
        }
      }
  CHECK(treeBases > 20 && fullCanopies > 0);
  CHECK(negativeXCrossing && positiveXCrossing && negativeZCrossing && positiveZCrossing);

  uint64_t nearLeafy = 0, nearGround = 0, farLeafy = 0, farGround = 0;
  for (int x = -WORLD_SIZE / 2; x < WORLD_SIZE / 2; x++)
    for (int z = -WORLD_SIZE / 2; z < WORLD_SIZE / 2; z++) {
      int surface = -1;
      int surfaceID = BLOCK_AIR;
      for (int y = 0; y < CHUNK_HEIGHT; y++) {
        int id = getBlock(&(Vec3i){x, y, z})->id;
        if (id == BLOCK_GRASS || id == BLOCK_LEAFY_GRASS) {
          surface = y;
          surfaceID = id;
        }
      }
      CHECK(surface >= 0);
      bool near = forestNearTrunk(trunkColumns, x, z, 3);
      if (near) {
        nearGround++;
        nearLeafy += surfaceID == BLOCK_LEAFY_GRASS;
      } else {
        farGround++;
        farLeafy += surfaceID == BLOCK_LEAFY_GRASS;
      }
    }
  CHECK(nearGround > 0 && farGround > 0);
  CHECK(nearLeafy * 100 >= nearGround * 30);
  CHECK(nearLeafy * farGround > farLeafy * nearGround * 3);

  size_t forestLabels = 0;
  for (int x = -WORLD_SIZE / 2; x < WORLD_SIZE / 2; x += 8)
    for (int z = -WORLD_SIZE / 2; z < WORLD_SIZE / 2; z += 8)
      if (strcmp(getCurrentBiomeText((float)x, (float)z), "Forest") == 0)
        forestLabels++;
  CHECK(forestLabels > 0);
  free(trunkColumns);
}

static void checkCurrentGroundMatchesLegacy(const Chunk* current, const Chunk* legacy) {
  for (int x = 0; x < CHUNK_SIZE; x++)
    for (int z = 0; z < CHUNK_SIZE; z++)
      for (int y = 0; y < CHUNK_HEIGHT; y++) {
        int oldID = legacy->blocks[x][y][z].id;
        if (oldID == BLOCK_GRASS)
          CHECK(current->blocks[x][y][z].id == BLOCK_GRASS || current->blocks[x][y][z].id == BLOCK_LEAFY_GRASS);
        else if (oldID != BLOCK_AIR)
          CHECK(current->blocks[x][y][z].id == oldID);
      }
}

static uint8_t* generatedWorldBlocks(uint32_t seed, uint32_t version) {
  uint8_t* blocks = malloc(WORLD_BLOCK_COUNT);
  CHECK(blocks != NULL);
  size_t offset = 0;
  for (int cx = 0; cx < CHUNKS_PER_AXIS; cx++)
    for (int cz = 0; cz < CHUNKS_PER_AXIS; cz++) {
      Chunk chunk = {.position = {cx - CHUNKS_PER_AXIS / 2, cz - CHUNKS_PER_AXIS / 2}};
      generateTerrainChunkVersioned(&chunk, seed, version);
      for (int x = 0; x < CHUNK_SIZE; x++)
        for (int y = 0; y < CHUNK_HEIGHT; y++)
          for (int z = 0; z < CHUNK_SIZE; z++)
            blocks[offset++] = chunk.blocks[x][y][z].id;
    }
  CHECK(offset == WORLD_BLOCK_COUNT);
  return blocks;
}

#endif
