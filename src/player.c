#include "player.h"
#include "chunk_manager.h"
#include "atlas.h"   /* g_block_defs + solid flag */
#include "config.h"
#include <math.h>

#define KC_PLAYER_EPS            0.001f
#define KC_GROUND_PROBE          0.05f

static int ifloorf(float x) { return (int)floorf(x); }

static inline bool block_solid(uint8_t id) {
    return (id != KC_BLOCK_AIR) && g_block_defs[id].solid;
}

static inline bool solid_at(const KC_ChunkManager* cm, int wx, int wy, int wz) {
    uint8_t id = kc_chunkman_get_block_world_fast(cm, wx, wy, wz);
    return block_solid(id);
}

static bool player_check_ground(const KC_Player* p, const KC_ChunkManager* cm) {
    /* Check a thin slice just below feet. */
    float minx = p->pos.x - p->half_w + KC_PLAYER_EPS;
    float maxx = p->pos.x + p->half_w - KC_PLAYER_EPS;
    float minz = p->pos.z - p->half_w + KC_PLAYER_EPS;
    float maxz = p->pos.z + p->half_w - KC_PLAYER_EPS;

    int x0 = ifloorf(minx);
    int x1 = ifloorf(maxx);
    int z0 = ifloorf(minz);
    int z1 = ifloorf(maxz);

    int y = ifloorf(p->pos.y - KC_GROUND_PROBE);

    for (int z = z0; z <= z1; z++) {
        for (int x = x0; x <= x1; x++) {
            if (solid_at(cm, x, y, z)) return true;
        }
    }
    return false;
}

static void move_x(KC_Player* p, const KC_ChunkManager* cm, float dx) {
    if (fabsf(dx) < 1e-8f) return;

    p->pos.x += dx;

    float minz = p->pos.z - p->half_w + KC_PLAYER_EPS;
    float maxz = p->pos.z + p->half_w - KC_PLAYER_EPS;
    float miny = p->pos.y + KC_PLAYER_EPS;
    float maxy = p->pos.y + p->height - KC_PLAYER_EPS;

    int z0 = ifloorf(minz), z1 = ifloorf(maxz);
    int y0 = ifloorf(miny), y1 = ifloorf(maxy);

    if (dx > 0.0f) {
        float maxx = p->pos.x + p->half_w;
        int bx = ifloorf(maxx);

        for (int y = y0; y <= y1; y++) {
            for (int z = z0; z <= z1; z++) {
                if (solid_at(cm, bx, y, z)) {
                    /* Clamp to the left face of block bx */
                    p->pos.x = (float)bx - p->half_w - KC_PLAYER_EPS;
                    p->vel.x = 0.0f;
                    return;
                }
            }
        }
    } else {
        float minx = p->pos.x - p->half_w;
        int bx = ifloorf(minx);

        for (int y = y0; y <= y1; y++) {
            for (int z = z0; z <= z1; z++) {
                if (solid_at(cm, bx, y, z)) {
                    /* Clamp to the right face of block bx */
                    p->pos.x = (float)(bx + 1) + p->half_w + KC_PLAYER_EPS;
                    p->vel.x = 0.0f;
                    return;
                }
            }
        }
    }
}

static void move_z(KC_Player* p, const KC_ChunkManager* cm, float dz) {
    if (fabsf(dz) < 1e-8f) return;

    p->pos.z += dz;

    float minx = p->pos.x - p->half_w + KC_PLAYER_EPS;
    float maxx = p->pos.x + p->half_w - KC_PLAYER_EPS;
    float miny = p->pos.y + KC_PLAYER_EPS;
    float maxy = p->pos.y + p->height - KC_PLAYER_EPS;

    int x0 = ifloorf(minx), x1 = ifloorf(maxx);
    int y0 = ifloorf(miny), y1 = ifloorf(maxy);

    if (dz > 0.0f) {
        float maxz = p->pos.z + p->half_w;
        int bz = ifloorf(maxz);

        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                if (solid_at(cm, x, y, bz)) {
                    p->pos.z = (float)bz - p->half_w - KC_PLAYER_EPS;
                    p->vel.z = 0.0f;
                    return;
                }
            }
        }
    } else {
        float minz = p->pos.z - p->half_w;
        int bz = ifloorf(minz);

        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                if (solid_at(cm, x, y, bz)) {
                    p->pos.z = (float)(bz + 1) + p->half_w + KC_PLAYER_EPS;
                    p->vel.z = 0.0f;
                    return;
                }
            }
        }
    }
}

static void move_y(KC_Player* p, const KC_ChunkManager* cm, float dy) {
    if (fabsf(dy) < 1e-8f) {
        /* still keep grounded accurate */
        p->on_ground = player_check_ground(p, cm);
        return;
    }

    p->pos.y += dy;
    p->on_ground = false;

    float minx = p->pos.x - p->half_w + KC_PLAYER_EPS;
    float maxx = p->pos.x + p->half_w - KC_PLAYER_EPS;
    float minz = p->pos.z - p->half_w + KC_PLAYER_EPS;
    float maxz = p->pos.z + p->half_w - KC_PLAYER_EPS;

    int x0 = ifloorf(minx), x1 = ifloorf(maxx);
    int z0 = ifloorf(minz), z1 = ifloorf(maxz);

    if (dy > 0.0f) {
        float maxy = p->pos.y + p->height;
        int by = ifloorf(maxy);

        for (int z = z0; z <= z1; z++) {
            for (int x = x0; x <= x1; x++) {
                if (solid_at(cm, x, by, z)) {
                    p->pos.y = (float)by - p->height - KC_PLAYER_EPS;
                    p->vel.y = 0.0f;
                    return;
                }
            }
        }
    } else {
        float miny = p->pos.y;
        int by = ifloorf(miny);

        for (int z = z0; z <= z1; z++) {
            for (int x = x0; x <= x1; x++) {
                if (solid_at(cm, x, by, z)) {
                    p->pos.y = (float)(by + 1) + KC_PLAYER_EPS;
                    p->vel.y = 0.0f;
                    p->on_ground = true;
                    return;
                }
            }
        }
    }

    /* If we didn't collide vertically, still update grounded via probe */
    p->on_ground = player_check_ground(p, cm);
}

void kc_player_init(KC_Player* p, v3 start_feet) {
    p->pos = start_feet;
    p->vel = (v3){0,0,0};
    p->half_w = 0.30f;
    p->height = 1.80f;
    p->eye_y  = 1.62f;
    p->on_ground = false;
}

void kc_player_step(KC_Player* p, const KC_Input* in, const KC_ChunkManager* cm, float dt, float yaw) {
    /* Build desired horizontal velocity from input (camera yaw, XZ-only) */
    float sy = sinf(yaw), cy = cosf(yaw);
    v3 fwd = (v3){ sy, 0.0f, -cy };  /* Note: -cy for OpenGL forward direction */
    v3 rgt = (v3){ cy, 0.0f, sy };

    v3 wish = (v3){0,0,0};
    if (in->keys[KC_KEY_W]) wish = v3_add(wish, fwd);
    if (in->keys[KC_KEY_S]) wish = v3_sub(wish, fwd);
    if (in->keys[KC_KEY_D]) wish = v3_add(wish, rgt);
    if (in->keys[KC_KEY_A]) wish = v3_sub(wish, rgt);

    float speed = KC_MOVE_SPEED;
    if (in->keys[KC_KEY_LSHIFT]) speed *= KC_SPRINT_MULT;

    if (v3_len(wish) > 0.001f) wish = v3_norm(wish);

    p->vel.x = wish.x * speed;
    p->vel.z = wish.z * speed;

    /* NOTE: no gravity yet; keep vel.y as-is (later you'll apply gravity here). */

    /* Axis-by-axis sweep: X then Z then Y */
    move_x(p, cm, p->vel.x * dt);
    move_z(p, cm, p->vel.z * dt);
    move_y(p, cm, p->vel.y * dt);
}
