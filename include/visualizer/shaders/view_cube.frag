#version 430 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
flat in int FaceId;

uniform vec3 faceColors[6];
uniform sampler2D faceTexture;
uniform mat4 viewMatrix;  // Add this declaration

out vec4 FragColor;

void main() {
    // Get base color for this face
    vec3 baseColor = faceColors[FaceId];

    // Simple lighting
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float diff = max(dot(normalize(Normal), lightDir), 0.0);
    vec3 diffuse = diff * baseColor;

    // Add some ambient
    vec3 ambient = 0.3 * baseColor;

    // Edge darkening for better visibility
    vec2 edgeDist = abs(TexCoord - 0.5) * 2.0;
    float edge = 1.0 - max(edgeDist.x, edgeDist.y);
    edge = smoothstep(0.8, 1.0, edge);

    vec3 color = ambient + diffuse;
    color *= (0.7 + 0.3 * edge);

    FragColor = vec4(color, 1.0);
}