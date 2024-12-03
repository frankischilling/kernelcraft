#version 330 core
#extension GL_ARB_separate_shader_objects : enable

out vec4 FragColor;

in vec3 FragPos;  // Fragment position in world space
in vec3 Normal;   // Surface normal at fragment
in vec2 TexCoord; // Texture coordinates

uniform vec3 lightPos;    // Position of the light source
uniform vec3 viewPos;     // Camera position for specular calculation
uniform vec3 lightColor;  // Color of the light source
uniform sampler2D texture1; // Texture sampler

void main() {
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
    vec3 result = (ambient + diffuse + specular) * texture(texture1, TexCoord).rgb;
    FragColor = vec4(result, 1.0);
}
