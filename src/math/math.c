/**
 * @file math.c
 * @brief Math module for vector and matrix operations.
 * @author frankischilling
 * @date 2024-11-19
 */
#include <math.h>
#include "math.h"
void vec3_add(Vec3 result, const Vec3 a, const Vec3 b) {
    result[0] = a[0] + b[0];
    result[1] = a[1] + b[1];
    result[2] = a[2] + b[2];
}

void vec3_subtract(Vec3 result, const Vec3 a, const Vec3 b) {
    result[0] = a[0] - b[0];
    result[1] = a[1] - b[1];
    result[2] = a[2] - b[2];
}

void vec3_scale(Vec3 result, const Vec3 v, float scale) {
    result[0] = v[0] * scale;
    result[1] = v[1] * scale;
    result[2] = v[2] * scale;
}

void vec3_normalize(Vec3 result, const Vec3 v) {
    float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    result[0] = v[0] / length;
    result[1] = v[1] / length;
    result[2] = v[2] / length;
}

void vec3_cross(Vec3 result, const Vec3 a, const Vec3 b) {
    result[0] = a[1] * b[2] - a[2] * b[1];
    result[1] = a[2] * b[0] - a[0] * b[2];
    result[2] = a[0] * b[1] - a[1] * b[0];
}

float vec3_dot(const Vec3 a, const Vec3 b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

float toRadians(float degrees) {
    return degrees * (M_PI / 180.0f);
}

void mat4_identity(Mat4 result) {
    for(int i = 0; i < 16; i++) {
        result[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }
}

void mat4_perspective(Mat4 result, float fovy, float aspect, float near, float far) {
    float f = 1.0f / tanf(toRadians(fovy) / 2.0f);
    
    result[0] = f / aspect;
    result[1] = 0.0f;
    result[2] = 0.0f;
    result[3] = 0.0f;
    
    result[4] = 0.0f;
    result[5] = f;
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