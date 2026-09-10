#version 330 core
in vec2 screenPosition;
out vec4 FragColor;
uniform vec3 cameraFront, cameraRight, cameraUp;
uniform vec2 viewScale;
uniform vec3 weights, sunDirection, moonDirection;
uniform float starBrightness;
uniform sampler2D dayPalette, twilightPalette, nightPalette, sunImage, moonImage;

uint hashCell(uvec2 cell) {
    uint h = cell.x * 0x8da6b343u ^ cell.y * 0xd8163841u;
    h = (h ^ (h >> 16u)) * 0x7feb352du;
    return h ^ (h >> 15u);
}

vec3 palette(sampler2D image, float position) {
    // Sample one center from each supplied swatch, then interpolate between
    // those colors instead of stretching hard-edged stripes over the sky.
    const float rows[5] = float[5](8.0, 28.0, 44.0, 52.0, 60.0);
    float band = clamp(position, 0.0, 1.0) * 4.0;
    int low = min(int(floor(band)), 3);
    vec3 a = texture(image, vec2(0.5, (rows[low] + 0.5) / 64.0)).rgb;
    vec3 b = texture(image, vec2(0.5, (rows[low + 1] + 0.5) / 64.0)).rgb;
    return mix(a, b, smoothstep(0.0, 1.0, band - float(low)));
}

vec3 body(vec3 color, vec3 ray, vec3 direction, sampler2D picture, vec3 glowColor, float pixelStrength, float mieStrength) {
    // A world-oriented square subtending about nine degrees. The fixed Z axis
    // stays perpendicular to the east/west orbit, including at the zenith.
    vec3 right = vec3(0.0, 0.0, 1.0);
    vec3 up = cross(direction, right);
    float facing = dot(ray, direction);
    if (facing <= 0.0 || ray.y <= 0.0)
        return color;
    vec2 offset = vec2(dot(ray, right), -dot(ray, up)) / facing;
    float horizonFade = smoothstep(0.0, 0.025, ray.y);
    // A coarse, world-oriented mask supplies the stepped square glow of a
    // nearest-sampled sprite. It remains fixed to the body, not screen pixels.
    vec2 cell = (floor(offset / 0.02) + 0.5) * 0.02;
    // A fourth-power radius rounds the corners in pixel steps instead of
    // producing a stack of large, uniformly opaque rectangular borders.
    float squareRadius = sqrt(length(cell * cell));
    float pixelHalo = 1.0 - smoothstep(0.075, 0.23, squareRadius);
    pixelHalo *= pixelHalo;

    // Peak-normalized Henyey-Greenstein forward lobe approximates Mie glare.
    // Both ray and direction point toward the sky, so forward is dot = +1.
    const float g = 0.8;
    float mie = pow((1.0 - g) * (1.0 - g) / (1.0 + g * g - 2.0 * g * facing), 1.5);
    float atmosphere = mix(1.5, 1.0, smoothstep(0.0, 0.5, direction.y));
    float extent = smoothstep(cos(radians(35.0)), cos(radians(20.0)), facing);
    // A soft bloom skirt follows the square emitter. This is celestial glow
    // inside the sky pass, not a full-scene HDR postprocessing pipeline.
    vec2 outside = max(abs(offset) - 0.08, vec2(0.0));
    float bloom = exp(-dot(outside, outside) / 0.008);
    float visibility = horizonFade * (1.0 - smoothstep(0.0, 0.08, -direction.y));
    color = mix(color, glowColor, (mie * mieStrength * atmosphere + bloom * 0.12 * pixelStrength) * extent * visibility);
    color = mix(color, glowColor, pixelHalo * pixelStrength * visibility);
    vec2 uv = offset / 0.16 + 0.5;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))))
        return color;
    vec4 pixel = texture(picture, uv);
    return mix(color, pixel.rgb, pixel.a * horizonFade);
}

void main() {
    vec3 ray = normalize(cameraFront + screenPosition.x * viewScale.x * cameraRight + screenPosition.y * viewScale.y * cameraUp);
    // Lower the gradient's bottom anchor by six degrees, keeping the zenith
    // and the physical horizon used by celestial bodies and stars in place.
    float angle = asin(clamp(ray.y, -1.0, 1.0));
    float elevation = clamp((angle + radians(6.0)) / radians(96.0), 0.0, 1.0);
    // Day's pale cyan bands belong near the horizon. Reach the strongest
    // supplied blue at 24 degrees and keep it across the rest of the sky.
    float dayElevation = clamp((angle + radians(6.0)) / radians(30.0), 0.0, 1.0);
    // Blue daylight and dark purple night sit overhead. Sunrise/sunset's
    // strongest orange sits just below the horizon, below its paler pinks.
    vec3 color = palette(dayPalette, dayElevation) * weights.x
               + palette(twilightPalette, elevation) * weights.y
               + palette(nightPalette, 1.0 - elevation) * weights.z;
    if (starBrightness > 0.0 && ray.y > 0.0) {
        // Fixed spherical cells form a repeatable decorative field. Equal-area
        // latitude coordinates avoid crowding at the poles. A later milestone
        // will replace this with astronomical positions and constellations.
        vec2 grid = vec2(atan(ray.z, ray.x) / 6.28318530718 + 0.5, ray.y) * vec2(360.0, 100.0);
        uvec2 cell = uvec2(floor(grid));
        cell.x %= 360u;
        uint h = hashCell(cell);
        vec2 center = vec2(0.3) + vec2(float((h >> 8u) & 255u), float((h >> 16u) & 255u)) / 255.0 * 0.4;
        // Longitude wraps at west. Differentiate its shortest angular distance
        // so a quad crossing 360 -> 0 cannot stretch a star along that seam.
        vec2 angularStep = abs(vec2(dFdx(grid.x), dFdy(grid.x)));
        angularStep = min(angularStep, 360.0 - angularStep);
        vec2 footprint = vec2(angularStep.x + angularStep.y, fwidth(grid.y));
        vec2 offset = (fract(grid) - center) / max(footprint, vec2(0.0001));
        float star = (h % 100u < 2u) ? 1.0 - smoothstep(0.4, 1.2, length(offset)) : 0.0;
        color = mix(color, vec3(1.0, 0.97, 0.91), star * starBrightness * smoothstep(0.0, 0.15, ray.y));
    }
    color = body(color, ray, sunDirection, sunImage, vec3(1.0, 0.84, 0.42), 0.85, 0.18);
    color = body(color, ray, moonDirection, moonImage, vec3(1.0), 0.45, 0.10);
    FragColor = vec4(color, 1.0);
}
