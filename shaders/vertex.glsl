#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aFacetCenter;

flat out vec3 NormalVS;        // Transform to view space here
flat out vec3 FaceCenterVS;    // Transform to view space here

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    vec4 worldPosition = model * vec4(aPos, 1.0);
    
    // Transform normal to view space (do expensive operation once per vertex, not per triangle)
    vec3 normalWorld = mat3(transpose(inverse(model))) * aNormal;
    NormalVS = normalize((view * vec4(normalWorld, 0.0)).xyz);
    
    // Transform facet center to view space
    FaceCenterVS = vec3(view * model * vec4(aFacetCenter, 1.0));
    
    gl_Position = projection * view * worldPosition;
}
