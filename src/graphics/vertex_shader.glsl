/**
 * @file vertex_shader.glsl
 * @brief Vertex shader for rendering a cube.
 * @author frankischilling
 * @date 2024-11-19
 */
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}