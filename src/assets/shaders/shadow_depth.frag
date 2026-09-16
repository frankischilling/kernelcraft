#version 330 core
in vec2 TexCoord;
flat in float Material;
uniform sampler2DArray materials;
uniform bool cutoutEnabled;
void main() {
    if (cutoutEnabled && Material == 12.0 && texture(materials, vec3(TexCoord, Material)).a < 0.5)
        discard;
}
