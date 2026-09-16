#include "world/item_model.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                                                                                                           \
  do {                                                                                                                                                                             \
    if (!(condition)) {                                                                                                                                                            \
      fprintf(stderr, "Item model test: %s (line %d)\n", #condition, __LINE__);                                                                                                    \
      exit(EXIT_FAILURE);                                                                                                                                                          \
    }                                                                                                                                                                              \
  } while (0)

static const float EPSILON = 0.00001f;

static bool closeFloat(float a, float b) {
  return fabsf(a - b) < EPSILON;
}

static Vec3 subtract(Vec3 a, Vec3 b) {
  return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static Vec3 cross(Vec3 a, Vec3 b) {
  return (Vec3){a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

static float dot(Vec3 a, Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static int faceFromNormal(Vec3 normal) {
  if (normal.x == 1 && normal.y == 0 && normal.z == 0)
    return RIGHT;
  if (normal.x == -1 && normal.y == 0 && normal.z == 0)
    return LEFT;
  if (normal.x == 0 && normal.y == 1 && normal.z == 0)
    return TOP;
  if (normal.x == 0 && normal.y == -1 && normal.z == 0)
    return BOTTOM;
  if (normal.x == 0 && normal.y == 0 && normal.z == 1)
    return FRONT;
  if (normal.x == 0 && normal.y == 0 && normal.z == -1)
    return REAR;
  return -1;
}

static void expectedUV(const ItemModelVertex* vertex, float* u, float* v) {
  int face = faceFromNormal(vertex->normal);
  CHECK(face >= 0);
  if (face == RIGHT || face == LEFT) {
    *u = vertex->position.z + 0.5f;
    *v = 0.5f - vertex->position.y;
  } else if (face == TOP) {
    *u = vertex->position.x + 0.5f;
    *v = vertex->position.z + 0.5f;
  } else if (face == BOTTOM) {
    *u = vertex->position.x + 0.5f;
    *v = 0.5f - vertex->position.z;
  } else {
    *u = vertex->position.x + 0.5f;
    *v = 0.5f - vertex->position.y;
  }
}

static float expectedBlockLayer(uint16_t item, int face) {
  if (item == ITEM_GRASS_BLOCK)
    return face == TOP ? 2 : face == BOTTOM ? 1 : 3;
  if (item == ITEM_DIRT)
    return 1;
  if (item == ITEM_STONE)
    return 0;
  if (item == ITEM_COBBLESTONE)
    return 7;
  if (item == ITEM_OAK_PLANKS)
    return 8;
  if (item == ITEM_STONE_BRICKS)
    return 9;
  if (item == ITEM_OAK_LOG)
    return face == TOP || face == BOTTOM ? 11 : 10;
  CHECK(item == ITEM_OAK_LEAVES);
  return 12;
}

static float signedVolume(const ItemModel* model) {
  float volume = 0;
  for (size_t i = 0; i < model->count; i += 3) {
    Vec3 a = model->vertices[i].position;
    Vec3 b = model->vertices[i + 1].position;
    Vec3 c = model->vertices[i + 2].position;
    volume += dot(a, cross(b, c)) / 6.0f;
  }
  return volume;
}

static void checkGeometry(const ItemModel* model) {
  CHECK(model->count > 0 && model->count <= ITEM_MODEL_VERTEX_CAPACITY && model->count % 3 == 0);
  Vec3 low = {INFINITY, INFINITY, INFINITY}, high = {-INFINITY, -INFINITY, -INFINITY};
  for (size_t i = 0; i < model->count; i++) {
    const ItemModelVertex* vertex = &model->vertices[i];
    CHECK(isfinite(vertex->position.x) && isfinite(vertex->position.y) && isfinite(vertex->position.z));
    CHECK(isfinite(vertex->normal.x) && isfinite(vertex->normal.y) && isfinite(vertex->normal.z));
    CHECK(isfinite(vertex->u) && isfinite(vertex->v) && isfinite(vertex->layer));
    CHECK(faceFromNormal(vertex->normal) >= 0);
    if (vertex->position.x < low.x)
      low.x = vertex->position.x;
    if (vertex->position.y < low.y)
      low.y = vertex->position.y;
    if (vertex->position.z < low.z)
      low.z = vertex->position.z;
    if (vertex->position.x > high.x)
      high.x = vertex->position.x;
    if (vertex->position.y > high.y)
      high.y = vertex->position.y;
    if (vertex->position.z > high.z)
      high.z = vertex->position.z;
  }
  CHECK(closeFloat(low.x, model->min.x) && closeFloat(low.y, model->min.y) && closeFloat(low.z, model->min.z));
  CHECK(closeFloat(high.x, model->max.x) && closeFloat(high.y, model->max.y) && closeFloat(high.z, model->max.z));

  for (size_t i = 0; i < model->count; i += 3) {
    const ItemModelVertex *a = &model->vertices[i], *b = &model->vertices[i + 1], *c = &model->vertices[i + 2];
    Vec3 ab = subtract(b->position, a->position), ac = subtract(c->position, a->position);
    Vec3 geometric = cross(ab, ac);
    CHECK(dot(geometric, a->normal) > EPSILON);
    CHECK(a->normal.x == b->normal.x && a->normal.y == b->normal.y && a->normal.z == b->normal.z);
    CHECK(a->normal.x == c->normal.x && a->normal.y == c->normal.y && a->normal.z == c->normal.z);
  }
}

static void testBlocks(void) {
  static const uint16_t blocks[] = {ITEM_GRASS_BLOCK, ITEM_DIRT, ITEM_STONE, ITEM_COBBLESTONE, ITEM_OAK_PLANKS, ITEM_STONE_BRICKS, ITEM_OAK_LOG, ITEM_OAK_LEAVES};
  for (size_t blockIndex = 0; blockIndex < sizeof(blocks) / sizeof(blocks[0]); blockIndex++) {
    uint16_t item = blocks[blockIndex];
    ItemModel model;
    CHECK(itemModelBuild(item, &model));
    CHECK(model.count == 36);
    CHECK(closeFloat(model.min.x, -0.5f) && closeFloat(model.min.y, -0.5f) && closeFloat(model.min.z, -0.5f));
    CHECK(closeFloat(model.max.x, 0.5f) && closeFloat(model.max.y, 0.5f) && closeFloat(model.max.z, 0.5f));
    checkGeometry(&model);

    int faceVertices[6] = {0};
    for (size_t i = 0; i < model.count; i++) {
      const ItemModelVertex* vertex = &model.vertices[i];
      int face = faceFromNormal(vertex->normal);
      CHECK(face >= 0);
      if (face >= 0) {
        faceVertices[face]++;
        CHECK(closeFloat(vertex->layer, expectedBlockLayer(item, face)));
      }
      float expectedU = 0, expectedV = 0;
      expectedUV(vertex, &expectedU, &expectedV);
      CHECK(closeFloat(vertex->u, expectedU) && closeFloat(vertex->v, expectedV));
    }
    for (int face = 0; face < 6; face++)
      CHECK(faceVertices[face] == 6);

    if (item == ITEM_OAK_LOG) {
      int bark = 0, endGrain = 0;
      for (size_t i = 0; i < model.count; i++) {
        bark += closeFloat(model.vertices[i].layer, 10);
        endGrain += closeFloat(model.vertices[i].layer, 11);
      }
      CHECK(bark == 24 && endGrain == 12);
    } else if (item == ITEM_OAK_LEAVES) {
      for (size_t i = 0; i < model.count; i++)
        CHECK(closeFloat(model.vertices[i].layer, 12));
    }

    // A centered unit cube with outward triangles encloses one cubic unit. This
    // rejects flat sprites even if their bounds or per-face counts look plausible.
    CHECK(closeFloat(fabsf(signedVolume(&model)), 1.0f));
  }
}

static Vec3 triangleCentroid(const ItemModel* model, size_t triangle) {
  const ItemModelVertex* v = model->vertices + triangle * 3;
  return (Vec3){(v[0].position.x + v[1].position.x + v[2].position.x) / 3.0f, (v[0].position.y + v[1].position.y + v[2].position.y) / 3.0f,
                (v[0].position.z + v[1].position.z + v[2].position.z) / 3.0f};
}

static int countOpeningTriangles(const ItemModel* model, uint16_t item) {
  int count = 0;
  for (size_t triangle = 0; triangle < model->count / 3; triangle++) {
    const ItemModelVertex* vertex = model->vertices + triangle * 3;
    Vec3 center = triangleCentroid(model, triangle);
    if (item == ITEM_LEATHER_HELMET) {
      if (vertex->normal.y < -0.5f && closeFloat(center.y, -0.5f) && fabsf(center.x) < 0.25f && center.z > -0.25f)
        count++;
    } else if (item == ITEM_LEATHER_CHESTPLATE) {
      if (vertex->normal.z > 0.5f && center.z > 0.26f && center.y > 0.20f && fabsf(center.x) < 0.12f)
        count++;
    } else if (item == ITEM_LEATHER_LEGGINGS) {
      if (center.y < 0.25f && fabsf(center.x) < 0.07f)
        count++;
    } else if (item == ITEM_LEATHER_BOOTS) {
      float bootCenter = center.x < 0 ? -0.24f : 0.24f;
      if (vertex->normal.y > 0.5f && closeFloat(center.y, 0.30f) && fabsf(center.x - bootCenter) < 0.10f && fabsf(center.z) < 0.15f)
        count++;
    }
  }
  return count;
}

static void checkBounds(const ItemModel* model, const float expected[6]) {
  CHECK(closeFloat(model->min.x, expected[0]) && closeFloat(model->max.x, expected[1]));
  CHECK(closeFloat(model->min.y, expected[2]) && closeFloat(model->max.y, expected[3]));
  CHECK(closeFloat(model->min.z, expected[4]) && closeFloat(model->max.z, expected[5]));
  CHECK(closeFloat(model->min.x + model->max.x, 0) && closeFloat(model->min.y + model->max.y, 0) && closeFloat(model->min.z + model->max.z, 0));
  CHECK(model->min.x >= -1 && model->max.x <= 1 && model->min.y >= -1 && model->max.y <= 1 && model->min.z >= -1 && model->max.z <= 1);
}

static void testEquipment(void) {
  static const struct {
    uint16_t item;
    size_t vertices;
    float bounds[6];
  } cases[] = {
      {ITEM_LEATHER_HELMET, 144, {-0.45f, 0.45f, -0.50f, 0.50f, -0.40f, 0.40f}},
      {ITEM_LEATHER_CHESTPLATE, 252, {-0.42f, 0.42f, -0.50f, 0.50f, -0.275f, 0.275f}},
      {ITEM_LEATHER_LEGGINGS, 360, {-0.42f, 0.42f, -0.48f, 0.48f, -0.275f, 0.275f}},
      {ITEM_LEATHER_BOOTS, 288, {-0.44f, 0.44f, -0.30f, 0.30f, -0.36f, 0.36f}},
  };

  bool seenCounts[ITEM_MODEL_VERTEX_CAPACITY + 1] = {false};
  for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
    ItemModel model;
    CHECK(itemModelBuild(cases[c].item, &model));
    CHECK(model.count == cases[c].vertices && !seenCounts[model.count]);
    seenCounts[model.count] = true;
    checkBounds(&model, cases[c].bounds);
    checkGeometry(&model);
    for (size_t i = 0; i < model.count; i++) {
      CHECK(model.vertices[i].layer == ITEM_MODEL_SOLID_LAYER);
      CHECK(model.vertices[i].u >= 0 && model.vertices[i].u <= 1 && model.vertices[i].v >= 0 && model.vertices[i].v <= 1);
    }
    CHECK(fabsf(signedVolume(&model)) > 0.01f);
    // Each model deliberately leaves its wearable opening uncovered: helmet
    // bottom/front, tunic neck, pants inner seam, and both boot tops.
    CHECK(countOpeningTriangles(&model, cases[c].item) == 0);
  }
}

static void testInvalidIsAtomic(void) {
  ItemModel output;
  memset(&output, 0xA5, sizeof(output));
  ItemModel before = output;
  CHECK(!itemModelBuild(ITEM_NONE, &output));
  CHECK(memcmp(&output, &before, sizeof(output)) == 0);
  CHECK(!itemModelBuild((uint16_t)(ITEM_ID_LAST + 1), &output));
  CHECK(memcmp(&output, &before, sizeof(output)) == 0);
  CHECK(!itemModelBuild(UINT16_MAX, &output));
  CHECK(memcmp(&output, &before, sizeof(output)) == 0);
  CHECK(!itemModelBuild(ITEM_STONE, NULL));
}

int main(void) {
  testBlocks();
  testEquipment();
  testInvalidIsAtomic();
  puts("Bounded 3D item model geometry tests passed");
  return 0;
}
