#include "chunk_manager.h"
#include "config.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifndef KC_UPLOAD_MS_BUDGET
#define KC_UPLOAD_MS_BUDGET 1.0f
#endif

typedef enum SlotState { SLOT_EMPTY = 0, SLOT_USED = 1, SLOT_TOMB = 2 } SlotState;

typedef struct KC_ChunkSlot {
    SlotState st;
    KC_Chunk chunk;
    KC_MeshGPU gpu;
    bool has_gpu;

    /* Meshing state */
    bool dirty;
    bool in_dirtyq;      /* entry exists in manager dirty queue */
    uint32_t dirty_ver;  /* incremented on each dirty mark (prevents lost updates) */
} KC_ChunkSlot;

/* Monotonic time for frame budgeting (seconds). */
static double kc_now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

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

static int floor_div_i(int a, int b) { /* b > 0 */
    int q = a / b;
    int r = a % b;
    if (r < 0) q--;  /* fix toward -inf */
    return q;
}

static int floor_mod_i(int a, int b) { /* b > 0 */
    int r = a % b;
    if (r < 0) r += b;
    return r;
}

static KC_ChunkSlot* slot_at(KC_ChunkManager* m, int i) {
    return &((KC_ChunkSlot*)m->slots)[i];
}

#define KC_DIRTY_PICK_WINDOW 64

static void dirtyq_reserve(KC_ChunkManager* m, int need) {
    if (m->dirty_cap >= need) return;
    int nc = m->dirty_cap ? m->dirty_cap : 64;
    while (nc < need) nc *= 2;
    m->dirty = (KC_DirtyEntry*)realloc(m->dirty, (size_t)nc * sizeof(KC_DirtyEntry));
    KC_ASSERT(m->dirty);
    m->dirty_cap = nc;
}

static void dirtyq_push_slot(KC_ChunkManager* m, int idx) {
    KC_ChunkSlot* s = slot_at(m, idx);
    if (s->st != SLOT_USED) return;
    if (s->in_dirtyq) return;

    dirtyq_reserve(m, m->dirty_len + 1);
    m->dirty[m->dirty_len++] = (KC_DirtyEntry){ s->chunk.cx, s->chunk.cy, s->chunk.cz };
    s->in_dirtyq = true;
}

static void mark_dirty_slot(KC_ChunkManager* m, int idx) {
    KC_ChunkSlot* s = slot_at(m, idx);
    if (s->st != SLOT_USED) return;
    s->dirty = true;
    s->dirty_ver++;
    dirtyq_push_slot(m, idx);
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

static void cache_invalidate(KC_ChunkManager* m) {
    m->cache_valid = false;
}

static void cache_rebuild(KC_ChunkManager* m, int ccx, int ccz) {
    m->cache_ccx = ccx;
    m->cache_ccz = ccz;

    for (int dz = -KC_CHUNK_CACHE_RAD; dz <= KC_CHUNK_CACHE_RAD; dz++) {
        for (int dx = -KC_CHUNK_CACHE_RAD; dx <= KC_CHUNK_CACHE_RAD; dx++) {
            int cx = ccx + dx;
            int cz = ccz + dz;

            int idx = table_find_index(m, cx, 0, cz); /* cy=0 cache for now */
            KC_Chunk* ptr = NULL;
            if (idx >= 0) {
                KC_ChunkSlot* s = &((KC_ChunkSlot*)m->slots)[idx];
                if (s->st == SLOT_USED) ptr = &s->chunk;
            }

            int ix = dx + KC_CHUNK_CACHE_RAD;
            int iz = dz + KC_CHUNK_CACHE_RAD;
            m->cache_chunks[iz * KC_CHUNK_CACHE_DIM + ix] = ptr;
        }
    }

    m->cache_valid = true;
}

static void mark_dirty_if_present(KC_ChunkManager* m, int cx, int cy, int cz) {
    int idx = table_find_index(m, cx, cy, cz);
    if (idx < 0) return;
    mark_dirty_slot(m, idx);
}

static void mark_neighbors_dirty(KC_ChunkManager* m, int cx, int cy, int cz) {
    /* 6-neighborhood */
    mark_dirty_if_present(m, cx + 1, cy, cz);
    mark_dirty_if_present(m, cx - 1, cy, cz);
    mark_dirty_if_present(m, cx, cy + 1, cz);
    mark_dirty_if_present(m, cx, cy - 1, cz);
    mark_dirty_if_present(m, cx, cy, cz + 1);
    mark_dirty_if_present(m, cx, cy, cz - 1);
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
            d->has_gpu = false;
            d->dirty = false;
            d->in_dirtyq = false;
            d->dirty_ver = 0;
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
    cache_invalidate(m);

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
        d->in_dirtyq = s->in_dirtyq;
        d->dirty_ver = s->dirty_ver;
    }

    free(old);

    if (m->has_center) {
        cache_rebuild(m, m->center_cx, m->center_cz);
    }
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

    m->dirty = NULL;
    m->dirty_len = 0;
    m->dirty_cap = 0;

    m->cache_valid = false;
    m->cache_ccx = 0;
    m->cache_ccz = 0;
    for (int i = 0; i < KC_CHUNK_CACHE_MAX; i++) m->cache_chunks[i] = NULL;

    kc_meshdata_init(&m->scratch);
    m->scratch_inited = true;
    return true;
}

void kc_chunkman_shutdown(KC_ChunkManager* m, KC_Renderer* r) {
    if (!m || !m->slots) return;

    if (m->scratch_inited) {
        kc_meshdata_free(&m->scratch);
        m->scratch_inited = false;
    }

    free(m->dirty);
    m->dirty = NULL;
    m->dirty_len = 0;
    m->dirty_cap = 0;

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
        mark_dirty_slot(m, idx);

        /* New neighbor chunk affects boundary culling; re-mesh adjacent chunks. */
        mark_neighbors_dirty(m, cx, cy, cz);
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

uint8_t kc_chunkman_get_block_world_fast(const KC_ChunkManager* m, int wx, int wy, int wz) {
    if (m && m->cache_valid) {
        int cx = floor_div_i(wx, KC_CHUNK_X);
        int cy = floor_div_i(wy, KC_CHUNK_Y);
        int cz = floor_div_i(wz, KC_CHUNK_Z);

        if (cy == 0) { /* cache is cy=0 for now */
            int dx = cx - m->cache_ccx;
            int dz = cz - m->cache_ccz;

            if ((unsigned)(dx + KC_CHUNK_CACHE_RAD) < (unsigned)KC_CHUNK_CACHE_DIM &&
                (unsigned)(dz + KC_CHUNK_CACHE_RAD) < (unsigned)KC_CHUNK_CACHE_DIM) {

                KC_Chunk* c = m->cache_chunks[(dz + KC_CHUNK_CACHE_RAD) * KC_CHUNK_CACHE_DIM +
                                              (dx + KC_CHUNK_CACHE_RAD)];
                if (!c) return KC_BLOCK_AIR;

                int lx = floor_mod_i(wx, KC_CHUNK_X);
                int ly = floor_mod_i(wy, KC_CHUNK_Y);
                int lz = floor_mod_i(wz, KC_CHUNK_Z);
                return kc_chunk_get(c, lx, ly, lz);
            }
        }
    }

    /* Fallback (hash lookup) */
    return kc_chunkman_get_block_world(m, wx, wy, wz);
}

void kc_chunkman_get_stats(const KC_ChunkManager* m, KC_ChunkStats* stats) {
    if (!m || !stats) return;

    stats->total_chunks = m->cap;
    stats->loaded_chunks = 0;
    stats->dirty_chunks = 0;
    stats->gpu_chunks = 0;

    const KC_ChunkSlot* slots = (const KC_ChunkSlot*)m->slots;
    for (int i = 0; i < m->cap; i++) {
        const KC_ChunkSlot* s = &slots[i];
        if (s->st != SLOT_USED) continue;

        stats->loaded_chunks++;
        if (s->dirty) stats->dirty_chunks++;
        if (s->has_gpu) stats->gpu_chunks++;
    }
}

void kc_chunkman_stream_around(KC_ChunkManager* m, v3 cam_pos) {
    int ccx = chunk_coord_from_worldf(cam_pos.x, KC_CHUNK_X);
    int ccz = chunk_coord_from_worldf(cam_pos.z, KC_CHUNK_Z);

    bool moved = (!m->has_center) || (ccx != m->center_cx) || (ccz != m->center_cz);

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
            s->in_dirtyq = false;
            s->st = SLOT_TOMB;
            m->count--;
            m->tombs++;
        }
    }

    maybe_rehash_if_tombs(m);

    /* Only rebuild cache when we actually moved (or cache was invalidated). */
    if (moved || !m->cache_valid) {
        cache_rebuild(m, ccx, ccz);
    }
}

static uint8_t query_block_cb(void* user, int wx, int wy, int wz) {
    const KC_ChunkManager* m = (const KC_ChunkManager*)user;
    return kc_chunkman_get_block_world_fast(m, wx, wy, wz);
}

typedef struct KC_Plane {
    float a, b, c, d; /* ax+by+cz+d >= 0 is inside */
} KC_Plane;

static void kc_plane_normalize(KC_Plane* p) {
    float l = sqrtf(p->a*p->a + p->b*p->b + p->c*p->c);
    if (l > 0.000001f) {
        p->a /= l; p->b /= l; p->c /= l; p->d /= l;
    }
}

/* m is column-major (OpenGL-style), like your m4_mul implementation */
static void kc_frustum_from_vp(m4 m, KC_Plane out6[6]) {
    /* Row vectors of the matrix (because it’s stored column-major) */
    float r0x = m.m[0],  r0y = m.m[4],  r0z = m.m[8],  r0w = m.m[12];
    float r1x = m.m[1],  r1y = m.m[5],  r1z = m.m[9],  r1w = m.m[13];
    float r2x = m.m[2],  r2y = m.m[6],  r2z = m.m[10], r2w = m.m[14];
    float r3x = m.m[3],  r3y = m.m[7],  r3z = m.m[11], r3w = m.m[15];

    /* Left   = r3 + r0 */
    out6[0] = (KC_Plane){ r3x+r0x, r3y+r0y, r3z+r0z, r3w+r0w };
    /* Right  = r3 - r0 */
    out6[1] = (KC_Plane){ r3x-r0x, r3y-r0y, r3z-r0z, r3w-r0w };
    /* Bottom = r3 + r1 */
    out6[2] = (KC_Plane){ r3x+r1x, r3y+r1y, r3z+r1z, r3w+r1w };
    /* Top    = r3 - r1 */
    out6[3] = (KC_Plane){ r3x-r1x, r3y-r1y, r3z-r1z, r3w-r1w };
    /* Near   = r3 + r2 */
    out6[4] = (KC_Plane){ r3x+r2x, r3y+r2y, r3z+r2z, r3w+r2w };
    /* Far    = r3 - r2 */
    out6[5] = (KC_Plane){ r3x-r2x, r3y-r2y, r3z-r2z, r3w-r2w };

    for (int i = 0; i < 6; i++) kc_plane_normalize(&out6[i]);
}

static bool kc_aabb_in_frustum(const KC_Plane fr[6],
                               float minx, float miny, float minz,
                               float maxx, float maxy, float maxz) {
    for (int i = 0; i < 6; i++) {
        const KC_Plane* p = &fr[i];

        /* “positive vertex” test */
        float x = (p->a >= 0.0f) ? maxx : minx;
        float y = (p->b >= 0.0f) ? maxy : miny;
        float z = (p->c >= 0.0f) ? maxz : minz;

        if (p->a*x + p->b*y + p->c*z + p->d < 0.0f)
            return false;
    }
    return true;
}

void kc_chunkman_rebuild_dirty(KC_ChunkManager* m, KC_Renderer* r, v3 cam_pos, float ms_mesh_budget, float ms_upload_budget) {
    if (!m || !m->slots) return;
    if (ms_mesh_budget <= 0.0f) return;

    const double mesh_budget_sec = (double)ms_mesh_budget / 1000.0;
    const double upload_budget_sec = (double)KC_UPLOAD_MS_BUDGET / 1000.0;
    const double t0 = kc_now_sec();
    double upload_spent = 0.0;

    while (m->dirty_len > 0) {
        if (kc_now_sec() - t0 >= mesh_budget_sec) break;

        /* Nearest-first pick, scan a small window for O(1) behavior. */
        int window = m->dirty_len < KC_DIRTY_PICK_WINDOW ? m->dirty_len : KC_DIRTY_PICK_WINDOW;

        int best_i = -1;
        float best_d2 = 0.0f;

        for (int i = 0; i < window; i++) {
            KC_DirtyEntry e = m->dirty[i];

            float ox = (float)(e.cx * KC_CHUNK_X);
            float oy = (float)(e.cy * KC_CHUNK_Y);
            float oz = (float)(e.cz * KC_CHUNK_Z);

            float cx = ox + 0.5f * (float)KC_CHUNK_X;
            float cy = oy + 0.5f * (float)KC_CHUNK_Y;
            float cz = oz + 0.5f * (float)KC_CHUNK_Z;

            float dx = cx - cam_pos.x;
            float dy = cy - cam_pos.y;
            float dz = cz - cam_pos.z;
            float d2 = dx*dx + dy*dy + dz*dz;

            if (best_i < 0 || d2 < best_d2) {
                best_i = i;
                best_d2 = d2;
            }
        }

        KC_ASSERT(best_i >= 0);

        /* Pop chosen entry (swap-remove). */
        KC_DirtyEntry e = m->dirty[best_i];
        m->dirty[best_i] = m->dirty[m->dirty_len - 1];
        m->dirty_len--;

        /* Resolve to current slot (safe across rehash). */
        int idx = table_find_index(m, e.cx, e.cy, e.cz);
        if (idx < 0) continue;

        KC_ChunkSlot* s = slot_at(m, idx);
        if (s->st != SLOT_USED) continue;

        /* Consume this entry. */
        s->in_dirtyq = false;
        if (!s->dirty) continue;

        uint32_t ver_before = s->dirty_ver;

        kc_meshdata_clear(&m->scratch);

        if (!kc_chunk_build_mesh_ex(&s->chunk, &m->scratch, query_block_cb, m)) {
            KC_ERR("chunk mesh build failed (%d,%d,%d)", s->chunk.cx, s->chunk.cy, s->chunk.cz);
            s->dirty = false;
            continue;
        }

        if (!s->has_gpu) {
            memset(&s->gpu, 0, sizeof(s->gpu));
            s->has_gpu = true;
        }

        /* Check upload budget before uploading */
        if (upload_spent >= upload_budget_sec) {
            /* Stop doing more uploads this frame; remaining dirty chunks stay queued */
            break;
        }

        double tu0 = kc_now_sec();
        kc_renderer_upload_mesh(r, &s->gpu, &m->scratch);
        upload_spent += (kc_now_sec() - tu0);

        /* Clear dirty only if nothing dirtied it again mid-build. */
        if (s->dirty_ver == ver_before) {
            s->dirty = false;
        } else {
            dirtyq_push_slot(m, idx);
        }
    }
}

void kc_chunkman_draw(const KC_ChunkManager* m, const KC_Renderer* r, m4 vp) {
    KC_Plane fr[6];
    kc_frustum_from_vp(vp, fr);

    const KC_ChunkSlot* slots = (const KC_ChunkSlot*)m->slots;
    for (int i = 0; i < m->cap; i++) {
        const KC_ChunkSlot* s = &slots[i];
        if (s->st != SLOT_USED) continue;
        if (!s->has_gpu || s->gpu.idx_count == 0) continue;

        float ox = (float)chunk_origin_x(s->chunk.cx);
        float oy = (float)chunk_origin_y(s->chunk.cy);
        float oz = (float)chunk_origin_z(s->chunk.cz);

        float minx = ox, miny = oy, minz = oz;
        float maxx = ox + (float)KC_CHUNK_X;
        float maxy = oy + (float)KC_CHUNK_Y;
        float maxz = oz + (float)KC_CHUNK_Z;

        if (!kc_aabb_in_frustum(fr, minx, miny, minz, maxx, maxy, maxz))
            continue;

        v3 t = (v3){ ox, oy, oz };
        m4 model = m4_translate(t);
        m4 mvp2 = m4_mul(vp, model);

        kc_renderer_draw_mesh(r, &s->gpu, mvp2);
    }
}


