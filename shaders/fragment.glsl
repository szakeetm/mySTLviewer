#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec3 WorldPos;

out vec4 FragColor;

uniform int renderMode; // 0 = solid, 1 = wireframe

void main()
{
    if (renderMode == 1) {
        // Wireframe mode - pure white
        FragColor = vec4(1.0, 1.0, 1.0, 1.0);
    } else {
        // Solid mode - grey material with lighting
        vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
        vec3 norm = normalize(Normal);
        
        // Ambient lighting
        float ambientStrength = 0.3;
        vec3 ambient = ambientStrength * vec3(1.0);
        
        // Diffuse lighting
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * vec3(1.0);
        
        // Specular lighting for more material feel
        vec3 viewDir = normalize(-WorldPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        vec3 specular = 0.3 * spec * vec3(1.0);
        
        // Grey material color
        vec3 objectColor = vec3(0.5, 0.5, 0.5);
        
        vec3 result = (ambient + diffuse + specular) * objectColor;
        FragColor = vec4(result, 1.0);
    }
}
