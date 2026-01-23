#include "world.h"
#include "log.h"
#include <string.h>

static int idx3(int x, int y, int z) {
    return (y * KC_CHUNK_Z + z) * KC_CHUNK_X + x;
}

uint8_t kc_chunk_get(const KC_Chunk* c, int lx, int ly, int lz) {
    if (lx < 0 || lx >= KC_CHUNK_X) return KC_BLOCK_AIR;
    if (ly < 0 || ly >= KC_CHUNK_Y) return KC_BLOCK_AIR;
    if (lz < 0 || lz >= KC_CHUNK_Z) return KC_BLOCK_AIR;
    return c->blocks[idx3(lx, ly, lz)];
}

void kc_chunk_set(KC_Chunk* c, int lx, int ly, int lz, uint8_t id) {
    if (lx < 0 || lx >= KC_CHUNK_X) return;
    if (ly < 0 || ly >= KC_CHUNK_Y) return;
    if (lz < 0 || lz >= KC_CHUNK_Z) return;
    c->blocks[idx3(lx, ly, lz)] = id;
}

void kc_chunk_generate_flat(KC_Chunk* c) {
    int cx = c->cx, cy = c->cy, cz = c->cz;
    memset(c->blocks, 0, sizeof(c->blocks));
    c->cx = cx;
    c->cy = cy;
    c->cz = cz;

    for (int z = 0; z < KC_CHUNK_Z; z++) {
        for (int x = 0; x < KC_CHUNK_X; x++) {
            for (int y = 0; y < KC_CHUNK_Y; y++) {
                int wy = cy * KC_CHUNK_Y + y;
                uint8_t id = KC_BLOCK_AIR;
                if (wy == 0) id = KC_BLOCK_STONE;
                else if (wy == 1) id = KC_BLOCK_DIRT;
                else if (wy == 2) id = KC_BLOCK_GRASS;
                kc_chunk_set(c, x, y, z, id);
            }
        }
    }
}

static uint8_t sample_block_worldaware(
    const KC_Chunk* c,
    int lx, int ly, int lz,
    int base_x, int base_y, int base_z,
    KC_BlockQueryFn q, void* user
) {
    if (lx >= 0 && lx < KC_CHUNK_X &&
        ly >= 0 && ly < KC_CHUNK_Y &&
        lz >= 0 && lz < KC_CHUNK_Z) {
        return kc_chunk_get(c, lx, ly, lz);
    }
    if (q) return q(user, base_x + lx, base_y + ly, base_z + lz);
    return KC_BLOCK_AIR;
}

static int face_for_axis_dir(int axis, int dir) {
    if (axis == 0) return (dir > 0) ? FACE_PX : FACE_NX;
    if (axis == 1) return (dir > 0) ? FACE_PY : FACE_NY;
    return (dir > 0) ? FACE_PZ : FACE_NZ;
}

static void emit_greedy_quad(
    KC_MeshData* out,
    int face, uint8_t tile,
    int axis, int w,
    int u0, int v0,
    int du, int dv
) {
    v2 uvs[4];
    for (int k = 0; k < 4; k++) kc_atlas_uv_for_tile(tile, k, &uvs[k]);

    KC_Vertex qv[4];

    float x0, x1, y0, y1, z0, z1;

    if (axis == 0) {
        float x = (float)w;
        y0 = (float)u0;
        y1 = (float)(u0 + du);
        z0 = (float)v0;
        z1 = (float)(v0 + dv);

        if (face == FACE_PX) {
            qv[0] = (KC_Vertex){x, y0, z0, uvs[0].x, uvs[0].y};
            qv[1] = (KC_Vertex){x, y0, z1, uvs[1].x, uvs[1].y};
            qv[2] = (KC_Vertex){x, y1, z1, uvs[2].x, uvs[2].y};
            qv[3] = (KC_Vertex){x, y1, z0, uvs[3].x, uvs[3].y};
        } else {
            qv[0] = (KC_Vertex){x, y0, z1, uvs[0].x, uvs[0].y};
            qv[1] = (KC_Vertex){x, y0, z0, uvs[1].x, uvs[1].y};
            qv[2] = (KC_Vertex){x, y1, z0, uvs[2].x, uvs[2].y};
            qv[3] = (KC_Vertex){x, y1, z1, uvs[3].x, uvs[3].y};
        }
    } else if (axis == 1) {
        float y = (float)w;
        x0 = (float)u0;
        x1 = (float)(u0 + du);
        z0 = (float)v0;
        z1 = (float)(v0 + dv);

        if (face == FACE_PY) {
            qv[0] = (KC_Vertex){x0, y, z0, uvs[0].x, uvs[0].y};
            qv[1] = (KC_Vertex){x1, y, z0, uvs[1].x, uvs[1].y};
            qv[2] = (KC_Vertex){x1, y, z1, uvs[2].x, uvs[2].y};
            qv[3] = (KC_Vertex){x0, y, z1, uvs[3].x, uvs[3].y};
        } else {
            qv[0] = (KC_Vertex){x0, y, z1, uvs[0].x, uvs[0].y};
            qv[1] = (KC_Vertex){x1, y, z1, uvs[1].x, uvs[1].y};
            qv[2] = (KC_Vertex){x1, y, z0, uvs[2].x, uvs[2].y};
            qv[3] = (KC_Vertex){x0, y, z0, uvs[3].x, uvs[3].y};
        }
    } else {
        float z = (float)w;
        x0 = (float)u0;
        x1 = (float)(u0 + du);
        y0 = (float)v0;
        y1 = (float)(v0 + dv);

        if (face == FACE_PZ) {
            qv[0] = (KC_Vertex){x1, y0, z, uvs[0].x, uvs[0].y};
            qv[1] = (KC_Vertex){x0, y0, z, uvs[1].x, uvs[1].y};
            qv[2] = (KC_Vertex){x0, y1, z, uvs[2].x, uvs[2].y};
            qv[3] = (KC_Vertex){x1, y1, z, uvs[3].x, uvs[3].y};
        } else {
            qv[0] = (KC_Vertex){x0, y0, z, uvs[0].x, uvs[0].y};
            qv[1] = (KC_Vertex){x1, y0, z, uvs[1].x, uvs[1].y};
            qv[2] = (KC_Vertex){x1, y1, z, uvs[2].x, uvs[2].y};
            qv[3] = (KC_Vertex){x0, y1, z, uvs[3].x, uvs[3].y};
        }
    }

    (void)kc_meshdata_push_quad(out, qv);
}

static uint32_t pack_mask(uint8_t block_id, uint8_t face, uint8_t tile) {
    if (block_id == KC_BLOCK_AIR) return 0;
    return (uint32_t)block_id | ((uint32_t)face << 8) | ((uint32_t)tile << 16);
}

bool kc_chunk_build_mesh_ex(const KC_Chunk* c, KC_MeshData* out, KC_BlockQueryFn q, void* user) {
    kc_meshdata_clear(out);

    const int base_x = c->cx * KC_CHUNK_X;
    const int base_y = c->cy * KC_CHUNK_Y;
    const int base_z = c->cz * KC_CHUNK_Z;

#define KC_MAX2(a,b) ((a)>(b)?(a):(b))
#define KC_MAX3(a,b,c) (KC_MAX2(KC_MAX2((a),(b)),(c)))

    uint32_t mask[ KC_MAX3(
        KC_CHUNK_X * KC_CHUNK_Y,
        KC_CHUNK_X * KC_CHUNK_Z,
        KC_CHUNK_Y * KC_CHUNK_Z
    ) ];

    const int size[3] = { KC_CHUNK_X, KC_CHUNK_Y, KC_CHUNK_Z };

    for (int axis = 0; axis < 3; axis++) {
        int u, v;
        if (axis == 0) { u = 1; v = 2; }
        else if (axis == 1) { u = 0; v = 2; }
        else { u = 0; v = 1; }

        const int su = size[u];
        const int sv = size[v];

        for (int w = 0; w <= size[axis]; w++) {
            for (int j = 0; j < sv; j++) {
                for (int i = 0; i < su; i++) {
                    int acoord[3] = {0,0,0};
                    int bcoord[3] = {0,0,0};
                    acoord[axis] = w - 1;
                    bcoord[axis] = w;
                    acoord[u] = i; acoord[v] = j;
                    bcoord[u] = i; bcoord[v] = j;

                    uint8_t ida = sample_block_worldaware(
                        c, acoord[0], acoord[1], acoord[2],
                        base_x, base_y, base_z, q, user
                    );
                    uint8_t idb = sample_block_worldaware(
                        c, bcoord[0], bcoord[1], bcoord[2],
                        base_x, base_y, base_z, q, user
                    );

                    int sa = (ida != KC_BLOCK_AIR) && g_block_defs[ida].solid;
                    int sb = (idb != KC_BLOCK_AIR) && g_block_defs[idb].solid;

                    uint32_t cell = 0;

                    if (sa && !sb) {
                        int face = face_for_axis_dir(axis, +1);
                        uint8_t tile = g_block_defs[ida].tile[face];
                        cell = pack_mask(ida, (uint8_t)face, tile);
                    } else if (!sa && sb) {
                        int face = face_for_axis_dir(axis, -1);
                        uint8_t tile = g_block_defs[idb].tile[face];
                        cell = pack_mask(idb, (uint8_t)face, tile);
                    }

                    mask[i + j * su] = cell;
                }
            }

            for (int j = 0; j < sv; j++) {
                for (int i = 0; i < su; i++) {
                    uint32_t val = mask[i + j * su];
                    if (val == 0) continue;

                    int du = 1;
                    while (i + du < su && mask[(i + du) + j * su] == val) du++;

                    int dv = 1;
                    for (;;) {
                        if (j + dv >= sv) break;
                        int k;
                        for (k = 0; k < du; k++) {
                            if (mask[(i + k) + (j + dv) * su] != val) break;
                        }
                        if (k != du) break;
                        dv++;
                    }

                    uint8_t block_id = (uint8_t)(val & 0xFFu);
                    uint8_t face     = (uint8_t)((val >> 8) & 0xFFu);
                    uint8_t tile     = (uint8_t)((val >> 16) & 0xFFu);

                    (void)block_id;

                    emit_greedy_quad(out, face, tile, axis, w, i, j, du, dv);

                    for (int y = 0; y < dv; y++) {
                        for (int x = 0; x < du; x++) {
                            mask[(i + x) + (j + y) * su] = 0;
                        }
                    }

                    i += du - 1;
                }
            }
        }
    }

    return true;
}
