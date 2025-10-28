#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aFacetCenter;

flat out vec3 Normal;
flat out vec3 FacetCenter;
out vec3 WorldPos;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    vec4 worldPosition = model * vec4(aPos, 1.0);
    WorldPos = vec3(view * worldPosition);
    Normal = aNormal;
    FacetCenter = aFacetCenter;
    gl_Position = projection * view * worldPosition;
}
