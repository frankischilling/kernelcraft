/**
 * @file graphics/vertex_shader.glsl
 * @brief Vertex shader implementing basic 3D transformations and normal calculations
 *        for Phong lighting model. Transforms vertices from model space to clip space
 *        and prepares lighting calculations for the fragment shader.
 * @author frankischilling
 * @date 2024-11-19
 */
#version 330 core
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 aPos; // Vertex position
layout (location = 1) in vec3 aNormal; // Vertex normal
layout (location = 2) in vec2 aTexCoord; // Texture coordinates

uniform mat4 model; // Model matrix
uniform mat4 view; // View matrix
uniform mat4 projection; // Projection matrix

out vec3 FragPos; // Fragment position in world space
out vec3 Normal; // Surface normal at fragment
out vec2 TexCoord; // Texture coordinates

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0)); // Transform vertex position to world space
    Normal = mat3(transpose(inverse(model))) * aNormal; // Transform normal to world space
    TexCoord = aTexCoord; // Pass texture coordinates to fragment shader
    gl_Position = projection * view * model * vec4(aPos, 1.0); // Transform vertex position to clip space
}
