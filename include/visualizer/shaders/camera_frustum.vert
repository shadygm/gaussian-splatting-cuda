#version 330 core

// Vertex attributes
layout(location = 0) in vec3 aPos;

// Instance attributes
layout(location = 1) in mat4 aInstanceMatrix;
layout(location = 5) in vec3 aInstanceColor;
layout(location = 6) in vec3 aFovAspect; // fov_x, fov_y, aspect

// Uniforms
uniform mat4 viewProj;
uniform float frustumScale;
uniform vec3 viewPos;

// Outputs to fragment shader
out vec3 FragPos;
out vec3 Normal;
out vec4 vertexColor;

void main() {
    // Apply instance transformation
    vec4 worldPos = aInstanceMatrix * vec4(aPos, 1.0);

    // Calculate normal (simplified - assuming uniform scaling)
    Normal = mat3(aInstanceMatrix) * vec3(0.0, 0.0, 1.0); // Frustums face forward

    // Pass through color
    vertexColor = vec4(aInstanceColor, 1.0);

    // World position for lighting
    FragPos = vec3(worldPos);

    // Final position
    gl_Position = viewProj * worldPos;
}