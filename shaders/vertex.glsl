#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;
out vec3 WorldPos;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    vec4 worldPosition = model * vec4(aPos, 1.0);
    FragPos = vec3(worldPosition);
    WorldPos = vec3(view * worldPosition);
    Normal = mat3(transpose(inverse(model))) * aNormal;
    
    gl_Position = projection * view * worldPosition;
}
