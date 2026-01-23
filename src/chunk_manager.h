#pragma once
#include "world.h"
#include "renderer.h"
#include "mesh_builder.h"  /* for KC_MeshData */
#include "math.h"          /* for v3 */
#include <stdbool.h>

#ifndef KC_CHUNK_CACHE_RAD
#define KC_CHUNK_CACHE_RAD 2            /* 2 => 5x5, 4 => 9x9 */
#endif

#define KC_CHUNK_CACHE_DIM (KC_CHUNK_CACHE_RAD * 2 + 1)
#define KC_CHUNK_CACHE_MAX (KC_CHUNK_CACHE_DIM * KC_CHUNK_CACHE_DIM)

typedef struct KC_DirtyEntry {
    int cx, cy, cz;
} KC_DirtyEntry;

typedef struct KC_ChunkManager {
    int radius; /* view radius in chunks (XZ plane) */

    struct KC_ChunkSlot* slots;
    int cap;   /* power of two */
    int count; /* used slots */
    int tombs; /* tombstones */

    int center_cx, center_cz;
    bool has_center;

    /* Dirty queue (no full-table scans) */
    KC_DirtyEntry* dirty;
    int dirty_len;
    int dirty_cap;

    /* Persistent scratch mesh (no per-frame alloc/free) */
    KC_MeshData scratch;
    bool scratch_inited;

    /* Fast O(1) chunk cache for nearby blocks */
    int cache_ccx, cache_ccz;
    bool cache_valid;
    KC_Chunk* cache_chunks[KC_CHUNK_CACHE_MAX];
} KC_ChunkManager;

bool kc_chunkman_init(KC_ChunkManager* m, int radius);
void kc_chunkman_shutdown(KC_ChunkManager* m, KC_Renderer* r);

void kc_chunkman_stream_around(KC_ChunkManager* m, v3 cam_pos);
void kc_chunkman_rebuild_dirty(KC_ChunkManager* m, KC_Renderer* r, v3 cam_pos, float ms_mesh_budget, float ms_upload_budget);
void kc_chunkman_draw(const KC_ChunkManager* m, const KC_Renderer* r, m4 vp);

uint8_t kc_chunkman_get_block_world(const KC_ChunkManager* m, int wx, int wy, int wz);
uint8_t kc_chunkman_get_block_world_fast(const KC_ChunkManager* m, int wx, int wy, int wz);

/* Performance stats */
typedef struct KC_ChunkStats {
    int total_chunks;
    int loaded_chunks;
    int dirty_chunks;
    int gpu_chunks;
} KC_ChunkStats;

void kc_chunkman_get_stats(const KC_ChunkManager* m, KC_ChunkStats* stats);


