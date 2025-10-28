#version 330 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in vec3 WorldPos[];
flat in vec3 Normal[];
flat in vec3 FacetCenter[];

uniform mat4 view;
uniform mat4 model;

flat out vec3 NormalVS;
flat out vec3 FaceCenterVS;

void main() {
    // Transform facet normal to view space
    vec3 normalWorld = mat3(transpose(inverse(model))) * Normal[0];
    NormalVS = normalize((view * vec4(normalWorld, 0.0)).xyz);
    
    // Transform facet center to view space
    FaceCenterVS = vec3(view * model * vec4(FacetCenter[0], 1.0));
    
    // Emit triangle
    for (int i = 0; i < 3; i++) {
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
    }
    EndPrimitive();
}
