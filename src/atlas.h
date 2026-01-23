#pragma once
#include "math.h"
#include <stdint.h>

/* Block IDs */
typedef enum KC_BlockId {
    KC_BLOCK_AIR   = 0,
    KC_BLOCK_DIRT  = 1,
    KC_BLOCK_GRASS = 2,
    KC_BLOCK_STONE = 3,
} KC_BlockId;

typedef enum KC_Face {
    FACE_PX = 0,
    FACE_NX = 1,
    FACE_PY = 2,
    FACE_NY = 3,
    FACE_PZ = 4,
    FACE_NZ = 5,
    FACE_COUNT = 6
} KC_Face;

typedef struct KC_BlockDef {
    uint8_t solid;
    /* which atlas tile (0..N-1) each face uses */
    uint8_t tile[FACE_COUNT];
} KC_BlockDef;

/* Atlas layout: 4 tiles wide x 1 tall (matches your 64x16 atlas). */
#define KC_ATLAS_TILES_X 4
#define KC_ATLAS_TILES_Y 1

extern KC_BlockDef g_block_defs[256];

/* UV helper for a given tile id and vertex corner (0..3) */
void kc_atlas_uv_for_tile(uint8_t tile_id, int corner, v2* out_uv);
