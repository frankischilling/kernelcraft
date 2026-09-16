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
uniform float shellInflation;
uniform bool viewModel;
uniform mat4 viewModelTransform;

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
    vec3 p = aPos * (partSize + 2.0 * shellInflation) + partCenter;
    vec3 normal;

    if (viewModel) {
        p = vec3(viewModelTransform * vec4(p, 1.0));
        normal = mat3(viewModelTransform) * aNormal;
        gl_Position = viewProjection * vec4(p, 1.0);
    } else {
        p = rotatePose(p * partScale);
        vec3 safeScale = max(abs(partScale), vec3(0.000001));
        normal = normalize(rotatePose(aNormal / safeScale));
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
