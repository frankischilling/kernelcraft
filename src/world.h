#pragma once
#include "atlas.h"
#include "mesh_builder.h"
#include "config.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct KC_Chunk {
    int cx, cy, cz;
    uint8_t blocks[KC_CHUNK_X * KC_CHUNK_Y * KC_CHUNK_Z];
} KC_Chunk;

void kc_chunk_generate_flat(KC_Chunk* c);

uint8_t kc_chunk_get(const KC_Chunk* c, int lx, int ly, int lz);
void kc_chunk_set(KC_Chunk* c, int lx, int ly, int lz, uint8_t id);

typedef uint8_t (*KC_BlockQueryFn)(void* user, int wx, int wy, int wz);

bool kc_chunk_build_mesh_ex(const KC_Chunk* c, KC_MeshData* out, KC_BlockQueryFn q, void* user);
