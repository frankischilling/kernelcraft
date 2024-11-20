#ifndef MATH_H
#define MATH_H

#include <math.h>

typedef float Vec3[3];
typedef float Mat4[16];

// Helper functions
float toRadians(float degrees);

// Vector operations
void vec3_add(Vec3 result, const Vec3 a, const Vec3 b);
void vec3_subtract(Vec3 result, const Vec3 a, const Vec3 b);
void vec3_scale(Vec3 result, const Vec3 v, float scale);
void vec3_normalize(Vec3 result, const Vec3 v);
void vec3_cross(Vec3 result, const Vec3 a, const Vec3 b);
float vec3_dot(const Vec3 a, const Vec3 b);

// Matrix operations
void mat4_identity(Mat4 result);
void mat4_perspective(Mat4 result, float fovy, float aspect, float near, float far);
void mat4_lookAt(Mat4 result, const Vec3 eye, const Vec3 center, const Vec3 up);

// Noise generation
float noise2d(float x, float z);
float smoothstep(float edge0, float edge1, float x);
float lerp(float a, float b, float t);

// Perlin noise functions
float fade(float t);
float grad(int hash, float x, float y, float z);
float perlin(float x, float y, float z);
int perm(int i);
float perlin2d(float x, float z);

#endif