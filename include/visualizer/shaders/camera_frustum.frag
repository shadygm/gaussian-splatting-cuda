#version 430 core

in vec3 fragColor;
in vec3 worldPos;
in vec3 cameraCenter;

out vec4 FragColor;

uniform vec3 viewPos;
uniform bool enableShading = true;

void main() {
    vec3 color = fragColor;

    if (enableShading) {
        // Simple distance-based fading for depth perception
        float distanceFromCamera = length(cameraCenter - viewPos);
        float fogFactor = 1.0 - smoothstep(5.0, 50.0, distanceFromCamera);

        // Add slight gradient from apex to base
        float height = length(worldPos - cameraCenter);
        float gradientFactor = 1.0 - height * 0.2;

        color = color * gradientFactor * (0.3 + 0.7 * fogFactor);
    }

    // Increase line thickness perception with slight glow
    float alpha = 1.0;

    FragColor = vec4(color, alpha);
}