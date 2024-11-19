/**
 * @file shader.c
 * @brief Shader module for loading and compiling shaders.
 * @author frankischilling
 * @date 2024-11-19
 */
#include <stdio.h>
#include <stdlib.h>
#include <GL/glew.h>
#include "shader.h"

// Function to read shader code from a file
static char* readShaderFile(const char* filePath) {
    FILE* file = fopen(filePath, "r");
    if (!file) {
        fprintf(stderr, "Failed to open shader file: %s\n", filePath);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = (char*)malloc(length + 1);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate memory for shader file: %s\n", filePath);
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);

    return buffer;
}

// Function to compile a shader
static GLuint compileShader(const char* code, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &code, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compilation failed: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

// Function to load and link shaders
GLuint loadShaders(const char* vertexPath, const char* fragmentPath) {
    // Read vertex shader
    FILE* vertexFile = fopen(vertexPath, "r");
    if (!vertexFile) {
        fprintf(stderr, "Could not open vertex shader file\n");
        return 0;
    }
    
    // Get file size
    fseek(vertexFile, 0, SEEK_END);
    long vertexSize = ftell(vertexFile);
    rewind(vertexFile);
    
    char* vertexSource = (char*)malloc(vertexSize + 1);
    fread(vertexSource, 1, vertexSize, vertexFile);
    vertexSource[vertexSize] = '\0';
    fclose(vertexFile);
    
    // Similar process for fragment shader
    FILE* fragmentFile = fopen(fragmentPath, "r");
    if (!fragmentFile) {
        fprintf(stderr, "Could not open fragment shader file\n");
        free(vertexSource);
        return 0;
    }
    
    fseek(fragmentFile, 0, SEEK_END);
    long fragmentSize = ftell(fragmentFile);
    rewind(fragmentFile);
    
    char* fragmentSource = (char*)malloc(fragmentSize + 1);
    fread(fragmentSource, 1, fragmentSize, fragmentFile);
    fragmentSource[fragmentSize] = '\0';
    fclose(fragmentFile);
    
    // Create and compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, (const char**)&vertexSource, NULL);
    glCompileShader(vertexShader);
    
    // Create and compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, (const char**)&fragmentSource, NULL);
    glCompileShader(fragmentShader);
    
    // Create shader program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    
    // Clean up
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    free(vertexSource);
    free(fragmentSource);
    
    return shaderProgram;
}