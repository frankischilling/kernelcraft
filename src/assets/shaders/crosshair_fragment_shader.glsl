/**
 * @file crosshair_fragment_shader.glsl
 * @brief Fragment shader for rendering the crosshair in the HUD
 * @author frankischilling
 * @date 2024-12-03
 */
#version 330 core

out vec4 FragColor;

uniform vec3 objectColor;

void main() {
    FragColor = vec4(objectColor, 1.0);
}
