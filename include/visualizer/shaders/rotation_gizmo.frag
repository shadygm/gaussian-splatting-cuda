#version 430 core

uniform vec4 color;
uniform bool isActive;

out vec4 FragColor;

void main() {
    FragColor = color;
    if (isActive) {
        FragColor.rgb *= 1.5; // Brighten when active
    }
}
