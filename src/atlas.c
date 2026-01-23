#include "atlas.h"

/*
Atlas tile order (based on your 64x16 image):
  0 stone
  1 dirt
  2 grass-top
  3 grass-side
*/

KC_BlockDef g_block_defs[256] = {0};

static void init_defs(void) __attribute__((constructor));
static void init_defs(void) {
    /* air */
    g_block_defs[KC_BLOCK_AIR] = (KC_BlockDef){ .solid = 0 };

    /* dirt: all faces dirt (tile 1) */
    KC_BlockDef dirt = { .solid = 1 };
    for (int f=0; f<FACE_COUNT; f++) dirt.tile[f] = 1;
    g_block_defs[KC_BLOCK_DIRT] = dirt;

    /* stone: all faces stone (tile 0) */
    KC_BlockDef stone = { .solid = 1 };
    for (int f=0; f<FACE_COUNT; f++) stone.tile[f] = 0;
    g_block_defs[KC_BLOCK_STONE] = stone;

    /* grass: top=grass-top (2), bottom=dirt (1), sides=grass-side (3) */
    KC_BlockDef grass = { .solid = 1 };
    for (int f=0; f<FACE_COUNT; f++) grass.tile[f] = 3; /* sides default */
    grass.tile[FACE_PY] = 2;
    grass.tile[FACE_NY] = 1;
    g_block_defs[KC_BLOCK_GRASS] = grass;
}

/* corners: 0=(u0,v0), 1=(u1,v0), 2=(u1,v1), 3=(u0,v1) */
void kc_atlas_uv_for_tile(uint8_t tile_id, int corner, v2* out_uv) {
    const float tile_w = 1.0f / (float)KC_ATLAS_TILES_X;
    const float tile_h = 1.0f / (float)KC_ATLAS_TILES_Y;

    const int tx = (int)tile_id % KC_ATLAS_TILES_X;
    const int ty = (int)tile_id / KC_ATLAS_TILES_X;

    const float u0 = (float)tx * tile_w;
    const float v0 = (float)ty * tile_h;
    const float u1 = u0 + tile_w;
    const float v1 = v0 + tile_h;

    switch (corner & 3) {
        case 0: *out_uv = (v2){u0, v0}; break;
        case 1: *out_uv = (v2){u1, v0}; break;
        case 2: *out_uv = (v2){u1, v1}; break;
        default:*out_uv = (v2){u0, v1}; break;
    }
}
