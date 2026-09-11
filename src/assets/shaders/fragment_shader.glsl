/**
 * @file graphics/fragment_shader.glsl
 * @brief Matte terrain lighting with directional diffuse and hemispheric fill.
 * @author frankischilling
 * @date 2024-11-20
 */
#version 330 core
#extension GL_ARB_separate_shader_objects : enable

out vec4 FragColor;

in vec3 FragPos;  // Fragment position in world space
in vec3 Normal;   // Surface normal at fragment
in vec2 TexCoord; // Texture coordinates
in vec4 ShadowCoord; // Position in the directional shadow map
flat in float Material;

uniform vec3 lightDirection; // World-space direction toward the sun or moon
uniform vec3 lightColor;     // Linear diffuse intensity
uniform vec3 skyColor;       // Linear upper-hemisphere fill
uniform vec3 groundColor;    // Linear lower-hemisphere fill
uniform sampler2DArray texture1; // One independent repeating tile per layer
uniform sampler2D shadowMap;
uniform float shadowMapTexelSize;
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

vec3 srgbToLinear(vec3 color) {
    return mix(pow((color + 0.055) / 1.055, vec3(2.4)), color / 12.92,
               lessThanEqual(color, vec3(0.04045)));
}

vec3 linearToSrgb(vec3 color) {
    return mix(1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055, 12.92 * color,
               lessThanEqual(color, vec3(0.0031308)));
}

float directionalVisibility(vec3 norm) {
    vec3 projected = ShadowCoord.xyz / ShadowCoord.w;
    projected = projected * 0.5 + 0.5;
    if (projected.x <= 0.0 || projected.x >= 1.0 || projected.y <= 0.0 || projected.y >= 1.0 || projected.z <= 0.0 || projected.z >= 1.0)
        return 1.0;
    float slope = 1.0 - max(dot(norm, normalize(lightDirection)), 0.0);
    float bias = max(0.0015, 0.006 * slope);
    float visibility = 0.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            float depth = texture(shadowMap, projected.xy + vec2(x, y) * shadowMapTexelSize).r;
            visibility += projected.z - bias <= depth ? 1.0 : 0.0;
        }
    return visibility / 9.0;
}

void main() {
    if (drawGrid) {
        FragColor = vec4(0.3, 0.3, 0.3, 1.0);
        return;
    }
    vec3 norm = normalize(Normal);
    vec3 ambient = mix(groundColor, skyColor, norm.y * 0.5 + 0.5);
    vec3 diffuse = max(dot(norm, normalize(lightDirection)), 0.0) * lightColor * directionalVisibility(norm);
    // RGBA8 tiles contain sRGB colors. Shade in linear light, then encode for
    // the existing display framebuffer. HUD and selection keep their own path;
    // GL_FRAMEBUFFER_SRGB stays disabled so output is encoded exactly once.
    vec3 albedo = srgbToLinear(texture(texture1, vec3(TexCoord, terrainLayer(norm))).rgb);
    FragColor = vec4(linearToSrgb(albedo * (ambient + diffuse)), 1.0);
}
