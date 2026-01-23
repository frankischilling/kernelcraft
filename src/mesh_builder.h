#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct KC_Vertex {
    float px, py, pz;
    float u, v;
} KC_Vertex;

typedef struct KC_MeshData {
    KC_Vertex* verts;
    uint32_t* indices;
    size_t vert_count;
    size_t idx_count;
    size_t vert_cap;
    size_t idx_cap;
} KC_MeshData;

void kc_meshdata_init(KC_MeshData* m);
void kc_meshdata_free(KC_MeshData* m);
void kc_meshdata_clear(KC_MeshData* m);

bool kc_meshdata_push_quad(KC_MeshData* m,
                           const KC_Vertex v[4]); /* adds 4 verts, 6 indices */
