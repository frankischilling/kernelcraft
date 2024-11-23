/**
 * @file graphics/camera.c
 * @brief Camera module for handling camera movement and orientation.
 * @author frankischilling
 * @date 2024-11-19
 */
#include <GLFW/glfw3.h>
#include <stdbool.h>
#include <math.h>

#include "../math/math.h"
#include "../utils/inputs.h"
#include "camera.h"

// Global variables
static float lastX = 1920.0f;
static float lastY = 1080.0f;
static bool firstMouse = true;

void initCamera(Camera* camera) {
    camera->position[0] = 0.0f;
    camera->position[1] = 0.0f;
    camera->position[2] = 3.0f;
    
    camera->front[0] = 0.0f;
    camera->front[1] = 0.0f;
    camera->front[2] = -1.0f;
    
    camera->up[0] = 0.0f;
    camera->up[1] = 1.0f;
    camera->up[2] = 0.0f;
    
    camera->yaw = -90.0f;
    camera->pitch = 0.0f;
    camera->speed = 5.0f;
    camera->sensitivity = 0.05f;
    
    updateCameraVectors(camera);
}

void updateCameraVectors(Camera* camera) {
    // Calculate new front vector
    camera->front[0] = cosf(toRadians(camera->yaw)) * cosf(toRadians(camera->pitch));
    camera->front[1] = sinf(toRadians(camera->pitch));
    camera->front[2] = sinf(toRadians(camera->yaw)) * cosf(toRadians(camera->pitch));
    
    // Normalize the front vector
    vec3_normalize(camera->front, camera->front);
}

void updateViewMatrix(Camera* camera, float* viewMatrix) {
    // Calculate the new front vector
    float frontX = cosf(camera->yaw) * cosf(camera->pitch);
    float frontY = sinf(camera->pitch);
    float frontZ = sinf(camera->yaw) * cosf(camera->pitch);
    camera->front[0] = frontX;
    camera->front[1] = frontY;
    camera->front[2] = frontZ;

    // Normalize the front vector
    float length = sqrtf(frontX * frontX + frontY * frontY + frontZ * frontZ);
    for (int i = 0; i < 3; i++) camera->front[i] /= length;

    // Calculate the right and up vectors
    float right[3] = {
        camera->up[1] * camera->front[2] - camera->up[2] * camera->front[1],
        camera->up[2] * camera->front[0] - camera->up[0] * camera->front[2],
        camera->up[0] * camera->front[1] - camera->up[1] * camera->front[0]
    };

    // Normalize the right vector
    length = sqrtf(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
    for (int i = 0; i < 3; i++) right[i] /= length;

    // Recalculate the up vector
    float newUp[3] = {
        right[1] * camera->front[2] - right[2] * camera->front[1],
        right[2] * camera->front[0] - right[0] * camera->front[2],
        right[0] * camera->front[1] - right[1] * camera->front[0]
    };

    // Construct the view matrix
    // This is a simplified version; typically, you'd use a library for this
    viewMatrix[0] = right[0];
    viewMatrix[1] = newUp[0];
    viewMatrix[2] = -camera->front[0];
    viewMatrix[3] = 0.0f;

    viewMatrix[4] = right[1];
    viewMatrix[5] = newUp[1];
    viewMatrix[6] = -camera->front[1];
    viewMatrix[7] = 0.0f;

    viewMatrix[8] = right[2];
    viewMatrix[9] = newUp[2];
    viewMatrix[10] = -camera->front[2];
    viewMatrix[11] = 0.0f;

    viewMatrix[12] = -camera->position[0];
    viewMatrix[13] = -camera->position[1];
    viewMatrix[14] = -camera->position[2];
    viewMatrix[15] = 1.0f;
}

void mat4_lookAt(Mat4 result, const Vec3 eye, const Vec3 center, const Vec3 up) {
    Vec3 f, s, u, temp;
    
    // Calculate forward vector
    vec3_subtract(temp, center, eye);
    vec3_normalize(f, temp);
    
    // Calculate side vector
    vec3_cross(temp, f, up);
    vec3_normalize(s, temp);
    
    // Calculate up vector
    vec3_cross(u, s, f);
    
    result[0] = s[0];
    result[1] = u[0];
    result[2] = -f[0];
    result[3] = 0.0f;
    
    result[4] = s[1];
    result[5] = u[1];
    result[6] = -f[1];
    result[7] = 0.0f;
    
    result[8] = s[2];
    result[9] = u[2];
    result[10] = -f[2];
    result[11] = 0.0f;
    
    result[12] = -vec3_dot(s, eye);
    result[13] = -vec3_dot(u, eye);
    result[14] = vec3_dot(f, eye);
    result[15] = 1.0f;
}