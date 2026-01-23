#pragma once
#include <stdbool.h>

/* Define M_PI if the C library doesn't provide it (not standard in C99) */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct v3 { float x, y, z; } v3;
typedef struct v2 { float x, y; } v2;

typedef struct m4 {
    /* column-major 4x4 */
    float m[16];
} m4;

v3 v3_add(v3 a, v3 b);
v3 v3_sub(v3 a, v3 b);
v3 v3_mul(v3 a, float s);
float v3_dot(v3 a, v3 b);
v3 v3_cross(v3 a, v3 b);
float v3_len(v3 a);
v3 v3_norm(v3 a);

m4 m4_identity(void);
m4 m4_mul(m4 a, m4 b);
m4 m4_translate(v3 t);
m4 m4_perspective(float fovy_rad, float aspect, float znear, float zfar);
m4 m4_look(v3 eye, v3 center, v3 up);
