#include "chunk_manager.h"
#include "config.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef enum SlotState { SLOT_EMPTY = 0, SLOT_USED = 1, SLOT_TOMB = 2 } SlotState;

typedef struct KC_ChunkSlot {
    SlotState st;
    KC_Chunk chunk;
    KC_MeshGPU gpu;
    bool has_gpu;
    bool dirty;
} KC_ChunkSlot;

static int is_pow2(int x) { return x > 0 && ((x & (x - 1)) == 0); }

static uint32_t hash3i(int x, int y, int z) {
    uint32_t h = 2166136261u;
    h = (h ^ (uint32_t)x) * 16777619u;
    h = (h ^ (uint32_t)y) * 16777619u;
    h = (h ^ (uint32_t)z) * 16777619u;
    return h;
}

static int chunk_origin_x(int cx) { return cx * KC_CHUNK_X; }
static int chunk_origin_y(int cy) { return cy * KC_CHUNK_Y; }
static int chunk_origin_z(int cz) { return cz * KC_CHUNK_Z; }

static int chunk_coord_from_worldf(float w, int chunk_size) {
    return (int)floorf(w / (float)chunk_size);
}

static int abs_i(int x) { return x < 0 ? -x : x; }

static KC_ChunkSlot* slot_at(KC_ChunkManager* m, int i) {
    return &((KC_ChunkSlot*)m->slots)[i];
}

static void table_alloc(KC_ChunkManager* m, int cap) {
    KC_ASSERT(is_pow2(cap));
    m->slots = (KC_ChunkSlot*)calloc((size_t)cap, sizeof(KC_ChunkSlot));
    m->cap = cap;
    m->count = 0;
    m->tombs = 0;
}

static void table_free(KC_ChunkManager* m) {
    free(m->slots);
    m->slots = NULL;
    m->cap = m->count = m->tombs = 0;
}

static int table_find_index(const KC_ChunkManager* m, int cx, int cy, int cz) {
    if (!m->slots || m->cap == 0) return -1;

    uint32_t h = hash3i(cx, cy, cz);
    int mask = m->cap - 1;
    int i = (int)(h & (uint32_t)mask);

    for (int probe = 0; probe < m->cap; probe++) {
        const KC_ChunkSlot* s = &((const KC_ChunkSlot*)m->slots)[i];
        if (s->st == SLOT_EMPTY) return -1;
        if (s->st == SLOT_USED) {
            if (s->chunk.cx == cx && s->chunk.cy == cy && s->chunk.cz == cz) return i;
        }
        i = (i + 1) & mask;
    }
    return -1;
}

static int table_find_or_insert_index(KC_ChunkManager* m, int cx, int cy, int cz, bool* out_inserted) {
    *out_inserted = false;

    uint32_t h = hash3i(cx, cy, cz);
    int mask = m->cap - 1;
    int i = (int)(h & (uint32_t)mask);

    int first_tomb = -1;

    for (int probe = 0; probe < m->cap; probe++) {
        KC_ChunkSlot* s = slot_at(m, i);

        if (s->st == SLOT_EMPTY) {
            int use = (first_tomb >= 0) ? first_tomb : i;
            KC_ChunkSlot* d = slot_at(m, use);
            d->st = SLOT_USED;
            d->chunk.cx = cx;
            d->chunk.cy = cy;
            d->chunk.cz = cz;
            d->dirty = true;
            d->has_gpu = false;
            *out_inserted = true;
            m->count++;
            if (first_tomb >= 0) m->tombs--;
            return use;
        }

        if (s->st == SLOT_TOMB) {
            if (first_tomb < 0) first_tomb = i;
        } else if (s->st == SLOT_USED) {
            if (s->chunk.cx == cx && s->chunk.cy == cy && s->chunk.cz == cz) {
                return i;
            }
        }

        i = (i + 1) & mask;
    }

    return -1;
}

static void rehash(KC_ChunkManager* m, int new_cap) {
    KC_ChunkSlot* old = (KC_ChunkSlot*)m->slots;
    int old_cap = m->cap;

    table_alloc(m, new_cap);

    for (int i = 0; i < old_cap; i++) {
        KC_ChunkSlot* s = &old[i];
        if (s->st != SLOT_USED) continue;

        bool ins = false;
        int idx = table_find_or_insert_index(m, s->chunk.cx, s->chunk.cy, s->chunk.cz, &ins);
        KC_ASSERT(idx >= 0);
        KC_ChunkSlot* d = slot_at(m, idx);

        d->chunk = s->chunk;
        d->gpu = s->gpu;
        d->has_gpu = s->has_gpu;
        d->dirty = s->dirty;
    }

    free(old);
}

static void maybe_grow(KC_ChunkManager* m) {
    int usedish = m->count + m->tombs;
    if (usedish * 10 < m->cap * 7) return;
    rehash(m, m->cap * 2);
}

static void maybe_rehash_if_tombs(KC_ChunkManager* m) {
    if (m->tombs * 10 < m->cap * 2) return;
    rehash(m, m->cap);
}

bool kc_chunkman_init(KC_ChunkManager* m, int radius) {
    memset(m, 0, sizeof(*m));
    m->radius = radius;
    table_alloc(m, 256);
    m->has_center = false;
    return true;
}

void kc_chunkman_shutdown(KC_ChunkManager* m, KC_Renderer* r) {
    if (!m || !m->slots) return;

    KC_ChunkSlot* slots = (KC_ChunkSlot*)m->slots;
    for (int i = 0; i < m->cap; i++) {
        KC_ChunkSlot* s = &slots[i];
        if (s->st != SLOT_USED) continue;
        if (s->has_gpu) kc_meshgpu_destroy(&s->gpu);
        (void)r;
    }

    table_free(m);
}

static KC_ChunkSlot* get_or_create(KC_ChunkManager* m, int cx, int cy, int cz) {
    maybe_grow(m);

    bool inserted = false;
    int idx = table_find_or_insert_index(m, cx, cy, cz, &inserted);
    KC_ASSERT(idx >= 0);

    KC_ChunkSlot* s = slot_at(m, idx);

    if (inserted) {
        kc_chunk_generate_flat(&s->chunk);
        s->dirty = true;
    }

    return s;
}

uint8_t kc_chunkman_get_block_world(const KC_ChunkManager* m, int wx, int wy, int wz) {
    int cx = (int)floorf((float)wx / (float)KC_CHUNK_X);
    int cy = (int)floorf((float)wy / (float)KC_CHUNK_Y);
    int cz = (int)floorf((float)wz / (float)KC_CHUNK_Z);

    int idx = table_find_index(m, cx, cy, cz);
    if (idx < 0) return KC_BLOCK_AIR;

    const KC_ChunkSlot* s = &((const KC_ChunkSlot*)m->slots)[idx];

    int ox = chunk_origin_x(cx);
    int oy = chunk_origin_y(cy);
    int oz = chunk_origin_z(cz);

    int lx = wx - ox;
    int ly = wy - oy;
    int lz = wz - oz;

    return kc_chunk_get(&s->chunk, lx, ly, lz);
}

void kc_chunkman_stream_around(KC_ChunkManager* m, v3 cam_pos) {
    int ccx = chunk_coord_from_worldf(cam_pos.x, KC_CHUNK_X);
    int ccz = chunk_coord_from_worldf(cam_pos.z, KC_CHUNK_Z);

    m->center_cx = ccx;
    m->center_cz = ccz;
    m->has_center = true;

    for (int dz = -m->radius; dz <= m->radius; dz++) {
        for (int dx = -m->radius; dx <= m->radius; dx++) {
            int cx = ccx + dx;
            int cz = ccz + dz;
            (void)get_or_create(m, cx, 0, cz);
        }
    }

    KC_ChunkSlot* slots = (KC_ChunkSlot*)m->slots;
    for (int i = 0; i < m->cap; i++) {
        KC_ChunkSlot* s = &slots[i];
        if (s->st != SLOT_USED) continue;

        int dx = abs_i(s->chunk.cx - ccx);
        int dz = abs_i(s->chunk.cz - ccz);

        if (dx > m->radius || dz > m->radius) {
            if (s->has_gpu) kc_meshgpu_destroy(&s->gpu);
            memset(&s->chunk, 0, sizeof(s->chunk));
            s->has_gpu = false;
            s->dirty = false;
            s->st = SLOT_TOMB;
            m->count--;
            m->tombs++;
        }
    }

    maybe_rehash_if_tombs(m);
}

static uint8_t query_block_cb(void* user, int wx, int wy, int wz) {
    const KC_ChunkManager* m = (const KC_ChunkManager*)user;
    return kc_chunkman_get_block_world(m, wx, wy, wz);
}

void kc_chunkman_rebuild_dirty(KC_ChunkManager* m, KC_Renderer* r, int budget) {
    if (budget <= 0) return;

    KC_MeshData tmp;
    kc_meshdata_init(&tmp);

    KC_ChunkSlot* slots = (KC_ChunkSlot*)m->slots;

    for (int i = 0; i < m->cap && budget > 0; i++) {
        KC_ChunkSlot* s = &slots[i];
        if (s->st != SLOT_USED) continue;
        if (!s->dirty) continue;

        kc_meshdata_clear(&tmp);

        if (!kc_chunk_build_mesh_ex(&s->chunk, &tmp, query_block_cb, m)) {
            KC_ERR("chunk mesh build failed (%d,%d,%d)", s->chunk.cx, s->chunk.cy, s->chunk.cz);
            s->dirty = false;
            continue;
        }

        if (!s->has_gpu) {
            memset(&s->gpu, 0, sizeof(s->gpu));
            s->has_gpu = true;
        }

        kc_renderer_upload_mesh(r, &s->gpu, &tmp);

        s->dirty = false;
        budget--;
    }

    kc_meshdata_free(&tmp);
}

void kc_chunkman_draw(const KC_ChunkManager* m, const KC_Renderer* r, m4 vp) {
    const KC_ChunkSlot* slots = (const KC_ChunkSlot*)m->slots;
    for (int i = 0; i < m->cap; i++) {
        const KC_ChunkSlot* s = &slots[i];
        if (s->st != SLOT_USED) continue;
        if (!s->has_gpu || s->gpu.idx_count == 0) continue;

        v3 t = (v3){
            (float)chunk_origin_x(s->chunk.cx),
            (float)chunk_origin_y(s->chunk.cy),
            (float)chunk_origin_z(s->chunk.cz)
        };
        m4 model = m4_translate(t);
        m4 mvp = m4_mul(vp, model);

        kc_renderer_draw_mesh(r, &s->gpu, mvp);
    }
}


