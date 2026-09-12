#version 330 core
in vec2 screenPosition;
out vec4 fragmentColor;
uniform vec3 cameraFront, cameraRight, cameraUp, cloudOrigin, weights;
uniform vec2 viewScale, depthProjection;
uniform uvec2 cloudRows[64];

// The original correlated shape is precomputed once. Wrap before indexing so
// negative world coordinates and drifting cells use the same repeating field.
bool occupied(ivec2 cell) {
    uvec2 p = uvec2(cell) & uvec2(63u);
    uint word = p.x < 32u ? cloudRows[p.y].x : cloudRows[p.y].y;
    return (word & (1u << (p.x & 31u))) != 0u;
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

    vec3 tint = vec3(1.0) * weights.x + vec3(0.95, 0.69, 0.60) * weights.y
               + vec3(0.22, 0.25, 0.36) * weights.z;
    vec3 radiance = vec3(0);
    float transmission = 1.0, firstHit = -1.0, shade = 1.0;
    bool inside = false;
    // Beer-Lambert extinction per block: a four-block path is 92% opaque.
    const float extinction = 0.631432;
    // At most 92 cell crossings within the distance limit, including diagonal rays.
    for (int i = 0; i < 96; ++i) {
        float crossing = min(next.x, next.y);
        float length = max(0.0, min(crossing, finish) - start);
        bool filled = occupied(cell);
        if (filled && length > 0.0) {
            if (firstHit < 0.0) firstHit = start;
            // Adjacent occupied cells belong to one volume; internal cell faces
            // must not introduce darker bands. Air gaps expose a new entry face.
            if (!inside)
                shade = normal.y > 0.5 ? 1.0 : normal.y < -0.5 ? 0.82 : abs(normal.x) > 0.5 ? 0.72 : 0.77;
            float alpha = 1.0 - exp(-extinction * length);
            radiance += transmission * alpha * tint * shade;
            transmission *= 1.0 - alpha;
        }
        inside = filled;
        if (crossing >= finish || transmission < 0.001) break;
        // Advance both axes on exact corners, avoiding a zero-width side hit.
        bool crossX = next.x <= next.y;
        bool crossZ = next.y <= next.x;
        if (crossX) { cell.x += stepCell.x; next.x += delta.x; }
        if (crossZ) { cell.y += stepCell.y; next.y += delta.y; }
        normal = crossX ? vec3(-stepCell.x, 0, 0) : vec3(0, 0, -stepCell.y);
        start = crossing;
    }
    if (firstHit < 0.0 || transmission >= 1.0) discard;
    float viewDepth = firstHit * dot(ray, cameraFront);
    if (viewDepth <= 0.1) discard;
    gl_FragDepth = 0.5 * (-depthProjection.x + depthProjection.y / viewDepth) + 0.5;
    float alpha = 1.0 - transmission;
    // The renderer uses straight-alpha blending. Convert the accumulated color
    // once, then retain the layer's distance fade without tinting the background.
    fragmentColor = vec4(radiance / alpha, alpha * (1.0 - smoothstep(480.0, 768.0, firstHit)));
}
