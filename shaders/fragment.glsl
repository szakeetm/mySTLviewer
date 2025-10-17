#version 330 core

in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

void main()
{
    // Directional light from top-right-front
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
    vec3 norm = normalize(Normal);
    
    // Ambient lighting
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * vec3(1.0);
    
    // Diffuse lighting
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0);
    
    // Edge highlighting for better depth perception
    float edge = abs(dot(norm, vec3(0.0, 0.0, 1.0)));
    edge = pow(1.0 - edge, 2.0) * 0.2;
    
    // Base color (light gray)
    vec3 objectColor = vec3(0.7, 0.75, 0.8);
    
    vec3 result = (ambient + diffuse + edge) * objectColor;
    FragColor = vec4(result, 1.0);
}
