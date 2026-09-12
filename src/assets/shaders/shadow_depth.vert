#version 330 core
layout(location = 0) in vec3 position;
uniform mat4 shadowTransform;
void main() {
    gl_Position = shadowTransform * vec4(position, 1.0);
}
