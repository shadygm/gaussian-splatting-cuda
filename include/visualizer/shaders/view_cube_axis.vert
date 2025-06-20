#version 430 core

layout(location = 0) in vec3 aPos;

uniform mat4 mvpMatrix;

void main() {
    gl_Position = mvpMatrix * vec4(aPos * 0.8, 1.0);  // Scale down slightly
}
