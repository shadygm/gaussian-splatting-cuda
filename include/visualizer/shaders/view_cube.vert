#version 430 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in int aFaceId;

uniform mat4 mvpMatrix;
uniform mat4 viewMatrix;
uniform mat4 modelMatrix;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
flat out int FaceId;

void main() {
    FragPos = vec3(modelMatrix * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(modelMatrix))) * aNormal;
    TexCoord = aTexCoord;
    FaceId = aFaceId;
    
    gl_Position = mvpMatrix * vec4(aPos, 1.0);
}
