#include "mesh_builder.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>

static bool grow(void** ptr, size_t* cap, size_t elem_size, size_t want) {
    if (*cap >= want) return true;
    size_t ncap = (*cap == 0) ? 1024 : (*cap * 2);
    while (ncap < want) ncap *= 2;
    void* np = realloc(*ptr, ncap * elem_size);
    if (!np) return false;
    *ptr = np;
    *cap = ncap;
    return true;
}

void kc_meshdata_init(KC_MeshData* m) {
    memset(m, 0, sizeof(*m));
}

void kc_meshdata_free(KC_MeshData* m) {
    free(m->verts);
    free(m->indices);
    memset(m, 0, sizeof(*m));
}

void kc_meshdata_clear(KC_MeshData* m) {
    m->vert_count = 0;
    m->idx_count = 0;
}

bool kc_meshdata_push_quad(KC_MeshData* m, const KC_Vertex v[4]) {
    const size_t base = m->vert_count;
    if (!grow((void**)&m->verts, &m->vert_cap, sizeof(KC_Vertex), base + 4)) return false;
    if (!grow((void**)&m->indices, &m->idx_cap, sizeof(uint32_t), m->idx_count + 6)) return false;

    memcpy(&m->verts[base], v, 4 * sizeof(KC_Vertex));
    m->vert_count += 4;

    /* two triangles: (0,1,2) (0,2,3) */
    m->indices[m->idx_count++] = (uint32_t)(base + 0);
    m->indices[m->idx_count++] = (uint32_t)(base + 1);
    m->indices[m->idx_count++] = (uint32_t)(base + 2);
    m->indices[m->idx_count++] = (uint32_t)(base + 0);
    m->indices[m->idx_count++] = (uint32_t)(base + 2);
    m->indices[m->idx_count++] = (uint32_t)(base + 3);

    return true;
}
