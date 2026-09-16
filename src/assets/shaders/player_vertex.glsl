#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 viewProjection;
uniform vec3 feet;
uniform float rootYaw;
uniform float rootScale;
uniform vec3 partSize;
uniform vec3 partPivot;
uniform vec3 partCenter;
uniform vec3 partTranslation;
uniform vec3 partRotation;
uniform vec3 partScale;
uniform vec3 shellScale;
uniform bool viewModel;
uniform vec3 viewModelOffset;
uniform float viewModelScale;

out vec3 Normal;
out vec2 TexCoord;

vec3 rotateX(vec3 p, float angle) {
    float c = cos(angle), s = sin(angle);
    return vec3(p.x, c * p.y - s * p.z, s * p.y + c * p.z);
}

vec3 rotateY(vec3 p, float angle) {
    float c = cos(angle), s = sin(angle);
    return vec3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
}

vec3 rotateZ(vec3 p, float angle) {
    float c = cos(angle), s = sin(angle);
    return vec3(c * p.x - s * p.y, s * p.x + c * p.y, p.z);
}

vec3 rotatePose(vec3 p) {
    p = rotateX(p, partRotation.x);
    p = rotateY(p, partRotation.y);
    return rotateZ(p, partRotation.z);
}

void main() {
    vec3 p = aPos * partSize * shellScale + partCenter;
    p *= partScale;
    p = rotatePose(p);
    vec3 safeScale = max(abs(partScale), vec3(0.000001));
    vec3 normal = normalize(rotatePose(aNormal / safeScale));

    if (viewModel) {
        // Use the same right-arm cuboid and UVs as the world model. A compact
        // camera-space transform keeps every animation pose below/right of aim.
        p += partTranslation;
        p = rotateX(p, radians(-24.0));
        p = rotateY(p, radians(-18.0));
        p = rotateZ(p, radians(-8.0));
        p *= viewModelScale;
        p += viewModelOffset;
        normal = rotateX(normal, radians(-24.0));
        normal = rotateY(normal, radians(-18.0));
        normal = rotateZ(normal, radians(-8.0));
        gl_Position = viewProjection * vec4(p, 1.0);
    } else {
        p += partPivot + partTranslation;
        p *= rootScale;
        p = rotateY(p, rootYaw);
        p += feet;
        normal = rotateY(normal, rootYaw);
        gl_Position = viewProjection * vec4(p, 1.0);
    }

    Normal = normalize(normal);
    TexCoord = aTexCoord;
}
