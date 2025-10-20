#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec3 WorldPos;

out vec4 FragColor;

uniform bool isWireframe; // true = wireframe, false = solid

void main()
{
    if (isWireframe) {
        // Wireframe mode - pure white lines only
        FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        return;
    }

    // Solid mode - grey material with lighting
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
    vec3 norm = normalize(Normal);
    
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
