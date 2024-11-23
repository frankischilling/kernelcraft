/**
 * @file graphics/fragment_shader.glsl
 * @brief Fragment shader implementing Phong lighting model with ambient, diffuse and specular components
 * @author frankischilling
 * @date 2024-11-20
 */
#version 330 core
out vec4 FragColor;

in vec3 FragPos;  // Fragment position in world space
in vec3 Normal;   // Surface normal at fragment

// Light and material properties
uniform vec3 lightPos;    // Position of the light source
uniform vec3 viewPos;     // Camera position for specular calculation
uniform vec3 lightColor;  // Color of the light source
uniform vec3 objectColor; // Base color of the object

void main() {
    // Calculate ambient lighting component
    // Provides basic illumination to areas not directly lit
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * lightColor;

    // Calculate diffuse lighting component
    // Varies based on angle between surface normal and light direction
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);  // Clamp negative values to zero
    vec3 diffuse = diff * lightColor;

    // Calculate specular lighting component
    // Creates highlights based on viewer position and light reflection
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);  // 32 is the shininess factor
    vec3 specular = specularStrength * spec * lightColor;

    // Combine all lighting components and apply object color
    vec3 result = (ambient + diffuse + specular) * objectColor;
    FragColor = vec4(result, 1.0);  // Final color with full opacity
}