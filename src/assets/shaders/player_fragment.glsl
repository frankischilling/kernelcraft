#version 330 core

in vec3 Normal;
in vec2 TexCoord;

uniform sampler2D skin;
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform vec3 skyColor;
uniform vec3 groundColor;
uniform bool outerLayer;
uniform int outerPass;
uniform bool viewModel;
uniform bool solidColorEnabled;
uniform vec4 solidColor;

out vec4 FragColor;

vec3 srgbToLinear(vec3 color) {
    return mix(pow((color + 0.055) / 1.055, vec3(2.4)), color / 12.92,
               lessThanEqual(color, vec3(0.04045)));
}

vec3 linearToSrgb(vec3 color) {
    color = max(color, vec3(0.0));
    return mix(1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055, 12.92 * color,
               lessThanEqual(color, vec3(0.0031308)));
}

void main() {
    vec4 texel = solidColorEnabled ? solidColor : texture(skin, TexCoord);
    if (texel.a <= 0.001)
        discard;
    if (!solidColorEnabled && outerLayer && outerPass == 1 && texel.a < 0.999)
        discard;
    if (!solidColorEnabled && outerLayer && outerPass == 2 && texel.a >= 0.999)
        discard;

    vec3 normal = normalize(Normal);
    vec3 direction = viewModel ? normalize(vec3(-0.35, 0.80, 0.45)) : normalize(lightDirection);
    vec3 ambient = mix(groundColor, skyColor, normal.y * 0.5 + 0.5);
    vec3 diffuse = max(dot(normal, direction), 0.0) * lightColor;
    vec3 shaded = srgbToLinear(texel.rgb) * (ambient + diffuse);
    FragColor = vec4(linearToSrgb(shaded), texel.a);
}
