/**
 * @file graphics/fragment_shader.glsl
 * @brief Fragment shader implementing Phong lighting model with ambient, diffuse and specular components
 * @author frankischilling
 * @date 2024-11-20
 */
#version 330 core
#extension GL_ARB_separate_shader_objects : enable

out vec4 FragColor;

in vec3 FragPos;  // Fragment position in world space
in vec3 Normal;   // Surface normal at fragment
in vec2 TexCoord; // Texture coordinates

uniform vec3 lightPos;    // Position of the light source
uniform vec3 viewPos;     // Camera position for specular calculation
uniform vec3 lightColor;  // Color of the light source

uniform sampler2D grassTopTexture;
uniform sampler2D grassSideTexture;
uniform sampler2D dirtTexture;
uniform sampler2D stoneTexture;

void main() {
    // Calculate ambient, diffuse, and specular lighting components
    float ambientStrength = 0.4;
    vec3 ambient = ambientStrength * lightColor;

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    // Determine which texture to use based on the normal vector
    vec4 textureColor;
    if (norm.y > 0.9) {
        // Top face
        textureColor = texture(grassTopTexture, TexCoord);
    } else if (norm.y < -0.9) {
        // Bottom face
        textureColor = texture(dirtTexture, TexCoord);
    } else {
        // Side faces
        textureColor = texture(grassSideTexture, TexCoord);
    }

    // Combine all lighting components and apply texture color
    vec3 result = (ambient + diffuse + specular) * textureColor.rgb;
    FragColor = vec4(result, 1.0);
}
