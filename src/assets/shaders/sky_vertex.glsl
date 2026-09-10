#version 330 core
out vec2 screenPosition;
void main() {
    vec2 position = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    screenPosition = position * 2.0 - 1.0;
    gl_Position = vec4(screenPosition, 0.0, 1.0);
}
