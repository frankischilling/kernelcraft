/**
 * @file graphics/fragment_shader.glsl
 * @brief Fragment shader implementing Phong lighting model with ambient, diffuse and specular components
 * @author frankischilling
 * @date 2024-11-20
 */
#version 330 core
#extension GL_ARB_separate_shader_objects : enable

out vec4 FragColor;

in vec3 FragPos;  // Fragment position in world space
in vec3 Normal;   // Surface normal at fragment
in vec2 TexCoord; // Texture coordinates
flat in float Material;

uniform vec3 lightPos;    // Position of the light source
uniform vec3 viewPos;     // Camera position for specular calculation
uniform vec3 lightColor;  // Color of the light source
uniform sampler2DArray texture1; // One independent repeating tile per layer
uniform bool drawGrid;
uniform uint worldSeed;
uniform float blockSize;

float terrainLayer(vec3 normal) {
    if (Material == 0.0 || Material >= 7.0)
        return Material; // Stone and building materials have no alternate tile.
    // Move just inside the face to identify its owning voxel on either sign
    // of each axis. World coordinates keep variants stable across merged quads,
    // chunk seams, edits, and saved-world reloads.
    uvec3 block = uvec3(ivec3(floor(FragPos / blockSize - normal * 0.001)));
    uint h = worldSeed ^ (block.x * 0x8da6b343u) ^ (block.y * 0xd8163841u) ^
             (block.z * 0xcb1ab31fu) ^ (uint(Material) * 0x9e3779b9u);
    h = (h ^ (h >> 16u)) * 0x7feb352du;
    h = (h ^ (h >> 15u)) * 0x846ca68bu;
    h ^= h >> 16u;
    uint roll = h % 100u;
    // Layers match world_renderer.c: rocky dirt 25%, leafy tops 10%, bugs 2%.
    if (Material == 1.0 && roll < 25u) return 4.0;
    if (Material == 2.0 && roll < 10u) return 5.0;
    if (Material == 3.0 && roll < 2u) return 6.0;
    return Material;
}

void main() {
    if (drawGrid) {
        FragColor = vec4(0.3, 0.3, 0.3, 1.0);
        return;
    }
    // Calculate ambient lighting component
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * lightColor;

    // Calculate diffuse lighting component
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Calculate specular lighting component
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    // Combine all lighting components and apply texture color
    vec3 result = (ambient + diffuse + specular) * texture(texture1, vec3(TexCoord, terrainLayer(norm))).rgb;
    FragColor = vec4(result, 1.0);
}
