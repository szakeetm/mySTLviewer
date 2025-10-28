#version 330 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

// Inputs from vertex shader
in vec3 FragPos[];    // world space (not used here)
in vec3 WorldPos[];   // view space positions
flat in vec3 Normal[]; // world-space facet normals passed per-vertex (identical for the triangle)

uniform mat4 view;

// Flat outputs to fragment shader
flat out vec3 FaceNormalVS;   // face normal in view space
flat out vec3 FaceCenterVS;   // face center in view space

void main() {
    // Use the provided facet normal, transform to view space
    vec3 nvs = normalize((view * vec4(Normal[0], 0.0)).xyz);
    // Center in view space
    vec3 centerVS = (WorldPos[0] + WorldPos[1] + WorldPos[2]) / 3.0;

    // Emit the triangle with flat varyings
    FaceNormalVS = nvs;
    FaceCenterVS = centerVS;
    gl_Position = gl_in[0].gl_Position; EmitVertex();
    gl_Position = gl_in[1].gl_Position; EmitVertex();
    gl_Position = gl_in[2].gl_Position; EmitVertex();
    EndPrimitive();
}
