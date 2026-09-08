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

uniform mat4 viewProjection;

out vec3 FragPos; // Fragment position in world space
out vec3 Normal; // Surface normal at fragment
out vec2 TexCoord; // Texture coordinates

void main() {
    FragPos = aPos; // Chunk vertices and normals are stored in world space.
    Normal = aNormal;
    TexCoord = aTexCoord; // Pass texture coordinates to fragment shader
    gl_Position = viewProjection * vec4(aPos, 1.0);
}
