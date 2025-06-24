#version 330 core

// Inputs from vertex shader
in vec3 FragPos;
in vec3 Normal;
in vec4 vertexColor;

// Output
out vec4 FragColor;

// Uniforms
uniform vec3 viewPos;
uniform bool enableShading;
uniform int highlightIndex;
uniform vec3 highlightColor;

void main() {
    vec4 finalColor = vertexColor;

    if (enableShading) {
        // Simple lighting
        vec3 lightPos = viewPos + vec3(10.0, 10.0, 10.0);
        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);

        // Ambient
        float ambientStrength = 0.3;
        vec3 ambient = ambientStrength * vec3(1.0);

        // Diffuse
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * vec3(1.0);

        // Specular
        float specularStrength = 0.5;
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
        vec3 specular = specularStrength * spec * vec3(1.0);

        // Combine lighting
        vec3 result = (ambient + diffuse + specular) * vertexColor.rgb;
        finalColor = vec4(result, vertexColor.a);
    } else {
        // Wireframe mode - much darker for better contrast
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);  // Black wireframe
    }

    FragColor = finalColor;
}