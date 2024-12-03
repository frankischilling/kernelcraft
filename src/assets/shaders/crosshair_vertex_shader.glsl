/**
 * @file crosshair_vertex_shader.glsl
 * @brief Vertex shader for rendering the crosshair in the HUD
 * @author frankischilling
 * @date 2024-12-03
 */
#version 330 core

layout (location = 0) in vec2 aPos;

uniform mat4 projection;

void main() {
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
}
