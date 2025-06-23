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
out vec3 worldPos;
out vec3 cameraCenter;

void main() {
    vec3 pos = aPos;

    // Scale frustum based on FOV only for non-apex vertices
    // The apex is at position (0,0,0) in our geometry
    if (length(aPos) > 0.01) {  // Not the apex
        pos.x *= tan(cameraParams.x * 0.5) * frustumScale;
        pos.y *= tan(cameraParams.y * 0.5) * frustumScale;
        pos.z *= frustumScale;
    }

    // Transform from camera space to world space
    vec4 worldPosition = cameraToWorld * vec4(pos, 1.0);
    worldPos = worldPosition.xyz;
    cameraCenter = cameraToWorld[3].xyz;

    gl_Position = viewProj * worldPosition;

    // Set color - check both current index and highlight index
    if (currentIndex == highlightIndex && highlightIndex >= 0) {
        fragColor = highlightColor;
    } else {
        fragColor = instanceColor;
    }
}