#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in float layer;

uniform mat4 model;
uniform mat4 viewProjection;
out vec3 Normal;
out vec2 TexCoord;
flat out float Layer;

void main() {
    gl_Position = viewProjection * model * vec4(position, 1.0);
    Normal = transpose(inverse(mat3(model))) * normal;
    TexCoord = uv;
    Layer = layer;
}
