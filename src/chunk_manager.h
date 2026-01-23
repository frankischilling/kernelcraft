#pragma once
#include "world.h"
#include "renderer.h"
#include "math.h"
#include <stdbool.h>

typedef struct KC_ChunkManager {
    int radius; /* view radius in chunks (XZ plane) */

    struct KC_ChunkSlot* slots;
    int cap;   /* power of two */
    int count; /* used slots */
    int tombs; /* tombstones */

    int center_cx, center_cz;
    bool has_center;
} KC_ChunkManager;

bool kc_chunkman_init(KC_ChunkManager* m, int radius);
void kc_chunkman_shutdown(KC_ChunkManager* m, KC_Renderer* r);

void kc_chunkman_stream_around(KC_ChunkManager* m, v3 cam_pos);
void kc_chunkman_rebuild_dirty(KC_ChunkManager* m, KC_Renderer* r, int budget);
void kc_chunkman_draw(const KC_ChunkManager* m, const KC_Renderer* r, m4 vp);

uint8_t kc_chunkman_get_block_world(const KC_ChunkManager* m, int wx, int wy, int wz);


