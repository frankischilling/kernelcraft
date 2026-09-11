#version 330 core
in vec2 screenPosition;
out vec4 fragmentColor;
uniform vec3 cameraFront, cameraRight, cameraUp, cloudOrigin, weights;
uniform vec2 viewScale, depthProjection;

// Correlated cells give connected, square-edged patches instead of isolated cubes.
// Sixteen coarse nodes, four cells per node, twelve blocks per cell: 768 blocks.
float node(ivec2 p) {
    uvec2 wrapped = uvec2(p) & uvec2(15u);
    uint h = wrapped.x * 374761393u + wrapped.y * 668265263u + 1447u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= h >> 16u;
    return float(h & 65535u) / 65535.0;
}

bool occupied(ivec2 cell) {
    vec2 p = (vec2(cell) + 0.5) / 4.0;
    ivec2 base = ivec2(floor(p));
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float value = mix(mix(node(base), node(base + ivec2(1, 0)), f.x),
                      mix(node(base + ivec2(0, 1)), node(base + ivec2(1, 1)), f.x), f.y);
    return value > 0.50;
}

void main() {
    vec3 ray = normalize(cameraFront + cameraRight * screenPosition.x * viewScale.x
                                    + cameraUp * screenPosition.y * viewScale.y);
    // Start just beyond the near plane, including when flying inside a cloud.
    float start = 0.101 / dot(ray, cameraFront), finish = 768.0;
    vec3 normal = vec3(0, -1, 0);
    if (abs(ray.y) < 0.00001) {
        if (cloudOrigin.y < 120.0 || cloudOrigin.y > 124.0) discard;
    } else {
        float bottom = (120.0 - cloudOrigin.y) / ray.y;
        float top = (124.0 - cloudOrigin.y) / ray.y;
        start = max(start, min(bottom, top));
        finish = min(finish, max(bottom, top));
        normal = vec3(0, -sign(ray.y), 0);
    }
    if (start >= finish) discard;

    vec2 position = (cloudOrigin + ray * (start + 0.001)).xz / 12.0;
    ivec2 cell = ivec2(floor(position));
    ivec2 stepCell = ivec2(sign(ray.xz));
    vec2 delta = 12.0 / max(abs(ray.xz), vec2(0.000001));
    vec2 boundary = (vec2(cell) + step(vec2(0), ray.xz)) * 12.0;
    vec2 next = vec2(1e20);
    if (abs(ray.x) > 0.000001) next.x = (boundary.x - cloudOrigin.x) / ray.x;
    if (abs(ray.z) > 0.000001) next.y = (boundary.y - cloudOrigin.z) / ray.z;

    // At most 92 cell crossings within the distance limit, including diagonal rays.
    for (int i = 0; i < 96; ++i) {
        if (occupied(cell)) {
            float viewDepth = start * dot(ray, cameraFront);
            if (viewDepth <= 0.1) discard;
            gl_FragDepth = 0.5 * (-depthProjection.x + depthProjection.y / viewDepth) + 0.5;
            float shade = normal.y > 0.5 ? 1.0 : normal.y < -0.5 ? 0.82 : abs(normal.x) > 0.5 ? 0.72 : 0.77;
            vec3 tint = vec3(1.0) * weights.x + vec3(0.95, 0.69, 0.60) * weights.y
                       + vec3(0.22, 0.25, 0.36) * weights.z;
            float alpha = 0.92 * (1.0 - smoothstep(480.0, 768.0, start));
            fragmentColor = vec4(tint * shade, alpha);
            return;
        }
        // Advance both axes on exact corners, avoiding a zero-width side hit.
        float crossing = min(next.x, next.y);
        if (crossing >= finish) discard;
        bool crossX = next.x <= next.y;
        bool crossZ = next.y <= next.x;
        if (crossX) { cell.x += stepCell.x; next.x += delta.x; }
        if (crossZ) { cell.y += stepCell.y; next.y += delta.y; }
        normal = crossX ? vec3(-stepCell.x, 0, 0) : vec3(0, 0, -stepCell.y);
        start = crossing;
    }
    discard;
}
