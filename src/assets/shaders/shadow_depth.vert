#version 330 core
layout(location = 0) in vec3 position;
layout(location = 2) in vec2 uv;
layout(location = 3) in float material;
uniform mat4 shadowTransform;
out vec2 TexCoord;
flat out float Material;
void main() {
    TexCoord = uv;
    Material = material;
    gl_Position = shadowTransform * vec4(position, 1.0);
}
