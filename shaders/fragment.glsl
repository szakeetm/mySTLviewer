#version 330 core

// For flat shading, we use values output by the geometry shader
flat in vec3 FaceNormalVS;  // face normal in view space
flat in vec3 FaceCenterVS;  // face center in view space

out vec4 FragColor;

uniform bool isWireframe; // legacy, solid uses separate program
uniform mat4 view;        // used to transform light dir to view space

void main()
{
    if (isWireframe) {
        // Wireframe mode - pure white lines only
        FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        return;
    }

    // Solid mode - flat shading using per-face values from geometry shader
    // Transform constant world-space light to view space (w=0 for direction)
    vec3 lightDirVS = normalize((view * vec4(0.5, 1.0, 0.5, 0.0)).xyz);
    vec3 norm = normalize(FaceNormalVS);
    
    // Ambient lighting (slightly higher for brighter base)
    float ambientStrength = 0.45;
    vec3 ambient = ambientStrength * vec3(1.0);
    
    // Diffuse lighting
    float diff = max(dot(norm, lightDirVS), 0.0);
    vec3 diffuse = diff * vec3(1.0);
    
    // Specular lighting for more material feel (a touch stronger)
    vec3 viewDir = normalize(-FaceCenterVS);
    vec3 reflectDir = reflect(-lightDirVS, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = 0.45 * spec * vec3(1.0);
    
    // Brighter grey material color
    vec3 objectColor = vec3(0.75, 0.75, 0.75);
    
    vec3 result = (ambient + diffuse + specular) * objectColor;
    FragColor = vec4(result, 1.0);
}
