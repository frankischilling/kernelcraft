#include "item_model.h"
#include "mesh.h"

#include <stddef.h>

typedef struct {
  Vec3 center;
  Vec3 size;
} ItemPrimitive;

_Static_assert(MATERIAL_STONE == 0 && MATERIAL_DIRT == 1 && MATERIAL_GRASS_TOP == 2 && MATERIAL_GRASS_SIDE == 3, "Item block layers must match terrain base layers");
_Static_assert(MATERIAL_COBBLESTONE == 7 && MATERIAL_OAK_PLANKS == 8 && MATERIAL_STONE_BRICKS == 9, "Item building layers must match terrain array order");
_Static_assert(MATERIAL_OAK_LOG_SIDE == 10 && MATERIAL_OAK_LOG_TOP == 11 && MATERIAL_OAK_LEAVES == 12, "Oak item layers must match terrain array order");
_Static_assert(ITEM_MODEL_VERTEX_CAPACITY >= 16 * 36, "Item model capacity must hold sixteen cuboids");

static void includePoint(ItemModel* model, Vec3 point) {
  if (model->count == 0) {
    model->min = model->max = point;
    return;
  }
  if (point.x < model->min.x)
    model->min.x = point.x;
  if (point.y < model->min.y)
    model->min.y = point.y;
  if (point.z < model->min.z)
    model->min.z = point.z;
  if (point.x > model->max.x)
    model->max.x = point.x;
  if (point.y > model->max.y)
    model->max.y = point.y;
  if (point.z > model->max.z)
    model->max.z = point.z;
}

static bool appendFace(ItemModel* model, int face, Vec3 center, Vec3 size, float layer) {
  if (!model || face < RIGHT || face > REAR || model->count > ITEM_MODEL_VERTEX_CAPACITY - 6)
    return false;
  const float* source = getCubeFaceVertices(face);
  if (!source)
    return false;

  // cube.c supplies four useful corners as source vertices 0,1,2,4. The world
  // mesher reverses RIGHT/TOP/REAR to make those faces wind outward.
  static const int corners[4] = {0, 1, 2, 4};
  static const int outward[6] = {0, 1, 2, 2, 3, 0};
  static const int reversed[6] = {0, 2, 1, 2, 0, 3};
  const int* winding = face == RIGHT || face == TOP || face == REAR ? reversed : outward;

  for (int i = 0; i < 6; i++) {
    const float* input = source + corners[winding[i]] * 8;
    ItemModelVertex vertex = {
        .position = {center.x + input[0] * size.x, center.y + input[1] * size.y, center.z + input[2] * size.z},
        .normal = {input[3], input[4], input[5]},
        .u = input[6],
        .v = input[7],
        .layer = layer,
    };
    includePoint(model, vertex.position);
    model->vertices[model->count++] = vertex;
  }
  return true;
}

static bool appendCuboid(ItemModel* model, Vec3 center, Vec3 size, float layer) {
  if (size.x <= 0 || size.y <= 0 || size.z <= 0 || model->count > ITEM_MODEL_VERTEX_CAPACITY - 36)
    return false;
  for (int face = RIGHT; face <= REAR; face++)
    if (!appendFace(model, face, center, size, layer))
      return false;
  return true;
}

static float blockLayer(uint16_t item, int face) {
  switch (item) {
  case ITEM_GRASS_BLOCK:
    return face == TOP ? MATERIAL_GRASS_TOP : face == BOTTOM ? MATERIAL_DIRT : MATERIAL_GRASS_SIDE;
  case ITEM_DIRT:
    return MATERIAL_DIRT;
  case ITEM_STONE:
    return MATERIAL_STONE;
  case ITEM_COBBLESTONE:
    return MATERIAL_COBBLESTONE;
  case ITEM_OAK_PLANKS:
    return MATERIAL_OAK_PLANKS;
  case ITEM_STONE_BRICKS:
    return MATERIAL_STONE_BRICKS;
  case ITEM_OAK_LOG:
    return face == TOP || face == BOTTOM ? MATERIAL_OAK_LOG_TOP : MATERIAL_OAK_LOG_SIDE;
  case ITEM_OAK_LEAVES:
    return MATERIAL_OAK_LEAVES;
  default:
    return ITEM_MODEL_SOLID_LAYER;
  }
}

static bool buildBlock(uint16_t item, ItemModel* model) {
  const Vec3 center = VEC3_ZERO;
  const Vec3 size = {1, 1, 1};
  for (int face = RIGHT; face <= REAR; face++)
    if (!appendFace(model, face, center, size, blockLayer(item, face)))
      return false;
  return true;
}

static bool buildPrimitives(ItemModel* model, const ItemPrimitive* primitives, size_t count) {
  if (!primitives || count > ITEM_MODEL_VERTEX_CAPACITY / 36)
    return false;
  for (size_t i = 0; i < count; i++)
    if (!appendCuboid(model, primitives[i].center, primitives[i].size, ITEM_MODEL_SOLID_LAYER))
      return false;
  return true;
}

static bool buildHelmet(ItemModel* model) {
  // Open front and bottom with a real shell thickness around the cavity.
  static const ItemPrimitive pieces[] = {
      {{0, 0.425f, 0}, {0.90f, 0.15f, 0.80f}},
      {{-0.40f, -0.075f, 0}, {0.10f, 0.85f, 0.80f}},
      {{0.40f, -0.075f, 0}, {0.10f, 0.85f, 0.80f}},
      {{0, -0.075f, -0.35f}, {0.70f, 0.85f, 0.10f}},
  };
  return buildPrimitives(model, pieces, sizeof(pieces) / sizeof(pieces[0]));
}

static bool buildChestplate(ItemModel* model) {
  // Separate front panels leave a neck opening; front/back/side thickness forms
  // a hollow torso instead of one solid rectangular brick.
  static const ItemPrimitive pieces[] = {
      {{-0.18f, -0.15f, 0.225f}, {0.30f, 0.60f, 0.10f}}, {{0.18f, -0.15f, 0.225f}, {0.30f, 0.60f, 0.10f}}, {{0, -0.075f, -0.225f}, {0.66f, 0.85f, 0.10f}},
      {{-0.26f, 0.425f, 0}, {0.14f, 0.15f, 0.45f}},      {{0.26f, 0.425f, 0}, {0.14f, 0.15f, 0.45f}},      {{-0.375f, -0.075f, 0}, {0.09f, 0.85f, 0.35f}},
      {{0.375f, -0.075f, 0}, {0.09f, 0.85f, 0.35f}},
  };
  return buildPrimitives(model, pieces, sizeof(pieces) / sizeof(pieces[0]));
}

static bool buildLeggings(ItemModel* model) {
  // Four waist rails surround a hollow waist. Each leg is a three-sided shell,
  // leaving both the inner seam and the bottom open.
  static const ItemPrimitive pieces[] = {
      {{0, 0.40f, 0.225f}, {0.84f, 0.16f, 0.10f}},  {{0, 0.40f, -0.225f}, {0.84f, 0.16f, 0.10f}},     {{-0.37f, 0.40f, 0}, {0.10f, 0.16f, 0.35f}},
      {{0.37f, 0.40f, 0}, {0.10f, 0.16f, 0.35f}},   {{-0.22f, -0.08f, 0.19f}, {0.26f, 0.80f, 0.08f}}, {{-0.22f, -0.08f, -0.19f}, {0.26f, 0.80f, 0.08f}},
      {{-0.35f, -0.08f, 0}, {0.08f, 0.80f, 0.30f}}, {{0.22f, -0.08f, 0.19f}, {0.26f, 0.80f, 0.08f}},  {{0.22f, -0.08f, -0.19f}, {0.26f, 0.80f, 0.08f}},
      {{0.35f, -0.08f, 0}, {0.08f, 0.80f, 0.30f}},
  };
  return buildPrimitives(model, pieces, sizeof(pieces) / sizeof(pieces[0]));
}

static bool buildBoots(ItemModel* model) {
  // Two low C-shaped boot shells. Soles and toes provide depth while the tops
  // remain open and the pair stays visibly separated at the center line.
  static const ItemPrimitive pieces[] = {
      {{-0.24f, -0.24f, 0}, {0.32f, 0.12f, 0.72f}},     {{-0.24f, -0.06f, 0.30f}, {0.32f, 0.36f, 0.12f}}, {{-0.24f, 0.06f, -0.32f}, {0.32f, 0.48f, 0.08f}},
      {{-0.40f, 0.06f, -0.05f}, {0.08f, 0.48f, 0.50f}}, {{0.24f, -0.24f, 0}, {0.32f, 0.12f, 0.72f}},      {{0.24f, -0.06f, 0.30f}, {0.32f, 0.36f, 0.12f}},
      {{0.24f, 0.06f, -0.32f}, {0.32f, 0.48f, 0.08f}},  {{0.40f, 0.06f, -0.05f}, {0.08f, 0.48f, 0.50f}},
  };
  return buildPrimitives(model, pieces, sizeof(pieces) / sizeof(pieces[0]));
}

bool itemModelBuild(uint16_t item, ItemModel* output) {
  if (!output || item == ITEM_NONE || item > ITEM_ID_LAST)
    return false;

  ItemModel model = {0};
  bool built = false;
  if (item >= ITEM_GRASS_BLOCK && item <= ITEM_STONE_BRICKS)
    built = buildBlock(item, &model);
  else
    switch (item) {
    case ITEM_OAK_LOG:
    case ITEM_OAK_LEAVES:
      built = buildBlock(item, &model);
      break;
    case ITEM_LEATHER_HELMET:
      built = buildHelmet(&model);
      break;
    case ITEM_LEATHER_CHESTPLATE:
      built = buildChestplate(&model);
      break;
    case ITEM_LEATHER_LEGGINGS:
      built = buildLeggings(&model);
      break;
    case ITEM_LEATHER_BOOTS:
      built = buildBoots(&model);
      break;
    default:
      break;
    }

  if (!built || model.count == 0 || model.count > ITEM_MODEL_VERTEX_CAPACITY || model.count % 3)
    return false;
  *output = model;
  return true;
}
