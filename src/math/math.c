/**
 * @file math/math.c
 * @brief Math module for vector and matrix operations.
 * @author frankischilling
 * @version 0.1
 * @date 2024-11-19
 *
 */
#include "math.h"
#include <math.h>

void vec3_add(Vec3* result, const Vec3* a, const Vec3* b) {
  result->x = a->x + b->x;
  result->y = a->y + b->y;
  result->z = a->z + b->z;
}
void vec3_subtract(Vec3* result, const Vec3* a, const Vec3* b) {
  result->x = a->x - b->x;
  result->y = a->y - b->y;
  result->z = a->z - b->z;
}
void vec3_scale(Vec3* result, const Vec3* v, float scale) {
  result->x = v->x * scale;
  result->y = v->y * scale;
  result->z = v->z * scale;
}
void vec3_normalize(Vec3* result, const Vec3* v) {
  float length = sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
  result->x = v->x / length;
  result->y = v->y / length;
  result->z = v->z / length;
}
void vec3_cross(Vec3* result, const Vec3* a, const Vec3* b) {
  result->x = a->y * b->z - a->z * b->y;
  result->y = a->z * b->x - a->x * b->z;
  result->z = a->x * b->y - a->y * b->x;
}
float vec3_dot(const Vec3* a, const Vec3* b) {
  return a->x * b->x + a->y * b->y + a->z * b->z;
}
float vec3_distance(const Vec3* a, const Vec3* b) {
  Vec3 diff;
  vec3_subtract(&diff, a, b);
  return sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
}

void vec3i_add(Vec3i* result, const Vec3i* a, const Vec3i* b) {
  result->x = a->x + b->x;
  result->y = a->y + b->y;
  result->z = a->z + b->z;
}
void vec3i_subtract(Vec3i* result, const Vec3i* a, const Vec3i* b) {
  result->x = a->x - b->x;
  result->y = a->y - b->y;
  result->z = a->z - b->z;
}
void vec3i_scale(Vec3i* result, const Vec3i* v, int scale) {
  result->x = v->x * scale;
  result->y = v->y * scale;
  result->z = v->z * scale;
}
void vec3i_normalize(Vec3i* result, const Vec3i* v) {
  float length = sqrtf((float)(v->x * v->x + v->y * v->y + v->z * v->z));
  result->x = (int)(v->x / length);
  result->y = (int)(v->y / length);
  result->z = (int)(v->z / length);
}
void vec3i_cross(Vec3i* result, const Vec3i* a, const Vec3i* b) {
  result->x = a->y * b->z - a->z * b->y;
  result->y = a->z * b->x - a->x * b->z;
  result->z = a->x * b->y - a->y * b->x;
}
int vec3i_dot(const Vec3i* a, const Vec3i* b) {
  return a->x * b->x + a->y * b->y + a->z * b->z;
}
float vec3i_distance(const Vec3i* a, const Vec3i* b) {
  Vec3i diff;
  vec3i_subtract(&diff, a, b);
  return sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
}

void vec2i_add(Vec2i* result, const Vec2i* a, const Vec2i* b) {
  result->a = a->a + b->a;
  result->b = a->b + b->b;
}
void vec2i_subtract(Vec2i* result, const Vec2i* a, const Vec2i* b) {
  result->a = a->a - b->a;
  result->b = a->b - b->b;
}
void vec2i_scale(Vec2i* result, const Vec2i* v, int scale) {
  result->a = v->a * scale;
  result->b = v->b * scale;
}
void vec2i_normalize(Vec2i* result, const Vec2i* v) {
  float length = sqrtf((float)(v->a * v->a + v->b * v->b));
  result->a = (int)(v->a / length);
  result->b = (int)(v->b / length);
}
int vec2i_dot(const Vec2i* a, const Vec2i* b) {
  return a->a * b->a + a->b * b->b;
}
float vec2i_distance(const Vec2i* a, const Vec2i* b) {
  Vec2i diff;
  vec2i_subtract(&diff, a, b);
  return sqrtf(diff.a * diff.a + diff.b * diff.b);
}

float toRadians(float degrees) {
  return degrees * (M_PI / 180.0f);
}

void mat4_identity(Mat4 result) {
  for (int i = 0; i < 16; i++) {
    result[i] = (i % 5 == 0) ? 1.0f : 0.0f;
  }
}

void mat4_perspective(Mat4 result, float fovy, float aspect, float near, float far) {
  float tanHalfFovy = tanf(toRadians(fovy) / 2.0f);

  result[0] = 1.0f / (aspect * tanHalfFovy);
  result[1] = 0.0f;
  result[2] = 0.0f;
  result[3] = 0.0f;

  result[4] = 0.0f;
  result[5] = 1.0f / tanHalfFovy;
  result[6] = 0.0f;
  result[7] = 0.0f;

  result[8] = 0.0f;
  result[9] = 0.0f;
  result[10] = -(far + near) / (far - near);
  result[11] = -1.0f;

  result[12] = 0.0f;
  result[13] = 0.0f;
  result[14] = -(2.0f * far * near) / (far - near);
  result[15] = 0.0f;
}

void mat4_lookAt(Mat4 result, const Vec3* eye, const Vec3* center, const Vec3* up) {
  Vec3 f, s, u, temp;

  // Calculate forward vector
  vec3_subtract(&temp, center, eye);
  vec3_normalize(&f, &temp);

  // Calculate side vector
  vec3_cross(&temp, &f, up);
  vec3_normalize(&s, &temp);

  // Calculate up vector
  vec3_cross(&u, &s, &f);

  result[0] = s.x;
  result[1] = u.x;
  result[2] = -f.x;
  result[3] = 0.0f;

  result[4] = s.y;
  result[5] = u.y;
  result[6] = -f.y;
  result[7] = 0.0f;

  result[8] = s.z;
  result[9] = u.z;
  result[10] = -f.z;
  result[11] = 0.0f;

  result[12] = -vec3_dot(&s, eye);
  result[13] = -vec3_dot(&u, eye);
  result[14] = vec3_dot(&f, eye);
  result[15] = 1.0f;
}

static const int permutation[256] = {
    151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,   225, 140, 36,  103, 30,  69,  142, 8,   99,  37,  240, 21,  10,  23,  190, 6,   148,
    247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203, 117, 35,  11,  32,  57,  177, 33,  88,  237, 149, 56,  87,  174, 20,  125, 136, 171, 168, 68,  175,
    74,  165, 71,  134, 139, 48,  27,  166, 77,  146, 158, 231, 83,  111, 229, 122, 60,  211, 133, 230, 220, 105, 92,  41,  55,  46,  245, 40,  244, 102, 143, 54,
    65,  25,  63,  161, 1,   216, 80,  73,  209, 76,  132, 187, 208, 89,  18,  169, 200, 196, 135, 130, 116, 188, 159, 86,  164, 100, 109, 198, 173, 186, 3,   64,
    52,  217, 226, 250, 124, 123, 5,   202, 38,  147, 118, 126, 255, 82,  85,  212, 207, 206, 59,  227, 47,  16,  58,  17,  182, 189, 28,  42,  223, 183, 170, 213,
    119, 248, 152, 2,   44,  154, 163, 70,  221, 153, 101, 155, 167, 43,  172, 9,   129, 22,  39,  253, 19,  98,  108, 110, 79,  113, 224, 232, 178, 185, 112, 104,
    218, 246, 97,  228, 251, 34,  242, 193, 238, 210, 144, 12,  191, 179, 162, 241, 81,  51,  145, 235, 249, 14,  239, 107, 49,  192, 214, 31,  181, 199, 106, 157,
    184, 84,  204, 176, 115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205, 93,  222, 114, 67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156, 180};

float fade(float t) {
  return t * t * t * (t * (t * 6 - 15) + 10);
}

float grad(int hash, float x, float y, float z) {
  int h = hash & 15;
  float u = h < 8 ? x : y;
  float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
  return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

int perm(int i) {
  return permutation[i & 255];
}

float perlin(float x, float y, float z) {
  int X = (int)floor(x) & 255;
  int Y = (int)floor(y) & 255;
  int Z = (int)floor(z) & 255;

  x -= floor(x);
  y -= floor(y);
  z -= floor(z);

  float u = fade(x);
  float v = fade(y);
  float w = fade(z);

  int A = perm(X) + Y;
  int AA = perm(A) + Z;
  int AB = perm(A + 1) + Z;
  int B = perm(X + 1) + Y;
  int BA = perm(B) + Z;
  int BB = perm(B + 1) + Z;

  float res = lerp(
      lerp(lerp(grad(perm(AA), x, y, z), grad(perm(BA), x - 1, y, z), u), lerp(grad(perm(AB), x, y - 1, z), grad(perm(BB), x - 1, y - 1, z), u), v),
      lerp(lerp(grad(perm(AA + 1), x, y, z - 1), grad(perm(BA + 1), x - 1, y, z - 1), u), lerp(grad(perm(AB + 1), x, y - 1, z - 1), grad(perm(BB + 1), x - 1, y - 1, z - 1), u), v),
      w);
  return (res + 1.0f) / 2.0f;
}

float perlin2d(float x, float z) {
  float y = 0.0f;
  float amplitude = 1.0f;
  float frequency = 0.1f;
  float persistence = 0.5f;

  for (int i = 0; i < 4; i++) {
    y += perlin(x * frequency, 0, z * frequency) * amplitude;
    amplitude *= persistence;
    frequency *= 2.0f;
  }

  return y;
}

float smoothstep(float edge0, float edge1, float x) {
  float t = (x - edge0) / (edge1 - edge0);     // Normalize x to 0-1
  t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); // Clamp t to 0 or 1
  return t * t * (3.0f - 2.0f * t);            // Cubic Hermite interpolation
}

float lerp(float a, float b, float t) {
  return a + t * (b - a); // Linear interpolation
}

// Simple 2D noise function
float noise2d(float x, float z) {
  return perlin2d(x, z);
}