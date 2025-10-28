#version 330 core

in vec3 FragPos;   // world-space position
in vec3 Normal;    // interpolated normal (unused for flat shading)
in vec3 WorldPos;  // view-space position

out vec4 FragColor;

uniform bool isWireframe; // legacy, solid uses separate program

void main()
{
    if (isWireframe) {
        // Wireframe mode - pure white lines only
        FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        return;
    }

    // Solid mode - flat shading using face normal from screen-space derivatives
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
    // Compute face normal from derivatives of world-space position
    vec3 faceN = normalize(cross(dFdx(FragPos), dFdy(FragPos)));
    // Ensure consistent orientation w.r.t. front-facing
    vec3 norm = gl_FrontFacing ? faceN : -faceN;
    
    // Ambient lighting (slightly higher for brighter base)
    float ambientStrength = 0.45;
    vec3 ambient = ambientStrength * vec3(1.0);
    
    // Diffuse lighting
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0);
    
    // Specular lighting for more material feel (a touch stronger)
    vec3 viewDir = normalize(-WorldPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = 0.45 * spec * vec3(1.0);
    
    // Brighter grey material color
    vec3 objectColor = vec3(0.75, 0.75, 0.75);
    
    vec3 result = (ambient + diffuse + specular) * objectColor;
    FragColor = vec4(result, 1.0);
}
