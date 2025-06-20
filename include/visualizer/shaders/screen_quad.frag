#version 430 core

in vec2 TexCoord;
uniform sampler2D screenTexture;
out vec4 FragColor;

void main()
{
    vec4 texColor = texture(screenTexture, vec2(TexCoord.x, 1.0 - TexCoord.y));

    // If the texture has alpha, use it for blending
    // This allows the grid to show through where there are no splats
    FragColor = texColor;

    // Optional: You can also modify the alpha to ensure some transparency
    // FragColor.a = min(texColor.a, 0.95); // Slight transparency
}