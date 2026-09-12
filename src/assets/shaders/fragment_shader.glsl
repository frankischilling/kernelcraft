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
flat in float Material;

uniform vec3 lightDirection; // World-space direction toward the sun or moon
uniform vec3 lightColor;     // Linear diffuse intensity
uniform vec3 skyColor;       // Linear upper-hemisphere fill
uniform vec3 groundColor;    // Linear lower-hemisphere fill
uniform sampler2DArray texture1; // One independent repeating tile per layer
uniform bool drawGrid;
uniform uint worldSeed;
uniform float blockSize;
uniform sampler2DShadow terrainShadow;
uniform sampler2DShadow nextTerrainShadow;
uniform mat4 shadowTransform;
uniform mat4 nextShadowTransform;
uniform float shadowBlend;
uniform vec2 shadowScale; // Texel width and reciprocal world-space depth span.

float filteredVisibility(sampler2DShadow depths, mat4 transform, vec3 normal) {
    // Offset the receiver slightly, then compare each filter tap against its
    // own point on the receiver plane. This avoids dark stripes on sloped
    // light projections without a large bias that detaches cast shadows.
    vec3 position = (transform * vec4(FragPos + normal * 0.025 * blockSize, 1.0)).xyz * 0.5 + 0.5;
    if (any(lessThan(position, vec3(0.0))) || any(greaterThan(position, vec3(1.0))))
        return 1.0;
    vec3 lightNormal = mat3(transform) * normal;
    vec2 gradient = -lightNormal.xy / min(lightNormal.z, -0.000001);
    position.z -= 0.01 * blockSize * shadowScale.y;
    vec2 texel = position.xy / shadowScale.x - 0.5;
    vec2 base = floor(texel), fraction = fract(texel);
    float visibility = 0.0;
    for (int y = 0; y < 2; y++)
        for (int x = 0; x < 2; x++) {
            vec2 uv = (base + vec2(x,y) + 0.5) * shadowScale.x;
            vec2 weight = mix(1.0 - fraction, fraction, vec2(x,y));
            visibility += weight.x * weight.y * texture(depths, vec3(uv, position.z + dot(gradient, uv - position.xy)));
        }
    // Compare at each actual depth texel's receiver-plane position, then
    // interpolate visibility. Hardware bilinear comparison uses one reference
    // for all four texels, which made sloped receivers shadow themselves.
    return visibility;
}

float sunlightVisibility(vec3 normal) {
    float previous = filteredVisibility(terrainShadow, shadowTransform, normal);
    float next = filteredVisibility(nextTerrainShadow, nextShadowTransform, normal);
    return mix(previous, next, shadowBlend);
}

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

void main() {
    if (drawGrid) {
        FragColor = vec4(0.3, 0.3, 0.3, 1.0);
        return;
    }
    vec3 norm = normalize(Normal);
    vec3 ambient = mix(groundColor, skyColor, norm.y * 0.5 + 0.5);
    float cosine = max(dot(norm, normalize(lightDirection)), 0.0);
    vec3 diffuse = cosine * lightColor;
    if (cosine > 0.0)
        diffuse *= sunlightVisibility(norm);
    // RGBA8 tiles contain sRGB colors. Shade in linear light, then encode for
    // the existing display framebuffer. HUD and selection keep their own path;
    // GL_FRAMEBUFFER_SRGB stays disabled so output is encoded exactly once.
    vec3 albedo = srgbToLinear(texture(texture1, vec3(TexCoord, terrainLayer(norm))).rgb);
    FragColor = vec4(linearToSrgb(albedo * (ambient + diffuse)), 1.0);
}
