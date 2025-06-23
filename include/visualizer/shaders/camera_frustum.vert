#version 430 core

layout(location = 0) in vec3 aPos;

// Per-instance data
layout(location = 1) in mat4 cameraToWorld;
layout(location = 5) in vec3 instanceColor;
layout(location = 6) in vec3 cameraParams; // fov_x, fov_y, aspect

uniform mat4 viewProj;
uniform float frustumScale;
uniform int currentIndex;
uniform int highlightIndex;
uniform vec3 highlightColor;

out vec3 fragColor;

void main() {
    vec3 pos = aPos;

    // Scale frustum based on FOV if not at apex
    if (aPos.z > 0.0) {
        pos.x *= tan(cameraParams.x * 0.5) * frustumScale;
        pos.y *= tan(cameraParams.y * 0.5) * frustumScale;
        pos.z *= frustumScale;
    }

    // Transform from camera space to world space
    vec4 worldPos = cameraToWorld * vec4(pos, 1.0);
    gl_Position = viewProj * worldPos;

    // Set color - check both current index and highlight index
    if (currentIndex == highlightIndex && highlightIndex >= 0) {
        fragColor = highlightColor;
    } else {
        fragColor = instanceColor;
    }
}