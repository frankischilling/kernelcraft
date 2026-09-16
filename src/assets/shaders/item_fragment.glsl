#version 330 core

in vec3 Normal;
in vec2 TexCoord;
flat in float Layer;
uniform sampler2DArray materials;
uniform vec3 itemColor;
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform vec3 skyColor;
uniform vec3 groundColor;
out vec4 FragColor;

vec3 toLinear(vec3 color) {
    return mix(pow((color + 0.055) / 1.055, vec3(2.4)), color / 12.92, lessThanEqual(color, vec3(0.04045)));
}

vec3 toSrgb(vec3 color) {
    return mix(1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055, 12.92 * color, lessThanEqual(color, vec3(0.0031308)));
}

void main() {
    vec3 albedo = Layer < 0.0 ? itemColor : texture(materials, vec3(TexCoord, Layer)).rgb;
    vec3 n = normalize(Normal);
    vec3 ambient = mix(groundColor, skyColor, n.y * 0.5 + 0.5);
    vec3 direction = lightDirection / max(length(lightDirection), 0.000001);
    vec3 diffuse = max(dot(n, direction), 0.0) * lightColor;
    FragColor = vec4(toSrgb(toLinear(albedo) * max(ambient + diffuse, vec3(0.0))), 1.0);
}
